param(
	[int] $Port = 2543,
	[int] $StartupTimeoutSeconds = 90,
	[int] $ManualTimeoutSeconds = 0,
	[switch] $Repack,
	[switch] $ConvertConfig,
	[bool] $EnableDebugCommands = $true,
	[switch] $StartClient,
	[switch] $NoBattleEyeClient,
	[switch] $FailOnConfigWarnings,
	[switch] $KeepServerRunning
)

$ErrorActionPreference = "Stop"

$Repo = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrepareScript = Join-Path $Repo "Start-StandaloneNamalskTest.ps1"
$ClientScript = Join-Path $Repo "Start-StandaloneNamalskClient.ps1"
$ServerExe = "C:\Program Files (x86)\Steam\steamapps\common\DayZServer\DayZServer_x64.exe"
$ServerDir = Split-Path -Parent $ServerExe
$ServerTest = Join-Path $Repo "build\server-test"
$Profile = Join-Path $ServerTest "profile-local-standalone-namalsk"
$Config = Join-Path $ServerTest "serverDZ.standalone-namalsk.cfg"
$BattleEyePath = Join-Path $Profile "BattlEye"

$RequiredLogPatterns = @(
	"DAFDeathmatch: settings loaded",
	"DAFDeathmatch: loaded \d+ arenas",
	"DAFDeathmatch: loaded \d+ loadout pools",
	"DAFDeathmatch: config report \(startup\)",
	"DAFDeathmatch: config flags \(startup\)",
	"DAFDeathmatch: discord flags \(startup\)",
	"DAFDeathmatch: voting flags \(startup\)",
	"DAFDeathmatch: .+ round started in arena"
)

$ErrorLogPatterns = @(
	"Cannot compile",
	"SCRIPT\s+\(E\)",
	"NULL pointer to instance",
	"Can't compile",
	"Can't find variable",
	"Undefined function",
	"Function is marked as override, but there is no function with this name in the base class"
)

function Assert-Path {
	param(
		[Parameter(Mandatory)] [string] $Path,
		[Parameter(Mandatory)] [string] $Description
	)

	if (-not (Test-Path -LiteralPath $Path)) {
		throw "$Description was not found: $Path"
	}
}

function Get-LatestScriptLog {
	if (-not (Test-Path -LiteralPath $Profile)) {
		return $null
	}

	return Get-ChildItem -LiteralPath $Profile -Filter "script_*.log" |
		Sort-Object LastWriteTime -Descending |
		Select-Object -First 1
}

function Read-LogText {
	param([Parameter(Mandatory)] [System.IO.FileInfo] $Log)

	try {
		return Get-Content -LiteralPath $Log.FullName -Raw -ErrorAction Stop
	} catch {
		return ""
	}
}

function Test-PatternSet {
	param(
		[Parameter(Mandatory)] [string] $Text,
		[Parameter(Mandatory)] [string[]] $Patterns
	)

	$missing = @()
	foreach ($pattern in $Patterns) {
		if ($Text -notmatch $pattern) {
			$missing += $pattern
		}
	}

	return $missing
}

function Stop-OwnedServer {
	param([System.Diagnostics.Process] $Process)

	if (-not $Process) {
		return
	}

	try {
		$Process.Refresh()
		if (-not $Process.HasExited) {
			Stop-Process -Id $Process.Id -Force
		}
	} catch {
		Write-Warning "Could not stop server process $($Process.Id): $_"
	}
}

function Write-Result {
	param(
		[Parameter(Mandatory)] [string] $Name,
		[Parameter(Mandatory)] [bool] $Passed,
		[string] $Detail = ""
	)

	if ($Passed) {
		Write-Host "[PASS] $Name" -ForegroundColor Green
	} else {
		Write-Host "[FAIL] $Name" -ForegroundColor Red
	}

	if ($Detail -ne "") {
		Write-Host "       $Detail"
	}
}

Assert-Path $PrepareScript "Standalone server prepare script"
Assert-Path $ServerExe "DayZ server executable"

Write-Host "Preparing standalone E2E environment..." -ForegroundColor Cyan
$prepareArgs = @{
	Port = $Port
	NoStart = $true
	NoPause = $true
	EnableDebugCommands = $EnableDebugCommands
}
if ($Repack) {
	$prepareArgs.Repack = $true
}
if ($ConvertConfig) {
	$prepareArgs.ConvertConfig = $true
}

& $PrepareScript @prepareArgs

Assert-Path $Config "Standalone server config"
Assert-Path $BattleEyePath "Local BattlEye path"

$modList = @(
	Join-Path $ServerTest "@NamalskIsland"
	Join-Path $ServerTest "@NamalskSurvival"
	Join-Path $ServerTest "@DAFDeathmatch"
) -join ";"

$serverArgs = @(
	"-config=$Config",
	"-port=$Port",
	"-profiles=$Profile",
	"-doLogs",
	"-adminLog",
	"-netLog",
	"-freezeCheck",
	"-BEpath=$BattleEyePath",
	"-mod=$modList"
)

$startedAfter = Get-Date
Write-Host "Starting DayZ server for E2E checks..." -ForegroundColor Cyan
$serverProcess = Start-Process -FilePath $ServerExe -ArgumentList $serverArgs -WorkingDirectory $ServerDir -WindowStyle Hidden -PassThru
Write-Host "Server PID: $($serverProcess.Id)"

$log = $null
$logText = ""
$missingRequired = $RequiredLogPatterns
$deadline = (Get-Date).AddSeconds($StartupTimeoutSeconds)

try {
	while ((Get-Date) -lt $deadline) {
		$serverProcess.Refresh()
		if ($serverProcess.HasExited) {
			break
		}

		$candidate = Get-LatestScriptLog
		if ($candidate -and $candidate.LastWriteTime -ge $startedAfter.AddSeconds(-2)) {
			$log = $candidate
			$logText = Read-LogText $log
			$missingRequired = Test-PatternSet $logText $RequiredLogPatterns

			if ($missingRequired.Count -eq 0) {
				break
			}
		}

		Start-Sleep -Seconds 2
	}

	if (-not $log) {
		throw "No script log was produced under $Profile"
	}

	$logText = Read-LogText $log
	$missingRequired = Test-PatternSet $logText $RequiredLogPatterns
	$errorMatches = @()
	foreach ($pattern in $ErrorLogPatterns) {
		$matches = [regex]::Matches($logText, $pattern)
		if ($matches.Count -gt 0) {
			$errorMatches += $pattern
		}
	}

	$configWarningCount = ([regex]::Matches($logText, "DAFDeathmatch config warning:")).Count

	Write-Host ""
	Write-Host "E2E startup results" -ForegroundColor Cyan
	Write-Result "Script log produced" ($null -ne $log) $log.FullName
	Write-Result "Server stayed up through startup check" (-not $serverProcess.HasExited) "PID $($serverProcess.Id)"
	$missingDetail = ""
	if ($missingRequired.Count -gt 0) {
		$missingDetail = "Missing: " + ($missingRequired -join "; ")
	}
	Write-Result "Required DAFDeathmatch startup markers" ($missingRequired.Count -eq 0) $missingDetail
	Write-Result "No script compile/runtime error markers" ($errorMatches.Count -eq 0) (($errorMatches -join "; "))

	if ($FailOnConfigWarnings) {
		Write-Result "No DAFDeathmatch config warnings" ($configWarningCount -eq 0) "$configWarningCount warning(s)"
	} else {
		Write-Result "Config warnings counted" $true "$configWarningCount warning(s); use -FailOnConfigWarnings to make these fail"
	}

	if ($missingRequired.Count -gt 0 -or $errorMatches.Count -gt 0 -or ($FailOnConfigWarnings -and $configWarningCount -gt 0) -or $serverProcess.HasExited) {
		throw "Standalone E2E startup checks failed"
	}

	if ($StartClient) {
		Assert-Path $ClientScript "Standalone client launch script"
		$clientArgs = @{
			HostAddress = "127.0.0.1"
			Port = $Port
		}
		if ($NoBattleEyeClient) {
			$clientArgs.NoBattleEye = $true
		}

		Write-Host ""
		Write-Host "Starting client for manual E2E checks..." -ForegroundColor Cyan
		& $ClientScript @clientArgs
	}

	if ($ManualTimeoutSeconds -gt 0) {
		Write-Host ""
		Write-Host "Manual E2E window: $ManualTimeoutSeconds seconds" -ForegroundColor Cyan
		Write-Host "Suggested in-game checks:"
		Write-Host "  @status"
		Write-Host "  @inspect"
		Write-Host "  @spawnreport"
		Write-Host "  @endvote then @vote 1"
		Write-Host "  @arenavote Lubjansk then @vote 1"
		Write-Host "  @roundvote snipers then @vote 1"
		Start-Sleep -Seconds $ManualTimeoutSeconds
	}

	Write-Host ""
	Write-Host "Standalone E2E checks passed." -ForegroundColor Green
} finally {
	if ($KeepServerRunning -and $serverProcess) {
		$serverProcess.Refresh()
		if ($serverProcess.HasExited) {
			Write-Host "KeepServerRunning set, but server process already exited." -ForegroundColor Yellow
		} else {
			Write-Host "KeepServerRunning set; leaving server PID $($serverProcess.Id) running." -ForegroundColor Yellow
		}
	} else {
		Stop-OwnedServer $serverProcess
		Write-Host "Stopped E2E server process." -ForegroundColor Yellow
	}
}

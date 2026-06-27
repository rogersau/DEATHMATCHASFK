param(
    [int] $Port = 2543,
    [switch] $Repack,
    [switch] $ConvertConfig,
    [switch] $NoStart,
    [switch] $NoPause
)

$ErrorActionPreference = "Stop"

$Repo = Split-Path -Parent $MyInvocation.MyCommand.Path
$ServerExe = "C:\Program Files (x86)\Steam\steamapps\common\DayZServer\DayZServer_x64.exe"
$ServerDir = Split-Path -Parent $ServerExe
$ServerTest = Join-Path $Repo "build\server-test"
$Profile = Join-Path $ServerTest "profile-local-standalone-namalsk"
$BaseConfig = Join-Path $ServerTest "serverDZ.prd1-namalsk.cfg"
$Config = Join-Path $ServerTest "serverDZ.standalone-namalsk.cfg"
$ServerMission = Join-Path $ServerDir "mpmissions\deathmatch.namalsk"
$BackupMission = Join-Path $Repo "build\rar-extract\deathmatch-backup\deathmatch back up\Deathmatch\mpmissions\deathmatch.namalsk"
$StandaloneInit = Join-Path $Repo "mpmissions\deathmatch.namalsk\init.c"
$ConvertedConfig = Join-Path $Repo "build\converted-deathmatch\DAFDeathmatch"
$Converter = Join-Path $Repo "tools\Convert-CrimsonConfig.ps1"
$BattleEyePath = Join-Path $Profile "BattlEye"
$AddonBuilder = "C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools\Bin\AddonBuilder\AddonBuilder.exe"
$DayZTools = "C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools"

$WorkshopMods = @{
    "@NamalskIsland" = "C:\Program Files (x86)\Steam\steamapps\workshop\content\221100\2289456201"
    "@NamalskSurvival" = "C:\Program Files (x86)\Steam\steamapps\workshop\content\221100\2289461232"
}

function Assert-Path {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Description
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description was not found: $Path"
    }
}

function Ensure-Directory {
    param([Parameter(Mandatory)] [string] $Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Ensure-Junction {
    param(
        [Parameter(Mandatory)] [string] $Link,
        [Parameter(Mandatory)] [string] $Target
    )

    if (Test-Path -LiteralPath $Link) {
        return
    }

    Assert-Path $Target "Workshop mod folder"
    New-Item -ItemType Junction -Path $Link -Target $Target | Out-Null
}

function Repack-Addon {
    param([Parameter(Mandatory)] [string] $Name)

    Assert-Path $AddonBuilder "AddonBuilder"
    $Source = Join-Path $Repo $Name
    Assert-Path $Source "$Name source folder"
    Ensure-Directory (Join-Path $Repo "build\Addons")

    & $AddonBuilder $Source (Join-Path $Repo "build\Addons") -packonly -clear "-project=$Repo" "-toolsDirectory=$DayZTools"
    if ($LASTEXITCODE -ne 0) {
        throw "AddonBuilder failed for $Name with exit code $LASTEXITCODE"
    }
}

function Copy-Addon {
    param([Parameter(Mandatory)] [string] $Name)

    $Pbo = Join-Path $Repo "build\Addons\$Name.pbo"
    if (-not (Test-Path -LiteralPath $Pbo)) {
        $Pbo = Join-Path $ServerTest "@$Name\Addons\$Name.pbo"
    }

	Assert-Path $Pbo "$Name PBO"
	$Dest = Join-Path $ServerTest "@$Name\Addons"
	Ensure-Directory $Dest
	$DestPbo = Join-Path $Dest "$Name.pbo"
	if ((Resolve-Path -LiteralPath $Pbo).Path -eq (Resolve-Path -LiteralPath $DestPbo -ErrorAction SilentlyContinue).Path) {
		return
	}

	Copy-Item -LiteralPath $Pbo -Destination $DestPbo -Force
}

function Ensure-LocalBattleEyeDisabled {
	Assert-Path $BaseConfig "Base server config"
	$Content = Get-Content -LiteralPath $BaseConfig -Raw
	if ($Content -match '(?m)^\s*BattlEye\s*=') {
		$Content = [regex]::Replace($Content, '(?m)^\s*BattlEye\s*=.*?;', 'BattlEye = 0;')
	} else {
		$Content = "BattlEye = 0;`r`n" + $Content
	}

	Set-Content -LiteralPath $Config -Value $Content -Encoding ASCII
}

Assert-Path $ServerExe "DayZ server executable"
Assert-Path $StandaloneInit "Standalone deathmatch.namalsk init.c"
Ensure-LocalBattleEyeDisabled
Ensure-Directory $BattleEyePath

foreach ($Entry in $WorkshopMods.GetEnumerator()) {
    Ensure-Junction (Join-Path $ServerTest $Entry.Key) $Entry.Value
}

if ($Repack) {
    Repack-Addon "DAFImprovements"
    Repack-Addon "DAFDeathmatch"
}

Copy-Addon "DAFImprovements"
Copy-Addon "DAFDeathmatch"

if (-not (Test-Path -LiteralPath $ServerMission)) {
    Assert-Path $BackupMission "deathmatch.namalsk backup mission"
    Copy-Item -LiteralPath $BackupMission -Destination $ServerMission -Recurse -Force
}

Copy-Item -LiteralPath $StandaloneInit -Destination (Join-Path $ServerMission "init.c") -Force

if ($ConvertConfig -or -not (Test-Path -LiteralPath (Join-Path $ConvertedConfig "settings.json"))) {
    Assert-Path $Converter "Crimson config converter"
    & $Converter
}

Ensure-Directory (Join-Path $Profile "DAFDeathmatch")
foreach ($FileName in @("settings.json", "arenas.json", "loadouts.json")) {
    Copy-Item -LiteralPath (Join-Path $ConvertedConfig $FileName) -Destination (Join-Path $Profile "DAFDeathmatch\$FileName") -Force
}

$ModList = @(
    Join-Path $ServerTest "@NamalskIsland"
    Join-Path $ServerTest "@NamalskSurvival"
    Join-Path $ServerTest "@DAFImprovements"
    Join-Path $ServerTest "@DAFDeathmatch"
) -join ";"

$Args = @(
    "-config=$Config"
    "-port=$Port"
    "-profiles=$Profile"
    "-doLogs"
    "-adminLog"
	"-netLog"
	"-freezeCheck"
	"-BEpath=$BattleEyePath"
	"-mod=$ModList"
)

Write-Host "Prepared DAF standalone Namalsk test server." -ForegroundColor Cyan
Write-Host "Profile: $Profile"
Write-Host "Mission: $ServerMission"
Write-Host "Port: $Port"
Write-Host ""

if ($NoStart) {
    Write-Host "NoStart set; not launching server." -ForegroundColor Yellow
    return
}

Write-Host "Starting DAF standalone Namalsk test server..." -ForegroundColor Cyan

Push-Location $ServerDir
try {
    & $ServerExe @Args
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "Server exited. Logs are in:" -ForegroundColor Yellow
Write-Host $Profile

if (-not $NoPause) {
    Read-Host "Press Enter to close"
}

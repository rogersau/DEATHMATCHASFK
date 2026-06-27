param(
	[string] $HostAddress = "127.0.0.1",
	[int] $Port = 2543,
	[switch] $NoBattleEye,
	[switch] $NoConnect
)

$ErrorActionPreference = "Stop"

$Repo = Split-Path -Parent $MyInvocation.MyCommand.Path
$ClientExe = "C:\Program Files (x86)\Steam\steamapps\common\DayZ\DayZ_BE.exe"
if ($NoBattleEye) {
	$ClientExe = "C:\Program Files (x86)\Steam\steamapps\common\DayZ\DayZ_x64.exe"
}
$ServerTest = Join-Path $Repo "build\server-test"

$Mods = @(
    Join-Path $ServerTest "@NamalskIsland"
    Join-Path $ServerTest "@NamalskSurvival"
    Join-Path $ServerTest "@DAFImprovements"
    Join-Path $ServerTest "@DAFDeathmatch"
)

if (-not (Test-Path -LiteralPath $ClientExe)) {
    throw "DayZ client executable was not found: $ClientExe"
}

foreach ($Mod in $Mods) {
    if (-not (Test-Path -LiteralPath $Mod)) {
        throw "Mod folder was not found: $Mod"
    }
}

$Args = @(
    "-mod=$($Mods -join ';')"
)

if (-not $NoConnect) {
    $Args += "-connect=$HostAddress"
    $Args += "-port=$Port"
}

Write-Host "Starting DayZ standalone Namalsk client..." -ForegroundColor Cyan
Write-Host "Mods:"
foreach ($Mod in $Mods) {
    Write-Host "  $Mod"
}

if (-not $NoConnect) {
    Write-Host "Connecting to $HostAddress`:$Port"
}

Start-Process -FilePath $ClientExe -ArgumentList $Args

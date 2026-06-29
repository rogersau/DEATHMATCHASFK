param(
    [string] $HostAddress = "127.0.0.1",
    [int] $Port = 2536,
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

Assert-Path $ClientExe "DayZ client executable"
foreach ($Mod in $Mods) {
    Assert-Path $Mod "Client mod folder"
}

$Args = @(
    "-mod=$($Mods -join ';')"
)

if (-not $NoConnect) {
    $Args += "-connect=$HostAddress"
    $Args += "-port=$Port"
}

Write-Host "Starting DayZ Crimson Namalsk client..." -ForegroundColor Cyan
Write-Host "Mods:"
foreach ($Mod in $Mods) {
    Write-Host "  $Mod"
}

if (-not $NoConnect) {
    Write-Host "Connecting to $HostAddress`:$Port"
}

Start-Process -FilePath $ClientExe -ArgumentList $Args

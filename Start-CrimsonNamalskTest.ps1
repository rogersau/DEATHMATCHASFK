param(
    [int] $Port = 2536,
    [switch] $Repack,
    [switch] $NoPause
)

$ErrorActionPreference = "Stop"

$Repo = Split-Path -Parent $MyInvocation.MyCommand.Path
$ServerExe = "C:\Program Files (x86)\Steam\steamapps\common\DayZServer\DayZServer_x64.exe"
$ServerDir = Split-Path -Parent $ServerExe
$ServerTest = Join-Path $Repo "build\server-test"
$Profile = Join-Path $ServerTest "profile-local-crimson-namalsk"
$Config = Join-Path $ServerTest "serverDZ.prd1-namalsk.cfg"
$BackupRoot = Join-Path $Repo "build\rar-extract\deathmatch-backup\deathmatch back up\Deathmatch"
$BackupProfile = Join-Path $BackupRoot "profiles\deathmatch"
$BackupMission = Join-Path $BackupRoot "mpmissions\deathmatch.namalsk"
$ServerMission = Join-Path $ServerDir "mpmissions\deathmatch.namalsk"
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

    & $AddonBuilder $Source (Join-Path $Repo "build\Addons") -packonly -clear "-project=$Repo" "-toolsDirectory=$DayZTools"
    if ($LASTEXITCODE -ne 0) {
        throw "AddonBuilder failed for $Name with exit code $LASTEXITCODE"
    }
}

function Copy-Addon {
    param([Parameter(Mandatory)] [string] $Name)

    $Pbo = Join-Path $Repo "build\Addons\$Name.pbo"
    if (-not (Test-Path -LiteralPath $Pbo)) {
        Write-Warning "$Name.pbo was not found in build\Addons; skipping copy."
        return
    }

    $Dest = Join-Path $ServerTest "@$Name\Addons"
    Ensure-Directory $Dest
    Copy-Item -LiteralPath $Pbo -Destination (Join-Path $Dest "$Name.pbo") -Force
}

Assert-Path $ServerExe "DayZ server executable"
Assert-Path $Config "Server config"

foreach ($Entry in $WorkshopMods.GetEnumerator()) {
    Ensure-Junction (Join-Path $ServerTest $Entry.Key) $Entry.Value
}

if ($Repack) {
    Repack-Addon "DAFImprovements"
    Repack-Addon "DAFServerImprovements"
}

Copy-Addon "DAFImprovements"
Copy-Addon "DAFServerImprovements"

if (-not (Test-Path -LiteralPath $ServerMission)) {
    Assert-Path $BackupMission "deathmatch.namalsk backup mission"
    Copy-Item -LiteralPath $BackupMission -Destination $ServerMission -Recurse -Force
}

Ensure-Directory (Join-Path $Profile "deathmatch")

foreach ($FileName in @("settings.json", "events.json")) {
    $Source = Join-Path $BackupProfile $FileName
    if (Test-Path -LiteralPath $Source) {
        Copy-Item -LiteralPath $Source -Destination (Join-Path $Profile "deathmatch\$FileName") -Force
    } else {
        Write-Warning "Backup profile file missing: $Source"
    }
}

$ModList = @(
    Join-Path $ServerTest "@NamalskIsland"
    Join-Path $ServerTest "@NamalskSurvival"
    Join-Path $ServerTest "@DAFImprovements"
) -join ";"

$ServerModList = @(
    Join-Path $ServerTest "@CrimsonZamboniDeathmatch"
    Join-Path $ServerTest "@DAFServerImprovements"
) -join ";"

$Args = @(
    "-config=$Config"
    "-port=$Port"
    "-profiles=$Profile"
    "-doLogs"
    "-adminLog"
    "-netLog"
    "-freezeCheck"
    "-mod=$ModList"
    "-serverMod=$ServerModList"
)

Write-Host "Starting DAF Crimson Namalsk test server..." -ForegroundColor Cyan
Write-Host "Profile: $Profile"
Write-Host "Port: $Port"
Write-Host ""

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

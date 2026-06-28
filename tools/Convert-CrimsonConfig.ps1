param(
    [string] $CrimsonRoot = "build\rar-extract\deathmatch-backup\deathmatch back up\Deathmatch",
    [string] $CrimsonProfilePath,
    [string] $CrimsonMissionPath,
    [string] $OutputPath = "build\converted-deathmatch\DAFDeathmatch"
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath([string] $Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path (Get-Location) $Path
}

function Read-JsonFile([string] $Path) {
    if (!(Test-Path -LiteralPath $Path)) {
        throw "Missing JSON file: $Path"
    }

    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function As-Array($Value) {
    if ($null -eq $Value) {
        return @()
    }

    if ($Value -is [System.Array]) {
        return @($Value)
    }

    return @($Value)
}

function Split-Choices($Value) {
    $items = @()

    foreach ($entry in (As-Array $Value)) {
        if ($null -eq $entry) {
            continue
        }

        if ($entry -is [string]) {
            foreach ($piece in ($entry -split "\s+")) {
                if (![string]::IsNullOrWhiteSpace($piece)) {
                    $items += $piece.Trim()
                }
            }
            continue
        }

        foreach ($piece in (As-Array $entry)) {
            if ($null -ne $piece -and ![string]::IsNullOrWhiteSpace([string] $piece)) {
                $items += ([string] $piece).Trim()
            }
        }
    }

    return @($items)
}

function Convert-AttachmentGroups($Attachments) {
	$groups = @()

	if ($null -eq $Attachments) {
		return @()
	}

	foreach ($group in $Attachments) {
		$choices = @(Split-Choices $group)
		if ($choices.Count -gt 0) {
			$groups += , @($choices)
        }
    }

    return @($groups)
}

function Convert-WeaponPool($Weapon, [string] $FallbackName) {
    if ($null -eq $Weapon) {
        return $null
    }

    $variants = @(Split-Choices $Weapon.variants)
    if ($variants.Count -eq 0) {
        return $null
    }

    $name = $FallbackName
    if ($variants.Count -gt 0) {
        $name = $variants[0]
    }

    [ordered] @{
        name = $name
        weight = 1
        variants = @($variants)
        attachments = @(Convert-AttachmentGroups $Weapon.attachments)
        accessories = @(Split-Choices $Weapon.accessories)
    }
}

function Convert-WeaponPools($Weapons, [string] $FallbackPrefix) {
    $converted = @()
    $index = 1

    foreach ($weapon in (As-Array $Weapons)) {
        $poolWeapon = Convert-WeaponPool $weapon "$FallbackPrefix-$index"
        if ($null -ne $poolWeapon) {
            $converted += $poolWeapon
        }
        $index++
    }

    return @($converted)
}

function Convert-OutfitGroups($Outfits) {
    $groups = @()
    if ($null -eq $Outfits) {
        return @()
    }

    $source = $Outfits
    if ($Outfits.Count -gt 0 -and $Outfits[0] -is [System.Array]) {
        $source = $Outfits[0]
    }

    foreach ($slot in (As-Array $source)) {
        if ($null -eq $slot) {
            continue
        }

        $choices = @(Split-Choices $slot.choices)
        if ($choices.Count -gt 0) {
            $groups += , @($choices)
        }
    }

    return @($groups)
}

function Convert-AdminIds($Admins) {
    $ids = @()

    foreach ($admin in (As-Array $Admins)) {
        if ($admin -is [string] -and ![string]::IsNullOrWhiteSpace($admin)) {
            $ids += $admin
        } elseif ($null -ne $admin.id -and ![string]::IsNullOrWhiteSpace([string] $admin.id)) {
            $ids += [string] $admin.id
        }
    }

    return @($ids)
}

function Convert-Arena($Arena) {
    if ($null -eq $Arena -or [string]::IsNullOrWhiteSpace([string] $Arena.name)) {
        return $null
    }

    $converted = [ordered] @{
        name = [string] $Arena.name
        center = @(As-Array $Arena.center)
        radius = [float] $Arena.radius
        rectangular = [bool] $Arena.rectangular
        xSize = [float] $Arena.xSize
        zSize = [float] $Arena.zSize
        minimumPlayers = [int] $Arena.minimumPlayers
        maximumPlayers = [int] $Arena.maximumPlayers
        playerSpawns = @(As-Array $Arena.playerSpawns)
    }

    if ($converted.radius -le 0 -and $converted.rectangular -and $converted.xSize -gt 0 -and $converted.zSize -gt 0) {
        $converted.radius = [Math]::Sqrt(($converted.xSize * $converted.xSize) + ($converted.zSize * $converted.zSize)) / 2
    }

    return $converted
}

function Get-DisplayName([string] $Name, $CustomEvent) {
    if ($null -ne $CustomEvent -and ![string]::IsNullOrWhiteSpace([string] $CustomEvent.displayName)) {
        return [string] $CustomEvent.displayName
    }

    if ($Name -eq "normal") {
        return "Normal"
    }

    return (Get-Culture).TextInfo.ToTitleCase($Name)
}

function New-LoadoutEntry([string] $Name, $PrimaryWeapons, $SecondaryWeapons, $Outfits) {
    [ordered] @{
        name = "$Name-imported"
        weight = 1
        outfit = @(Convert-OutfitGroups $Outfits)
        primaryWeapons = @(Convert-WeaponPools $PrimaryWeapons "$Name-primary")
        secondaryWeapons = @(Convert-WeaponPools $SecondaryWeapons "$Name-secondary")
        items = @(
            [ordered] @{ type = "BandageDressing"; quickbarSlot = 2 },
            [ordered] @{ type = "Morphine"; quickbarSlot = 3 },
            [ordered] @{ type = "SalineBagIV"; quickbarSlot = 4 }
        )
    }
}

$root = Resolve-RepoPath $CrimsonRoot
if ([string]::IsNullOrWhiteSpace($CrimsonProfilePath)) {
    $CrimsonProfilePath = Join-Path $root "profiles\deathmatch"
}
if ([string]::IsNullOrWhiteSpace($CrimsonMissionPath)) {
    $CrimsonMissionPath = Join-Path $root "mpmissions\deathmatch.namalsk"
}

$profilePath = Resolve-RepoPath $CrimsonProfilePath
$missionPath = Resolve-RepoPath $CrimsonMissionPath
$outPath = Resolve-RepoPath $OutputPath

$settings = Read-JsonFile (Join-Path $profilePath "settings.json")
$eventsConfigPath = Join-Path $profilePath "events.json"
$eventsConfig = $null
if (Test-Path -LiteralPath $eventsConfigPath) {
    $eventsConfig = Read-JsonFile $eventsConfigPath
}

$arenaSource = Read-JsonFile (Join-Path $missionPath "arenas.json")
$customArenaPath = Join-Path $missionPath "custom-arenas.json"
$customArenaSource = $null
if (Test-Path -LiteralPath $customArenaPath) {
    $customArenaSource = Read-JsonFile $customArenaPath
}

$missionInitPath = Join-Path $missionPath "init.c"
$missionRemovedArenas = @()
if (Test-Path -LiteralPath $missionInitPath) {
    $missionInit = Get-Content -LiteralPath $missionInitPath -Raw
    foreach ($match in [regex]::Matches($missionInit, 'RemoveArena\("([^"]+)"\)')) {
        $missionRemovedArenas += $match.Groups[1].Value
    }

    if ($missionInit -match 'DMArena|DMArenaRegistry|OnRoundStart|OnRoundEnd') {
        Write-Warning "Mission init contains Crimson Deathmatch hooks/classes. Use a clean standalone mission init when testing DAFDeathmatch."
    }
}

$customEventsByName = @{}
if ($null -ne $eventsConfig) {
    foreach ($custom in (As-Array $eventsConfig.custom)) {
        if ($null -ne $custom.name) {
            $customEventsByName[[string] $custom.name] = $custom
        }
    }
}

$eventChance = [double] $settings.events.chance
$roundTypes = @()
$roundTypes += [ordered] @{
    name = "normal"
    displayName = "Normal"
    weight = [int] [Math]::Round((1 - $eventChance) * 100, 0, [MidpointRounding]::AwayFromZero)
    roundMinutes = [int] $settings.roundMinutes
    loadoutPool = "normal"
    arenaNames = @()
}

foreach ($eventType in (As-Array $settings.events.types)) {
    $name = [string] $eventType.name
    $chance = [double] $eventType.chance
    if ([string]::IsNullOrWhiteSpace($name) -or $chance -le 0) {
        if (![string]::IsNullOrWhiteSpace($name)) {
            Write-Warning "Skipping disabled event '$name'."
        }
        continue
    }

    $canonicalName = $name.ToLowerInvariant()
    $custom = $customEventsByName[$name]
    if ($null -eq $custom) {
        $custom = $customEventsByName[$canonicalName]
    }

    $roundTypes += [ordered] @{
        name = $canonicalName
        displayName = Get-DisplayName $canonicalName $custom
        weight = [int] [Math]::Round(($eventChance * $chance) * 100, 0, [MidpointRounding]::AwayFromZero)
        roundMinutes = [int] $eventType.roundMinutes
        loadoutPool = $canonicalName
        arenaNames = @(Split-Choices $eventType.arenas)
    }
}

$arenasByName = [ordered] @{}
foreach ($arena in (As-Array $arenaSource.arenas)) {
    $converted = Convert-Arena $arena
    if ($null -ne $converted) {
        $arenasByName[$converted.name] = $converted
    }
}
if ($null -ne $customArenaSource) {
    foreach ($arena in (As-Array $customArenaSource.arenas)) {
        $converted = Convert-Arena $arena
        if ($null -ne $converted) {
            $arenasByName[$converted.name] = $converted
        }
    }
}

$excluded = @(Split-Choices $settings.excludeArenas)
foreach ($removedArena in $missionRemovedArenas) {
    if ($excluded -notcontains $removedArena) {
        Write-Warning "Adding mission-removed arena '$removedArena' to standalone excludedArenas."
        $excluded += $removedArena
    }
}
$arenaRotation = @()
foreach ($arenaName in $arenasByName.Keys) {
    if ($excluded -notcontains $arenaName) {
        $arenaRotation += $arenaName
    }
}

$loadoutPools = @()
$loadoutPools += [ordered] @{
    name = "normal"
    entries = @(New-LoadoutEntry "normal" $settings.primaryWeapons $settings.secondaryWeapons $settings.outfits)
}

foreach ($roundType in $roundTypes) {
    $poolName = [string] $roundType.loadoutPool
    if ($poolName -eq "normal") {
        continue
    }

    $custom = $customEventsByName[$poolName]
    if ($null -eq $custom) {
        Write-Warning "No custom event loadout found for '$poolName'; using normal weapons."
        $loadoutPools += [ordered] @{
            name = $poolName
            entries = @(New-LoadoutEntry $poolName $settings.primaryWeapons $settings.secondaryWeapons $settings.outfits)
        }
        continue
    }

    $loadoutPools += [ordered] @{
        name = $poolName
        entries = @(New-LoadoutEntry $poolName $custom.primaryWeapons $custom.secondaryWeapons $custom.outfits)
    }
}

$standaloneSettings = [ordered] @{
    roundMinutes = [int] $settings.roundMinutes
    respawnSeconds = 3
    roundTransitionSeconds = 10
    autoRespawn = [bool] $settings.autoRespawn
    commandCharacter = [string] $settings.commandCharacter
    version = "DAFDeathmatch migrated config"
    spawnArenaWalls = !([bool] $settings.disableWalls)
    arenaWallType = "Land_ConcreteBlock"
    arenaWallSegments = 24
    corpseCleanupSeconds = 5
    deathDropCleanupSeconds = 60
    enableDiscordKillfeed = $false
    discordKillfeedWebhookUrl = ""
    enableDiscordServerEvents = $false
    discordServerEventsWebhookUrl = ""
    discordServerName = ""
    discordSuppressEmbeds = $false
    arenaRotation = @($arenaRotation)
    excludedArenas = @($excluded)
    admins = @(Convert-AdminIds $settings.admins)
    roundTypes = @($roundTypes)
}

New-Item -ItemType Directory -Force -Path $outPath | Out-Null
$standaloneSettings | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath (Join-Path $outPath "settings.json") -Encoding UTF8
@($arenasByName.Values) | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath (Join-Path $outPath "arenas.json") -Encoding UTF8
@($loadoutPools) | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath (Join-Path $outPath "loadouts.json") -Encoding UTF8

Write-Host "Wrote standalone DAFDeathmatch config:"
Write-Host "  $outPath"
Write-Host "Round types: $($roundTypes.Count)"
Write-Host "Arenas: $($arenasByName.Count) ($($excluded.Count) excluded from rotation)"
Write-Host "Loadout pools: $($loadoutPools.Count)"

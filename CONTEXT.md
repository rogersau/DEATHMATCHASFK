# DEATHMATCHASFK Context

This repo contains DayZ deathmatch mods for the DAF server. The current live path is a Crimson Zamboni Deathmatch compatibility stack, while the long-term direction is a standalone `DAFDeathmatch` client/server mod that owns the game mode outright.

## Current Addons

### `DAFImprovements`

Historically this is the shared client/server addon. It contains client-visible HUD and generic DayZ gameplay improvements:

- Killfeed UI and HUD widgets
- Player count UI
- Top-center round time remaining HUD
- Active VOIP speaker overlay driven by client-received VON notifications
- Per-round kill/death display in the killfeed
- Complete weapon display parsing for killfeed events
- Client-side and server-side footwear drop/removal prevention
- IV saline full-heal behavior with an 8 second apply time
- IV saline drop/stash prevention and delete-on-death behavior
- Carried inventory damage protection
- Infected damage blocking
- Optional weapon fire-mode preference logic, disabled by default
- Weapon jam prevention for the DAF PvP loop
- Client-side respawn cursor repair RPC handling

`DAFImprovements` must not reference Crimson Zamboni Deathmatch classes. Clients load this addon, and clients do not have the reference Deathmatch mod installed.

### `DAFServerImprovements`

This is the server-only compatibility addon for the existing Crimson Zamboni Deathmatch stack. Keep it small and treat it as transitional bridge code.

It is allowed to reference Deathmatch-specific classes, callbacks, or objects, including:

- `DMAutoRespawn`
- `DMDiscordWebhook`
- `DMScoreBoard`
- `MissionServer` round hooks from the Deathmatch mod
- `MissionBaseWorld` round hooks from the Deathmatch mod
- Deathmatch command/autorespawn behavior
- Discord killfeed/round-summary enhancements
- End-of-round highlights such as furthest shot and top weapon
- Direct Deathmatch scoreboard reset hook for round K/D reset reliability
- Persisted auto-respawn preferences
- Round timer RPC broadcasts that feed the shared HUD
- Cleanup rules tied to Deathmatch settings, such as deleting spawned infected when player count reaches `forceInfectedPlayerLimit`
- Low-pop magazine refill tied to `forceInfectedPlayerLimit`
- Deathmatch loadout post-processing, including bandage/morphine/saline quickbar slots and Easter Egg removal
- Server RPC sends that coordinate with `DAFImprovements` client handlers

The current Crimson Zamboni compatibility load shape is:

```text
-mod=@DAFImprovements
-serverMod=@CrimsonZamboniDeathmatch;@DAFServerImprovements
```

## Standalone Direction

`DAFDeathmatch` is the in-progress replacement for Crimson Zamboni Deathmatch.

The target final architecture is one opinionated client/server deathmatch mod:

```text
-mod=@DAFDeathmatch
```

During migration it may temporarily load beside `DAFImprovements`:

```text
-mod=@DAFImprovements;@DAFDeathmatch
```

Do not load `DAFDeathmatch` together with `CrimsonZamboniDeathmatch`. They both want to own round lifecycle, spawning, and scoring.

`DAFDeathmatch` should eventually absorb the useful gameplay improvements from `DAFImprovements` and `DAFServerImprovements`, then retire the bridge addons.

## Product Direction

DAFDeathmatch is opinionated. It should default to the live DAF server experience, because the goal is to replicate and improve the region's popular deathmatch server rather than ship a neutral framework. Config exists for tuning and disabling features, not for making the default bland.

The first playable cut is player-experience led:

- Arena import/conversion
- Round lifecycle
- Spawn/respawn
- Loadouts
- Killfeed
- K/D reset and display
- Basic commands
- Clean server RPT startup

Admin/server-ops polish such as Discord, votes, events, leaderboards, and advanced cleanup can follow after the core PvP loop is solid.

## Config Direction

Runtime `DAFDeathmatch` should read its own config only. Crimson Zamboni migration should be handled by an external converter, not by runtime compatibility code.

The Crimson migration converter is a one-off operator tool, not a polished product surface. Prefer a straightforward PowerShell entry point such as `tools/Convert-CrimsonConfig.ps1` that takes explicit input and output folders, writes standalone `settings.json`, `arenas.json`, and `loadouts.json`, and prints warnings for anything it cannot faithfully convert.

The converter should run locally and write to a separate output folder for inspection. It should not write directly into the live server profile by default; copying the generated config to the server is an operator step.

The first converter pass should migrate only standalone-owned essentials: admins, round timings, enabled round types and weights, excluded arena rotation, arena geometry and spawns, and loadout pools. Skip Crimson settings for systems standalone does not own yet, including Discord, infected behavior, crates, holiday props, cosmetics, and unused oddball fields.

Target config files:

```text
$profile:DAFDeathmatch/settings.json
$profile:DAFDeathmatch/arenas.json
$profile:DAFDeathmatch/loadouts.json
```

`settings.json` should own round behavior and round type selection. `loadouts.json` should own loadout pool definitions. `arenas.json` should own spatial arena data.

Round types are first-class and define the round flavor. Initial flavors:

- `normal`
- `snipers`
- `freshies`
- `juiced`

Round flavor is always visible on the HUD with the timer. Arena name is not shown on the HUD.

Round type selection should be weighted random with one anti-repeat reroll. Round types can restrict allowed arenas, but default to any arena.

Crimson Zamboni events should be converted into standalone round types, not preserved as a separate runtime event system. Standalone `DAFDeathmatch` uses one round-type concept for flavor, weighting, duration overrides, arena restrictions, and loadout pool selection.

When converting Crimson Zamboni event probabilities, preserve the effective chance of each round flavor. Crimson's global event chance becomes the total non-normal weight, while the leftover probability becomes `normal`. For the live Namalsk config, `events.chance = 0.35` with `snipers = 0.25`, `freshies = 0.25`, and `juiced = 0.5` converts to readable standalone weights of roughly `normal = 65`, `snipers = 9`, `freshies = 9`, and `juiced = 18`.

The converter should not carry disabled Crimson events into standalone config. Zero-chance events such as `Cowboy` and `Halloween` are discarded rather than preserved as disabled round types.

The converter should import both Crimson `arenas.json` and `custom-arenas.json`. Standalone `DAFDeathmatch` should support radius arenas and rectangular arenas rather than dropping custom arena shapes during migration.

When converting Crimson `excludeArenas`, preserve the arena definitions in standalone `arenas.json` but keep them out of active rotation through standalone settings/config. Excluded arenas are authored data, not migration garbage.

Arenas are a Crimson pain point and should remain open to future expansion behavior as player count grows. That dynamic arena expansion concept is not required for the first migration pass, but the standalone arena model should avoid choices that make it hard to add later.

## Loadouts

Loadout config should be simpler than Crimson Zamboni's config, while the loadout engine should be more flexible.

Use named loadout pools with weighted entries. Players roll independently inside the selected round type's loadout pool. Weapons can overlap between round types.

Standalone loadouts should keep a familiar Crimson-like weapon pool model rather than expanding every possible weapon combination into separate fixed loadout entries. Pools should be readable balancing units, with weapons able to describe variants, attachments, accessories, magazines, and related random-choice groups in a structured way.

Use weighted random with light anti-repeat:

- Track last primary/secondary per player ID.
- Reroll once if the same option appears and the pool has alternatives.
- Accept the second result.

## First-Cut Gameplay Decisions

Infected/low-pop warmup waits until after the core PvP loop is solid.

Discord waits until after the core PvP loop is solid, though config placeholders are fine.

First admin commands:

- `@endround`
- `@reloadconfig`

`@reloadconfig` should reload settings for future rounds only. Use `@endround` to force the transition.

Radius and rectangular arenas should both be supported. Basic arena walls should be included in the first playable cut. Round-end cleanup should delete loose items and tracked round-created objects, with sane exclusions for players, vehicles, buildings, and persistent world objects. During long rounds, rely on DayZ central economy cleanup rather than periodic cleanup.

## Death Flow

Start with real DayZ death plus controlled respawn, not fake death.

Death-drop rules:

- Only the weapon in hands drops.
- If no weapon is in hands, nothing drops.
- Dropped weapons preserve attachments.
- Ammo is normalized on pickup.
- Only tracked death-drop weapons grant the one-mag pickup bonus.
- The original owner can recover their weapon, but gets no mag bonus.
- Pickup unregisters the dropped weapon either way.
- Corpses disappear after 5 seconds by default.
- Death-dropped weapons despawn after 60 seconds by default, configurable.
- Death-dropped weapons have no marker or beacon; they should feel like normal grounded loot.

The one-mag pickup bonus should be granted only when another player picks up a tracked death-dropped weapon.

## Spawn Safety And Team Modes

Standalone `DAFDeathmatch` should improve on Crimson Zamboni by avoiding obviously unfair spawn moments. Player spawn selection should score configured arena spawns instead of choosing blindly, reject spawns that are too close to alive enemies, and reject spawns inside an enemy's forward view cone. If every authored spawn is pressured, choose the least-bad spawn rather than failing the respawn.

Team deathmatch is a first-class round mode, not a separate mod. Round types default to free-for-all unless their `gameMode` is set to `tdm`. TDM should use random balanced team assignment by default, while supporting future event operations through config-based player ID preassignment. Preassigned players keep their configured team; unassigned players fill the smaller team.

TDM scoring should count enemy kills toward both player K:D and team score. Friendly kills should not inflate team score. Spawn safety should become team-aware during TDM, treating opposing teams as enemies for close-range and view-cone rejection.

Admins can force a one-off TDM round for testing or events without rewriting config. This is an operational override for the next round only; configured round types remain the durable source of normal rotation behavior.

TDM loadouts must preserve readable team identity. After the normal loadout outfit roll, the server enforces configured team clothing for red and blue teams by replacing body, legs, feet, mask, and armband slots with matching tracksuits, sneaker-style shoes, bandana face coverings, and armbands before weapons and inventory items are created.

Worn and carried player items should not degrade during combat in either FFA or TDM. `DAFImprovements` protects damage on living-player-owned items, and standalone spawn/loadout code should repair freshly equipped player attachments so each respawn starts with pristine clothing.

Weapons should not jam during the DAF PvP loop. `DAFImprovements` disables weapon jam state and jam chance at the shared weapon base layer so the guarantee applies to both the standalone `DAFDeathmatch` path and the Crimson Zamboni compatibility stack.

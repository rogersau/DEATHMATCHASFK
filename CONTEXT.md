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
- `juicer`

Round flavor is always visible on the HUD with the timer. Arena name is not shown on the HUD.

Round type selection should be weighted random with one anti-repeat reroll. Round types can restrict allowed arenas, but default to any arena.

## Loadouts

Loadout config should be simpler than Crimson Zamboni's config, while the loadout engine should be more flexible.

Use named loadout pools with weighted entries. Players roll independently inside the selected round type's loadout pool. Weapons can overlap between round types.

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

Radius arenas are enough. Basic circular walls should be included in the first playable cut. Round-end cleanup should delete loose items and tracked round-created objects, with sane exclusions for players, vehicles, buildings, and persistent world objects. During long rounds, rely on DayZ central economy cleanup rather than periodic cleanup.

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

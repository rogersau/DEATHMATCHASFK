# Agent Notes

## Addon Split

This repo has two DayZ addons with a deliberate client/server boundary.

### `DAFImprovements`

Use this addon for anything that can safely run on both client and server.

Put features here when they are generic DayZ behavior, UI, or shared prediction/smoothness helpers:

- Killfeed UI and HUD widgets
- Player count UI
- Per-round kill/death display sent through the killfeed
- Complete weapon display parsing for killfeed events
- Client-side and server-side footwear drop/removal prevention
- IV saline full-heal behavior
- IV saline drop/stash prevention and delete-on-death behavior
- Carried inventory damage protection
- Infected damage blocking
- Optional weapon fire-mode preference logic, currently disabled by default
- Client-side respawn cursor repair RPC handling

`DAFImprovements` must not reference classes that only exist in the Crimson Zamboni Deathmatch server mod. Clients load this addon, and clients do not have the reference Deathmatch mod installed.

### `DAFServerImprovements`

Use this addon only for server-side glue that depends on the server-only Crimson Zamboni Deathmatch mod.

Put code here when it references Deathmatch-specific classes, callbacks, or objects, including:

- `DMAutoRespawn`
- `DMDiscordWebhook`
- `MissionServer` round hooks from the Deathmatch mod
- `MissionBaseWorld` round hooks from the Deathmatch mod
- Deathmatch command/autorespawn behavior
- Discord killfeed/round-summary enhancements
- End-of-round highlights such as furthest shot and top weapon
- Persisted auto-respawn preferences
- Cleanup rules tied to Deathmatch settings, such as deleting spawned infected when player count reaches `forceInfectedPlayerLimit`
- Server RPC sends that coordinate with `DAFImprovements` client handlers

Keep this addon small. If a feature can run without importing or modding a Deathmatch class, prefer `DAFImprovements`.

## Expected Load Shape

Clients and the server load:

```text
-mod=@DAFImprovements
```

The server additionally loads:

```text
-serverMod=@CrimsonZamboniDeathmatch;@DAFServerImprovements
```

`DAFServerImprovements` depends on both `DAFImprovements` and `CrimsonZamboniDeathmatch`.

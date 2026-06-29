# Agent Notes

## Agent skills

### Issue tracker

Issues are tracked in GitHub Issues for `rogersau/DEATHMATCHASFK`. See `docs/agents/issue-tracker.md`.

### Triage labels

Use the default five-label triage vocabulary. See `docs/agents/triage-labels.md`.

### Domain docs

This repo uses a single-context domain-doc layout. See `docs/agents/domain.md`.

## Domain Context

Read `CONTEXT.md` before making architecture, feature-planning, or issue-writing decisions. It contains the current addon split, the transitional Crimson Zamboni compatibility model, and the long-term standalone `DAFDeathmatch` direction.

## Addon Split

This repo has two DayZ addons with a deliberate client/server boundary.

### `DAFImprovements`

Use this addon for anything that can safely run on both client and server.

Put features here when they are generic DayZ behavior, UI, or shared prediction/smoothness helpers:

- Killfeed UI and HUD widgets
- Player count UI
- Top-center round time remaining HUD
- Active VOIP speaker overlay, driven by client-received VON notifications
- Per-round kill/death display sent through the killfeed
- Complete weapon display parsing for killfeed events
- Server-to-client RPC seam (`DAFRPC`): the single owner of the shared wire ids and typed send helpers used by all three DAF addons. Any new server→client RPC goes through this module; do not introduce raw RPC ids or build `Param` payloads at call sites. See [ADR 0002](docs/adr/0002-adopt-community-framework-rpc.md).
- Client-side and server-side footwear drop/removal prevention
- IV saline full-heal behavior, currently balanced with an 8 second apply time
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
- Direct Deathmatch scoreboard reset hook for round K/D reset reliability
- Persisted auto-respawn preferences
- Round timer RPC broadcasts that feed the shared HUD
- Cleanup rules tied to Deathmatch settings, such as deleting spawned infected when player count reaches `forceInfectedPlayerLimit`
- Low-pop magazine refill tied to `forceInfectedPlayerLimit`
- Deathmatch loadout post-processing, including bandage/morphine/saline quickbar slots and Easter Egg removal
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

## Standalone Deathmatch

`DAFDeathmatch` is the in-progress replacement for the Crimson Zamboni Deathmatch server mod.

Use `DAFDeathmatch` when building replacement-owned core gameplay:

- Settings and arena config owned by DAF
- Round start/end lifecycle
- Arena rotation and spawn selection
- Player loadouts and quickbar assignment
- Scoring and round K/D source of truth
- Auto-respawn behavior
- Future command, vote, Discord, cleanup, infected, and event systems

Do not load `DAFDeathmatch` together with `CrimsonZamboniDeathmatch` in the same test. They both want to own round lifecycle, spawning, and scoring.

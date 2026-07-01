# DAF Improvements

DayZ deathmatch improvement addons.

## Addons

### DAFImprovements

Shared/client-visible addon for the killfeed and HUD:

- Killfeed UI
- Player count view
- Top-center round time remaining view
- Active VOIP speaker overlay
- Per-round kills/deaths display
- Full weapon display support from server kill events
- Footwear drop/removal prevention
- IV saline full-heal behavior with an 8 second application time
- IV saline drop/stash prevention and delete-on-death behavior
- Carried inventory damage protection
- Infected damage blocking
- Optional weapon fire-mode preference code, disabled by default
- Auto-respawn cursor repair client RPC handling

Load this as a normal mod on both client and server:

```text
-mod=@DAFImprovements
```

### DAFServerImprovements

Server-side addon for Crimson Zamboni Deathmatch compatibility and deathmatch gameplay rules:

- Deathmatch Discord kill/round stat enhancements
- Headshot flagging for Discord kill events
- End-of-round highlights for furthest shot and top weapon
- Persisted auto-respawn preference
- Server signal that asks clients to repair the stuck respawn cursor
- Round timer broadcasts for the shared HUD
- Infected cleanup when player count reaches `forceInfectedPlayerLimit`
- Low-pop magazine auto-refill while player count is below `forceInfectedPlayerLimit`
- Survival item quickbar normalization: bandage key 3, morphine key 4, saline key 5
- Deathmatch Easter Egg loadout item removal

Load this after the reference deathmatch mod as a server mod:

```text
-serverMod=@CrimsonZamboniDeathmatch;@DAFServerImprovements
```

`DAFServerImprovements` depends on both `DAFImprovements` and `CrimsonZamboniDeathmatch`.

### DAFDeathmatch

Standalone replacement Deathmatch client/server addon under active development.

Current first slice:

- Own settings file at `$profile:DAFDeathmatch/settings.json`
- Own arena file at `$profile:DAFDeathmatch/arenas.json`
- Basic round start/end lifecycle
- Absorbed HUD/RPC client layer from `DAFImprovements`
- Basic player spawning into the selected arena
- Basic weapon/survival item loadout with quickbar slots
- Basic internal scoreboard shell, with shared HUD K:D reset at round start
- Basic player commands: `help`, `players`, `timeleft`, `score`, `autorespawn`, `version`

Test this separately from Crimson Zamboni. Do not load both round managers together:

```text
-mod=@DAFDeathmatch
```

# DAF Improvements

DayZ deathmatch improvement addons.

## Addons

### DAFImprovements

Shared/client-visible addon for the killfeed and HUD:

- Killfeed UI
- Player count view
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
- Infected cleanup when player count reaches `forceInfectedPlayerLimit`
- Low-pop magazine auto-refill while player count is below `forceInfectedPlayerLimit`
- Survival item quickbar normalization: bandage key 3, morphine key 4, saline key 5
- Deathmatch Easter Egg loadout item removal

Load this after the reference deathmatch mod as a server mod:

```text
-serverMod=@CrimsonZamboniDeathmatch;@DAFServerImprovements
```

`DAFServerImprovements` depends on both `DAFImprovements` and `CrimsonZamboniDeathmatch`.

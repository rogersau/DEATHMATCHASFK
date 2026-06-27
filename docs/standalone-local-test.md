# Standalone Local Test

Use this for local MVP testing of `DAFDeathmatch` on Namalsk without Crimson Zamboni.

## Start Server

Close any running DayZ server first, then run:

```powershell
.\Start-StandaloneNamalskTest.ps1 -Repack -ConvertConfig
```

The launcher prepares:

- local Namalsk workshop junctions
- `@DAFImprovements`
- `@DAFDeathmatch`
- clean standalone `deathmatch.namalsk/init.c`
- converted `DAFDeathmatch` profile config
- local server config with `BattlEye = 0`

## Start Client

In another PowerShell window:

```powershell
.\Start-StandaloneNamalskClient.ps1
```

The client launches with local mod folders and connects to `127.0.0.1:2543`.

## Useful Commands

```text
@endround
@forceround normal
@forceround snipers
@forceround freshies
@forceround juiced
@forcearena Lubjansk
@forcenext snipers Lubjansk
@status
@inspect
@spawndummy
@cleardummies
@testdrop
@testdrop M4A1 Mag_STANAG_30Rnd
@respawn
@autorespawn
@timeleft
```

`@forcearena` and `@forcenext` are admin test helpers. Explicitly forced arenas can include arenas excluded from normal rotation, which is useful for checking old problem arenas without putting them back into live selection.

## Smoke Checklist

- Clothes spawn.
- Weapon spawns in normal, snipers, freshies, and juiced rounds.
- Snipers rounds always provide a primary rifle.
- Timer appears on first join.
- Timer appears again after disconnect/reconnect.
- Death auto-respawns after the configured delay.
- `@respawn` manually recovers the player if auto-respawn fails during testing.
- `@endround` starts a clean next round.
- `@forcenext snipers <arena>` starts a predictable sniper test round.
- `@inspect` shows the server's current round, arena, loadout pool, last rolled loadout, and item in hands.
- `@spawndummy` creates a survivor-looking test target near you for solo shooting/death-drop checks.
- `@cleardummies` removes tracked test targets without ending the round.
- `@testdrop [weapon] [bonus]` creates a tracked death-drop weapon near you without requiring a kill.
- Picking up a tracked test/death drop unregisters it and grants the configured bonus only when the picker is not the original owner.
- `@inspect` also shows tracked dummy, death-drop, and pending corpse-cleanup counts.

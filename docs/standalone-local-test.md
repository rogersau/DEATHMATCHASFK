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
@forcetdm normal Lubjansk
@status
@teams
@inspect
@shuffleteams
@spawndummy
@cleardummies
@testdrop
@testdrop M4A1 Mag_STANAG_30Rnd
@respawn
@autorespawn
@timeleft
```

`@forcearena`, `@forcenext`, `@forcetdm`, and `@shuffleteams` are admin test helpers. Explicitly forced arenas can include arenas excluded from normal rotation, which is useful for checking old problem arenas without putting them back into live selection. `@forcetdm <type> [arena]` overrides only the next round's mode; it does not rewrite the configured round type.

`@spawndummy`, `@cleardummies`, and `@testdrop` require admin access and `enableAdminTestCommands = true` in `settings.json`. `@respawn` is admin-only unless `enablePlayerRespawnCommand = true`.

TDM is enabled per round type by setting that round type's `gameMode` to `tdm`; omitted or `ffa` keeps the existing free-for-all behavior. `teamNames` defaults to `red` and `blue`, and `preassignedTeams` can pin event players by player ID while everyone else fills the smaller team.

TDM enforces team clothing by default: red team gets red tracksuit jacket/pants, red sneaker-style shoes, red bandana face covering, and red armband; blue team gets the blue equivalents. The item class names are configurable in `settings.json`.

## Smoke Checklist

- Clothes spawn.
- First spawn appears directly in the current arena without a visible wrong-position flash.
- New spawns avoid close enemies and enemies looking directly at the spawn where the arena has safer alternatives.
- Weapon spawns in normal, snipers, freshies, and juiced rounds.
- Weapons do not jam, including after sustained fire or when a weapon would normally be damaged enough to jam.
- Snipers rounds always provide a primary rifle.
- FFA and TDM clothing starts pristine and does not degrade from combat damage while worn.
- Timer appears on first join.
- Timer appears again after disconnect/reconnect.
- Death auto-respawns after the configured delay.
- `@respawn` manually recovers the player if auto-respawn fails during testing.
- `@endround` starts a clean next round.
- `@forcenext snipers <arena>` starts a predictable sniper test round.
- `@forcetdm normal <arena>` starts a predictable TDM test round without editing config.
- `@teams` shows current team assignment counts and team score during TDM.
- TDM players spawn with matching red/blue tracksuits, shoes, bandana face coverings, and armbands even when the loadout pool would normally roll different clothing.
- `@inspect` shows the server's current round, mode, team, arena, loadout pool, last rolled loadout, and item in hands.
- `@spawndummy` creates a survivor-looking test target near you for solo shooting/death-drop checks.
- `@cleardummies` removes tracked test targets without ending the round.
- `@testdrop [weapon] [bonus]` creates a tracked death-drop weapon near you without requiring a kill.
- Picking up a tracked test/death drop unregisters it and grants the configured bonus only when the picker is not the original owner.
- `@inspect` also shows tracked dummy, death-drop, and pending corpse-cleanup counts.

# Standalone Local Test

Use this for local MVP testing of `DAFDeathmatch` on Namalsk without Crimson Zamboni.

## Start Server

Close any running DayZ server first, then run:

```powershell
.\Start-StandaloneNamalskTest.ps1 -Repack -ConvertConfig
```

For an automated startup E2E check, run:

```powershell
.\Invoke-StandaloneE2ETest.ps1 -Repack -ConvertConfig
```

The E2E script prepares the local standalone server, starts it hidden, waits for the `DAFDeathmatch` startup markers in the script log, fails on script compile/runtime error markers, reports config-warning counts, then stops the server it started. Use `-StartClient -NoBattleEyeClient -ManualTimeoutSeconds 300` when you want the same checked boot plus a five-minute manual client window.

Local debug/admin test commands are enabled by default by both start scripts. Use `-EnableDebugCommands:$false` for a production-like local run where `@spawndummy`, `@cleardummies`, and `@testdrop` are disabled.

The launcher prepares:

- local Namalsk workshop junctions
- `@DAFDeathmatch`
- clean standalone `deathmatch.namalsk/init.c`
- converted `DAFDeathmatch` profile config
- local server config with `BattlEye = 0`

On startup and after `@reloadconfig`, `DAFDeathmatch` writes a config report to the script log. Check any `DAFDeathmatch config warning` lines before a serious test; missing loadout classes usually mean the local test launch is missing a weapon/attachment mod or the converted config still references an unavailable class.

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
@spawnreport
@shuffleteams
@endvote
@arenavote Lubjansk
@roundvote snipers
@eventvote juiced
@vote 1
@vote 2
@spawndummy 9
@cleardummies
@testdrop
@testdrop M4A1 Mag_STANAG_30Rnd
@discordtest killfeed
@discordtest events
@respawn
@autorespawn
@timeleft
```

`@forcearena`, `@forcenext`, `@forcetdm`, `@spawnreport`, and `@shuffleteams` are admin test helpers. Explicitly forced arenas can include arenas excluded from normal rotation, which is useful for checking old problem arenas without putting them back into live selection. `@forcetdm <type> [arena]` overrides only the next round's mode; it does not rewrite the configured round type.

`@spawnreport` explains the current arena's spawn-safety pick for you: chosen spawn index, whether the pick is a fallback, nearby player/enemy counts, view-cone threats, nearest player/enemy distance, and active thresholds.

Voting is enabled by default and follows the Zamboni-style command shape: one active vote at a time, `@vote 1` for yes, and `@vote 2` for no. `@endvote` ends the current round if it passes. `@arenavote <arena>` and `@roundvote <type>` set the next arena or round type if they pass; `@eventvote <type>` is a familiar alias for round-type voting. Voting does not expose TDM directly; players can only vote for configured round types.

`@spawndummy [1-9]`, `@cleardummies`, and `@testdrop` require admin access and `enableAdminTestCommands = true` in `settings.json`. `@respawn` is admin-only unless `enablePlayerRespawnCommand = true`.

## Discord

Standalone `DAFDeathmatch` has two independent Discord webhook endpoints. Both default to disabled, and missing URLs never break server startup.

- `enableDiscordKillfeed` / `discordKillfeedWebhookUrl`: noisy PvP kill messages only.
- `enableDiscordServerEvents` / `discordServerEventsWebhookUrl`: lower-volume server ready, round start, and round-end summary messages.
- `discordServerName`: name shown on Discord messages, defaults to `DAF Deathmatch`.
- `discordSuppressEmbeds`: optional, sets the Discord suppress-embeds flag on posted messages.

To test locally:

1. Create two Discord channels and a webhook per channel (channel settings → Integrations → Webhooks → New Webhook → Copy Webhook URL).
2. Open `<profile>\DAFDeathmatch\settings.json` and paste each webhook URL into the matching field, then set both `enableDiscordKillfeed` and `enableDiscordServerEvents` to `true`.
3. Run `@reloadconfig` (admin) so the next round picks up the change, or restart the server.

Webhook URLs are server-side only. The config report masks them (`https://discord.com/api/webhooks/...abc123`); the full URL is never printed to logs. Do not commit a real webhook URL into this repo.

`@discordtest killfeed` and `@discordtest events` are admin-only and test each endpoint independently. When the endpoint is disabled or the URL is missing/invalid, the command reports that cleanly without posting.

TDM is enabled per round type by setting that round type's `gameMode` to `tdm`; omitted or `ffa` keeps the existing free-for-all behavior. `teamNames` defaults to `red` and `blue`, and `preassignedTeams` can pin event players by player ID while everyone else fills the smaller team.

TDM enforces team clothing by default: red team gets red tracksuit jacket/pants, red sneaker-style shoes, red bandana face covering, and red armband; blue team gets the blue equivalents. The item class names are configurable in `settings.json`.

Low-pop warmup is enabled by default. With one real player connected, warmup activates after `warmupActivateDelaySeconds` seconds, defaulting to 30. The player count HUD should show `Warmup Mode`, warmup infected should keep respawning near the active arena, infected should not damage the player, and spare magazines should refill while the loaded magazine still has to be reloaded normally. Warmup turns off immediately when a second real player joins.

## Smoke Checklist

- Clothes spawn.
- Hotbar is `1` primary weapon, `2` secondary weapon, `3` knife, `4` morphine, `5` bandage, `6` saline.
- Script log shows a `DAFDeathmatch: config report` and `DAFDeathmatch: config flags` line on startup.
- Script log shows a `DAFDeathmatch: voting flags` line on startup.
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
- With two real players, `@endvote` starts a vote, duplicate `@vote` attempts are rejected, and a yes majority ends the current round.
- With two real players, `@arenavote <arena>` and `@roundvote <type>` pass without ending the current round, then apply to the next round.
- `@teams` shows current team assignment counts and team score during TDM.
- TDM players spawn with matching red/blue tracksuits, shoes, bandana face coverings, and armbands even when the loadout pool would normally roll different clothing.
- With one real player connected for 30 seconds, the player count HUD shows `Warmup Mode`.
- Warmup infected spawn near the arena, move slower than normal, respawn after being killed, and cannot damage the player.
- During warmup, spare magazines refill but the currently loaded magazine still runs down and requires reloading.
- When a second real player joins, warmup status clears and warmup infected are deleted.
- `@inspect` shows the server's current round, mode, team, arena, loadout pool, last rolled loadout, and item in hands.
- `@inspect` also shows whether warmup is active or pending and how many warmup infected are currently tracked.
- `@spawnreport` shows why the current arena's spawn-safety logic picked a spawn.
- `@spawndummy [1-9]` creates survivor-looking test targets near you for solo shooting/death-drop checks.
- `@cleardummies` removes tracked test targets without ending the round.
- `@testdrop [weapon] [bonus]` creates a tracked death-drop weapon near you without requiring a kill.
- Picking up a tracked test/death drop unregisters it and grants the configured bonus only when the picker is not the original owner.
- `@inspect` also shows tracked dummy, death-drop, and pending corpse-cleanup counts.

## Discord Smoke Checklist

- Script log shows a `DAFDeathmatch: discord flags` line on startup; no raw webhook token appears anywhere in the log.
- With both endpoints disabled (or URLs blank), startup still succeeds and `@discordtest` reports disabled/missing cleanly without posting.
- With URLs configured and endpoints enabled, `@discordtest killfeed` posts only to the killfeed channel and `@discordtest events` posts only to the events channel.
- A PvP kill posts only to the killfeed endpoint; warmup infected kills, test dummy kills, death-drop pickups, and respawns do not post.
- Round start and round end post only to the events endpoint.
- The round-end summary includes highlights when available: top player, top K/D, furthest shot, top weapon, and TDM team scores for TDM rounds.

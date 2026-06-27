# PRD: DAFDeathmatch First Playable Cut

Intended issue label: `ready-for-agent`

## Problem Statement

The current live DAF deathmatch stack depends on the Crimson Zamboni Deathmatch server mod plus local bridge addons. That has allowed fast improvements, but the integration is increasingly awkward: round timing, K/D reset, Discord summaries, respawn behavior, HUD data, low-pop rules, and loadout tweaks all have to hook around a game mode that was not designed as DAF's extension API.

The DAF server needs a standalone, opinionated deathmatch mod that owns the core gameplay loop directly while preserving an easy migration path from the current server experience. The replacement should not copy Crimson Zamboni verbatim, but it should avoid needless reinvention where the current config language and gameplay model already map well to what players expect.

## Solution

Build `DAFDeathmatch` as the long-term standalone client/server deathmatch mod for the DAF server. The first playable cut should prove the core PvP loop: round flavor selection, arena selection, spawning, loadouts, killfeed/K:D, timer HUD, death/respawn flow, lightweight commands, cleanup, and clean DayZ script startup.

During migration, `DAFDeathmatch` may temporarily load beside `DAFImprovements`, but the final target is one client/server mod:

```text
-mod=@DAFDeathmatch
```

`DAFDeathmatch` must not load with Crimson Zamboni Deathmatch. The old `DAFServerImprovements` addon remains a transitional bridge for the existing live stack only; useful improvements should be ported into the new core deliberately.

The new mod should be opinionated by default. It should recreate the live DAF server experience out of the box, with config flags for tuning or disabling features where useful.

## User Stories

1. As a DAF server owner, I want `DAFDeathmatch` to own round lifecycle directly, so that I do not have to work around private state in another deathmatch mod.
2. As a DAF server owner, I want the final deployment to be a normal client/server mod, so that client HUD code and server gameplay code can share a controlled contract without a bridge addon.
3. As a DAF server owner, I want an external converter from the current Crimson Zamboni config, so that migration is easy without adding runtime compatibility debt.
4. As a DAF server owner, I want `DAFDeathmatch` to read only its own config at runtime, so that the replacement can evolve independently.
5. As a DAF server owner, I want split config files for settings, arenas, and loadouts, so that each file stays understandable and editable.
6. As a DAF server admin, I want opinionated defaults matching the live DAF experience, so that new installs feel like our server without hours of tuning.
7. As a DAF server admin, I want round types such as normal, snipers, freshies, and juicer, so that rounds have recognizable flavors.
8. As a DAF server admin, I want round types to use weighted random selection, so that common modes happen often and special modes remain special.
9. As a DAF server admin, I want round type selection to avoid immediate repeats with one reroll, so that the server feels varied without a complex scheduler.
10. As a DAF server admin, I want round types to optionally restrict allowed arenas, so that some modes can run only where they make sense.
11. As a player, I want the current round flavor to be visible on the HUD, so that I understand why everyone has a particular style of gear.
12. As a player, I want the top-center HUD to show round flavor and time remaining, so that I can track the match without opening chat.
13. As a player, I do not need the arena name constantly on the HUD, so that the combat UI stays compact.
14. As a player, I want fast round transitions, so that the server keeps its deathmatch pace.
15. As a player, I want arena walls in the first playable cut, so that arena boundaries are visible and do not feel arbitrary.
16. As a player, I want radius arenas supported first, so that the current arena model works without unnecessary complexity.
17. As a player, I want player spawns to come from arena config, so that each arena can provide intentional spawn positions.
18. As a player, I want respawns to use configured arena spawns, so that returning to the fight feels consistent.
19. As a player, I want basic auto-respawn after a delay, so that I return to the action quickly.
20. As a player, I want to toggle autorespawn, so that I can work around client issues if needed.
21. As a server admin, I want autorespawn to default to the DAF experience, so that the server remains fast-paced.
22. As a player, I want loadouts to feel familiar from the current server, so that the replacement does not change the meta accidentally.
23. As a server admin, I want loadout config to be simpler than the old config, so that tuning the server is less painful.
24. As a server admin, I want loadout pools to be more flexible internally, so that round flavors can reuse or specialize weapons.
25. As a player, I want each player to roll independently within the round loadout pool, so that rounds have variety.
26. As a player, I want light anti-repeat behavior for weapons, so that I do not get the same gun repeatedly when alternatives exist.
27. As a server admin, I want weapon entries to support weights, attachments, loaded mags, spare mags, and quickbar slots, so that common loadout patterns are easy to express.
28. As a player, I want survival items assigned to predictable hotbar keys, so that bandage, morphine, and saline are always where expected.
29. As a player, I want saline to retain the DAF full-heal behavior, so that the replacement preserves the current server's balance identity.
30. As a player, I want the killfeed to continue showing complete weapon previews, so that kills communicate the real weapon setup.
31. As a player, I want killfeed K:D to reset each round, so that scores reflect the active round.
32. As a player, I want player count and round timer HUD behavior to keep working, so that the replacement has feature parity with current DAF HUD basics.
33. As a player, I want VOIP speaker display preserved eventually, so that audible speakers are less anonymous.
34. As a player, I want death to feel like real DayZ death for now, so that the mode does not feel cheap or arcade-forced.
35. As a player, I want only my in-hands weapon to drop on death, so that death drops create battlefield swaps without turning bodies into loot crates.
36. As a player, I want nothing to drop if I die without a weapon in hands, so that medical items or inventory do not become farmable.
37. As a player, I want death-dropped weapons to keep attachments, so that picking one up preserves meaningful weapon identity.
38. As a player, I want death-dropped weapon ammo normalized on pickup, so that looted weapons are useful without giving excessive resources.
39. As a player, I want only another player to receive the one-mag pickup bonus, so that recovering my own weapon does not become a farming loop.
40. As a player, I want death-dropped weapons to look like normal grounded loot, so that the deathmatch feel stays grounded rather than loot-beacon-like.
41. As a player, I want corpses cleaned quickly, so that arenas do not fill with bodies.
42. As a server admin, I want corpse cleanup defaulted to 5 seconds, so that bodies disappear quickly while still allowing a death moment.
43. As a server admin, I want death-dropped weapons cleaned after a configurable timeout, so that rounds with 40-100 deaths do not become cluttered.
44. As a server admin, I want death-dropped weapon cleanup defaulted to 60 seconds, so that weapon swaps are possible without clutter.
45. As a server admin, I want round-end cleanup for loose items and tracked round-created objects, so that each round starts clean.
46. As a server admin, I want to rely on DayZ central economy during long rounds, so that periodic cleanup does not accidentally remove useful gear mid-fight.
47. As a server admin, I want `@endround`, so that I can recover from bad test rounds without restarting the server.
48. As a server admin, I want `@reloadconfig`, so that I can iterate settings during tests.
49. As a server admin, I want `@reloadconfig` changes to apply to future rounds by default, so that config reloads do not suddenly disrupt active fights.
50. As a player, I want `@help`, `@players`, `@timeleft`, `@score`, `@autorespawn`, and `@version`, so that I can interact with the mode in familiar ways.
51. As a DAF maintainer, I want a clean server RPT startup before private player testing, so that script compile issues are caught early.
52. As a DAF maintainer, I want every implementation slice to pack with DayZ Tools, so that the PBO remains deployable.
53. As a DAF maintainer, I want infected warmup to wait until after the core PvP loop works, so that the first cut stays focused.
54. As a DAF maintainer, I want Discord webhook polish to wait until after the core PvP loop works, so that the first cut does not get blocked by server-ops polish.
55. As a future server operator, I want config flags for opinionated features, so that I can tune the DAF experience without editing code.

## Implementation Decisions

- `DAFDeathmatch` is the final addon name and long-term home of the game mode.
- `DAFDeathmatch` should become a client/server mod, not a server-only bridge.
- `DAFImprovements` and `DAFServerImprovements` are transitional. Useful features should be ported into `DAFDeathmatch`; the final load shape should be `-mod=@DAFDeathmatch`.
- The first playable cut may temporarily use `DAFImprovements` HUD/client pieces, but `DAFDeathmatch` should own deathmatch facts: round flavor, timer, score, kills, loadout pool, drops, and cleanup.
- Runtime compatibility with Crimson Zamboni config is out. Migration should be an external converter/tool.
- The converter should create a first playable `normal` round type from existing config rather than trying to infer snipers/freshies/juicer from old data.
- Config is split into `settings.json`, `arenas.json`, and `loadouts.json`.
- `settings.json` owns round behavior, command character, feature defaults, admin IDs, and round type selection.
- `arenas.json` owns spatial arena data and spawn positions.
- `loadouts.json` owns loadout pools and weighted loadout entries.
- Round types are first-class. Initial round flavors are `normal`, `snipers`, `freshies`, and `juicer`.
- Round flavor is always visible on the HUD with the timer. Arena name is not shown on the HUD.
- Round type selection is weighted random with one anti-repeat reroll.
- Round types can restrict arenas; no restriction means any arena.
- Loadouts are selected independently per player within the active round type's loadout pool.
- Loadout weapon selection uses weighted random with one anti-repeat reroll per player for primary/secondary selections.
- The loadout engine should support attachments, loaded mags, spare mags, quickbar assignments, and future event/round overrides.
- Radius arenas are enough for the first playable cut.
- Basic circular walls are in scope for the first playable cut.
- Round-end cleanup should delete tracked round-created objects and loose arena items while avoiding players, vehicles, buildings, and persistent world objects.
- Periodic cleanup during rounds is out for the first cut; rely on DayZ central economy during long rounds.
- Use real DayZ death plus controlled respawn for the first cut; fake death/downed reset is not part of this PRD.
- Death-drop rules are weapon-focused: only the in-hands weapon drops, attachments are preserved, and no other inventory becomes loot.
- Death-dropped weapons are tracked by the server. Only tracked death-drop weapons can grant one normalized mag on pickup.
- The one-mag pickup bonus applies only when the picker is not the original owner.
- Any pickup unregisters the tracked dropped weapon, including owner pickup.
- Corpses are cleaned after 5 seconds by default.
- Death-dropped weapons are cleaned after 60 seconds by default.
- Death-dropped weapons are not visually marked.
- First player commands include `help`, `players`, `timeleft`, `score`, `autorespawn`, and `version`.
- First admin commands include `endround` and `reloadconfig`.
- `reloadconfig` applies to future rounds; admins can pair it with `endround` for immediate transition.
- Infected/low-pop warmup is out of the first playable cut.
- Discord webhook messages, embeds, furthest shot, top weapon, and leaderboards are out of the first playable cut.

## Testing Decisions

- Tests should prioritize external behavior over implementation details. The important question is whether the server behaves correctly for players and admins, not whether a private helper method was called.
- Highest validation seam is a real DayZ server boot/RPT smoke test. The mod must load with the intended migration or final load shape and produce clean Game/World/Mission script startup from DAF code.
- PBO packing with DayZ Tools AddonBuilder is required for every implementation slice.
- Config/import behavior should be tested at the converter boundary: given Crimson Zamboni-style source config, the converter writes valid `DAFDeathmatch` settings, arenas, and loadouts without runtime dependency on old files.
- Round lifecycle should be tested through server behavior: start round, select flavor, select arena, spawn walls, broadcast timer/flavor, end round, clean up, and transition to the next round.
- Round type anti-repeat should be tested through observable selection behavior with deterministic/small test pools where possible.
- Loadout selection should be tested through player spawn behavior: correct pool, weighted entries, attachments, mags, quickbar assignments, and light anti-repeat.
- Player loop should be validated with a two-client private test: spawn, kill, killfeed, K:D, death-drop, pickup bonus, corpse cleanup, weapon cleanup, respawn, and round reset.
- Chat command behavior should be tested in game through chat as player/admin rather than by directly invoking command handlers.
- Cleanup should be validated in a live/test arena by verifying bodies, dropped weapons, walls, and loose items follow the configured cleanup rules.
- Prior art in this repo includes DayZ Tools PBO packing and short DayZServer smoke boots used for the existing DAF addons.

## Out of Scope

- Full feature parity with Crimson Zamboni Deathmatch.
- Runtime loading of Crimson Zamboni config by `DAFDeathmatch`.
- Loading `DAFDeathmatch` together with Crimson Zamboni Deathmatch.
- Discord webhook messages, embeds, leaderboards, furthest shot, and top weapon.
- Votes and vote UX.
- Event system beyond first-class round flavors and loadout pools.
- Infected/low-pop warmup mode.
- Low-pop magazine refill.
- Periodic cleanup during rounds.
- Rectangular arenas.
- Fake death/downed-reset pipeline.
- Steam Workshop publishing or release packaging.

## Further Notes

The first playable cut should be good enough for a private server test with a couple of players, not production cutover. The live server can remain on the Crimson Zamboni compatibility stack while `DAFDeathmatch` matures.

The strongest risk is accidentally recreating the bridge-shaped architecture inside the new mod. Keep `DAFDeathmatch` as the owner of deathmatch facts, even if it temporarily reuses old HUD rendering during migration.

The GitHub issue for this PRD could not be created at the time of writing because the configured remote `rogersau/DEATHMATCHASFK` was not resolvable by `gh`/GitHub. Publish this document as a GitHub issue with the `ready-for-agent` label once the remote repository exists or the remote URL is corrected.

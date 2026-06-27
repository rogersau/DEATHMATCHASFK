# PRD: DAFDeathmatch Config Migration and Import

Intended issue label: `ready-for-agent`

## Problem Statement

The live DAF deathmatch server has meaningful Crimson Zamboni configuration: Namalsk arenas, custom arenas, excluded arenas, admins, round timing, event probabilities, and weapon pools. The first playable `DAFDeathmatch` cut proves the standalone loop, but it does not yet have a migration-quality config model or a converter that can bring the live server setup across cleanly.

Without a converter, migration means manually rebuilding painful config by hand, especially arenas and loadouts. Without improving the standalone runtime schema first, any converter would either discard important data or invent config that `DAFDeathmatch` cannot consume.

## Solution

Extend `DAFDeathmatch` config support first, then build a one-off local PowerShell migration tool that converts the live Crimson Zamboni config into standalone `DAFDeathmatch` config files.

The runtime should support the migrated shape directly: radius arenas, rectangular arenas, familiar Crimson-like weapon pools, enabled round types, round weights, arena restrictions, and excluded arenas kept out of rotation. The converter should run locally, write output to an inspection folder, and leave copying to the server as an operator step.

The migration should be intentionally narrow. It should convert only the standalone-owned essentials needed for a useful Namalsk standalone test, and it should not become a runtime compatibility layer for Crimson Zamboni.

## User Stories

1. As a DAF server owner, I want to convert the existing Crimson Zamboni config locally, so that I can test standalone `DAFDeathmatch` without rebuilding server config by hand.
2. As a DAF server owner, I want the converter to write a separate output folder, so that I can inspect generated files before copying them to the server.
3. As a DAF server owner, I want `DAFDeathmatch` to read only standalone config at runtime, so that the new mod does not carry Crimson compatibility debt.
4. As a DAF server owner, I want the converter to emit `settings.json`, `arenas.json`, and `loadouts.json`, so that the generated config matches the standalone config split.
5. As a DAF server owner, I want the standalone schema implemented before the converter, so that generated config is actually loadable by the mod.
6. As a DAF server admin, I want admins migrated from the live Crimson config, so that known admins can keep testing without manual re-entry.
7. As a DAF server admin, I want live round timings migrated, so that standalone testing starts with familiar pacing.
8. As a DAF server admin, I want Crimson event probabilities converted into standalone round type weights, so that round flavor frequency feels like the live server.
9. As a DAF server admin, I want the normal round chance preserved as leftover non-event probability, so that special rounds do not become too frequent.
10. As a DAF server admin, I want enabled Crimson events converted to round types, so that `snipers`, `freshies`, and `juiced` remain playable.
11. As a DAF server admin, I do not want disabled Crimson events converted, so that rubbish old rounds like `Cowboy` and `Halloween` stay gone.
12. As a DAF server admin, I want the canonical standalone flavor name to be `juiced`, so that it matches the live config language.
13. As a DAF server admin, I want event arena restrictions migrated to round type arena restrictions, so that modes like `freshies` still run only where they make sense.
14. As a DAF server admin, I want Crimson `excludeArenas` preserved as inactive rotation config, so that bad arenas do not accidentally return.
15. As a DAF server admin, I want excluded arena definitions kept in `arenas.json`, so that hand-authored arena data is not destroyed during conversion.
16. As a DAF server admin, I want both `arenas.json` and `custom-arenas.json` imported, so that custom Namalsk arenas are not lost.
17. As a DAF server admin, I want rectangular arenas supported by standalone `DAFDeathmatch`, so that custom arena shapes survive migration.
18. As a DAF server admin, I want radius arenas still supported, so that existing ordinary arena definitions keep working.
19. As a DAF server admin, I want player spawns migrated for each arena, so that converted arenas are immediately usable.
20. As a DAF server admin, I want arena geometry and spawn data validated during conversion, so that obvious bad input is reported before a server boot.
21. As a DAF server admin, I want warnings for skipped or unsupported fields, so that I know what did not migrate.
22. As a DAF server admin, I want the converter to ignore Crimson systems standalone does not own yet, so that first-pass migration stays focused.
23. As a DAF server admin, I want Discord config skipped, so that server-ops polish does not block gameplay migration.
24. As a DAF server admin, I want infected behavior skipped for now, so that low-pop and PvE systems can be designed separately later.
25. As a DAF server admin, I want crates and holiday props skipped for now, so that unused or noisy config does not shape the standalone core.
26. As a DAF server admin, I want loadouts migrated into familiar weapon pools, so that balancing stays readable.
27. As a DAF server admin, I want standalone weapon pools to support variants, so that migrated weapons can preserve Crimson-style variation.
28. As a DAF server admin, I want standalone weapon pools to support attachment choice groups, so that migrated loadouts do not explode into hundreds of fixed entries.
29. As a DAF server admin, I want standalone weapon pools to support accessories and magazines, so that converted weapons spawn with useful supporting gear.
30. As a DAF server admin, I want round types to select named loadout pools, so that normal, snipers, freshies, and juiced can use distinct gear.
31. As a DAF server admin, I want weapons to remain overlapping between pools where useful, so that balancing can stay practical.
32. As a DAF maintainer, I want the converter to be straightforward PowerShell, so that it is easy to run once and easy to delete or ignore later.
33. As a DAF maintainer, I want the converter to accept explicit input and output paths, so that it can run against backups or live-profile copies safely.
34. As a DAF maintainer, I want the converter to be deterministic enough to diff generated output, so that migration changes can be reviewed.
35. As a DAF maintainer, I want generated config to load in a standalone server smoke test, so that migration success is proven by DayZ itself.
36. As a future DAF maintainer, I want the arena model to leave room for dynamic arena expansion by player count, so that this migration does not block a known future direction.

## Implementation Decisions

- Implement the standalone runtime schema before writing the converter.
- Keep runtime `DAFDeathmatch` independent from Crimson Zamboni files and classes.
- Use a one-off local PowerShell converter entry point rather than a polished application.
- The converter takes explicit source and destination folders.
- The converter writes to a local output folder for inspection; it does not write directly into the live server profile by default.
- The output config files are `settings.json`, `arenas.json`, and `loadouts.json`.
- Convert only standalone-owned essentials: admins, round timings, enabled round types and weights, excluded arena rotation, arena geometry and spawns, and loadout pools.
- Skip Crimson systems that standalone does not own yet, including Discord, infected behavior, crates, holiday props, cosmetics, and unused oddball fields.
- Convert Crimson events into standalone round types rather than preserving a separate runtime event system.
- Use `normal`, `snipers`, `freshies`, and `juiced` as the initial migrated round flavors.
- Discard zero-chance Crimson events rather than preserving them as disabled round types.
- Preserve effective Crimson event probabilities. The global event chance becomes total non-normal weight, and leftover probability becomes `normal`.
- For the live Namalsk config, convert `events.chance = 0.35` with `snipers = 0.25`, `freshies = 0.25`, and `juiced = 0.5` into readable weights around `normal = 65`, `snipers = 9`, `freshies = 9`, and `juiced = 18`.
- Support radius arenas and rectangular arenas in standalone runtime config.
- Import both Crimson `arenas.json` and `custom-arenas.json`.
- Preserve excluded arena definitions in standalone `arenas.json`, but keep those arenas out of active rotation through settings/config.
- Keep the arena model open to future player-count-based expansion, but do not implement dynamic expansion in this PRD.
- Use a familiar Crimson-like weapon pool model in standalone loadouts.
- Do not expand every weapon variant and attachment combination into separate fixed entries.
- Weapon pool entries should be able to describe variants, attachments, accessories, magazines, and related random-choice groups in structured config.
- Round types select named loadout pools.

## Testing Decisions

- Tests should validate external behavior and generated artifacts, not private converter helper details.
- The highest converter seam is file-in/file-out: given a Crimson config folder, the tool writes valid standalone `settings.json`, `arenas.json`, and `loadouts.json`.
- Use the provided Namalsk backup config as the primary fixture for migration validation.
- Validate generated JSON by loading it through standalone `DAFDeathmatch`, not only by checking JSON syntax.
- Add focused checks that disabled zero-chance events are omitted.
- Add focused checks that effective round weights are preserved within readable integer rounding.
- Add focused checks that excluded arenas remain defined but do not participate in active rotation.
- Add focused checks that rectangular custom arenas survive conversion with their geometry.
- Add focused checks that weapon pools preserve variants, attachments, accessories, and magazines as structured choices.
- Run a DayZ Tools pack step after runtime schema changes.
- Run a standalone DayZServer smoke boot with the converted config and confirm clean DAF script startup.

## Out of Scope

- Runtime compatibility with Crimson Zamboni config.
- Loading `DAFDeathmatch` together with Crimson Zamboni Deathmatch.
- A polished migration UI or long-lived migration product.
- Direct writes into the live server profile by default.
- Migrating disabled Crimson events such as `Cowboy` and `Halloween`.
- Discord migration.
- Infected/low-pop migration.
- Crate migration.
- Holiday prop migration.
- Cosmetic migration.
- Dynamic arena expansion by player count.
- Full production cutover to standalone `DAFDeathmatch`.

## Further Notes

This PRD is the next phase after the first playable cut. The goal is not broad feature parity with Crimson Zamboni; it is getting the real Namalsk server shape into standalone-owned config with enough fidelity to test seriously.

The most important architectural constraint is that migration remains outside the mod. `DAFDeathmatch` should become better at expressing DAF deathmatch concepts directly, not better at pretending to be Crimson Zamboni.

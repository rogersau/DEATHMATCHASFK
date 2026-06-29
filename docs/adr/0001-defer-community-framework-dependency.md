# ADR 0001: Defer DayZ Community Framework Dependency

## Status

Superseded by [ADR 0002](0002-adopt-community-framework-rpc.md).

The decision was correct at the time, but its load-bearing reason — that Community Framework *"adds a required client/server mod dependency"* — no longer holds: clients on the live DAF server already load Community Framework for anticheat. ADR 0002 also builds the local RPC wrapper this ADR mandated but was never implemented. See ADR 0002 for the replacement decision.

## Context

`DAFDeathmatch` is intended to become the standalone replacement for the Crimson Zamboni Deathmatch server mod. The current product direction is one opinionated DAF-owned deathmatch mod that owns round lifecycle, arena selection, spawning, loadouts, scoring, death flow, and cleanup.

DayZ Community Framework could provide useful infrastructure, especially named/conflict-resistant RPC handling, module lifecycle patterns, and a path toward future ecosystem integrations such as admin tooling or modular extension points.

The first playable cut is not blocked by those infrastructure concerns. Its highest-risk work is domain-specific gameplay behavior: replacing Crimson Zamboni round ownership, importing arena/loadout config, spawning players correctly, resetting and showing round K/D, and keeping the startup/runtime path clean.

Adding Community Framework now would also add a required client/server mod dependency and load-order/support surface before the standalone gameplay loop is proven.

## Decision

Do not adopt DayZ Community Framework for the first playable cut of `DAFDeathmatch`.

Keep core deathmatch code self-contained for now. Centralize local RPC IDs and send/receive helpers behind a small DAF-owned wrapper so that RPC handling is less scattered and a future Community Framework migration remains contained.

Revisit Community Framework after the core PvP loop is stable, especially if the project needs richer client/server sync, public-mod compatibility, admin tooling, votes, Discord/leaderboard modules, or explicit integration with Community Framework-based ecosystems.

## Consequences

- `DAFDeathmatch` keeps a smaller dependency surface during the standalone migration.
- Core gameplay work can proceed without making every client and server load Community Framework.
- Existing hardcoded RPC usage should be paid down through a local wrapper rather than expanded.
- Future Community Framework adoption remains possible, but should be treated as a deliberate dependency decision rather than a default foundation.

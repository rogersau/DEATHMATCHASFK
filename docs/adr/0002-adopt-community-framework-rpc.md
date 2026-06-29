# ADR 0002: Centralize RPC Behind a DAF-Owned Seam, Then Adopt Community Framework RPC

## Status

Accepted — supersedes [ADR 0001](0001-defer-community-framework-dependency.md).

## Context

ADR 0001 deferred DayZ Community Framework for the first playable cut of `DAFDeathmatch`, and prescribed two things:

1. Keep core deathmatch code self-contained for now.
2. **Centralize local RPC ids and send/receive helpers behind a small DAF-owned wrapper** so that RPC handling is less scattered and a future Community Framework migration remains contained.

The first cut shipped without ever building that wrapper. The consequence is visible in the source: eight RPC ids (`-74700005` through `-74700012`) were hand-coordinated as raw integer literals across **three addons** — `DAFImprovements` (the only addon clients load, and the home of the client receiver in `PlayerBase.OnRPC`), `DAFDeathmatch`, and `DAFServerImprovements`. Each send site duplicated its own `GetPlayerList` + `RPCSingleParam` loop. There was no single source of truth for the wire ids. A collision, a renumber, or a drifted `Param` shape would silently break the HUD or killfeed with no central place to detect it.

Meanwhile the load-bearing reason ADR 0001 gave for deferring Community Framework — that it *"adds a required client/server mod dependency and load-order/support surface before the standalone gameplay loop is proven"* — no longer holds on the live DAF server: **clients already load Community Framework**, because the server already depends on it for anticheat. The dependency cost ADR 0001 was avoiding is already paid.

This is exactly the "revisit after the core PvP loop is stable, especially if the project needs richer client/server sync" condition ADR 0001 named for reconsideration.

## Decision

Adopt Community Framework RPC, in two behind-the-seam steps.

### Step 1 (this change): build the DAF-owned RPC seam ADR 0001 mandated but was never built.

Stand up one module — `DAFRPC` in `DAFImprovements/scripts/4_world/classes/dafrpc.c` — that owns the entire RPC surface:

- **Named id constants** are the single source of truth for the eight wire ids. No other file references the raw integers.
- **Typed server→client send helpers** (`SendKillfeedItem`, `SendRoundTimeSeconds`, `SendRoundLabel`, `SendRoundHudState`, `SendPlayerCountStatus`, `SendRespawnCursorFix`, `SendKillfeedReset`, `SendPlayerCount`, plus per-identity and broadcast variants) are the only server-facing surface. Callers pass a semantic payload and never build a `Param` or mention an id. Each helper owns its own player iteration, collapsing the duplicated loops that lived at every send site.
- **Client dispatch** is centralized: `PlayerBase.OnRPC` delegates to one handler that reads and dispatches against the same named ids, replacing the 80-line inline branch chain.

The seam lives in `DAFImprovements` because that is the only addon clients load, the client receiver already lives there, and both server addons (`DAFDeathmatch`, `DAFServerImprovements`) send to that same client. One definition, three consumers.

### Step 2 (follow-on): swap the implementation behind the seam to Community Framework RPC.

Because callers only know the typed helpers, the swap is contained inside `DAFRPC`:

- The named id constants become Community Framework `ModRPC` lookups instead of integer ids.
- The per-helper broadcast loop becomes CF's `Send`/`SendTo` on the resolved `ModRPC`.
- Client registration moves from `OnRPC` integer matching to CF's named-handler registration.

No caller outside `DAFRPC` changes.

## Consequences

- **Resolves the debt ADR 0001 flagged.** The wrapper is built. The eight scattered magic numbers live in one module; the duplicated player loops are gone.
- **Retires ADR 0001's stale reason.** The "adds a required dependency" objection is moot on the DAF server, where Community Framework is already a client-loaded dependency for anticheat.
- **CF adoption becomes a contained, behind-the-seam swap**, not a cross-cutting change touching five files across three addons. It can be done as its own focused change with its own validation, after this seam is confirmed in place.
- **New signals are cheap and safe.** Adding a client-facing RPC means one id constant, one typed helper, one dispatch branch — all in `DAFRPC` — rather than coordinating a fresh magic number across addons.
- **The wire format is unchanged by step 1.** The ids and `Param` shapes are identical to before, so this step is interoperable with already-deployed clients and is independently shippable.
- **ADR 0001 is superseded, not erased.** Its caution about not bloating the dependency surface before the gameplay loop is proven was correct at the time; this ADR records that the condition it was waiting on (client CF already loaded, RPC debt needing centralization) has now arrived.

## Open follow-on

Step 2 (swap `DAFRPC` internals to Community Framework `ModRPC`) is tracked as the next change on this seam. It should land after this centralized seam is confirmed working on the live server, and it should be validated as a self-contained change since no caller outside `DAFRPC` is touched.

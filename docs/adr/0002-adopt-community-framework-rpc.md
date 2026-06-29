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

## Open follow-on — step 2 DEFERRED (2026-06-29)

Step 2 (swap `DAFRPC` internals to Community Framework `ModRPC`) is **deferred indefinitely**. The original ADR framed it as a clean zero-caller-change swap behind the seam. Reading the actual CF RPC API surfaced two costs the ADR did not account for:

1. **Receive-side routing does not fit the seam shape.** Community Framework RPC does not deliver through vanilla `PlayerBase.OnRPC` — it intercepts its own id range and dispatches to handler functions registered on a singleton. The client UI state the round/HUD signals mutate (`s_KillFeed`, `s_DAF_PendingRoundSeconds`, the HUD widgets) is private to `DAFImprovements`' `PlayerBase`. Routing CF's receive path onto that state forces a choice: either `DAFImprovements` hard-depends on CF (it is currently CF-free and meant to be the generic addon), or a second client `PlayerBase` mod in `DAFDeathmatch` exposes a one-method public seam back into `DAFImprovements`. Neither is a zero-caller-change swap.

2. **The marginal benefit is thin.** Step 2's only additional payoff over step 1 is CF-managed, conflict-free ids. The `-7470000x` range is already conflict-free in practice (negative, far from vanilla and other mods). Step 1 already captured the locality and leverage wins — centralized ids, no duplicated broadcast loops, cheap new signals.

A future architecture review should not re-suggest step 2 unless one of these changes: the id range actually collides, CF becomes load-bearing for a different reason that justifies the receive-side restructure, or `DAFDeathmatch` fully absorbs the HUD widgets so `DAFImprovements` stops owning the receive state. Until then, `DAFRPC` stays on vanilla `RPCSingleParam`.

Priority shifts to the `DAFDeathmatch` manager god class (currently ~2,770 lines owning ~14 unrelated responsibilities), which is a larger architectural payoff than step 2's marginal id benefit.

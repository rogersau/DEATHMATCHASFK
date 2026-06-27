# Domain Docs

This repo uses a single-context domain-doc layout.

Expected files:

- `CONTEXT.md` at the repo root for project/domain context.
- `docs/adr/` for architectural decision records.

Skills that need domain context should read `CONTEXT.md` first when it exists, then relevant ADRs under `docs/adr/`.

If `CONTEXT.md` does not exist yet, infer carefully from `README.md`, `AGENTS.md`, and the codebase, then suggest creating or updating `CONTEXT.md` when domain decisions become stable.

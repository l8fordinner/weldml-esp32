# Agent Instructions — weldml-esp32

## Start of every session

1. **Read `docs/PROJECT_STATUS.md` first.** It is the canonical project state ledger: current phase, what is done, what is blocked, and the exact next action. Do not start work without reading it.
2. Check `docs/OPEN_QUESTIONS.md` for any unresolved question that blocks the area you are about to touch. If a blocker exists, surface it and stop rather than working around it.

## Secrets and environment

- **Never read, print, summarize, or inspect `.env` or any file containing credentials, tokens, or API keys.**
- Never commit `.env`, secrets, or generated `sdkconfig` files.

## Scanning discipline

- **Do not broad-scan** (recursive grep across the whole repo, `find /`, large `ls -R`, etc.) unless the user explicitly asks for it.
- Read specific files that are directly relevant to the task. Use targeted searches when you need to locate something.

## Build commands

Always pass the board explicitly. Never run plain `idf.py build`.

```sh
# Correct
idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build

# Wrong — do not use
idf.py build
```

A passing build means `idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build` completes with zero errors and zero warnings.

## Context and handoff

- Monitor context usage. At the **yellow threshold (~128K–130K tokens)**, stop implementation work and run the handoff procedure.
- Write a new "Session Handoff" block at the top of `docs/PROJECT_STATUS.md` with: what was done, what is next, the exact next-session prompt, branch and commit state, and working tree status.
- Commit the updated `docs/PROJECT_STATUS.md` before the session ends.
- Do not let context run into the red before handing off — compressed context loses detail that the handoff preserves.

## Project-specific knowledge

All project-specific context (hardware, stage status, open questions, feature formulas, parser contracts, fixture traceability) lives in `docs/PROJECT_STATUS.md` and `docs/OPEN_QUESTIONS.md`. Do not duplicate that content in generic skills or memory — read the source docs instead.

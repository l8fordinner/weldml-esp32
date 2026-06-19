# Context Policy

Rules for managing context, token budget, and session handoff in Claude Code.

---

## Session Start Checklist

At the start of every session (or after a context reset), read these in order:

1. `CLAUDE.md` — operating rules (Claude Code reads this automatically)
2. `docs/OPEN_QUESTIONS.md` — check for unresolved decisions blocking current work
3. `docs/MVP_REQUIREMENTS.md` — confirm scope before taking any action
4. `docs/PRIOR_WORK_CONTEXT.md` — hardware-verified facts and prior art
5. `NOTES.md` — current hardware test status

Do not read all docs speculatively. If the session is focused on a specific area
(e.g., LCD driver, SD init), read only the docs directly relevant to that area.

---

## What Must Be Re-Confirmed After a Long Gap

If more than a few days have passed since the last session, or after a context reset
mid-task, re-confirm before taking action:

- `docs/OPEN_QUESTIONS.md` resolution log — decisions may have changed
- `NOTES.md` — hardware tests may have been completed or invalidated
- `git log --oneline -10` — understand what changed since the last known state
- `git status` — confirm no unintended staged or modified files

Do not rely on memory of prior session state. Always read current file state.

---

## Token Budget and Context Thresholds

### Percentage thresholds

**At ~70% context used:**
- Do not start new implementation tasks.
- Update `docs/PROJECT_STATUS.md` with current state if not already current.
- Finish only work already in progress; do not open new files or new areas.

**At ~80% context used:**
- Stop the current task at the next safe boundary (never mid-edit, never mid-build).
- Write a handoff note (see below) in the conversation.
- Commit any completed work that passes `idf.py build`.
- Report to the user: done, in progress, next action.

**At ~85% context used:**
- State capture only. No new coding, no new planning.
- Output the handoff note, update PROJECT_STATUS.md, commit if needed, then stop.

### End-of-session requirement

Every major working session must end with `docs/PROJECT_STATUS.md` updated and committed.
Decisions made in chat are not recorded until they appear in a committed doc.
Never rely on conversation memory alone for decisions — write them to the file before closing.

### Do not let context fill silently

If a task is generating large outputs (build logs, PDF text, schematic dumps), use
`run_in_background` or subprocess redirection to avoid flooding the context. Summarize
findings rather than pasting raw tool output.

---

## Handoff Protocol

When ending a long session, leaving a task incomplete, or handing off between sessions,
write a brief handoff note in the conversation AND update `docs/PROJECT_STATUS.md`. Include:

- **Goal:** what this session was trying to accomplish.
- **Done:** what was completed and confirmed (build pass, hardware test, doc written).
- **Files changed:** list every modified or created file and whether it is committed.
- **Verification status:** for each file/component changed — built? flashed? hardware tested?
- **Open questions:** which OPEN_QUESTIONS.md items are still unresolved and block what.
- **Next action:** exactly what to do first in the next session (one concrete step).

Example format:
```
HANDOFF
  Goal: SD SPI init — first hardware test on Waveshare board.
  Done: components/sd_card/sd_card.c written; idf.py build passes.
  Files changed:
    components/sd_card/sd_card.c — committed (commit abc1234)
    docs/PROJECT_STATUS.md — committed (updated hardware test log)
  Verification: built yes / flashed no / hardware tested no
  Open questions: Q3 (SD→firmware handoff) and Q6 (file-completion signal) still open.
  Next action: Flash to Waveshare, run `idf.py monitor`, confirm SD mount log line appears.
```

---

## Subagent Usage

Spawn a subagent (Agent tool) only when:
- Broad codebase exploration would otherwise flood the main context.
- A task can run independently in parallel (e.g., research while editing).
- The user explicitly asks for an agent.

Do not spawn a subagent to do work the main session can handle in 2-3 tool calls.
Subagents start cold and re-derive context — prefer inline work when practical.

---

## Memory System

The project memory directory is at:
`/home/casey/.claude/projects/-mnt-j-ReposWSL-weldml-esp32/memory/`

Save to memory when:
- A hardware fact is confirmed on the bench (add to `NOTES.md` too).
- A question in `OPEN_QUESTIONS.md` is resolved.
- A user preference or workflow decision is established that isn't already in a doc.

Do not save ephemeral session state (current task progress, intermediate results)
to memory — use the handoff note in the conversation instead.

---

## Large File Handling

- PDFs: use `pdftotext` or PyMuPDF spatial extraction; never paste full raw output.
  Summarize relevant findings.
- Build logs: capture with `2>&1 | tail -30` for errors; never paste full output.
- Schematic extractions: filter for relevant net names; don't paste all blocks.

---

## Scope Discipline

If a user request would require implementing something in `docs/MVP_REQUIREMENTS.md`'s
"Out of Scope" list, confirm explicitly before proceeding. Do not silently expand scope
because it "seems related."

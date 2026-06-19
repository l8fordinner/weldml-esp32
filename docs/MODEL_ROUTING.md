# Model Routing Policy

Abstract role definitions for Claude Code model selection on this project.
Roles are defined by capability requirement. Do not hard-code model names here —
they rotate and committed files become stale.

Verify actual command mappings against current Claude Code documentation before relying on them.

---

## Role Definitions

| Role | Capability requirement | When to use |
|------|----------------------|-------------|
| **Planning / review** | Strongest available reasoning model | Architecture decisions, OPEN_QUESTIONS.md resolution, PR review gating, complex multi-file reasoning, any decision that would be hard to reverse |
| **Implementation** | Default Claude Code model | All day-to-day coding, editing, build/flash tasks, file creation, documentation |
| **Summarization / handoff** | Reliable long-context model | Session handoff notes, context compression, doc synthesis at context limit |
| **Local / cheap** | Smallest acceptable model for the task | Only for fully specified, bounded tasks (e.g., format conversion, regex substitution) — requires clear input/output spec before use |

---

## Command Mappings (verify before use)

The following mappings are recorded as reference, not as authoritative configuration.
Check current Claude Code docs to confirm they are still accurate.

| Command / mechanism | Likely role | Verified? |
|---------------------|-------------|-----------|
| Default session model | Implementation | Unverified — check `/model` or `claude --version` |
| `/fast` toggle | May map to strongest reasoning model | Unverified — check Claude Code docs |
| `model:` in settings.json | Overrides default | Unverified — check settings schema |

Update this table when mappings are confirmed.

---

## Routing Rules

1. Use the **implementation** model for all routine work unless a specific role is warranted.
2. Switch to the **planning/review** model only when the task involves a decision that is
   hard to reverse, affects multiple components, or resolves an OPEN_QUESTIONS.md item.
3. Do not leave the session in a non-default model state. Switch back after the task.
4. Never use a **local/cheap** model for tasks involving hardware-pin assignments, SD
   ownership transitions, or TinyUSB initialization — the cost of error is high.

---

## What This File Is Not

- This is not an active router configuration. No routing software is installed.
- This is not a provider comparison. Claude Code native is the only provider for MVP.
- This does not define model names or version numbers. Those belong in local config only.

---

## Post-MVP Considerations

When the project reaches the OTA/MQTT/multi-PR phase:
- GSD may become relevant for workspace and seed management (see REQUIREMENTS.md note)
- Autonomous PR workflow via `CronCreate` + `RemoteTrigger` may use a different model
  assignment for CI-style tasks
- Revisit this file when that phase begins; do not add speculation now

# ThingsBoard Setup — Industry 4.0 Lab

Manual admin walkthrough for `iot.mwe-inc.com` (ThingsBoard CE v4.3.1.3). Not firmware work —
these are steps for a human to click through in the ThingsBoard UI. Decision rationale is in
`OPEN_QUESTIONS.md` Q11–Q15.

Prerequisite: a System Administrator login for `iot.mwe-inc.com`.

---

## 1. Create a dedicated tenant profile

The "Industry 4.0 Lab" tenant currently uses the `default` profile (all limits unlimited). Per
Q15, give it its own profile with generous-but-bounded containment limits, so a credential leak
or a malfunctioning/runaway device can't take down the platform or run up usage.

1. Log in as **System Administrator**.
2. Left sidebar → **Tenant profiles** → **+ Add tenant profile**.
3. **Name**: `Industry 4.0 Lab` (or `industry-4-lab-standard` — pick whichever naming
   convention the instance already uses for other profiles, if any exist).
4. Leave **"Use isolated ThingsBoard Rule Engine queues"** unchecked — not needed at this scale.
5. Fill in the **Profile configuration** sections with these values (all are judgment calls —
   ThingsBoard doesn't publish official breach/runaway-containment numbers, so adjust freely;
   these are sized to be far above real expected load but well below anything that could hurt
   the platform):

   **Entities**
   | Field | Value |
   |---|---|
   | Devices maximum number | 25 |
   | Dashboards maximum number | 10 |
   | Assets maximum number | 10 |
   | Users maximum number | 20 |

   **Rule Engine** (these are *monthly* quotas, not absolute or rolling — confirmed from
   ThingsBoard docs; they reset each calendar month and the platform disables the tenant's
   device connections at 100% usage)
   | Field | Value |
   |---|---|
   | Rule Engine executions maximum number | 100000 |
   | Transport messages maximum number | 100000 |

   **Calculated fields** — leave at the dialog's defaults (5 / 1000 / 10). Unrelated to
   containment; no reason to change them.

   **Time-to-live**
   | Field | Value |
   |---|---|
   | Data points storage days maximum number | 1000000 |
   | Storage TTL days by default | 0 (unlimited/forever) |
   | Alarms TTL days | 0 |
   | RPC TTL days | 0 |
   | Queue stats TTL days | 0 |
   | Rule Engine exceptions TTL days | 0 |

   Leave the TTL-days fields other than "Data points storage days maximum number" at 0
   (unlimited) — per Q15, retention is intentionally not capped here; the low expected data
   volume from a teaching lab has more archival value kept than purged, and runaway ingestion
   is contained by the Rule Engine/Transport monthly quotas and the rate limits below, not by
   TTL.

   **Alarms and notifications**
   | Field | Value |
   |---|---|
   | SMS enabled | **Off** (toggle it off — not used, no reason to leave an unused channel unlimited) |
   | Emails sent maximum number | 1000 |
   | Alarms created maximum number | 10000 |
   | Alarms reevaluation interval (seconds) | 60 (default, leave as-is) |

   **Debug**
   | Field | Value |
   |---|---|
   | Maximum debug duration (min) | 15 (default, leave as-is) |

   **Files**
   | Field | Value |
   |---|---|
   | Maximum total size of resources files (bytes) | 104857600 (100 MB) |
   | Maximum total size of OTA package files (bytes) | 10485760 (10 MB) — deliberately small and non-zero: this project doesn't use ThingsBoard OTA at all, so this closes off unbounded storage via an unused feature rather than leaving it unlimited by default |
   | Maximum resource file size (bytes) | 5242880 (5 MB) — plenty for a dashboard logo/branding asset |

   **WS**
   | Field | Value |
   |---|---|
   | Sessions per tenant maximum number | 50 |
   | Subscriptions per tenant maximum number | 500 |
   | Sessions per customer maximum number | 20 |
   | Subscriptions per customer maximum number | 200 |

   **Rate limits** — each field is not a text box; clicking the **+** next to it opens an
   "Edit ... rate limits" dialog with **Number of messages** / **Per seconds** fields for one
   tier, and an **Add limit** button to add a second (or third) tier for the same field. The
   dialog's **Preview** box shows the resulting `count:seconds[,count:seconds...]` string once
   filled in (e.g. `50:1,2000:60` = burst up to 50/sec, but no more than 2000 in any 60-second
   window) — that string is just the internal representation, you don't type it directly.
   "Tenant" fields are aggregate across every device under the tenant; "device" fields are
   per-individual-device — the device ones are the direct defense against one compromised or
   malfunctioning unit.

   | Field | Tier 1 (msgs / sec) | Tier 2 (msgs / sec) | Resulting preview |
   |---|---|---|---|
   | Transport tenant messages | 100 / 1 | 5000 / 60 | `100:1,5000:60` |
   | Transport device messages | 50 / 1 | 2000 / 60 | `50:1,2000:60` |
   | Transport tenant telemetry messages | 100 / 1 | 5000 / 60 | `100:1,5000:60` |
   | Transport device telemetry messages | 50 / 1 | 2000 / 60 | `50:1,2000:60` |
   | Transport gateway messages | — | — | Leave "Not set" (don't open the dialog) |
   | Transport gateway device messages | — | — | Leave "Not set" |
   | Transport gateway telemetry messages | — | — | Leave "Not set" |
   | Transport gateway device telemetry messages | — | — | Leave "Not set" |

   For each of the four fields with two tiers: click **+**, enter Tier 1's numbers, click
   **Add limit**, enter Tier 2's numbers in the row that appears, confirm the Preview box
   matches the "Resulting preview" column, then **Save** on that sub-dialog before moving to
   the next field.

   The gateway fields are deliberately left unset, not overlooked — this project's ESP32
   connects as a direct device, not through ThingsBoard's Gateway concept, so those fields are
   never exercised either way.

6. **Description** (optional): note why this profile exists, e.g. "Industry 4.0 Lab — bounded
   containment limits, see weldml-esp32 OPEN_QUESTIONS.md Q15."
7. Click **Add**.

## 2. Assign the profile to the "Industry 4.0 Lab" tenant

1. Still as System Administrator, left sidebar → **Tenants**.
2. Open the existing **"Industry 4.0 Lab"** tenant (already created).
3. Edit it (pencil icon) → change the **Tenant profile** dropdown from `default` to the profile
   created in step 1.
4. Save.

## 3. Create the device entity for the ESP32

1. Either stay as System Administrator and switch into the tenant (there's usually a "Login as
   tenant admin" / switch-context option on the tenant's detail page), or log in directly as a
   Tenant Administrator for "Industry 4.0 Lab" if one already exists.
2. Left sidebar → **Devices** → **+ Add device**.
3. **Name**: something identifying this specific unit, e.g. `weldml-esp32-lab-01` (so a second
   unit later gets its own device entity and its own token — the tenant profile's "Devices
   maximum number: 25" leaves plenty of room to grow).
4. Leave the device profile at `default` unless you want device-level telemetry validation
   rules later — not needed for this milestone.
5. Save, then open the device and go to its **Details** or **Manage credentials** action.
6. Copy the **Access token** — this is the value the ESP32 firmware needs (stored in NVS,
   per the Milestone 2 spec's configuration decision). Treat it like a password: it grants
   telemetry-write access to this device entity. Don't paste it anywhere public (commit
   messages, chat logs shared outside this conversation, etc.).

**Optional smoke test**, before firmware exists, to confirm the token and endpoint work.
ThingsBoard's own "Device created — let's check connectivity!" dialog shows an example using
`http://iot.mwe-inc.com:8080/...` — **ignore that one**, port 8080 is ThingsBoard's internal
plain-HTTP transport port and is not exposed to the internet on this instance (confirmed:
`curl` to it times out). Use the HTTPS endpoint on port 443 instead, matching the Milestone 2
firmware design (Q13):

Bash/WSL:
```bash
curl -v -X POST "https://iot.mwe-inc.com/api/v1/$ACCESS_TOKEN/telemetry" \
  -H "Content-Type: application/json" \
  -d '{"test":true}'
```

Windows `cmd.exe` (no `$VAR` substitution, and JSON needs double-quote escaping instead of
single quotes — paste the literal token into the URL):
```
curl -v -X POST https://iot.mwe-inc.com/api/v1/YOUR_ACCESS_TOKEN_HERE/telemetry -H "Content-Type: application/json" -d "{\"test\":true}"
```

A `200 OK`, plus the value showing up under the device's "Latest telemetry" in the ThingsBoard
UI, confirms the device/token/HTTPS-endpoint chain works end-to-end before any firmware code
depends on it.

## 4. Create and brand the dashboard

Per Q10: this is **dashboard-level branding** (logo, color theme, custom widgets) — not
platform-wide White Labeling, which is a paid ThingsBoard PE-only feature and isn't what was
asked for or needed here.

1. As Tenant Administrator for "Industry 4.0 Lab", left sidebar → **Dashboards** →
   **+ Add new dashboard** (or **Import dashboard** if you have a `.json` dashboard template
   to start from — ThingsBoard's public widget library has weld/IoT-telemetry-style templates
   worth browsing first).
2. Give it a real title, e.g. "WeldML — Industry 4.0 Lab".
3. Add widgets bound to the `weldml-esp32-lab-01` device's telemetry keys — a time-series
   table is a natural fit for showing individual weld results (feature values, PASS/FAIL) as
   they arrive. Nothing will show until the firmware in issue #1 actually uploads something.
   For a running PASS/FAIL total, see the dedicated steps below.
4. For branding:
   - **Logo**: add an image widget (or use the dashboard's built-in logo/header settings if
     this ThingsBoard build exposes one) pointing at your lab's logo image.
   - **Color theme**: Dashboard settings (gear icon) → look for background color / widget
     theme options: set to match your lab's branding.
   - **Custom CSS**, if you want finer control than the settings dialog offers: some
     ThingsBoard dashboard widgets support custom CSS/HTML directly in their widget config.
5. Assign/share the dashboard to the tenant (and to a Customer entity too, if you want a
   read-only public-facing view for lab visitors distinct from the admin view — optional,
   not required for the demo case described).
6. Save.

## 5. Add a PASS/FAIL running-total widget

**Verified live against `iot.mwe-inc.com` on 2026-08-05/06, time window corrected 2026-08-08.**
The original approach in this
section (a `predicted_class`-only payload plus a data-key **post-processing function** to derive
PASS/FAIL, no firmware changes) does **not work** and has been replaced. Root cause: ThingsBoard's
`Sum` aggregation reduces the whole time window to a single already-summed value *before* any
post-processing function runs — so a function like `return value === 0 ? 1 : 0` only ever sees
one pre-summed number, never the individual raw readings, and both PASS/FAIL counters come back
`0` regardless of real data. A dashboard-level key **filter** can't substitute either — ThingsBoard
filters only apply to the *latest* value, not historical time-series points, so they can't scope
a running-total widget's query to `predicted_class == 0` vs `== 1`.

The fix: firmware uploads two additional telemetry keys, **`pass_flag`** and **`fail_flag`**
(`1`/`0`, mutually exclusive), alongside `predicted_class` — see `MILESTONE_2_REQUIREMENTS.md`.
Each becomes its own widget data key with plain `Sum` aggregation and **no post-processing
function needed at all**, which sidesteps the aggregation-order problem entirely. (A Rule Engine
transform node could derive the same two keys server-side instead of a firmware change, but that
trades a small, already-cheap firmware addition for actual ThingsBoard-side scripting — not worth
it here.)

1. On the dashboard, click **+ Add new widget** → widget bundle **Charts** → **Bar chart** (Basic
   mode). A **Pie chart** works the same way if you'd rather see PASS/FAIL as a proportion.
2. **Datasource**: select **Device** → `weldml-esp32-lab-01`.
3. **Series** (this dialog's data-key list is called "Series," not "Data keys," in this
   ThingsBoard build):
   - First row — Key: **`pass_flag`**, Label: `PASS total`, Color: green.
   - Second row — Key: **`fail_flag`**, Label: `FAIL total`, Color: red.
   - For each row, click the **pencil icon** to open **"Data Key Configuration"** — a single
     flat dialog in this version (no "General"/"Advanced" tabs despite what an earlier draft of
     this doc claimed). Set **Aggregation: Sum**. Leave **"Use data post-processing function"**
     off for both — do not re-add the `predicted_class`-based function from the old approach.
4. **Time window — this is the part that actually breaks if you get it wrong:**
   - Click **"Use widget time window"** (next to "Use dashboard time window") so this widget
     doesn't inherit whatever the dashboard's global window is set to.
   - Click the time-window button (e.g. "Realtime - Current day") → switch to the **History**
     tab. **Realtime mode does not reliably sum pre-existing points** — confirmed empirically:
     it undercounted a known 3/2 PASS/FAIL split as 2/2, apparently dropping whichever point
     existed before the live subscription started. History mode re-queries on load and was
     confirmed exact.
   - **Within History, use "Last N days," not "Range" (custom start/end dates) and not
     "Relative → Current day."** Both of the latter two are corrected as of 2026-08-08:
     - **"Current day" is too narrow** — `weld_cloud_build_payload()` timestamps each upload
       using the weld's own embedded timestamp, not upload time (a deliberate design choice so
       historical data doesn't get misfiled under today's date). Any upload not timestamped
       *today* — including every one of the historical lab fixtures under `test_data/` — is
       invisible to a "Current day" window.
     - **"Range" (a fixed start/end date pair) looks like a fix but goes stale.** Confirmed
       against [ThingsBoard's own docs](https://thingsboard.io/docs/user-guide/time-window/):
       Range is a "Fixed interval. Does not update automatically" — its end date stays anchored
       wherever you set it and does not advance to include tomorrow's data. "Last N days" is the
       one that continuously slides forward with real time.
     - Set **"Last" → `2700` days** (this build's Last picker only offers a day-unit, no
       weeks/months/years — just use a large day-count). That comfortably covers every fixture
       under `test_data/` (oldest is `2020-07-17`, ~2213 days back from 2026-08-08) with margin,
       while still being tight enough not to sum over irrelevant decades-old data. **Verified
       live:** after this change, the widget correctly showed PASS:7 / FAIL:4 / Total:11 across
       all historical uploads, not just today's.
   - **Gotcha:** editing a widget's Series list (e.g. changing a data key) can silently reset
     the time window back to the Realtime default. After any series edit, reopen the time-window
     button and confirm it still says **History - Last 2700 days** before saving.
5. Title the widget "PASS / FAIL Totals" and click **Apply**.
6. **Save at all three levels** — this is the other thing that bit us during setup:
   1. Data Key Configuration dialog → **Save**
   2. Add/Edit widget dialog → **Apply**
   3. The dashboard itself → click the checkmark/floppy-disk icon in the top toolbar to actually
      persist to the server. Skipping this step means everything reverts on the next page refresh.

As new weld results get uploaded, this widget's two bars grow — that's the running total. A
**Line chart** with the same two data keys and `Sum` aggregation over a narrower bucket interval
trends upward similarly if you want a literal climbing line instead of two static bars.

---

## Notes

- Steps 1–2 are one-time. Step 3 repeats per physical ESP32 unit the lab acquires. Step 4 is
  iterative — expect to revisit widget choices once real data is flowing.
- If any of the exact menu labels above don't match what you see in the live UI, ThingsBoard
  CE's admin navigation is fairly stable across 4.x point releases but does shift occasionally
  — the concepts (Tenant profiles → Tenants → Devices → Dashboards, all under the System
  Admin/Tenant Admin sidebars) will still be in roughly this shape.
- This document only covers the ThingsBoard side. The firmware side (webserver, upload button,
  cleanup button) is tracked as
  [GitHub issue #1](https://github.com/l8fordinner/weldml-esp32/issues/1).

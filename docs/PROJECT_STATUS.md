# Project Status

Single source of truth for cross-session continuity.
Update this at the end of each working session and commit it with the session's changes.

---

## Current State (2026-06-20)

**Phase:** Stage 5 Linux/Pi MSC verification — write-idle detection confirmed, `write_placeholder()` silently failing.

**Confirmed this session (2026-06-20, JSON diagnosis):**
- Write-idle detection confirmed on Pi: LCD went CYAN → YELLOW (writing) → BLUE (brief blink) → GREEN.
  `tud_msc_write10_complete_cb` fires. Monitor task detects idle. `process_sd()` runs.
- `weldml_result.json` NOT found after controlled read-only remount (no dirty-bit write from host
  after process_sd()). Confirmed by raw disk scan (`dd | strings | grep pending_parse` → 0 hits).
- FAT filesystem is healthy: `fsck.vfat -n /dev/sda1` → 8 files, 8/948992 clusters, no errors.
- **Root cause isolated: `write_placeholder()` fails silently.**
  `fopen("/sdcard/weldml_result.json", "w")` returns NULL. Function logs ESP_LOGE (invisible without
  serial monitor) and returns. `process_sd()` proceeds to `set_state(WELD_STATE_SUCCESS)` → GREEN.
  This is the documented bug (known since Stage 5 functional test on Windows).
- **FAT coordination theory ruled out.** The `tud_mount_cb`/`tud_umount_cb` callbacks in
  `tusb_msc_storage.c` correctly manage `is_fat_mounted`: host connects → `tud_mount_cb` →
  `tinyusb_msc_storage_unmount()` (host gets storage); idle fires → `process_sd()` →
  `tinyusb_msc_storage_mount()` → FatFS does a REAL fresh mount each cycle. No stale state.
- **Why fopen fails: UNKNOWN** — requires serial log capture to see errno. Filesystem is clean,
  card initialized, plenty of free space. Failure is inside FatFS during cluster/directory write.

**Branch:** `main`  
**Last committed:** `deecb30`  
**Working tree:** CLEAN

**Board state:** Connected to Pi USB hub. Running Stage 5 firmware (c98cbd3 build, deecb30 committed).
USB enumerating as `303a:4003`. SD card visible as sda1.

**Active flash path:** Direct-PC USB via usbipd-win (confirmed working). Pi RFC2217 path NOT re-tested
this session (no flash needed — firmware unchanged). Pi path may now work after reboot, unverified.

---

## Pi/Workbench USB Recovery Rule

**Apply this before spending time debugging USB failures on the Pi:**

If flashing or USB enumeration through the Pi/workbench fails with any of these symptoms:
- `dwc_otg` EPROTO state in dmesg
- Device stuck at `303a:1001` (JTAG/serial) without switching to `303a:4003`
- `esptool` "No serial data received"
- `openocd` `libusb_get_string_descriptor_ascii() = -1`
- RFC2217 timeout or "Remote does not accept parameter change"
- Missing `/dev/ttyACM0` devnode
- Stale `cdc_acm` state refusing to rebind

**Recovery sequence:**
1. Disconnect ESP32 from Pi USB hub.
2. Reboot the Pi: `ssh casey@192.168.1.43 "sudo reboot"` — then wait 30–60 s.
3. Confirm workbench API is back: `curl -s http://192.168.1.43:8080/api/info`
4. Confirm SLOT3 is `absent` in `/api/devices` (clean baseline before reconnect).
5. Reconnect ESP32 to Pi USB hub.
6. Check `/api/devices`, `dmesg | tail -30`, `lsusb`, and block-device enumeration.
7. Only proceed with Pi-side flashing or Linux MSC testing after state is clean.

**Why this works:** The Pi's `dwc_otg` USB host controller can enter an EPROTO (USB protocol error)
state that persists across ESP32 power cycles and driver rebinds. A full Pi reboot clears dwc_otg
state completely. Do NOT waste time with `cdc_acm` unbind/rebind or `esptool` retries — they do not
fix host-controller-level EPROTO. If a Pi reboot fixes enumeration, document it as the required step.

**Windows/direct-PC path (usbipd-win → WSL) is the fallback when Pi is stuck:**
- Disconnect from Pi, hold Key1, plug into Windows PC, release Key1.
- Admin PowerShell: `usbipd bind --busid <bus-id>` (one-time bind, persists until unbound).
- WSL: `"/mnt/c/Program Files/usbipd-win/usbipd.exe" attach --wsl --busid <bus-id>`
- Flash with WSL esptool. See prior handoffs for exact command.

---

## Session Handoff — 2026-06-20 (Stage 5 — write_placeholder() failure isolated)

**Goal:** Identify whether missing JSON is due to `write_placeholder()` silent failure or FatFS/Linux vfat coordination.

**Test performed:**
- Board on Pi USB hub, enumerating 303a:4003, sda1 visible.
- Mounted sda1 rw, wrote CSV, synced, unmounted. User watched LCD.
- Waited 8 seconds (> 5 s idle threshold). Mounted sda1 read-only. Checked for JSON.

**Results:**

| Observable | Expected | Actual | Verdict |
|---|---|---|---|
| LCD during idle | CYAN→YELLOW→BLUE→GREEN | YELLOW (write), brief blink (BLUE), GREEN | **PASS — process_sd() ran** |
| weldml_result.json (read-only mount) | Present | **NOT FOUND** | **FAIL** |
| Raw disk scan (`dd \| strings \| grep pending_parse`) | 1+ hits | **0 hits** | **Never written** |
| FAT filesystem health (`fsck.vfat -n`) | Clean | 8 files, 8/948992 clusters, no errors | **HEALTHY** |

**Root cause confirmed: `write_placeholder()` silent fopen failure.**

`fopen("/sdcard/weldml_result.json", "w")` returns NULL. The function logs an error and returns.
`process_sd()` does not check the return, proceeds to `tinyusb_msc_storage_unmount()` and
`set_state(WELD_STATE_SUCCESS)` → LCD GREEN. JSON never written.

**FAT coordination theory ruled out:**
The prior session's hypothesis (FatFS stale cache from startup mount causing cluster conflict)
was incorrect. Reading `tusb_msc_storage.c` confirms:
- `tud_mount_cb()` calls `tinyusb_msc_storage_unmount()` — host connecting gives host storage access
- `tud_umount_cb()` calls `tinyusb_msc_storage_mount()` — host disconnecting reclaims for FatFS
- `process_sd()` → `tinyusb_msc_storage_mount()` is a REAL fresh FatFS mount (not a no-op)
- The write-idle coordination is architecturally correct

**Why fopen fails: unknown** — requires serial log. FAT is clean, card initialized, 948984 free clusters.
Failure is inside FatFS during cluster allocation or directory write — exact errno not visible without logs.

**Next session — fix and diagnose:**
1. Fix `write_placeholder()`: change return type to `bool`, return false when fopen fails.
   Fix `process_sd()`: call `set_state(WELD_STATE_FAILURE)` if write_placeholder() returns false.
   Add errno logging: `ESP_LOGE(TAG, "fopen failed errno=%d", errno)` before returning false.
2. Build: `idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build`
3. Flash: direct-PC USB (usbipd-win → WSL). See flash procedure in recovery handoff below.
4. Run write test. If LCD shows RED → serial log has the errno.
5. Start passive serial read BEFORE triggering write test:
   `ssh casey@192.168.1.43 "python3 -c 'import serial; s=serial.Serial(\"/dev/ttyACM0\",115200,timeout=0.2); [print(s.read(2048).decode(errors=\"replace\"),end=\"\") for _ in range(200)]'" > /tmp/proc_log.txt &`
   Then mount/write/umount. After 15 s: `cat /tmp/proc_log.txt`. Look for fopen errno.

**Do NOT start Stage 6.** Fix and diagnose the fopen failure first.

---

## Session Handoff — 2026-06-20 (Stage 5 Linux/Pi MSC verification)

**Goal:** Verify Stage 5 firmware works as a Linux USB MSC device on Pi after Pi reboot cleared dwc_otg state.

**Context:**
- Windows direct-PC USB test (prior session) confirmed TinyUSB CDC+MSC works and write-idle detection
  fires (LCD GREEN). But `weldml_result.json` was not confirmed in Windows Explorer (FAT cache stale).
- Pi path was previously blocked by dwc_otg EPROTO. Pi was rebooted this session to clear it.
- Board moved from Windows PC to Pi USB hub. No firmware changes made this session.

**Pi USB host behavior after reboot:**

| Check | Expected | Actual | Result |
|---|---|---|---|
| Workbench API `/api/info` | Online | `{"hostname":"PiEspWrkbench","slots_configured":3}` | PASS |
| SLOT3 baseline (before connect) | `state: absent` | `state: absent` | PASS |
| USB enumeration after connect | `303a:4003` | `303a:4003`, `sda`/`sda1` 29 GiB | **PASS** |
| dmesg: PHY switch | `303a:1001 → 303a:4003` | Confirmed in dmesg | **PASS** |
| Pi can mount `/dev/sda1` | FAT filesystem visible | Mounted cleanly, files visible | **PASS** |
| Write CSV to sda1 | File lands on SD card | Confirmed after `drop_caches` | **PASS** |
| SCSI errors | None | None in dmesg throughout | PASS |
| `weldml_result.json` after idle | Present on SD | **NOT FOUND** | **UNVERIFIED** |

**Write-idle detection status: UNVERIFIED**

The JSON file did not appear after writing CSV files and waiting 8 seconds (> 5 s idle window).
LCD color was NOT reported by user at session end — cannot confirm whether `process_sd()` ran.

**Root cause investigation (analysis only — no firmware changed):**

Three possible explanations, ordered by likelihood:

1. **`tud_msc_write10_complete_cb` not firing for Pi writes, so idle timer never starts.**
   - No LCD color change was observed/reported. If LCD stayed CYAN, callback did not fire.
   - USB SCSI WRITE10 commands were issued (files are physically on SD), so CSW was received.
   - `tud_msc_write10_complete_cb` is called after CSW — it should fire. Needs confirmation.

2. **FAT coordination conflict (FatFS vs. Linux vfat, most likely root cause if callback fires).**
   - `usb_msc_sd_init()` calls `tinyusb_msc_storage_mount("/sdcard")` BEFORE `tinyusb_driver_install()`.
   - This sets `is_fat_mounted = true` at TinyUSB start. When `process_sd()` calls `mount()`, it's a no-op.
   - When `process_sd()` calls `tinyusb_msc_storage_unmount()` at end, FatFS is unregistered (`is_fat_mounted = false`).
   - After unmount, Linux sends more WRITE10 commands (next mount's dirty bit, subsequent tests).
   - These raw sector writes go to the same FAT/directory sectors FatFS just updated for the JSON.
   - Linux's stale in-memory FAT table does not know about FatFS's JSON allocation → cluster overlap → JSON overwritten.
   - Confirmed by `drop_caches` test: weld_test.csv and weld_run2.csv ARE physically on SD, meaning
     `_msc_storage_write_sector` did succeed at some point — which only happens when `is_fat_mounted = false`.
     This means `process_sd()` already ran (and unmounted FatFS) BEFORE those writes landed. JSON written
     by FatFS may have been overwritten by the subsequent Linux writes that used a conflicting FAT view.

3. **`write_placeholder()` silent failure (documented bug from previous session).**
   - If `fopen(RESULT_PATH, "w")` fails for any reason, function returns without propagating failure.
   - `process_sd()` still calls `set_state(WELD_STATE_SUCCESS)` — LCD shows GREEN regardless.
   - This bug is the documented fix needed before Stage 6.

**What is needed to isolate which cause:**
- Ask user to report LCD color after next write test — CYAN = callback not firing, GREEN = process_sd ran.
- Controlled test: write CSV → wait 7s (no remount) → check for JSON by mounting read-only
  (`mount -o ro /dev/sda1`) so no dirty-bit write contaminates the FAT.
- If JSON appears on read-only mount → FAT conflict theory confirmed (prior remounts overwrote it).
- If JSON absent on read-only mount → `write_placeholder()` is failing or LCD is CYAN (callback not firing).

**Pi reboot benefit confirmed:**
The Pi reboot cleared the prior `dwc_otg` EPROTO state that had blocked all USB communication for
multiple sessions. After reboot: clean 303a:1001 → 303a:4003 PHY switch, sda1 enumerated, R/W confirmed.
This proves Pi is a viable USB host for Stage 5 testing — it was the host controller state, not firmware.

**Why Linux/Pi behavior matters (even though direct-PC USB works):**
The Kawasaki robot's USB host (the real deployment target) is an embedded Linux system, not Windows.
Robot-side USB MSC behavior (SCSI TUR, write caching, FAT flush patterns) is closer to Linux than to
Windows Explorer. Windows Explorer uses FAT buffering and user-visible drive refresh cycles that differ
from how a robot's file writer will behave. Stage 5 must be verified on Linux before being trusted
in the field.

**Stage 5 completion criteria (all must be met before Stage 6):**
- [ ] `weldml_result.json` confirmed present on SD card after write-idle cycle on Pi
- [ ] LCD color sequence CYAN→YELLOW→BLUE→GREEN confirmed during write-idle cycle on Pi
- [ ] Fix write_placeholder() silent failure: return bool, set FAILURE state on false (one-line fix)
- [ ] Serial log captured during write-idle cycle (confirm `find_newest("csv")` finds file and `write_placeholder()` succeeds)

**Next concrete steps (in order):**
1. Reconnect board to Pi (it's already there). Ask user to watch LCD.
2. Mount sda1, write a CSV, sync, umount. Watch for LCD CYAN→YELLOW→BLUE→GREEN.
3. After idle fires (7–8 s), mount read-only: `sudo mount -o ro /dev/sda1 /tmp/sdmount && ls -la /tmp/sdmount/`
4. If JSON found → FAT conflict is the cause. Fix: do not remount between `process_sd()` and JSON check.
5. If JSON absent → `write_placeholder()` is failing silently or callback not firing. Fix the silent failure bug.
6. After JSON is confirmed: apply silent failure fix, build, flash (direct-PC USB), re-test on Pi.
7. Capture serial log (boot UART GPIO43/44 or boot-time USB JTAG before PHY switch).

**Flash path status:**
- Direct-PC USB (usbipd-win → WSL): **CONFIRMED WORKING** (prior session, deecb30). Use this for flashing.
- Pi RFC2217 path: **NOT RE-TESTED** this session. Likely works after reboot, but unverified.
  To test: stop OpenOCD → `idf.py -p rfc2217://192.168.1.43:4003 flash -D BOARD=waveshare-esp32-s3-lcd-147`

---

## Session Handoff — 2026-06-20 (Stage 5 write-idle functional test)

**Goal:** Verify SCSI write-idle detection: copy CSV → 5 s idle → LCD transitions → JSON appears.

**Test procedure:**
- Board on PC USB (usbipd-win); MSC visible on Windows as drive K.
- User copied `test.csv` into drive K root.
- Waited 6+ seconds.

**Results:**

| Observable | Expected | Actual | Verdict |
|---|---|---|---|
| LCD YELLOW during write | Activates during MSC writes | Observed as green throughout — transitions may have been too fast to notice | Inconclusive |
| LCD BLUE during processing | Brief during FatFS access | Same as above | Inconclusive |
| LCD GREEN at completion | WELD_STATE_SUCCESS | **Confirmed GREEN** | PASS |
| MSC returns to ready | Drive K remains accessible | Drive K accessible; test.csv visible | PASS |
| JSON file visible in Explorer | `weldml_result.json` in K:\ root | **Not visible** — Windows FAT cache stale | Unconfirmed |

**Key observations:**

1. **GREEN = SUCCESS confirmed.** `set_state(WELD_STATE_SUCCESS)` in `weld_processor.c` sets GREEN.
   The screen was already GREEN when the board first booted today (before the user copied the CSV).
   This means Windows auto-wrote something at MSC mount time (Windows often writes System Volume
   Information, dirty-bit updates, etc.), which triggered `tud_msc_write10_complete_cb`, started
   the idle timer, and ran `process_sd()` automatically. A second cycle then ran after the CSV copy.

2. **JSON not confirmed in Windows Explorer.** After `tinyusb_msc_storage_unmount()`, TinyUSB MSC
   sends UNIT_ATTENTION to the next SCSI Test Unit Ready. Windows is supposed to re-read the
   directory, but in practice the Explorer window does not auto-refresh and F5 did not show the
   new file. The JSON is likely on the SD card but requires a drive eject+replug to appear.
   Raw block device inspection from WSL failed (permission denied without sudo). Pi not used per scope.

3. **Silent failure bug in write_placeholder().** If `fopen(RESULT_PATH, "w")` fails, the function
   logs an error and returns — but `process_sd()` does not check the return and still calls
   `set_state(WELD_STATE_SUCCESS)`. LCD shows GREEN even if JSON write failed. This bug means
   GREEN does not confirm the JSON was written.

**Next session — confirm JSON and fix silent failure:**
1. Boot board fresh; attach to WSL via usbipd; run `sudo mount /dev/sde1 /tmp/sdcard` and
   check for `weldml_result.json`. This requires user to type password interactively (`! sudo mount ...`).
2. Alternatively: eject and replug USB from Windows to force Explorer to re-read FAT directory.
3. Fix the silent failure: `write_placeholder()` should return `bool`; `process_sd()` should call
   `set_state(WELD_STATE_FAILURE)` if it returns false. This is a one-line fix.
4. Capture serial log during a write-idle cycle to confirm `find_newest("csv", ...)` finds the file
   and `write_placeholder()` succeeds.

**Do NOT start Stage 6 until JSON write is confirmed on hardware.**

---

## Session Handoff — 2026-06-20 (Stage 5 recovery — direct-PC USB unblocked everything)

**Goal:** Restore flashing access; do not change Stage 5 policy or start Stage 6.

**Root cause (confirmed):** Pi dwc_otg USB host controller entered EPROTO state during prior
recovery attempts. This corrupts USB data transactions at the host-controller level — even after
power-cycling the ESP32 and rebinding cdc_acm, the Pi host cannot complete USB transfers.
`esptool` got "No serial data received" because the host was broken, not the ESP32.

**Recovery method:** Direct-PC USB via usbipd-win → WSL.
1. Disconnected USB cable from Pi, held Key1 (BOOT), plugged into Windows PC, released Key1.
2. Windows showed `303a:1001` on bus 7-2.
3. Admin PowerShell: `usbipd bind --busid 7-2`
4. WSL: `"/mnt/c/Program Files/usbipd-win/usbipd.exe" attach --wsl --busid 7-2`
5. `/dev/ttyACM0` appeared in WSL immediately.
6. Flashed with:
   ```
   ~/.espressif/python_env/idf5.3_py3.12_env/bin/python -m esptool \
     --chip esp32s3 -p /dev/ttyACM0 -b 115200 \
     --before=no_reset --after=no_reset --no-stub \
     write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
     0x0 build/bootloader/bootloader.bin \
     0x8000 build/partition_table/partition-table.bin \
     0x10000 build/weldml-esp32.bin
   ```
   All 3 regions SHA-verified. esptool v4.11.0 (IDF env). Note: v4.11.0 uses underscores
   (`write_flash`, `no_reset`, `--flash_mode`) — not dashes like newer versions.

**Result after boot (Key2):**
- USB re-enumerated as `303a:4003` (USB Serial Device COM8 + USB Mass Storage Device)
- Windows opened an MSC folder (SD card visible) — TinyUSB CDC+MSC is working
- LCD shows green/ready state
- Stage 5 TinyUSB PHY switch: **CONFIRMED WORKING** on direct-PC USB path

**What was NOT a firmware bug:**
All prior OpenOCD failures (`libusb_get_string_descriptor_ascii() = -1`) and esptool sync
failures ("No serial data received") were caused by the Pi's dwc_otg EPROTO state.
Stage 5 `usb_new_phy()` / TinyUSB PHY switching code is correct.

**Current flash procedure (direct-PC USB):**
1. Hold Key1 (BOOT), plug USB into Windows PC, release Key1 → board in download mode
2. Admin PowerShell: `usbipd bind --busid 7-2` (one-time; persists until unbound)
3. WSL: `"/mnt/c/Program Files/usbipd-win/usbipd.exe" attach --wsl --busid 7-2`
4. Verify `/dev/ttyACM0` in WSL
5. Flash with above esptool command
6. Press Key2 to boot

**Pi path status:** Avoid until Pi is rebooted (clears dwc_otg state). Once rebooted,
the Pi path should work again. But direct-PC is now the preferred and proven path.

**Next session:** Stage 5 functional testing — verify SCSI write-idle detection and SD processing.
Board is connected to PC USB. MSC folder is visible on Windows. Write a CSV file into the folder
to test whether the 5000 ms idle timeout fires and weld_processor mounts the SD and processes it.

---

## Session Handoff — 2026-06-20 (Stage 5 recovery — Pi dwc_otg stuck, all esptool paths blocked)

**Goal:** Execute documented power-cycle download-mode sequence to resolve esptool sync failure.

**Completed this session:**
- Committed instrumented `usb_msc_sd.c` change (c98cbd3)
- Restored cdc_acm on Pi: `echo '1-1.4:1.0' | sudo tee /sys/bus/usb/drivers/cdc_acm/bind` ✓
- Performed power-cycle download-mode sequence:
  1. Unplugged USB cable
  2. Held Key1 (BOOT / GPIO0)
  3. Plugged USB back in (still holding Key1)
  4. Released Key1 → device in download mode

**Flash attempt result: FAILED**
```
esptool v5.2.0 — Failed to connect to ESP32-S3: No serial data received.
Exit code: 2
```

**Device state after power-cycle:**
- lsusb: 303a:1001 (JTAG/serial debug unit) — correct for download mode
- /dev/ttyACM0: exists, freshly bound (dmesg timestamp 16423.919s)
- dmesg: no further 303a:4003 event
- esptool: cannot sync with ROM bootloader

**Conclusion:** Power-cycle did NOT resolve the issue. The handoff hypothesis was that unplugging USB would drop VDD_USB and fully reset USB PHY to ROM defaults. However, esptool still cannot communicate with the bootloader even with:
- Fresh USB enumeration (power cycled)
- Fresh cdc_acm driver binding
- Device in correct 303a:1001 mode
- /dev/ttyACM0 present and accessible

This suggests the root cause is deeper than cdc_acm driver state caching. Possible causes:
1. USB PHY hardware state on THIS device instance is damaged or in a non-recoverable state
2. ROM bootloader on this device is non-responsive (hardware fault)
3. cdc_acm/pyserial communication layer has a fundamental issue with this USB path
4. Device needs a different reset sequence (e.g., hardware brownout reset via GPIO)

**Next session options:**
1. **Attempt OpenOCD CPU reset via SSH** (documented in NOTES.md) instead of Button reset
   - `ssh casey@192.168.1.43 "openocd-esp32 -c 'source /usr/local/share/openocd-esp32/scripts/interface/esp_usb_jtag.cfg; source /usr/local/share/openocd-esp32/scripts/target/esp32s3.cfg; reset run; exit'"`
   - This uses JTAG path (which is working — device enumerates as 303a:1001) to issue CPU reset and enter bootloader
   - Then immediately attempt esptool flash
2. **Try GPIO-assisted download mode entry** if workbench GPIO wiring is confirmed working
   - Requires verification that Pi GPIO17 (EN/RESET) and GPIO18 (BOOT) are wired to the board
   - Current status: "NOT WIRED" per WORKBENCH section, but should be retested
3. **Inspect Stage 5 firmware for USB initialization bugs** that may leave ROM bootloader unresponsive
   - Check if TinyUSB CDC driver is interfering with ROM bootloader after soft reset
   - May require adding explicit USB PHY reset code before TinyUSB init
4. **Accept hardware limitation** and manually reset via a different method (if available)

**Do NOT proceed with Stage 6.** Stop on failure.

---

## Session Handoff — 2026-06-20 (Stage 5 — OpenOCD JTAG CPU reset diagnostic)

**Goal:** Diagnostic option A — use OpenOCD/JTAG to reset CPU and attempt to unblock esptool sync.

**Completed this session:**
- Started OpenOCD directly via CLI with init + reset run commands
- Connected to device (espressif USB JTAG found at 303a:1001, capabilities=0x2000)
- Attempted to initialize target and issue reset

**OpenOCD reset attempt result: FAILED**
```
OpenOCD: libusb_get_string_descriptor_ascii() failed with -1
```

**Diagnosis:**
OpenOCD cannot initialize the target because JTAG vendor strings are inaccessible. This is consistent with the earlier Stage 5 diagnosis (line ~118-121 of previous session): `usb_new_phy()` has partially disabled USB JTAG vendor-specific communication while leaving the device enumerated as 303a:1001.

**Conclusion:**
Stage 5 firmware has left the device in an unrecoverable USB state:
- ROM bootloader is non-responsive on serial (esptool sync fails)
- JTAG vendor strings are disabled (OpenOCD init fails)
- Device is stable at 303a:1001 but neither path is functional

**Stop condition reached:** "If OpenOCD cannot connect to the target, stop."

This rules out both Option A (OpenOCD) and any CPU reset. The device requires either:
1. **Firmware fix**: Add explicit USB PHY deinit/reset before `usb_new_phy()` call in Stage 5 code
2. **Hardware reset**: GPIO-assisted reset (Option B) if wiring is confirmed
3. **Full erase + re-flash**: If available via an alternative path (not blocked by sync issues)

**Next session recommendation:** Option C (firmware audit) — inspect Stage 5 `usb_msc_sd.c` and `weld_processor.c` for USB PHY initialization bugs. Specifically:
- Check if `usb_new_phy()` requires explicit USB Serial JTAG deinit before calling
- Check ESP-IDF 5.3.2 known issues with simultaneous USB JTAG console
- Possible fix: add `usb_serial_jtag_ll_disable_intr_mask()` or similar before TinyUSB init

**Do NOT proceed with Stage 6.** Stop on failure.

---

## Session Handoff — 2026-06-19 (Stage 5 — instrumented build done, reflash blocked by esptool sync failure)

**Goal:** Add boot instrumentation to usb_msc_sd.c, reflash, capture boot log.

**Changes this session:**
- `components/usb_msc_sd/usb_msc_sd.c` — replaced `ESP_ERROR_CHECK(tinyusb_driver_install(...))` with
  explicit return-code capture + before/after log lines:
  - `ESP_LOGI(TAG, "Calling tinyusb_driver_install...")`
  - `esp_err_t tusb_ret = tinyusb_driver_install(&tusb_cfg);`
  - `ESP_LOGI(TAG, "tinyusb_driver_install returned: %s (0x%x)", esp_err_to_name(tusb_ret), (unsigned)tusb_ret);`
  - On failure: `ESP_LOGE(TAG, "TinyUSB driver install FAILED...")` + return error
  - **NOT YET COMMITTED** — working tree is dirty

**Build status:** PASS. Incremental build completed successfully after the edit.
Binaries SCP'd to Pi at `/tmp/` (bootloader.bin, partition-table.bin, weldml-esp32.bin).

**Reflash status: BLOCKED**

Root cause: Stage 5 firmware is stuck at 303a:1001 (same USB PID as download mode). When the user
presses Key1+Key2, the board resets and re-enumerates — still at 303a:1001. The Linux cdc_acm driver
sees the same VID:PID reconnect and reuses cached endpoint/state from the running firmware session.
esptool cannot sync with the ROM bootloader through this stale cdc_acm state.

Evidence:
- dmesg confirms USB reconnect events at t=3768s and t=5521s (button presses DO cause resets)
- esptool fails with "No serial data received" after both attempts (sync timeout, not open failure)
- Same esptool command worked for Stage 4→5 flash when firmware was at 303a:4003 (different PID → fresh cdc_acm state)
- `usb://303a:1001` URI not supported by esptool v5.2.0 on Pi (pyserial lacks USB protocol handler)

**Pi state on handoff:**
- cdc_acm is UNBOUND (last action left it unbound)
- Restore with: `ssh casey@192.168.1.43 "echo '1-1.4:1.0' | sudo tee /sys/bus/usb/drivers/cdc_acm/bind"`
- Or wait for natural reconnect: key press will trigger udev autobind

**Next session instructions:**
1. Read `docs/PROJECT_STATUS.md` only.
2. First, restore cdc_acm if needed (see Pi state above). Verify: `ssh casey@192.168.1.43 "ls /dev/ttyACM*"`
3. Commit the working-tree change to `usb_msc_sd.c` (build already passes):
   `git add components/usb_msc_sd/usb_msc_sd.c && git commit -m "Stage 5 diagnosis — add tinyusb_driver_install return-code logging"`
4. Instrumented binaries are already at `/tmp/` on Pi. To reflash, **use the power-cycle download mode sequence**:
   a. Unplug the USB cable from the board (or from the Pi USB port)
   b. Press and hold Key1 (BOOT / GPIO0)
   c. Plug USB back in while holding Key1
   d. Release Key1 — device boots into download mode with fresh cdc_acm state
   e. Immediately run: `ssh casey@192.168.1.43 "python3 -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before=no-reset --after=no-reset write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 /tmp/bootloader.bin 0x10000 /tmp/weldml-esp32.bin 0x8000 /tmp/partition-table.bin"`
   OR alternatively: hold Key1 before plugging back in, then release Key1 after /dev/ttyACM0 appears.
5. Start serial reader on Pi BEFORE booting:
   `ssh casey@192.168.1.43 "python3 -c 'import serial,time; s=serial.Serial(\"/dev/ttyACM0\",115200,timeout=0.2); [print(s.read(2048).decode(errors=\"replace\"), end=\"\") for _ in range(100)]'" > /tmp/boot_log.txt &`
6. **Press Key2 alone** (no Key1) to boot. Wait ~5s.
7. Read log: `cat /tmp/boot_log.txt`
8. Check dmesg for 303a:4003: `ssh casey@192.168.1.43 "dmesg | tail -10"`
9. Interpret:
   - "Calling tinyusb_driver_install..." appears → function IS reached
   - "tinyusb_driver_install returned: ESP_OK" AND no 303a:4003 → PHY switch fails after install returns
   - "tinyusb_driver_install returned: ESP_OK" AND 303a:4003 appears → enumeration working, check later stage
   - No "Calling tinyusb_driver_install..." → something before it crashes; check earlier init steps
   - Error return → fix that error
10. If PHY switch confirmed failing (ESP_OK but no 303a:4003): check if `usb_new_phy()` requires
    explicit USB Serial JTAG deinit (`usb_serial_jtag_ll_disable_intr_mask()` or `USB_SERIAL_JTAG.conf0` clear)
    before calling. This is a known ESP-IDF 5.x issue on ESP32-S3.
11. Do NOT start Stage 6. Do NOT inspect .env.

**Why power-cycle download mode works vs EN-reset:**
VDD_USB is supplied by the USB 5V rail. When USB is connected and EN reset fires, VDD_USB does NOT drop —
the USB PHY may retain partial state from the Stage 5 firmware's USB PHY manipulation. Power cycling
(unplugging USB) drops VDD_USB, fully resetting the USB PHY to ROM defaults. This should give esptool
a clean ROM bootloader state to sync with.

---

## Session Handoff — 2026-06-19 (Stage 5 — TinyUSB enumeration diagnosis, boot log blocked)

**Goal:** Diagnose and fix TinyUSB PHY switch failure (303a:1001 stays instead of 303a:4003).

**Changes this session:** None — read-only investigation. Working tree remains clean.

**Build status:** PASS from previous session (d1c0587). No rebuild needed unless code is changed.

**Flash status:** Stage 5 already flashed. Board is running d1c0587 firmware.

**USB enumeration status:** STILL FAIL — USB stays at 303a:1001. Confirmed with lsusb and dmesg:
- Pi boot shows Stage 4 was at 303a:4003 (device 5)
- Stage 5 flash at t=1074s: device 5 reset/disconnect → device 6 as 303a:1001
- NO further USB events in dmesg after t=1074; no 303a:4003 ever appeared
- Device is stably at 303a:1001 (NOT crash-looping — crash loop would show repeated USB events)

**Diagnostic findings:**

1. **Device is stable at CYAN, not crash-looping.**
   - No repeated USB reconnects in dmesg
   - No serial output (expected: app_main in vTaskDelay, monitor_task blocked on s_write_seen=false)
   - This rules out crash loop as the cause of the stuck 303a:1001

2. **USB JTAG OpenOCD fails with `libusb_get_string_descriptor_ascii() failed with -1`.**
   - OpenOCD can see the device (VID=303a PID=1001, capabilities=0x2000) but fails on string descriptor
   - This suggests USB JTAG vendor-specific communication is impaired on the running device
   - Possible explanation: `usb_new_phy()` partially switched the PHY, disabling JTAG vendor strings
     but not completing OTG enumeration — leaving device in a state where JTAG appears functional
     to the kernel (cdc_acm claims ttyACM0 for serial) but JTAG debug protocol is broken
   - Attempted cdc_acm unbind + sudo OpenOCD — same failure

3. **All TinyUSB symbols are present and correctly resolved in the ELF:**
   - `tinyusb_driver_install` at 0x4200a420 ✓
   - `usb_new_phy` at 0x4200f560 ✓
   - `tud_msc_write10_complete_cb` at 0x42008f64 (our override from weld_processor.c) ✓
   - `tusb_rhport_init` at 0x42011970 (tusb_init() macro resolves to this) ✓
   - `CFG_TUD_ENABLED` is set (device mode active) ✓
   - No duplicate or conflicting symbol resolution

4. **Partition table is correct.** Board-specific partitions.csv has factory at 0x10000, matching
   flasher_args.json. Stage 5 was correctly written at 0x10000.

5. **Cannot capture boot log without manual button press.**
   - Board has no auto-reset circuit (DTR/RTS on ttyACM0 does not reset chip)
   - `/api/serial/reset` returns `ok: true` but triggers no USB events (confirmed via dmesg)
   - DTR assertion via Python pyserial also confirms no reset occurs
   - Need user to press Key1+Key2 for download mode → flash with added logging → Key2 to boot

6. **`tud_msc_write10_complete_cb` override is clean.** TinyUSB's `msc_device.c` has it as
   `TU_ATTR_WEAK`; our strong override in weld_processor.c correctly wins. tusb_msc_storage.c
   does NOT define this callback. No linker conflict.

**Root cause hypothesis (unconfirmed):**
`usb_new_phy()` is being called (LCD reaches CYAN, meaning tinyusb_driver_install returned OK),
but the USB PHY switch is either:
a) Failing silently after returning OK (USB JTAG partially disabled, OTG not enabled), OR
b) Completing the PHY switch but TinyUSB task fails to enumerate before the host gives up, causing
   the device to reset and re-appear as 303a:1001 (but this would show repeated USB events in dmesg,
   which we do NOT see)

Hypothesis (a) is most consistent with the evidence. The OpenOCD string descriptor failure at -1
while cdc_acm serial still works suggests a partial JTAG/OTG state.

**What would confirm or deny this:** Boot log from USB JTAG console. The last JTAG-visible log
line is "USB MSC+CDC ready" (after tinyusb_driver_install returns). If that line appears in the
boot log AND dmesg shows no subsequent PHY switch event, the PHY switch is the bug.

**Next session instructions:**
1. Read `docs/PROJECT_STATUS.md` only.
2. Add `ESP_LOGI(TAG, "Calling tinyusb_driver_install...")` before `tinyusb_driver_install()` in
   `components/usb_msc_sd/usb_msc_sd.c` (line 87, after the mount call).
3. Build: `cd /mnt/j/ReposWSL/weldml-esp32 && source ~/esp/esp-idf/export.sh && idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build`
4. SCP binaries to Pi: `scp build/bootloader/bootloader.bin build/partition_table/partition-table.bin build/weldml-esp32.bin casey@192.168.1.43:/tmp/`
5. **ASK USER to press Key1+Key2** on the board → device enters download mode (303a:1001 with ttyACM0)
6. Flash from Pi: `ssh casey@192.168.1.43 "python3 -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before=no-reset --after=no-reset write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 /tmp/bootloader.bin 0x10000 /tmp/weldml-esp32.bin 0x8000 /tmp/partition-table.bin"`
   **Use `--after=no-reset` this time** so device stays in download mode after flash.
7. Start reading serial in background on Pi: `ssh casey@192.168.1.43 "python3 -c 'import serial,time; s=serial.Serial(\"/dev/ttyACM0\",115200,timeout=0.2); [print(s.read(2048).decode(errors=\"replace\"), end=\"\") for _ in range(50)]'" > /tmp/boot_log.txt &`
8. **ASK USER to press Key2 alone** (no Key1) → device boots → captures boot log
9. Wait ~5s, then `cat /tmp/boot_log.txt` to see the log
10. In dmesg, check if 303a:4003 appears after the Key2 boot: `ssh casey@192.168.1.43 "dmesg | tail -15"`
11. **If boot log shows "Calling tinyusb_driver_install..." but no 303a:4003 in dmesg:**
    → PHY switch IS being called but doesn't complete. Check if `usb_new_phy()` needs explicit
      USB Serial JTAG deinit before calling. Check ESP-IDF 5.3.2 known issues.
12. **If no "Calling tinyusb_driver_install..." in log:**
    → Something before tinyusb_driver_install fails. Check which earlier function panics or returns error.
13. Do NOT start Stage 6. Do NOT inspect .env.

**Alternative to boot log: add `tinyusb_driver_install` return value logging via UART0.**
ESP32-S3 UART0 is on GPIO43(TX)/GPIO44(RX). If USB JTAG console goes dark after PHY switch, add
`uart_write_bytes(UART_NUM_0, msg, len)` AFTER tinyusb_driver_install returns to see if it returned
OK. Requires a USB-TTL adapter connected to GPIO43/44. Not available in current setup.

---

## Session Handoff — 2026-06-19 (Stage 5 — flash done, TinyUSB enumeration blocked)

**Goal:** Stage 5 — USB MSC write-idle handoff + placeholder JSON write.

**Stage 5 decision (final, do not revisit):**
- No Kawasaki pins, no GPIO handshaking, no DONE file, no rename marker, no robot-side changes
- 5000 ms after last USB MSC SCSI WRITE10 = ready for ESP processing
- Never allow FatFs and MSC to access SD simultaneously
- After processing (success or failure), always return MSC control (call `tinyusb_msc_storage_unmount()`)
- Known limitation: detects SCSI block-write idle, not a true Kawasaki file-close event

**Files changed this session (unstaged):**
- `CMakeLists.txt` — added `if(DEFINED BOARD)` before `elseif(DEFINED ENV{BOARD})` (CMake `-D` support)
- `main/CMakeLists.txt` — same `-D BOARD=` fix; added `weld_processor` to REQUIRES
- `main/main.c` — Stage 5: LCD state comments; calls `weld_processor_start()` after MSC init; halt loop on failure
- `components/weld_processor/` (NEW):
  - `weld_processor.h` — `weld_state_t` enum; `weld_processor_start()` declaration
  - `weld_processor.c` — `tud_msc_write10_complete_cb` override; monitor task (250 ms poll, 5000 ms idle threshold); `process_sd()` (mount→find newest CSV→write JSON placeholder→unmount); LCD state machine
  - `CMakeLists.txt` — REQUIRES: lcd_st7789, esp_tinyusb, esp_timer, freertos
- `docs/OPEN_QUESTIONS.md` — Q3 resolved (write-idle + mount/unmount); Q6 resolved (no robot signal)

**Build status:**
- `idf.py -D BOARD=waveshare-esp32-s3-lcd-147 build` — **PASS** (14/14 incremental, 363 KB, zero errors, zero warnings)
- **Build rule:** ALWAYS use `-D BOARD=waveshare-esp32-s3-lcd-147` on EVERY `idf.py` call (build AND flash). Omitting it poisons the CMake cache → `BOARD_LCD_MOSI_PIN undeclared` errors. Run `idf.py fullclean` to recover from a poisoned cache.

**Flash status:**
- **PASS** — flashed 2026-06-19 via SSH + Pi-side esptool; all 3 regions SHA-verified
- Flash procedure used (manual — GPIO not wired, see workbench findings below):
  1. Press **Key1+Key2** on board → device enters download mode (USB shows 303a:1001)
  2. `scp build/bootloader/bootloader.bin build/partition_table/partition-table.bin build/weldml-esp32.bin casey@192.168.1.43:/tmp/`
  3. `ssh casey@192.168.1.43 "python3 -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before=no-reset --after=hard-reset write-flash --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 /tmp/bootloader.bin 0x10000 /tmp/weldml-esp32.bin 0x8000 /tmp/partition-table.bin"`
  4. Press **Key2 alone** (no Key1) → device boots into new firmware

**Hardware verification status:**
- [x] LCD shows CYAN after clean reset → `weld_processor_start()` called → `usb_msc_sd_init()` returned OK
- [ ] **BLOCKED:** USB stays at `303a:1001` (USB JTAG/serial) — TinyUSB has NOT switched PHY to OTG
  - Expected: USB re-enumerates as `303a:4003` (TinyUSB CDC+MSC)
  - Actual: `303a:1001` persists; no serial output; dmesg shows no 303a:4003 event
  - Stage 4 confirmed this transition working — Stage 5 regression

**TinyUSB enumeration blocker — investigation notes:**
- `tinyusb_driver_install()` returns `ESP_OK` (otherwise firmware would abort, LCD would be RED)
- App reaches `weld_processor_start()` (sets CYAN) — execution past TinyUSB install confirmed
- `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` is intentional; Stage 4 used same config successfully
- `tud_msc_write10_complete_cb` defined in `weld_processor.c` (weak symbol override) — likely not the cause
- No serial output: console goes dark when TinyUSB should take over; if TinyUSB doesn't take over, no log
- `dmesg` on Pi shows device 5 reset + disconnect + device 6 as 303a:1001, then NO further USB events
- Possible causes: PSRAM/DMA memory allocation failure in TinyUSB task; task priority conflict; USB PHY state after repeated resets during this session; unknown ESP-IDF 5.3 regression

**Next session instructions:**
1. Read `docs/PROJECT_STATUS.md` only.
2. Build is NOT needed (already built; if needed, always use `-D BOARD=waveshare-esp32-s3-lcd-147`).
3. Diagnose TinyUSB enumeration failure. Approaches (in order):
   a. Add `ESP_LOGI` before and after `tinyusb_driver_install()` in `usb_msc_sd.c`; rebuild + reflash; capture early boot log via `/api/serial/reset` BEFORE TinyUSB installs (timing matters)
   b. Check if `esp_tinyusb` component has a known issue with simultaneous USB JTAG console on ESP32-S3 in IDF 5.3.2
   c. Check `tinyusb_driver_install()` return path — confirm no silent error path exists
4. To reflash: manual Key1+Key2 press, then SCP + SSH esptool as shown above.
5. To boot (after flash): Key2 press alone (no Key1).
6. **Do not start Stage 6. Do not implement ML inference. Do not inspect .env.**
7. Stop on failure.

---

## Workbench Findings — Session 2026-06-19 (corrections to earlier entries)

**GPIO wiring:** `gpio_boot=18` and `gpio_en=17` appear in `workbench.json` and portal config but are **NOT physically wired** to the Waveshare board. Confirmed by diagnostic: driving GPIO17 LOW via gpiod (`out lo` confirmed via `/sys/kernel/debug/gpio`) has zero effect on USB device state. Earlier entries claiming "automated download mode works" were premature — the portal API responded OK but wires are absent.

**`POST /api/flash`:** Returns `{"error": "not found"}` — this endpoint does NOT exist in the installed portal version. RFC2217 is the only remote flash path, but it fails when TinyUSB CDC is active (CDC rejects baud rate SET_CONTROL).

**`POST /api/serial/recover`:** Fails silently for SLOT3 — slot_key `"_fixed_SLOT3"` can't be parsed to a USB device path, so USB unbind step aborts. GPIO recovery never runs.

**`workbench.local`:** Does not resolve in WSL. Always use IP `192.168.1.43` directly.

**SSH:** `casey@192.168.1.43` — key auth, no password. Always available as fallback for direct Pi operations.

**OpenOCD via SSH:** Available as `openocd-esp32 -s /usr/local/share/openocd-esp32/scripts`. Useful for CPU reset when DTR/RTS resets re-enter download mode. Interface config: `interface/esp_usb_jtag.cfg`, target: `target/esp32s3.cfg`.

**RFC2217 flash (idf.py):** Fails with "Remote does not accept parameter change" when device runs TinyUSB CDC (`303a:4003`). Works when device is in download mode (`303a:1001`) using `--before=no-reset` via SSH esptool directly. The `?ign_set_control` URL parameter breaks RFC2217 negotiation entirely — do not use.

---

## Session Handoff — 2026-06-19 (Stage 4 implementation — build not yet run)

**Goal:** Stage 4 — USB MSC + SD SPI. Expose SD card as USB mass storage device.

**Files changed this session:**
- `components/lcd_st7789/lcd_st7789.c` — added `esp_lcd_panel_mirror(true, true)` after
  `esp_lcd_panel_init()` to fix 180° MADCTL rotation (deferred orientation fix from Stage 3)
- `components/usb_msc_sd/` (NEW component):
  - `CMakeLists.txt` — REQUIRES: driver, sdmmc, esp_tinyusb
  - `usb_msc_sd.h` — `usb_msc_sd_config_t` (miso/mosi/clk/cs pins) + `usb_msc_sd_init()`
  - `usb_msc_sd.c` — SPI3_HOST bus init, SDSPI host init, sdmmc_card_init, TinyUSB MSC+CDC
  - `idf_component.yml` — `espressif/esp_tinyusb: ^1.4.2`
- `main/main.c` — Stage 4: LCD (white→green/red) + `usb_msc_sd_init()` with board SD pins
- `main/CMakeLists.txt` — added `usb_msc_sd` to REQUIRES
- `main/idf_component.yml` (NEW) — `espressif/esp_tinyusb: ^1.4.2` (needed by CM)
- `idf_component.yml` (root) — added `espressif/esp_tinyusb: ^1.4.2`
- `boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults` — added
  `CONFIG_TINYUSB_CDC_ENABLED=y` and `CONFIG_TINYUSB_MSC_ENABLED=y`
- `docs/OPEN_QUESTIONS.md` — closed Q1, Q2, Q7
- `dependencies.lock` (NEW) — component manager lock file; commit this

**Q1/Q2/Q7 decisions (now closed):**
- Q1: Port SmrtUsbEsp code as new ESP-IDF components (Option A). Done: `components/usb_msc_sd/`
- Q2: Native ESP-IDF. In use since Stage 1.
- Q7: `esp_lcd` + ST7789 driver. Implemented in Stage 3.

**Build/flash/monitor status:**
- `idf.py set-target esp32s3` — DONE (CMake configured, target correct)
- `espressif/esp_tinyusb 1.7.6~2` + `espressif/tinyusb 0.19.0~3` — downloaded to
  `managed_components/` (gitignored; `dependencies.lock` committed instead)
- `BOARD=waveshare-esp32-s3-lcd-147 idf.py build` — **PASS** (1066/1066, 357 KB, zero errors, 2026-06-19)
- Flash — **BLOCKED** this session: workbench Pi went offline. Port 4003 RFC2217 returned
  "timeout while waiting for option 'purge'" then Pi became unreachable. Not a firmware issue.
- `POST /api/flash` — confirmed 404 on this portal (same as Q9 resolution; use RFC2217 fallback)

**Key finding — TinyUSB in ESP-IDF 5.3:**
- `esp_tinyusb` is NOT in core ESP-IDF 5.3; must use IDF Component Manager
- Manifest must be in COMPONENT directories (`main/idf_component.yml`,
  `components/usb_msc_sd/idf_component.yml`) — project root alone is insufficient
- `idf.py set-target esp32s3` needed after deleting `sdkconfig` (target is lost)
- `CONFIG_ESP_CONSOLE_USB_CDC=y` is ROM CDC driver, incompatible with TinyUSB — do NOT use
- Keep `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` for early boot logs; after `tinyusb_driver_install()`,
  USB PHY switches to OTG and JTAG logs stop. This is expected behavior.

**Monitoring after Stage 4 flash (important — new procedure):**
1. Capture boot log via `/api/serial/reset` BEFORE TinyUSB installs — SD card probe
   info (type, size) appears in this early log via USB Serial JTAG
2. After TinyUSB installs, USB Serial JTAG stops; verify USB MSC via Pi:
   `ssh casey@192.168.1.43 "dmesg | tail -20"` — should show USB MSC enumeration
   `ssh casey@192.168.1.43 "lsusb -t"` — should show CDC + mass storage
3. LCD shows result: green = success, red = SD card probe failed

**Hardware artifact notes (from Stage 3, carry forward):**
- Black semicircle at physical top-right = fixed hardware/panel artifact, NOT software
- Display content 180° rotated relative to USB-at-top physical orientation (now fixed in
  `lcd_st7789.c` via `mirror(true, true)`)

**Next session instructions:**
1. Read `docs/PROJECT_STATUS.md` only.
2. **Build already passed** — skip build step.
3. Verify workbench is back: `curl -s http://192.168.1.43:8080/api/info`
4. Check SLOT3 state: `curl -s http://192.168.1.43:8080/api/devices | python3 -m json.tool`
5. If OpenOCD is running, stop it: `curl -X POST http://192.168.1.43:8080/api/debug/stop -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}'`
6. Flash via RFC2217 (POST /api/flash is 404 on this portal):
   ```
   cd /mnt/j/ReposWSL/weldml-esp32 && source ~/esp/esp-idf/export.sh && idf.py -p rfc2217://192.168.1.43:4003 flash
   ```
7. Capture boot log via `/api/serial/reset` (while OpenOCD is still stopped — DTR/RTS path):
   ```
   curl -X POST http://192.168.1.43:8080/api/serial/reset -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}' | python3 -m json.tool
   ```
8. Check Pi dmesg/lsusb for USB MSC enumeration:
   ```
   ssh casey@192.168.1.43 "dmesg | tail -20"
   ssh casey@192.168.1.43 "lsusb -t"
   ```
9. Confirm LCD shows green (or red if SD card probe failed).
10. Update PROJECT_STATUS.md hardware test log, commit, push.
11. Stop on failure (do not start parser/model work, do not inspect .env).

---

## Session Handoff — 2026-06-19 (Stage 3 complete — visual confirmation)

**Goal:** Resolve gap calibration open issue from previous session; get user visual confirmation.

**Completed this session:**
- Confirmed via ESP-IDF source (`esp_lcd_panel_st7789.c`) that `esp_lcd_panel_mirror()`
  only sends a MADCTL update and does NOT change the CASET/RASET window. For a solid
  fill, mirror_x/mirror_y have no effect on which physical pixels are addressed.
  The previous session's gap calibration candidates 1 and 3 (mirror_x with same gap)
  were therefore not actionable for diagnosing a solid-fill black region.
- Tested `x_gap=0, y_gap=0`: black region unchanged from `x_gap=34` — confirmed the
  black is NOT from a gap/offset error.
- Added `lcd_st7789_fill_rect()` to the driver and ran a 4-quadrant color test.
- Reverted `main.c` to the original white→green pattern. x_gap=34 restored.
- Added extended color defines to `lcd_st7789.h` (BLUE, YELLOW, CYAN, MAGENTA).

**Visual confirmation results (user-reported, 2026-06-19):**

_Quadrant test (firmware sends TL=red, TR=blue, BL=cyan, BR=yellow):_
- User observed: TL=yellow, TR=cyan, BL=blue, BR=red
- This is an exact 180° rotation of the programmed layout.

_Black semicircle artifact:_
- A curved black region (quarter-circle arc, radius ≈ 1/3 of width, 10% of height)
  is present in the physical top-right corner when USB faces up.
- The arc curves inward toward the screen center (concave toward corner).
- The artifact appears identically on: solid white, solid green, and the quadrant test.
- Changing x_gap (34→0) had no effect on the artifact.
- **Conclusion: fixed hardware/panel artifact** — likely LCD glass defect, pressure
  damage, backlight masking, or damaged display area. Not a software issue.
  Not a blocker for Stage 4 firmware bring-up unless user chooses to replace the display.

**Stage 3 success criteria — all met for firmware bring-up:**
- [x] LCD driver builds (zero errors, binary ~248 KB)
- [x] Flash succeeds — bootloader, partition table, firmware SHA-verified (RFC2217)
- [x] Boot log clean — 16MB flash, 8MB PSRAM OK, white@~1s, green@~3s, no panic
- [x] Screen turns on — backlight active, color changes confirmed by user
- [x] Full-screen fill confirmed working (solid white, solid green displayed correctly)
- [x] Quadrant test: four distinct colors visible in correct relative positions
- [~] Hardware artifact: fixed black semicircle at physical top-right — hardware defect,
     not a software regression; documented and accepted for firmware bring-up

**Two open items for the next LCD session (NOT Stage 4 blockers):**
1. **MADCTL orientation**: Content is 180° rotated relative to expected with USB at top
   and MADCTL=0x00. Fix: add `esp_lcd_panel_mirror(s_panel, true, true)` after
   `esp_lcd_panel_init()` in `lcd_st7789_init()`. Confirmed needed before drawing
   any directional UI content.
2. **Black semicircle**: If the display is replaced or a second unit is available,
   test to confirm whether the artifact is unit-specific or panel-design characteristic.

**Exact next action for a fresh session:**
Stage 4 — USB MSC + SD SPI. Port TinyUSB MSC + CDC + SD SPI from SmrtUsbEsp.
Close Q1 and Q2 in OPEN_QUESTIONS.md before writing Stage 4 code.
Do not touch LCD driver orientation until Stage 4 is complete.

---

## Session Handoff — 2026-06-19 (Stage 2 board config)

**Goal:** Create `boards/waveshare-esp32-s3-lcd-147/` board config.

**Completed:**
- `boards/waveshare-esp32-s3-lcd-147/board.h` — all hardware-confirmed GPIO defines:
  LCD SPI (MOSI=45, CLK=40, CS=42, DC=41, RST=39), LCD backlight (GPIO48, MOSFET gate),
  SD SPI (CLK=14, MOSI=15, MISO=16, CS=21), WS2812B RGB LED (GPIO38), BOOT button (GPIO0).
  GPIO48 and GPIO38 named correctly to prevent misuse as generic GPIO.
- `boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults` — 16MB flash (overrides root 4MB),
  8MB Octal PSRAM (`CONFIG_SPIRAM=y`, `CONFIG_SPIRAM_MODE_OCT=y`).
- Build verified: `BOARD=waveshare-esp32-s3-lcd-147 idf.py build` → 1023/1023 targets, zero errors.
  Generated sdkconfig confirms `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`, `CONFIG_SPIRAM_MODE_OCT=y`.
  Flash command shows `--flash_size 16MB`.
- Committed `907570d`, pushed to origin/main.

**Success criteria — all met:**
- [x] `boards/waveshare-esp32-s3-lcd-147/board.h` created with all confirmed GPIO defines
- [x] `boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults` created with 16MB flash + PSRAM
- [x] `BOARD=waveshare-esp32-s3-lcd-147 idf.py build` passes (1023/1023, zero errors)
- [x] Generated sdkconfig confirms 16MB flash and Octal PSRAM
- [x] Committed and pushed

**Next action:** Resolve Q1 (architecture decision: port SmrtUsbEsp code vs. other options)
to unblock Stage 3 firmware components (TinyUSB MSC, SD init, LCD driver).

---

## Session Handoff — 2026-06-19 (RFC2217 monitor/reset fix)

**Goal:** Resolve Stage 1 monitor blocker — establish repeatable flash → reset/boot → log workflow.

**Completed:**
- Root-caused RFC2217 monitoring issue: when OpenOCD runs, `POST /api/serial/reset` uses JTAG (`reset run`), not DTR/RTS. JTAG path returns no boot log.
- Fix: stop OpenOCD → `POST /api/serial/reset` uses DTR/RTS path → captures full boot log → restart OpenOCD.
- Verified: 76-line boot log captured in single API call: IDF v5.3.2, no panic, softAP up, "Startup complete".
- Confirmed: `plain_rfc2217_server.py` already sets DTR=False/RTS=False on startup and after client disconnect — it does NOT hold DTR=1/RTS=1. Prior note in NOTES.md was inaccurate; corrected.
- Confirmed: `idf.py monitor --no-reset` is the safe interactive monitoring command (prevents DTR=True assertion via RFC2217).
- Updated NOTES.md with full root cause analysis and verified workflow commands.
- Updated PROJECT_STATUS.md flash command block with Step 4a (boot log) and Step 4b (interactive monitor).

**Success criteria — all met:**
- [x] Repeatable flash → reset/boot → monitor sequence established
- [x] Boot log captured from flashed firmware (76 lines, verified 2026-06-19)
- [x] PROJECT_STATUS.md updated
- [x] NOTES.md corrected

**Next action:** Stage 2 — Create `boards/waveshare-esp32-s3-lcd-147/` board config. Say "proceed Stage 2" to start.

---

## Session Handoff — 2026-06-19 (Stage 1 flash attempt)

**Goal:** Stage 1 smoke-test flash. Template firmware flashed and SHA verified.
Boot log blocked by workbench monitoring issue (see NOTES.md and below).

**Completed this session:**
- 7-stage implementation plan written to `~/.claude/plans/enter-planning-mode-for-rustling-kite.md`
- Stage 5 clarified: SCSI quiet-period detection (not sentinel file); Kawasaki marker file is upgrade path
- `idf.py build` passed (BOARD unset, 4MB flash sdkconfig, 1028/1028 targets)
- Template firmware flashed via `idf.py -p rfc2217://192.168.1.43:4003 flash` — ALL 5 binaries SHA verified
- Flash path discovery: `POST /api/flash` does NOT exist on this portal; RFC2217 is the correct path
- GPIO18 (BOOT) released (portal GPIO API); confirmed Pi GPIO18 = input/value=1
- New workbench finding: when OpenOCD runs, `POST /api/serial/reset` uses JTAG (no boot log). DTR/RTS path (with boot log) requires stopping OpenOCD first. See NOTES.md RFC2217 note.

**Stage 1 complete — all success criteria met:**
- [x] Flash binary verified (SHA hash, all 5 files, esptool v4.11.0)
- [x] Boot log captured: IDF v5.3.2, no panic, no crash loop, "Startup complete"
- [x] No erase_flash used
- [x] Commit and push
- [x] SESSION_HANDOFF.md updated

## Session Handoff — 2026-06-19 (workbench discovery)

**Goal this session:** Audit existing skills; discover full extent of workbench capabilities.

**Completed:**
- Confirmed full workbench HTTP portal running at `http://192.168.1.43:8080`
- `/api/devices` confirms Waveshare on SLOT3: state=idle, devnode=/dev/ttyACM0, url=rfc2217://192.168.1.43:4003
- Pi GPIO wiring confirmed: gpio_boot=18 (→ Key1/BOOT/GPIO0), gpio_en=17 (→ Key2/EN/RST)
- Automated download mode entry via portal GPIO API — no manual BOOT+RESET required for flash
- OpenOCD auto-started on SLOT3: debugging=true, debug_chip=esp32s3, gdb_port=3335, telnet=4446
- `esp-idf-handling` skill covers the full flash workflow; no new skill needed
- Q4 fully resolved; Q9 fully resolved

**First smoke-test flash: unblocked.** Awaiting explicit approval only.

**Exact commands for first flash (do not run until approved):**
```bash
# Step 1 — add hostname alias in WSL (one-time)
echo "192.168.1.43 workbench.local" | sudo tee -a /etc/hosts

# Step 2 — build (WSL)
. ~/esp/esp-idf/export.sh && cd /mnt/j/ReposWSL/weldml-esp32 && idf.py build

# Step 3 — flash via portal (Pi-side esptool; portal enters download mode via GPIO automatically)
cd build && curl -s -X POST http://workbench.local:8080/api/flash \
  -F slot=SLOT3 -F chip=esp32s3 -F baud=921600 \
  -F flash_args=@flash_args \
  -F bootloader.bin=@bootloader/bootloader.bin \
  -F partition-table.bin=@partition_table/partition-table.bin \
  -F ota_data_initial.bin=@ota_data_initial.bin \
  -F firmware.bin=@weldml_esp32.bin \
  | jq .

# Step 4a — capture boot log (stop OpenOCD so portal uses DTR/RTS path, not JTAG)
curl -X POST http://192.168.1.43:8080/api/debug/stop -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}'
curl -X POST http://192.168.1.43:8080/api/serial/reset -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}' | python3 -m json.tool
curl -X POST http://192.168.1.43:8080/api/debug/start -H 'Content-Type: application/json' -d '{"slot": "SLOT3"}'
# → Full boot log in JSON "output" array; OpenOCD resumed for GDB after

# Step 4b — interactive monitor (--no-reset prevents DTR=True assertion via RFC2217)
. ~/esp/esp-idf/export.sh && idf.py -p rfc2217://192.168.1.43:4003 monitor --no-reset
# Exit monitor: Ctrl+]

# Fallback flash (if portal API unavailable): manual BOOT+RESET then:
# . ~/esp/esp-idf/export.sh && idf.py -p rfc2217://192.168.1.43:4003 flash
```

**Verification status:**
- Portal API (`/api/flash`): not yet tested with full flash — command confirmed from skill docs
- RFC2217 connection: verified ✓ (chip_id tested)
- Flash: PASS — `idf.py -p rfc2217://192.168.1.43:4003 flash`, all 5 binaries SHA verified
- Boot log: PASS — IDF v5.3.2, no panic, softAP up, "Startup complete"
- OpenOCD/JTAG: auto-started, confirmed in `/api/devices`; GDB not yet connected from WSL

**Next action:** Say "approve first flash" to proceed, or resolve Q1/Q2 for firmware work.

---

## Open Questions — Resolution Log

See `docs/OPEN_QUESTIONS.md` for full context on each question.

| Q | Title | Status | Decision | Date |
|---|-------|--------|----------|------|
| Q1 | Port SmrtUsbEsp or rebase around it? | **Resolved** | Port as new components; no fork; `components/usb_msc_sd/` | 2026-06-19 |
| Q2 | Native ESP-IDF or PlatformIO? | **Resolved** (follows Q1) | Native ESP-IDF; in use since Stage 1 | 2026-06-19 |
| Q3 | SD ownership transition (MSC → firmware) | **Open** (follows Q6) | | |
| Q4 | Pi workbench flash path for Waveshare | **Resolved** — HTTP portal + GPIO automation confirmed; `POST /api/flash` is preferred path | 2026-06-19 |
| Q5 | Manual BOOT/RESET sufficient for early work? | **Resolved** — manual confirmed; automated GPIO path also available (gpio_boot=18, gpio_en=17) | 2026-06-19 |
| Q6 | Robot file-completion signal | **Open** | | |
| Q7 | LCD driver structure | **Open** (follows Q1/Q2) | | |
| Q8 | Weld file format | **Open** | | |

---

## Hardware Test Log

| Test | Board | Port | Date | Result | Notes |
|------|-------|------|------|--------|-------|
| idf.py build | HUZZAH32 (ESP32) | /dev/ttyUSB0 | (prior session) | PASS | Template build, not WeldML firmware |
| idf.py flash | HUZZAH32 (ESP32) | /dev/ttyUSB0 | (prior session) | PASS | 987 KB @ 541 kbit/s |
| Boot log, OTA table | HUZZAH32 (ESP32) | /dev/ttyUSB0 | (prior session) | PASS | IDF v5.3.2 |
| WiFi SoftAP + web UI | HUZZAH32 (ESP32) | /dev/ttyUSB0 | (prior session) | PASS | All 4 pages verified |
| MQTT connect | HUZZAH32 (ESP32) | /dev/ttyUSB0 | (prior session) | PASS | |
| OTA HTTP pull | HUZZAH32 (ESP32) | /dev/ttyUSB0 | (prior session) | PASS | ota_1 partition |
| Pi reachability | Waveshare (via Pi) | SSH 192.168.1.43 | 2026-06-19 | PASS | casey@PiEspWrkbench, key auth |
| CDC enumeration on Pi | Waveshare | /dev/ttyACM0 | 2026-06-19 | PASS | SmrtUsbEsp run mode |
| MSC enumeration on Pi | Waveshare | sda (29 GiB) | 2026-06-19 | PASS | TinyUSB Flash Storage 0.2; SD card visible |
| BOOT+RESET download mode | Waveshare | — | 2026-06-19 | PASS | Manual sequence required; no auto-reset circuit |
| chip-id (esptool) | Waveshare | /dev/ttyACM0 (Pi) | 2026-06-19 | PASS | ESP32-S3 QFN56 rev0.2, 8MB PSRAM, MAC 98:3d:ae:e4:4e:ac |
| flash-id (esptool) | Waveshare | /dev/ttyACM0 (Pi) | 2026-06-19 | PASS | 16MB Winbond W25Q128 (ef/4018), 3.3V quad SPI |
| idf.py build (esp32s3) | Waveshare (WSL build) | — | 2026-06-19 | PASS | IDF v5.3.2, 1028/1028 targets, zero errors |
| idf.py flash (template) | Waveshare | rfc2217://192.168.1.43:4003 | 2026-06-19 | PASS | All 5 binaries SHA verified; esptool v4.11.0 |
| Boot log (template) | Waveshare | /dev/ttyACM1 (passive read) | 2026-06-19 | PASS | IDF v5.3.2, no panic, softAP up, "Startup complete" |
| LCD white→green (Stage 3) | Waveshare | rfc2217://192.168.1.43:4003 | 2026-06-19 | PASS | SPI LCD driver; white@1s, green@3s; no panic; 248 KB binary |
| LCD 4-quadrant pattern | Waveshare | rfc2217://192.168.1.43:4003 | 2026-06-19 | PASS | TL=yellow, TR=cyan, BL=blue, BR=red observed (180° rotated vs. programmed) |
| LCD black semicircle | Waveshare | — | 2026-06-19 | HW DEFECT | Fixed curved artifact top-right; present on all fills; not a software issue |
| idf.py build (Stage 4) | Waveshare (WSL build) | — | 2026-06-19 | PASS | 1066/1066, 357 KB, zero errors; usb_msc_sd + esp_tinyusb 1.7.6 linked |
| idf.py flash (Stage 4) | Waveshare | rfc2217://192.168.1.43:4003 | 2026-06-19 | PASS | All 3 binaries SHA verified; BOARD= must be set on every idf.py call |
| USB TinyUSB enumeration | Waveshare | Pi USB | 2026-06-19 | PASS | 303a:1001 (JTAG) → 303a:4003 (TinyUSB CDC+MSC); OTG PHY switch confirmed |
| LCD green (Stage 4) | Waveshare | — | 2026-06-19 | PASS | SD card init success; usb_msc_sd_init() returned OK |
| idf.py build (Stage 5) | Waveshare (WSL build) | — | 2026-06-19 | PASS | 14/14 incremental, 363 KB, zero errors, zero warnings |
| idf.py flash (Stage 5) | Waveshare | SSH esptool on Pi | 2026-06-19 | PASS | SHA-verified; --before=no-reset after manual Key1+Key2 |
| LCD CYAN (Stage 5) | Waveshare | — | 2026-06-19 | PASS | weld_processor_start() called; WAITING state confirmed |
| TinyUSB enumeration (Stage 5) | Waveshare | Pi USB | 2026-06-19 | **FAIL** | USB stays 303a:1001 on Pi — root cause: Pi dwc_otg EPROTO state, not firmware bug |
| USB stable (no crash loop) | Waveshare | Pi dmesg | 2026-06-19 | CONFIRMED | 303a:1001 single stable enum, no repeated reconnects → device is NOT crash-looping |
| OpenOCD JTAG via esp_usb_jtag | Waveshare | Pi USB | 2026-06-19 | **FAIL** | libusb_get_string_descriptor_ascii()=-1; caused by Pi dwc_otg EPROTO, not firmware |
| Direct-PC USB flash (Stage 5) | Waveshare | PC USB → usbipd → WSL | 2026-06-20 | PASS | esptool v4.11.0, 115200 baud, no-stub; all 3 regions SHA-verified |
| TinyUSB enumeration (Stage 5, PC) | Waveshare | PC USB | 2026-06-20 | **PASS** | 303a:1001→303a:4003 confirmed; CDC+MSC; MSC folder opened on Windows |
| Write-idle detection (Stage 5) | Waveshare | PC USB | 2026-06-20 | **PARTIAL** | GREEN (SUCCESS) reached after CSV copy; idle timer and process_sd() ran. JSON not confirmed in Explorer (FAT cache stale on Windows). Serial log not captured. Fix needed: write_placeholder() failure must set FAILURE state. |
| Pi USB host recovery (reboot) | Waveshare | Pi USB | 2026-06-20 | **PASS** | Pi reboot cleared dwc_otg EPROTO. 303a:1001→303a:4003 PHY switch clean after reboot. |
| Linux USB MSC enumeration | Waveshare | Pi USB (sda1) | 2026-06-20 | **PASS** | sda/sda1 29 GiB, write-through, TinyUSB Flash Storage 0.2. No SCSI errors. |
| Linux MSC write (CSV files) | Waveshare | Pi USB (sda1) | 2026-06-20 | **PASS** | CSV files written by Pi confirmed physically on SD (survived drop_caches + remount). |
| Write-idle detection (Pi host) | Waveshare | Pi USB | 2026-06-20 | **CONFIRMED** | LCD: CYAN→YELLOW→BLUE(brief)→GREEN. process_sd() runs. tud_msc_write10_complete_cb fires. |
| JSON write (Pi host) | Waveshare | Pi USB | 2026-06-20 | **FAIL** | write_placeholder() fopen returns NULL. JSON never written (raw disk scan = 0 hits). FAT clean. |

---

## Workbench Status

| Item | Status |
|------|--------|
| Workbench Pi available | **Confirmed** — 192.168.1.43, casey@PiEspWrkbench, SSH key auth working |
| Pi workbench HTTP portal | **Confirmed** — `http://192.168.1.43:8080`; `slots_configured=3`; full REST API available |
| Waveshare assigned to SLOT3 | **Confirmed** — state=idle, devnode=/dev/ttyACM0, url=rfc2217://192.168.1.43:4003 |
| Pi sees Waveshare on /dev/ttyACM0 | **Confirmed** — CDC ACM in run mode, JTAG/serial in download mode |
| Pi sees Waveshare MSC (sda) | **Confirmed** — 29 GiB SD card via TinyUSB MSC |
| esptool on Pi | **Confirmed** — v5.2.0; invoked via `POST /api/flash` (portal handles lifecycle) |
| ESP-IDF on Pi | **Not installed** — build runs in WSL; Pi-side flash via portal API |
| WSL→Pi flash path | **Confirmed** — `POST /api/flash` preferred; fallback `idf.py -p rfc2217://192.168.1.43:4003 flash` |
| Pi GPIO wiring for Waveshare BOOT+RESET | **NOT WIRED** — gpio_boot=18/gpio_en=17 in workbench.json but wires are physically absent. Confirmed: GPIO17 driven LOW has zero effect on USB device state. Manual button press required. |
| Manual BOOT+RESET sequence | **Confirmed working** — hold Key1, tap Key2, release Key1 → 303a:1001 (download mode). Boot normally: Key2 alone (no Key1). |
| OpenOCD on Pi (SLOT3) | **Confirmed** — auto-started; debug_chip=esp32s3, gdb_port=3335, telnet=4446 |
| Hostname alias in WSL | **Not yet set** — add `192.168.1.43 workbench.local` to `/etc/hosts` before first flash |
| UDP log capture on Waveshare | **Not yet verified** |

---

## What Is Done

- [x] Hardware documentation extracted from schematic and datasheet (see docs/)
- [x] Verified pin table for Waveshare ESP32-S3-LCD-1.47 (schematic + SmrtUsbEsp confirmation)
- [x] Project context docs created: PRIOR_WORK_CONTEXT, MVP_REQUIREMENTS, DEBUG_WORKBENCH_CONTEXT, OPEN_QUESTIONS
- [x] Operating rules: CLAUDE.md (local), CONTEXT_POLICY.md, CODING_GUIDELINES.md, MODEL_ROUTING.md
- [x] Frozen WeldML ESP32 model exports committed (model_exports/esp32_port/)
- [x] Pi workbench confirmed reachable; SSH key auth established from WSL
- [x] Board hardware verified on Pi: chip ESP32-S3 QFN56 rev0.2, 8MB PSRAM, 16MB flash (Winbond W25Q128), 40MHz
- [x] BOOT+RESET download mode confirmed working (manual sequence required)
- [x] IDF v5.3.2 build confirmed for esp32s3 target (1028/1028, zero errors)
- [x] Q5 resolved: manual BOOT+RESET sufficient; automated GPIO path also confirmed
- [x] Q9 resolved: `POST /api/flash` 404 on this portal; RFC2217 is the confirmed flash path
- [x] Q4 resolved: Pi HTTP portal confirmed; GPIO wiring confirmed (gpio_boot=18, gpio_en=17)
- [x] OpenOCD auto-started on SLOT3 (GDB 3335, telnet 4446, esp32s3-builtin.cfg)
- [x] `esp-idf-handling` skill covers full flash workflow — no new skill needed
- [x] **Stage 1 complete** — template firmware flashed and boot-verified on Waveshare ESP32-S3
- [x] **Stage 2 complete** — `boards/waveshare-esp32-s3-lcd-147/` board config added, build verified
- [x] **Stage 3 complete** — LCD driver verified on hardware; white→green confirmed; quadrant test passed
  - `components/lcd_st7789/` — ST7789 SPI driver, 40 MHz, fill + fill_rect
  - Content displays 180° rotated with MADCTL=0x00 (needs mirror(true,true) before Stage 4 UI work)
  - Fixed black semicircle at physical top-right = hardware/panel artifact (not software)
- [x] **Stage 4 complete** — USB MSC + SD SPI verified on hardware (2026-06-19)
- [ ] **Stage 5 in progress** — Linux/Pi USB MSC confirmed; write-idle detection and JSON write unverified
  - `components/usb_msc_sd/` — TinyUSB CDC+MSC + SD SPI init; SPI3_HOST, sdspi host, sdmmc_card_init
  - LCD green = SD card init success; USB re-enumerated 303a:1001→303a:4003 (OTG PHY switch) — confirmed on both Windows and Pi
  - MADCTL mirror(true,true) applied in lcd_st7789.c (orientation fix from Stage 3)
  - Pi USB host enumeration confirmed after Pi reboot cleared dwc_otg EPROTO state
  - CSV writes to sda1 confirmed reaching SD card (drop_caches verified); JSON not confirmed
  - **Build rule:** always pass `BOARD=waveshare-esp32-s3-lcd-147` to every `idf.py` call (build AND flash); omitting it poisons the CMake cache with the wrong board.h

## What Is Blocked

- [x] MADCTL orientation: `mirror(true,true)` added to `lcd_st7789.c` (Stage 4 session)
- [ ] SD ownership transition (Q3) — follows Q6
- [ ] Robot file-completion signal (Q6) — open
- [ ] Weld file format (Q8) — open
- [ ] Automated workbench flash workflow — Pi GPIO for Key1/Key2 wiring unverified

## What Is Next

1. **Fix `write_placeholder()` silent failure (immediate — required before Stage 6).**
   `fopen(RESULT_PATH, "w")` returns NULL but the function doesn't propagate failure. Fix:
   - `write_placeholder()` must return `bool` (false if fopen returns NULL)
   - `process_sd()` must call `set_state(WELD_STATE_FAILURE)` if `write_placeholder()` returns false
   - After fix, RED instead of GREEN tells us immediately when JSON write fails
   Flash via direct-PC USB (usbipd-win → WSL). Always use `-D BOARD=waveshare-esp32-s3-lcd-147`.

2. **Capture serial log during write-idle cycle to find fopen root cause.**
   After applying fix and reflashing, run the write test. Expected: LCD shows RED.
   Capture ESP serial output during/after process_sd() to see:
   - `ESP_LOGE: Cannot write result to /sdcard/weldml_result.json` → add errno log
   - `ESP_LOGI: Placeholder: ... -> ...` → if this appears, fclose is the failure
   Use Pi passive serial read before process_sd() triggers:
   `ssh casey@192.168.1.43 "python3 -c 'import serial; s=serial.Serial(\"/dev/ttyACM0\",115200,timeout=0.2); [print(s.read(2048).decode(errors=\"replace\"),end=\"\") for _ in range(200)]'" > /tmp/proc_log.txt &`
   Then run write test. After 10s: `cat /tmp/proc_log.txt`.
   Serial output stops when TinyUSB takes over USB PHY, but process_sd() happens LATER — it goes
   through FatFS/SPI, not USB. May still get ESP logs if CDC serial is readable at that point.

3. **Add `workbench.local` to WSL `/etc/hosts`** — `echo "192.168.1.43 workbench.local" | sudo tee -a /etc/hosts`.

---

## Known Conflicts to Resolve Before First Build

| Issue | File | Current value | Correct value | Blocks first smoke-test flash? |
|-------|------|--------------|--------------|-------------------------------|
| Flash size declared as 4MB | root `sdkconfig.defaults` | 4 MB | 16 MB in `boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults` | **No** — partitions.csv fits within 4MB; upper 12MB unused but safe |
| PSRAM not enabled | `sdkconfig.defaults` | not set | CONFIG_SPIRAM=y, CONFIG_SPIRAM_MODE_OCT=y | No — template firmware does not use PSRAM |
| Generic SPI pins | `boards/esp32-s3/board.h` | MOSI=11, CLK=12, CS=10 | LCD: MOSI=45, CLK=40, CS=42; SD: CLK=14, MOSI=15, CS=21 | Yes — for WeldML firmware; not for template smoke test |
| LED pin conflict | `boards/esp32-s3/board.h` | BOARD_LED_PIN=48 | GPIO48 is LCD backlight — cannot be used as LED | Yes — for WeldML firmware; not for template smoke test |
| Partition table comment | `partitions.csv` | "Total flash assumed: 4 MB" | No functional change needed; layout is address-based and safe on 16MB | No |

**Fix path for flash size:** Create `boards/waveshare-esp32-s3-lcd-147/sdkconfig.defaults` with
`CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`. Do **not** change root `sdkconfig.defaults` or
`boards/esp32-s3/sdkconfig.defaults`. Required before OTA layout expansion, SPIFFS
growth above 4MB, or production WeldML firmware deployment.

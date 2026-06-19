# Coding Guidelines

Project-specific coding rules for WeldML ESP32 firmware.
These complement the operating rules in `CLAUDE.md`.

---

## Language and Style

- C99. No C++ unless a dependency forces it.
- `snake_case` for all identifiers: functions, variables, files, macros.
- Macros and enum values in `UPPER_SNAKE_CASE`.
- Header guards: `#pragma once`.
- No global mutable state outside of FreeRTOS task handles, queues, and semaphores
  declared at the top of their owning `.c` file.
- Maximum line length: 100 characters. No hard wrapping if a function call would be
  clearer on one line.

---

## Comments

Write no comments by default. Add one only when the WHY is non-obvious:
a hardware constraint, a timing invariant, a workaround for a specific bug,
behavior that would surprise a reader who knows the spec. Never describe what
the code does — name identifiers well enough that no explanation is needed.

---

## ESP-IDF Patterns

### Error handling

Always check `esp_err_t` returns. Use `ESP_ERROR_CHECK()` only in init paths
where failure is unrecoverable. In runtime paths, log and handle gracefully:

```c
esp_err_t ret = some_idf_call();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "some_idf_call failed: %s", esp_err_to_name(ret));
    // handle or return
}
```

Never silently discard an `esp_err_t`.

### Logging

Use ESP-IDF log macros (`ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`, `ESP_LOGD`).
Define `TAG` as a `static const char *TAG = "component_name";` at the top of each `.c`.
Log level defaults: INFO for normal events, WARN for recoverable issues, ERROR for
failures, DEBUG for development traces (disable before shipping).

### FreeRTOS tasks

Give every task a descriptive name, explicit stack size, and priority comment:

```c
xTaskCreate(inference_task, "inference", 8192, NULL, 5, &s_inference_handle);
```

Do not create tasks with default/magic stack sizes. Measure stack high-water mark
during development and size with headroom.

### Kconfig

Use Kconfig for anything that might reasonably be tuned per board or per build:
timeouts, buffer sizes, default filenames. Do not hardcode values that Kconfig
can own.

---

## Board-Specific Pin Rules

These pins are fixed by the Waveshare ESP32-S3-LCD-1.47 schematic.
Never reassign them. Never use them as fallback GPIOs.

| Pin | Function | Rule |
|-----|----------|------|
| GPIO0 | BOOT / Key1 | Input only. Must not be driven LOW at boot. |
| GPIO14 | SD CLK | SPI2 bus for SD. Fixed. |
| GPIO15 | SD MOSI/CMD | SPI2 bus for SD. Fixed. |
| GPIO16 | SD MISO/D0 | SPI2 bus for SD. Fixed. |
| GPIO17 | SD D2 | Pulled HIGH. Do not drive LOW in SPI mode. |
| GPIO18 | SD D1 | Pulled HIGH. Do not drive LOW in SPI mode. |
| GPIO19 | USB D− | Reserved for native USB. Never reconfigure. |
| GPIO20 | USB D+ | Reserved for native USB. Never reconfigure. |
| GPIO21 | SD CS | SPI2 CS for SD. Fixed. |
| GPIO38 | WS2812B LED | NeoPixel single-wire. RMT channel only. |
| GPIO39 | LCD RST | Active low. Assert before LCD init. |
| GPIO40 | LCD SCLK | SPI3 bus for LCD. Fixed. |
| GPIO41 | LCD DC | Data/command select. |
| GPIO42 | LCD CS | SPI3 CS for LCD. Fixed. |
| GPIO43 | UART0 TXD | Serial console. Do not repurpose. |
| GPIO44 | UART0 RXD | Serial console. Do not repurpose. |
| GPIO45 | LCD MOSI | SPI3 MOSI. Strapping pin — must not be HIGH at boot. |
| GPIO48 | LCD Backlight | MOSFET gate. PWM via LEDC. Never use as LED. |

Use two separate SPI buses:
- **SPI2** (HSPI): SD card (CLK=14, MOSI=15, MISO=16, CS=21)
- **SPI3** (VSPI): LCD (MOSI=45, SCLK=40, CS=42, DC=41)

---

## SD Card Access Rules

1. SD is **exclusively owned** at any moment — never shared between MSC and firmware.
2. Do not call any `sdmmc_*` or `spi_device_*` function on the SD bus while TinyUSB
   MSC is active. This includes SPIFFS/FAT mount calls.
3. The handoff sequence (MSC → firmware) is defined by OPEN_QUESTIONS.md Q3 and Q6.
   Do not implement SD reads until those questions are resolved and logged.
4. Acquire ownership with a FreeRTOS semaphore or event flag. Release it when done.
   Never leave the SD mounted indefinitely — unmount after reading.

---

## LCD Driver Rules

1. Use a dedicated SPI3 bus handle for the LCD. Never share with SD.
2. Initialize backlight GPIO48 via `gpio_config()` or LEDC before enabling the SPI bus.
3. Assert RST (GPIO39 LOW ≥ 10 ms) before sending ST7789V3 init sequence.
4. The MVP only needs full-screen color fills. Do not implement text, fonts, sprites, or
   graphics primitives until the MVP color-fill path is verified on hardware.
5. If `esp_lcd` component is used, follow Espressif's `esp_lcd_panel_io_spi` pattern.
   If a minimal driver is written, keep it in `components/lcd/` as a standalone component.

---

## WeldML Inference Rules

1. Model coefficients and tree structure are embedded at compile time (C arrays or
   `#include`d header files generated from the model source). No runtime file loading.
2. Feature extraction runs on a fixed 22-element array. Verify array bounds explicitly.
3. Both models run to completion before the decision rule is evaluated. Do not short-circuit
   on the first model's result.
4. The decision function signature must be pure (no side effects, no global writes):
   ```c
   weld_verdict_t weld_classify(const float features[22]);
   ```
5. Inference runs in its own task, not in the MSC callback or USB interrupt context.

---

## Build Discipline

- `idf.py build` must pass with zero errors and zero new warnings before any commit.
- Set target before first build: `idf.py set-target esp32s3`
- Use the board-specific sdkconfig: the Waveshare board needs its own
  `boards/waveshare-esp32-s3-lcd-147/` directory (see OPEN_QUESTIONS.md — this is
  blocked on Q1/Q2 resolution).
- `sdkconfig` is gitignored. `sdkconfig.defaults` and board-specific defaults are committed.
- Do not commit with `CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y` set globally.

---

## Testing and Verification

1. Every hardware behavior claim requires a log entry in `NOTES.md`.
2. First test of any new peripheral (SD mount, LCD fill, USB enum): add an `assert()`
   or `ESP_ERROR_CHECK()` on the init call so failure is loud, not silent.
3. Stack overflows: enable `CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK=y` during development.
4. For the MVP, manual verification on the Waveshare board is the acceptance criterion.
   Automated test harness comes in a later milestone.

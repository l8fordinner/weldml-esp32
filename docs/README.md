# docs/

This folder is intentionally empty in the base template.

**When forking this template**, add your hardware reference documents here:

- Datasheets for the ESP32 module you are using
- Schematics and PCB layout files for your board
- Pinout diagrams
- Peripheral datasheets (sensors, displays, actuators, etc.)
- Power supply or battery management notes
- Any other hardware-specific reference material

## Why This Matters

Claude Code reads this folder automatically before scaffolding or extending a
project.  Populated docs let the agent make informed decisions about:

- Pin assignments in `board.h`
- I²C / SPI bus layout
- ADC range and resolution for battery or sensor inputs
- Pull-up / pull-down requirements
- Any chip-specific errata relevant to your build

## Recommended Naming

Use clear, descriptive file names so both humans and the agent can find what
they need quickly:

```
docs/
  schematic-v1.0.pdf
  feather-huzzah32-pinout.pdf
  sensor-BME280-datasheet.pdf
  battery-charger-notes.md
```

No specific format is required — PDF, Markdown, and plain text all work.

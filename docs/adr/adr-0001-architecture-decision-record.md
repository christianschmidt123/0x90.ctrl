# 0x90.deck Architecture Decision Record (ADR)
## Status\nProposed
## Context\nThe project aims to create a DIY Streamdeck and Macro Pad based on ESP32-S3. It requires a clear architecture decision record to guide future development.
## Decision\nThe project will use the ESP32-S3 microcontroller with native USB-HID emulation to create a keyboard and media control device. The web interface will be hosted on the microcontroller using LittleFS.
## Consequences\n- Simplified setup process for users.\n- No need for external drivers on Windows, macOS, and Linux.\n- Easy configuration via a web interface.

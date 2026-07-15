# esp-cpp

Use this skill for ESP32 / ESP8266 / embedded C++ tasks and PlatformIO projects.

Rules:
- Clarify target board and framework first: ESP32, ESP8266, Arduino, or ESP-IDF.
- Prefer simple embedded code over generic desktop C++ patterns.
- Be careful with RAM, stack, flash, heap, fragmentation, and timing.
- Prefer non-blocking loops, millis/micros timing, and minimal allocations.
- Use GPIO, ADC, PWM, UART, I2C, SPI, Wi-Fi, MQTT only when needed.
- Handle interrupts safely and keep ISR code short.
- Prefer constexpr, static storage, and fixed-size buffers when practical.
- Avoid exceptions and heavy dynamic allocation unless the project already uses them.
- If a library already solves the hardware problem cleanly, prefer it over custom code.

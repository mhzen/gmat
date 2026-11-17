# Payload

ESP32 + MPU6050 + BMP280 + E32. Wiring: [schematic](../../schematic/skematik.pdf).
Pins/settings: [config.h](include/config.h).

From this directory:
```sh
pio run -e esp32dev
pio test -e native
pio run -e esp32dev -t upload --upload-port YOUR_PORT
```

5 Hz telemetry. ARM/FIRE simulate states only. GPIO25 stays LOW; keep ejection
hardware disconnected. Radio disabled until wiring, power and modem settings are confirmed.

## E32 UART radio (disabled by default)

Both ends must match settings and framing; see [source](include/radio_frame.h).
CRC is not authentication. No ACK/retry; AUX HIGH does not prove delivery or presence.

TODO: hardware checks, SD logging, battery calibration, flight logic.
Host tests passed; ESP32 build and hardware remain unverified.

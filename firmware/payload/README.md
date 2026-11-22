# Payload

ESP32 + MPU6050 + BMP280 + E32. Wiring: [schematic](../../schematic/skematik.pdf).
Pins/settings: [config.h](include/config.h).

From this directory:
```sh
pio run -e esp32dev
pio test -e native
pio run -e esp32dev -t upload --upload-port YOUR_PORT
```

5 Hz telemetry. GPIO25 is a live pyro output: an accepted FIRE drives it HIGH for
`firePulseMs` (1 s), then LOW. Keep the igniter disconnected for hardware tests.
Radio wiring, power and modem settings are still unconfirmed.

## Notes

- Ebyte E32-433T30D 10498-V1.1. 9600 baud, 8N1. see [radio framing](include/radio_frame.h).
- 31 bytes for telemetry, 4 for a command.
- SD card logging: flight_log.csv replaced every boot, failsfae is not available
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

## E32 UART radio

The payload and GCS both use Ebyte E32-433T30D modules marked 10498-V1.1.
Verify the saved UART and RF settings on both modules. The firmware assumes
9600 baud, 8N1. Both ends use the same [radio framing](include/radio_frame.h).
Frames are binary: 31 bytes for telemetry, 4 for a command; a bad CRC costs one frame.
At the Ebyte 2.4 kbps default air rate, 5 Hz telemetry takes about half the air time.
CRC is not authentication. No ACK/retry; AUX HIGH does not prove delivery or presence.

Payload operation is manual. The legacy `DESCENT` telemetry state is timer-based and does not indicate measured descent.

## microSD flight log

Insert a FAT-formatted card before boot. The payload replaces `flight_log.csv`
on every reset and records the same 5 Hz CSV rows as USB, with a header.
Copy the file before powering the payload back on if you need to keep it.
The schematic uses 3.3 V and common ground, with SCK GPIO18, MISO GPIO19,
MOSI GPIO23, and CS GPIO5.

SD errors are reported over USB. USB and radio telemetry continue if logging
fails. Slow writes can fill the queue and cause sequence gaps in the file.
If power is lost during a write, recent rows may be lost.

The project team verified microSD logging on hardware.

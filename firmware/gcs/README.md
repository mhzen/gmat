# GCS

ESP32 + Ebyte E32-433T30D (10498-V1.1), radio to USB at 115200 baud.
The [GCS schematic](../../schematic/gcs.jpeg) matches the shared radio pin
configuration. ESP32 GPIO17 goes to E32 RXD, GPIO16 to TXD, GPIO32 to M0,
GPIO33 to M1, and GPIO34 to AUX. See the
[payload radio notes](../payload/README.md#e32-uart-radio).

From this directory:
```sh
pio run -e esp32dev
pio test -e native
pio run -e esp32dev -t upload --upload-port YOUR_PORT
```

Radio enabled in `config.h`. Confirm GCS wiring, power and matching modem settings.
The GCS USB interface streams telemetry and diagnostics.
The GPIO13 status LED lights when fresh payload telemetry reports armed.
It goes dark after one second without valid telemetry or on a radio fault,
so a dark LED can also indicate loss of telemetry.
TODO: verify assembled wiring and test RF. Host tests passed; target build/hardware unverified.

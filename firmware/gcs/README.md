# GCS

ESP32 + E32, radio to USB at 115200. GCS pins still assumed from payload config.

From this directory:
```sh
pio run -e esp32dev
pio test -e native
pio run -e esp32dev -t upload --upload-port YOUR_PORT
```

Radio disabled. Confirm wiring, power and matching modem settings before enabling.
ARM/DISARM/FIRE are bench commands; no delivery guarantee. Keep ejection disconnected.
TODO: confirm actual GCS wiring and test RF. Host tests passed; target build/hardware unverified.

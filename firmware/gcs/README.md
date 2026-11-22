# GCS

- Ebyte E32-433T30D (10498-V1.1), 115200 baud.
- [GCS schematic](../../schematic/gcs.jpeg)
- [payload radio notes](../payload/README.md#e32-uart-radio).

Run:
```sh
pio run -e esp32dev
pio test -e native
pio run -e esp32dev -t upload --upload-port YOUR_PORT
```
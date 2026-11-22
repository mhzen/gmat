# GMAT 2026 Rocket Payload - Group 1

ESP32 rocket payload and ground station for the GMAT 2026 final project. Reads MPU6050 acceleration/gyro and BMP280 pressure/temperature at 5 Hz, with CSV logging and a local dashboard.

## Architecture

```mermaid
flowchart LR
    P[Payload ESP32] <-->|E32 LoRa| G[GCS ESP32]
    G <-->|USB serial| B[Python bridge]
    B --> C[CSV log]
    B <--> D[Local dashboard]
```

## Project Layout

| Path | Purpose |
|---|---|
| [firmware/payload](firmware/payload/README.md) | Sensors, payload states and E32 UART transport. |
| [firmware/gcs](firmware/gcs/README.md) | Radio-to-USB bridge. |
| [ground/bridge](ground/bridge/README.md) | Serial telemetry logger. |
| [ground/dashboard](ground/dashboard/README.md) | Plots, reported payload state, barometer values and raw terminal. |
| [flowchart/system_explanation.pdf](flowchart/system_explanation.pdf) | Payload schematic. |

## Run

Requires Python 3.9+ and PlatformIO Core.

```sh
python -m pip install -r ground/bridge/requirements.txt
pio run -d firmware/payload -e esp32dev
pio run -d firmware/gcs -e esp32dev
python ground/dashboard/dashboard.py --port COM5 --log telemetry.csv
```

## Telemetry

GCS USB uses 115200 baud. Python drops the raw flags column, maps the state to
`IDLE` or `FIRE_ACCEPTED`, and adds UTC `rx_time`:

```text
seq,t_ms,ax,ay,az,gx,gy,gz,state,pressure_pa,temperature_centi_c,rx_time
```

| Fields | Meaning |
| --- | --- |
| `seq`, `t_ms` | Sample sequence number and milliseconds since the payload booted |
| `ax`, `ay`, `az` | Three-axis acceleration in milligravity (`1000` = `1 g`) |
| `gx`, `gy`, `gz` | Three-axis angular velocity in tenths of a degree per second (`150` = `15 °/s`) |
| `state` | Reported payload state (`IDLE` or `FIRE_ACCEPTED`) |
| `pressure_pa` | Pressure in pascals |
| `temperature_centi_c` | Temperature in hundredths of a degree Celsius (`2500` = `25 °C`) |
| `rx_time` | UTC time when the laptop received the sample |

The USB serial interface carries telemetry and diagnostics. Radio packets
use CRC-protected framing but have no authentication or delivery acknowledgment.
Update both firmwares and Python together; old telemetry formats are rejected.
See [packet definitions](firmware/payload/include/packet.h) and
[radio framing](firmware/payload/README.md#e32-uart-radio).
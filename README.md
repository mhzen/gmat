# GMAT 2026 Rocket Payload - Group 1

ESP32 rocket payload and ground station for the GMAT 2026 final project.
Reads MPU6050 acceleration/gyro and BMP280 pressure/temperature at 5 Hz,
with CSV logging and a local dashboard.

Payload operation is manual. `ARM` enables firing and `DISARM` returns the
payload to `IDLE`. The firmware still reports a timer-based `DESCENT` state.
The dashboard shows `FIRE ACCEPTED` for both post-command states. That status
confirms command acceptance only.

**WIP: GPIO25 is a live pyro output.** An accepted `FIRE` drives GPIO25 HIGH for
`firePulseMs` (1 s), then LOW. Keep the igniter disconnected for hardware tests.
Radio wiring, power and E32 settings are still unconfirmed.

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
| [schematic/skematik.pdf](schematic/skematik.pdf) | Payload schematic. |

## Run

Requires Python 3.9+ and PlatformIO Core. Firmware targets classic ESP32.

```sh
python -m pip install -r ground/bridge/requirements.txt
pio run -d firmware/payload -e esp32dev
pio run -d firmware/gcs -e esp32dev
python ground/dashboard/dashboard.py --port COM5 --log telemetry.csv
```

Use the actual GCS port and a new log filename. Open http://127.0.0.1:8000.
Run either the dashboard or CLI bridge against the port, not both. See component
READMEs for wiring, upload instructions and tests. With radio disabled, GCS
receives no payload telemetry.

Without hardware: replace `--port COM5` with `--simulate` and use `--log sim.csv`.
This runs a time-seeded, 60-second flight demo at 5 Hz.

## Telemetry

GCS USB uses 115200 baud. Python drops the raw flags column, maps the state to
`IDLE` or `FIRE_ACCEPTED`, and adds UTC `rx_time`:

```text
seq,t_ms,ax,ay,az,gx,gy,gz,state,pressure_pa,temperature_centi_c,rx_time
```

Acceleration is mg, gyro is 0.1 degrees/s, pressure is Pa and temperature is
0.01 degrees C. The USB serial interface carries telemetry and diagnostics. Radio packets
use CRC-protected framing but have no authentication or delivery acknowledgment.
Update both firmwares and Python together; old telemetry formats are rejected.
See [packet definitions](firmware/payload/include/packet.h) and
[radio framing](firmware/payload/README.md#e32-uart-radio).

## Verification Status

- Both ESP32 firmware builds and the ground Python tests passed.
- Payload microSD logging was verified on hardware by the project team.
- Radio telemetry is assumed working. End-to-end radio operation and the GCS
  armed-status LED have yet to be verified on hardware.

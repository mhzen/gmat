# Dashboard

Python 3.9+. From repo root:
```sh
python -m pip install -r ground/bridge/requirements.txt
python ground/dashboard/dashboard.py --port COM5 --log telemetry.csv
```

Open http://127.0.0.1:8000. Use actual port and a new log filename.
Without hardware or pyserial, run:
```sh
python ground/dashboard/dashboard.py --simulate --log sim.csv
```
This repeats a 60-second fake flight at 5 Hz, feeds the plots and raw terminal,
and logs CSV through the bridge. The UI labels simulated data.
It is a visual demo, not a flight physics model. Stop with Ctrl+C.

The payload state panel shows `IDLE` until the payload reports command acceptance.
Both legacy post-command states display `FIRE ACCEPTED`. The armed indicator
shows command readiness separately. The CSV uses `IDLE` and `FIRE_ACCEPTED` and
omits raw flags.

## 3D vehicle view

Three.js is bundled locally (0.170.0, MIT); no CDN or build step is needed.
The model uses `schematic/Rocket Parts.pdf`: 75 mm diameter, 1000 mm body,
250 mm nose, and 150/70/75 mm fins with 80 mm sweep and 2 mm thickness.
The OpenRocket draft supplies the three-fin arrangement; its 300 mm nose differs
from the PDF, so the PDF takes precedence. The body is matte white as requested.

Motion integrates gyro rates in sensor coordinates, relative to the current
origin. Mounting alignment is unconfirmed; this is not calibrated flight attitude
or an absolute heading. Gyro bias drifts, and 5 Hz telemetry misses fast motion.
Missing packets, long gaps and firmware restarts reset the relative origin;
stale telemetry freezes the view. Reset relative motion sets a new visual origin.
Drag or use arrow keys on the view to orbit; scroll or +/- to zoom. Pause 3D
holds the displayed pose while telemetry continues. Reset camera restores the view.
WebGL/module failure leaves telemetry and controls running with a visible error.

Additional checks: `node ground/dashboard/test_motion.mjs` from the repo root.
Do not run CLI bridge on same port. Opening serial may reset ESP32.
The dashboard labels stale telemetry and shows the last reported payload state.
Desktop viewports (≥901×700) fit on one screen with no page scroll; smaller
windows and phones scroll normally.
Tests: `python -m unittest -v` in this directory. Headless Edge checked at
320, 390, 768, 1024 and 1440 px, including stale data and WebGL context loss.
Real sensor alignment, gyro accuracy and hardware behavior remain unverified.

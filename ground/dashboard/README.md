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

## Notes:
- Three.js 0.170.0
- Model based from mechanics' schema
# Dashboard

Python 3.9+. From repo root:
```sh
python -m pip install -r ground/bridge/requirements.txt
python ground/dashboard/dashboard.py --port COM5 --log bench.csv
```

Open http://127.0.0.1:8000. Use actual port and a new log filename.
Do not run CLI bridge on same port. Opening serial may reset ESP32.
Stale telemetry disables all commands, including DISARM. No retry/reconnect.
Bench only; keep ejection disconnected. UI is not a safety interlock.
Tests: `python -m unittest -v` in this directory. Browser/hardware unverified.
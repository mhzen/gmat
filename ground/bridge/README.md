# Bridge

Python 3.9+. From this directory:
```sh
python -m pip install -r requirements.txt
python bridge.py --port COM5 --log bench.csv
python -m unittest -v
```

Use actual port and a new log filename. Logs telemetry + UTC rx_time.
Type ARM/DISARM/FIRE. No retry/reconnect; write success is not payload acknowledgment.
Do not share port with dashboard. Opening serial may reset ESP32.
Bench only; keep ejection disconnected. Host tests passed; hardware unverified.

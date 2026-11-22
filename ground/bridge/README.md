# Bridge

Python 3.9+. From this directory:
```sh
python -m pip install -r requirements.txt
python bridge.py --port COM5 --log telemetry.csv
python -m unittest -v
```

Use actual port and a new log filename. Logs telemetry + UTC rx_time.
The bridge reads telemetry and diagnostics over USB.
Do not share port with dashboard. Opening serial may reset ESP32.

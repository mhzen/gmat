import assert from 'node:assert/strict';
import {RelativeMotion} from './motion.mjs';
import {Quaternion, Vector3} from './vendor/three.module.js';
const packet = (seq, ms, gx = 0, gy = 0, gz = 0) => [0, [seq, ms, 0, 0, 1000, gx, gy, gz, 0, 0, 101325, 2500]];
const close = (a, b) => assert.ok(a.angleTo(b) < 1e-6);
const tracker = new RelativeMotion();
tracker.update([packet(0, 0, 900)], true);
tracker.update([packet(0, 0, 900), packet(1, 200, 900), packet(2, 400, 900)], true);
close(tracker.quaternion, new Quaternion().setFromAxisAngle(new Vector3(1, 0, 0), Math.PI / 5));
const saved = tracker.quaternion.clone();
tracker.update([packet(2, 400, 900)], true);
close(tracker.quaternion, saved); // Polling the same packet must not integrate twice.
tracker.update([], false);
close(tracker.quaternion, saved); // Stale telemetry freezes motion.
tracker.update([packet(3, 600, 900)], true);
close(tracker.quaternion, new Quaternion()); // Fresh stream starts a new origin.
tracker.update([packet(5, 1000, 900)], true);
close(tracker.quaternion, new Quaternion().setFromAxisAngle(new Vector3(1, 0, 0), Math.PI / 5)); // Short seq gap still integrates.
tracker.update([packet(6, 2200, 900)], true);
close(tracker.quaternion, new Quaternion()); // Do not integrate long time gaps.
tracker.update([packet(0, 0, 900)], true);
close(tracker.quaternion, new Quaternion()); // Firmware reboot resets the baseline.
tracker.reset();
tracker.update([packet(0, 0)], true);
tracker.update([packet(1, 200)], true);
close(tracker.quaternion, new Quaternion());
console.log('Relative motion checks passed: units, batches, duplicates, stale, short gaps, long gaps, reboot, zero rates');

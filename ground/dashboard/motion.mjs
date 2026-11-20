import {Quaternion, Vector3} from './vendor/three.module.js';

// Sensor coordinates only: installation-to-rocket alignment is not calibrated.
export class RelativeMotion {
  constructor() {
    this.quaternion = new Quaternion();
    this.previous = null;
    this.axis = new Vector3();
    this.step = new Quaternion();
  }
  reset(sample = null) {
    this.quaternion.identity();
    this.previous = sample;
  }
  update(samples, fresh) {
    if (!fresh || !samples.length) { this.previous = null; return 'STALE / motion frozen'; }
    const latest = samples.at(-1)[1];
    if (!this.previous) { this.reset(latest); return 'Relative origin set'; }
    if (latest[0] === this.previous[0] && latest[1] === this.previous[1]) return 'Relative gyro motion';
    if (latest[0] <= this.previous[0] || latest[1] <= this.previous[1]) {
      this.reset(latest); return 'Stream restarted / origin reset';
    }
    let gap = false;
    for (const [, sample] of samples) {
      if (sample[0] <= this.previous[0]) continue;
      const dt = (sample[1] - this.previous[1]) / 1000;
      // Sequence gaps happen whenever the RF link drops a frame; only a long time gap resets.
      if (dt <= 0 || dt > 1) { this.reset(sample); gap = true; continue; }
      // Trapezoidal integration of 0.1 deg/s rates, once per telemetry packet.
      this.axis.set(...[5, 6, 7].map(k => (sample[k] + this.previous[k]) * Math.PI / 3600));
      const speed = this.axis.length();
      if (speed > 0) {
        this.step.setFromAxisAngle(this.axis.divideScalar(speed), speed * dt);
        this.quaternion.multiply(this.step).normalize();
      }
      this.previous = sample;
    }
    return gap ? 'Packet gap / origin reset' : 'Relative gyro motion';
  }
}

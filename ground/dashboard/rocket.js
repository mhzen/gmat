import * as THREE from './vendor/three.module.js';
import {RelativeMotion} from './motion.mjs';

export function mountRocket(host) {
  const $ = id => document.getElementById(id);
  const renderer = new THREE.WebGLRenderer({antialias: true, alpha: true});
  renderer.setPixelRatio(Math.min(devicePixelRatio || 1, 2));
  renderer.setClearColor(0x171a1d, 0);
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.5;
  renderer.domElement.setAttribute('aria-label', 'Rocket with white body, curved red nose and three plywood fins');
  renderer.domElement.setAttribute('role', 'img');
  host.prepend(renderer.domElement);
  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(32, 1, .01, 30);
  scene.add(new THREE.HemisphereLight(0xe8f2ff, 0x4d4540, 2.6));
  const key = new THREE.DirectionalLight(0xfff2de, 4); key.position.set(3, 4, 4); scene.add(key);
  const rim = new THREE.DirectionalLight(0xb5d7ff, 3); rim.position.set(-3, 1, -2); scene.add(rim);
  const rocket = new THREE.Group(); scene.add(rocket);
  const white = new THREE.MeshStandardMaterial({color: 0xeeeae2, roughness: .74});
  const noseMaterial = new THREE.MeshStandardMaterial({color: 0x733a31, roughness: .43});
  const plywood = new THREE.MeshStandardMaterial({color: 0xd5bd96, roughness: .9});
  const edge = new THREE.MeshStandardMaterial({color: 0x9e8766, roughness: .9});
  const dark = new THREE.MeshStandardMaterial({color: 0x303338, roughness: .75});
  // Rocket Parts.pdf: 1000 x 75 mm body, 250 mm nose; OpenRocket draft: three fins.
  const body = new THREE.Mesh(new THREE.CylinderGeometry(.0375, .0375, 1, 64), white);
  body.position.y = -.125; rocket.add(body);
  const profile = [];
  for (let i = 0; i <= 48; i++) {
    const x = i / 48, theta = Math.acos(1 - 2 * x);
    profile.push(new THREE.Vector2(.0375 * Math.sqrt(Math.max(0, (theta - Math.sin(2 * theta) / 2) / Math.PI)), .625 - .25 * x));
  }
  // Lathe points run from base to tip so exterior normals face outward.
  const nose = new THREE.Mesh(new THREE.LatheGeometry(profile.reverse(), 64), noseMaterial);
  rocket.add(nose);
  const seam = new THREE.Mesh(new THREE.CylinderGeometry(.0378, .0378, .002, 64), dark);
  seam.position.y = .375; rocket.add(seam);
  const finShape = new THREE.Shape();
  finShape.moveTo(.036, -.475); finShape.lineTo(.1125, -.555);
  finShape.lineTo(.1125, -.625); finShape.lineTo(.036, -.625); finShape.closePath();
  const finGeometry = new THREE.ExtrudeGeometry(finShape, {depth: .002, bevelEnabled: false});
  finGeometry.translate(0, 0, -.001);
  for (let i = 0; i < 3; i++) {
    const fin = new THREE.Mesh(finGeometry, [plywood, edge]);
    fin.rotation.y = i * Math.PI * 2 / 3; rocket.add(fin);
  }
  const motor = new THREE.Mesh(new THREE.CylinderGeometry(.019, .019, .012, 40), dark);
  motor.position.y = -.627; rocket.add(motor);
  const grid = new THREE.GridHelper(2.2, 22, 0x49525a, 0x2c3339);
  grid.position.y = -.74; scene.add(grid);
  const axes = new THREE.AxesHelper(.18); axes.position.set(-.47, -.73, 0); scene.add(axes);
  const tracker = new RelativeMotion();
  let yaw = .6, elevation = .12, distance = 2.65, paused = false, latestSamples = [], fresh = false;
  let target = new THREE.Quaternion(), lastFrame = performance.now();
  function placeCamera() {
    camera.position.set(distance * Math.cos(elevation) * Math.sin(yaw), distance * Math.sin(elevation), distance * Math.cos(elevation) * Math.cos(yaw));
    camera.lookAt(0, -.02, 0);
  }
  function resize() {
    const width = host.clientWidth, height = host.clientHeight;
    if (!width || !height) return;
    renderer.setSize(width, height, false); camera.aspect = width / height; camera.updateProjectionMatrix();
  }
  const observer = new ResizeObserver(resize); observer.observe(host);
  placeCamera(); resize();
  let pointer;
  host.addEventListener('pointerdown', event => { pointer = {x: event.clientX, y: event.clientY}; host.setPointerCapture(event.pointerId); });
  host.addEventListener('pointermove', event => {
    if (!pointer) return;
    yaw -= (event.clientX - pointer.x) * .008;
    elevation = THREE.MathUtils.clamp(elevation + (event.clientY - pointer.y) * .008, -1.2, 1.2);
    pointer = {x: event.clientX, y: event.clientY}; placeCamera();
  });
  host.addEventListener('pointerup', () => pointer = null);
  host.addEventListener('pointercancel', () => pointer = null);
  host.addEventListener('wheel', event => { event.preventDefault(); distance = THREE.MathUtils.clamp(distance + event.deltaY * .002, 1.5, 5); placeCamera(); }, {passive: false});
  host.addEventListener('keydown', event => {
    if (!['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', '+', '-', '='].includes(event.key)) return;
    event.preventDefault();
    if (event.key === 'ArrowLeft') yaw -= .1;
    if (event.key === 'ArrowRight') yaw += .1;
    if (event.key === 'ArrowUp') elevation = Math.min(1.2, elevation + .1);
    if (event.key === 'ArrowDown') elevation = Math.max(-1.2, elevation - .1);
    if (['+', '='].includes(event.key)) distance = Math.max(1.5, distance - .1);
    if (event.key === '-') distance = Math.min(5, distance + .1);
    placeCamera();
  });
  $('reset-camera').onclick = () => { yaw = .6; elevation = .12; distance = 2.65; placeCamera(); };
  $('reset-motion').onclick = () => { tracker.reset(latestSamples.at(-1)?.[1] || null); target.identity(); rocket.quaternion.identity(); $('motion-status').textContent = 'Relative origin reset'; };
  $('pause-motion').onclick = () => { paused = !paused; $('pause-motion').textContent = paused ? 'Resume 3D' : 'Pause 3D'; $('pause-motion').setAttribute('aria-pressed', String(paused)); };
  ['reset-motion', 'pause-motion', 'reset-camera'].forEach(id => $(id).disabled = false);
  $('render-error').hidden = true;
  const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');
  renderer.setAnimationLoop(() => {
    const now = performance.now(), dt = Math.min((now - lastFrame) / 1000, .1); lastFrame = now;
    if (!document.hidden && fresh && !paused) {
      if (reducedMotion.matches) rocket.quaternion.copy(target);
      else rocket.quaternion.slerp(target, 1 - Math.exp(-dt * 12));
    }
    if (!document.hidden) renderer.render(scene, camera);
  });
  renderer.domElement.addEventListener('webglcontextlost', event => {
    event.preventDefault(); renderer.setAnimationLoop(null);
    $('render-error').hidden = false;
    $('render-error').textContent = '3D context lost. Reload to restore. Telemetry remains active.';
    $('motion-status').textContent = '3D stopped';
    ['reset-motion', 'pause-motion', 'reset-camera'].forEach(id => $(id).disabled = true);
  });
  return {
    update(samples, isFresh, simulate) {
      latestSamples = samples; fresh = isFresh;
      const status = tracker.update(samples, isFresh);
      target.copy(tracker.quaternion);
      $('motion-status').textContent = (simulate ? 'FAKE DATA / ' : '') + status;
    }
  };
}

const $ = id => document.getElementById(id);
const names = ['IDLE', 'FIRE ACCEPTED'];
const colors = ['#80c7f0', '#f4a46a', '#a5d5b6'];
let snapshot, paused = false, cleared = '', vehicle;
$('states').replaceChildren(...names.map(name => {
  const span = document.createElement('span');
  span.textContent = name;
  return span;
}));

function draw(id, start, scale) {
  const canvas = $(id), rect = canvas.getBoundingClientRect(), dpr = Math.min(devicePixelRatio || 1, 2);
  canvas.width = Math.round(rect.width * dpr);
  canvas.height = Math.round(rect.height * dpr);
  const ctx = canvas.getContext('2d');
  ctx.scale(dpr, dpr);
  const w = rect.width, h = rect.height, now = Date.now() / 1000;
  const rows = (snapshot?.samples || []).filter(row => row[0] >= now - 60);
  let max = 1;
  rows.forEach(row => { for (let k = 0; k < 3; k++) max = Math.max(max, Math.abs(row[1][start + k] * scale)); });
  ctx.font = '10px Consolas, monospace';
  ctx.fillStyle = '#a4adb5';
  ctx.fillText('\u00b1' + max.toFixed(1), 0, 12);
  ctx.strokeStyle = '#30353a';
  for (const fraction of [.25, .5, .75]) {
    ctx.beginPath(); ctx.moveTo(0, h * fraction); ctx.lineTo(w, h * fraction); ctx.stroke();
  }
  colors.forEach((color, k) => {
    ctx.strokeStyle = color; ctx.lineWidth = 1.5; ctx.beginPath();
    let previous;
    rows.forEach(row => {
      const x = (row[0] - now + 60) / 60 * w;
      const y = h / 2 - row[1][start + k] * scale / max * (h / 2 - 18);
      if (!previous || row[0] - previous[0] > 1 || row[1][1] < previous[1][1]) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
      previous = row;
    });
    ctx.stroke();
  });
}

function render() {
  const s = snapshot, p = s.samples.at(-1)?.[1], fresh = s.connected && s.age <= 1;
  $('source').textContent = s.simulate ? 'SIMULATION' : 'USB';
  $('link').textContent = fresh ? 'LIVE' : 'STALE';
  $('link').className = fresh ? '' : 'warning';
  // The two legacy post-command states convey the same operator status.
  const state = p ? Number(p[8] >= 2) : undefined;
  $('state').textContent = p ? names[state] : 'Awaiting telemetry';
  $('state').style.opacity = fresh ? 1 : .5;
  [...$('states').children].forEach((el, i) => el.classList.toggle('active', state === i));
  $('flags').textContent = p ? `Armed ${!!(p[9] & 1)}` : 'Armed --';
  $('age').textContent = `Age ${p ? s.age.toFixed(1) + ' s' : '--'}`;
  $('sequence').textContent = `SEQ ${p ? String(p[0]).padStart(5, '0') : '--'}`;
  $('meta').textContent = `CSV / ${s.log}`;
  if (p) {
    $('pressure').replaceChildren(document.createTextNode((p[10] / 100).toFixed(2)), Object.assign(document.createElement('small'), {textContent: 'hPa'}));
    $('temperature').replaceChildren(document.createTextNode((p[11] / 100).toFixed(2)), Object.assign(document.createElement('small'), {textContent: '\u00b0C'}));
    $('acc').textContent = ['X', 'Y', 'Z'].map((axis, k) => axis + ' ' + p[k + 2]).join(' / ');
    $('gyro').textContent = ['X', 'Y', 'Z'].map((axis, k) => axis + ' ' + (p[k + 5] / 10).toFixed(1)).join(' / ');
  }
  draw('a', 2, 1); draw('g', 5, .1);
  vehicle?.update(s.samples, fresh, s.simulate);
  if (!paused) {
    const index = cleared ? s.terminal.indexOf(cleared) : -1;
    $('terminal').textContent = s.terminal.slice(index + 1).join('\n');
    $('terminal').scrollTop = $('terminal').scrollHeight;
  }
}

async function poll() {
  try {
    const response = await fetch('/api/snapshot');
    if (!response.ok) throw Error('Snapshot unavailable');
    snapshot = await response.json(); render();
  } catch {
    $('link').textContent = 'DISCONNECTED / STALE'; $('link').className = 'warning';
    vehicle?.update([], false, false);
  }
  setTimeout(poll, 500);
}
$('pause').onclick = () => { paused = !paused; $('pause').textContent = paused ? 'Resume' : 'Pause'; if (snapshot) render(); };
$('clear').onclick = () => { cleared = snapshot?.terminal.at(-1) || ''; $('terminal').textContent = ''; };
new ResizeObserver(() => { if (snapshot) { draw('a', 2, 1); draw('g', 5, .1); } }).observe(document.querySelector('.telemetry'));
poll();
// A failed GPU or module must not stop telemetry.
import('/rocket.js').then(({mountRocket}) => { vehicle = mountRocket($('rocket-view')); if (snapshot) render(); }).catch(error => {
  $('render-error').hidden = false;
  $('render-error').textContent = '3D unavailable. Telemetry and plots remain active.';
  $('motion-status').textContent = '3D unavailable';
  console.error(error);
});

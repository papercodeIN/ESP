/**
 * @file web_page.h
 * @brief Embedded single-page configuration and monitoring UI.
 *
 * Served from flash, no external assets, no internet access required.
 */
#pragma once

#include <Arduino.h>

static const char kWebIndexHtml[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 OPC UA Client</title>
<style>
:root{--bg:#0f1820;--card:#16222c;--line:#243544;--txt:#e6eef5;--dim:#8aa0b2;--accent:#31c48d;--warn:#f6a723;--bad:#ef5350}
*{box-sizing:border-box}
body{margin:0;font:15px/1.5 system-ui,"Segoe UI",Roboto,sans-serif;background:var(--bg);color:var(--txt)}
header{display:flex;align-items:center;gap:12px;padding:16px 20px;background:linear-gradient(180deg,#182634,#111c25);border-bottom:1px solid var(--line);position:sticky;top:0;z-index:5}
h1{font-size:17px;margin:0;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.pill{margin-left:auto;flex:none;font-size:12px;padding:4px 10px;border-radius:999px;background:#22323f;color:var(--dim)}
.pill.ok{background:rgba(49,196,141,.15);color:var(--accent)}
.pill.ap{background:rgba(246,167,35,.15);color:var(--warn)}
.pill.bad{background:rgba(239,83,80,.15);color:var(--bad)}
nav{display:flex;gap:6px;padding:10px 16px;overflow-x:auto;border-bottom:1px solid var(--line);background:#111c25;position:sticky;top:57px;z-index:4}
nav button{background:transparent;color:var(--dim);border:1px solid transparent;padding:8px 14px;border-radius:8px;cursor:pointer;font-size:14px;white-space:nowrap}
nav button.active{color:var(--txt);background:#1c2b37;border-color:var(--line)}
main{padding:16px;max-width:1020px;margin:0 auto 40px}
section{display:none}
section.active{display:block}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px;margin-bottom:16px}
h2{font-size:12px;text-transform:uppercase;letter-spacing:.08em;color:var(--dim);margin:0 0 12px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:0 18px}
.row{display:flex;justify-content:space-between;gap:12px;padding:7px 0;border-bottom:1px dashed #22323d;font-size:14px}
.row:last-child{border-bottom:0}
.row span:first-child{color:var(--dim)}
label{display:block;font-size:12px;color:var(--dim);margin-bottom:4px}
input,select{width:100%;padding:9px 10px;border-radius:8px;border:1px solid var(--line);background:#0d151c;color:var(--txt);font-size:14px;font-family:inherit}
input[type=checkbox]{width:auto;margin-right:8px}
.field{margin-bottom:12px}
.inline{display:flex;align-items:center;gap:8px;font-size:13px;color:var(--dim);margin-bottom:12px}
button{font-family:inherit}
button.primary{background:var(--accent);color:#06231a;border:0;padding:10px 18px;border-radius:8px;font-weight:600;cursor:pointer;font-size:14px}
button.ghost{background:#22323f;color:var(--txt);border:1px solid var(--line);padding:8px 14px;border-radius:8px;cursor:pointer;font-size:13px}
button.danger{background:transparent;color:var(--bad);border:1px solid var(--bad);padding:8px 14px;border-radius:8px;cursor:pointer;font-size:13px}
button.small{padding:5px 10px;font-size:12px}
table{width:100%;border-collapse:collapse;font-size:13px}
th,td{padding:8px 6px;text-align:left;border-bottom:1px solid #22323d;white-space:nowrap}
th{color:var(--dim);font-weight:500;font-size:12px;text-transform:uppercase;letter-spacing:.04em}
td input,td select{padding:5px 6px;font-size:13px;min-width:60px}
td input.wide{min-width:140px}
#toast{position:fixed;left:50%;bottom:24px;transform:translate(-50%,20px);background:#1f3340;border:1px solid var(--line);padding:10px 18px;border-radius:10px;opacity:0;transition:.25s;pointer-events:none;z-index:20;max-width:90vw}
#toast.show{opacity:1;transform:translate(-50%,0)}
.muted{color:var(--dim);font-size:12px}
.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:4px}
code{background:#0d151c;border:1px solid var(--line);padding:1px 6px;border-radius:5px;font-size:12px}
</style>
</head>
<body>
<header><h1>ESP32 OPC UA Client</h1><span class="pill" id="conn">loading</span></header>
<nav>
<button class="active" data-tab="status">Status</button>
<button data-tab="network">Network</button>
<button data-tab="connection">Connection</button>
<button data-tab="tags">Tags</button>
</nav>
<main>

<section id="tab-status" class="active">
  <div class="card">
    <h2>OPC UA session</h2>
    <div class="grid">
      <div>
        <div class="row"><span>Client state</span><span id="s-state">-</span></div>
        <div class="row"><span>Server</span><span id="s-url">-</span></div>
        <div class="row"><span>Poll interval</span><span id="s-poll">-</span></div>
        <div class="row"><span>Last error</span><span id="s-err">-</span></div>
      </div>
      <div>
        <div class="row"><span>Successful reads</span><span id="s-ok">-</span></div>
        <div class="row"><span>Failed reads</span><span id="s-fail">-</span></div>
        <div class="row"><span>Last read</span><span id="s-lastread">-</span></div>
      </div>
    </div>
    <div class="actions" style="margin-top:12px">
      <button class="primary" onclick="doConnect()">Connect / reconnect</button>
      <button class="ghost" onclick="doDisconnect()">Disconnect</button>
    </div>
    <p class="muted">The client connects with security policy <code>None</code>. Reads are batched into one Read service call per interval; connection failures retry every 10 s.</p>
  </div>
  <div class="card">
    <h2>Device</h2>
    <div class="grid">
      <div>
        <div class="row"><span>Mode</span><span id="s-mode">-</span></div>
        <div class="row"><span>Wi-Fi</span><span id="s-ssid">-</span></div>
        <div class="row"><span>IP address</span><span id="s-ip">-</span></div>
        <div class="row"><span>Signal</span><span id="s-rssi">-</span></div>
      </div>
      <div>
        <div class="row"><span>Uptime</span><span id="s-uptime">-</span></div>
        <div class="row"><span>Free heap</span><span id="s-heap">-</span></div>
        <div class="row"><span>Firmware</span><span>ESP32 OPC UA Client 1.0</span></div>
      </div>
    </div>
  </div>
</section>

<section id="tab-network">
  <div class="card">
    <h2>Wi-Fi station</h2>
    <div class="grid">
      <div>
        <div class="field"><label>SSID</label><input id="n-ssid"></div>
        <div class="field"><label>Password (blank = keep)</label><input id="n-pass" type="password"></div>
        <div class="field"><label>Host name (mDNS)</label><input id="n-host"></div>
        <div class="inline"><input type="checkbox" id="n-dhcp"><label for="n-dhcp" style="margin:0">Use DHCP</label></div>
      </div>
      <div>
        <div class="field"><label>Static IP</label><input id="n-ip" placeholder="e.g. 192.168.1.51"></div>
        <div class="field"><label>Gateway</label><input id="n-gw"></div>
        <div class="field"><label>Netmask</label><input id="n-mask" placeholder="255.255.255.0"></div>
        <div class="field"><label>Setup AP password</label><input id="n-appass" type="password"></div>
      </div>
    </div>
    <div class="actions">
      <button class="primary" onclick="saveSettings()">Save and restart</button>
      <button class="ghost" onclick="loadSettings()">Reload</button>
    </div>
    <p class="muted">Saving Wi-Fi changes restarts the device. If the station cannot connect afterwards, the setup AP comes back automatically.</p>
  </div>
  <div class="card">
    <h2>Maintenance</h2>
    <div class="actions">
      <button class="ghost" onclick="reboot()">Reboot</button>
      <button class="danger" onclick="factoryReset()">Factory reset</button>
    </div>
  </div>
</section>

<section id="tab-connection">
  <div class="card">
    <h2>OPC UA server connection</h2>
    <div class="grid">
      <div>
        <div class="field"><label>Endpoint URL</label><input id="c-url" placeholder="opc.tcp://192.168.1.50:4840"></div>
        <div class="field"><label>Poll interval (ms)</label><input id="c-poll" type="number" min="250" step="100"></div>
      </div>
      <div>
        <div class="field"><label>Username (blank = anonymous)</label><input id="c-user"></div>
        <div class="field"><label>Password (blank = keep)</label><input id="c-pass" type="password"></div>
      </div>
    </div>
    <div class="actions">
      <button class="primary" onclick="saveConnection()">Save and reconnect</button>
      <button class="ghost" onclick="loadSettings()">Reload</button>
    </div>
    <p class="muted">Without message encryption the username/password travels unencrypted inside the session — use this only on trusted networks.</p>
  </div>
</section>

<section id="tab-tags">
  <div class="card">
    <h2>Tag slots</h2>
    <table>
      <thead><tr><th>On</th><th>ns</th><th>Type</th><th>Identifier</th><th>Name</th><th>Live value</th><th>Status</th></tr></thead>
      <tbody id="tagrows"><tr><td colspan="7" class="muted">loading…</td></tr></tbody>
    </table>
    <div class="actions" style="margin-top:12px">
      <button class="primary" onclick="saveTags()">Save tag table</button>
      <button class="ghost" onclick="loadSettings()">Reload</button>
    </div>
    <p class="muted">Use <code>ns=1;s=Temperature</code> style string ids or numeric ids like <code>ns=0;i=2258</code> (standard server clock). The value column refreshes every 2 s.</p>
  </div>
</section>

</main>
<div id="toast"></div>
<script>
const $ = id => document.getElementById(id);
const SLOT_COUNT = 8;
let settingsCache = null;

function toast(msg, ok = true) {
  const t = $('toast');
  t.textContent = msg;
  t.style.borderColor = ok ? 'var(--accent)' : 'var(--bad)';
  t.classList.add('show');
  clearTimeout(t._h);
  t._h = setTimeout(() => t.classList.remove('show'), 2600);
}

async function api(url, opts) {
  const r = await fetch(url, opts);
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return r.json();
}

document.querySelectorAll('nav button').forEach(b => b.onclick = () => {
  document.querySelectorAll('nav button').forEach(x => x.classList.remove('active'));
  document.querySelectorAll('section').forEach(x => x.classList.remove('active'));
  b.classList.add('active');
  $('tab-' + b.dataset.tab).classList.add('active');
});

function fmtUptime(s) {
  const d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600),
        m = Math.floor(s % 3600 / 60), sec = s % 60;
  return (d ? d + 'd ' : '') + h + 'h ' + m + 'm ' + sec + 's';
}
function fmtBytes(n) {
  return n > 1048576 ? (n / 1048576).toFixed(1) + ' MB'
       : n > 1024 ? (n / 1024).toFixed(1) + ' kB' : n + ' B';
}
function fmtAgo(ms, upSeconds) {
  if (!ms) return 'never';
  /* Board millis compared against the uptime reported in the same response. */
  const d = Math.max(0, (upSeconds * 1000 - ms) / 1000);
  return d.toFixed(0) + ' s ago';
}

async function loadStatus() {
  try {
    const s = await api('/api/status');
    const statePill = $('conn');
    let cls = 'pill ';
    if (s.clientState === 'Connected') cls += 'ok';
    else if (s.mode === 'setup-ap') cls += 'ap';
    else if (s.clientState === 'Error') cls += 'bad';
    statePill.className = cls;
    statePill.textContent = s.clientState === 'Connected'
      ? 'connected' : (s.mode === 'setup-ap' ? 'setup AP · ' : '') + s.clientState.toLowerCase();
    $('s-state').textContent = s.clientState;
    $('s-url').textContent = s.serverUrl || '-';
    $('s-poll').textContent = s.pollMs + ' ms';
    $('s-err').textContent = s.lastError || 'none';
    $('s-ok').textContent = s.readsOk;
    $('s-fail').textContent = s.readsFailed;
    $('s-lastread').textContent = fmtAgo(s.lastReadAt, s.uptime);
    $('s-mode').textContent = s.mode === 'setup-ap' ? 'Setup AP' : 'Station';
    $('s-ssid').textContent = s.ssid || '-';
    $('s-ip').textContent = s.ip;
    $('s-rssi').textContent = s.connected ? s.rssi + ' dBm' : '-';
    $('s-uptime').textContent = fmtUptime(s.uptime);
    $('s-heap').textContent = fmtBytes(s.heap);
  } catch (e) { /* transient */ }
}

function fillSettings(s) {
  settingsCache = s;
  $('n-ssid').value = s.wifiSsid;
  $('n-pass').value = '';
  $('n-host').value = s.hostname;
  $('n-dhcp').checked = s.dhcp;
  $('n-ip').value = s.ip;
  $('n-gw').value = s.gateway;
  $('n-mask').value = s.netmask;
  $('n-appass').value = '';
  $('c-url').value = s.serverUrl;
  $('c-user').value = s.username;
  $('c-pass').value = '';
  $('c-poll').value = s.pollMs;
  renderSlots(s.tags);
}

async function loadSettings() {
  fillSettings(await api('/api/settings'));
}

function formBody(pairs) {
  const body = new URLSearchParams();
  for (const [k, v] of pairs) if (v !== null && v !== undefined && v !== '') body.append(k, v);
  return body.toString();
}

async function post(url, pairs) {
  const r = await fetch(url, {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: pairs ? formBody(pairs) : ''
  });
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return r.json();
}

async function saveSettings() {
  const pairs = [
    ['wifiSsid', $('n-ssid').value],
    ['hostname', $('n-host').value],
    ['ip', $('n-ip').value], ['gateway', $('n-gw').value], ['netmask', $('n-mask').value],
    ['apPass', $('n-appass').value]
  ];
  if ($('n-pass').value) pairs.push(['wifiPass', $('n-pass').value]);
  if ($('n-dhcp').checked) pairs.push(['dhcp', '1']);
  try {
    await post('/api/settings', pairs);
    toast('Saved. Restarting…');
    setTimeout(() => location.reload(), 4000);
  } catch (e) { toast('Save failed: ' + e.message, false); }
}

async function saveConnection() {
  const pairs = [
    ['serverUrl', $('c-url').value],
    ['username', $('c-user').value],
    ['pollMs', $('c-poll').value]
  ];
  if ($('c-pass').value) pairs.push(['password', $('c-pass').value]);
  try {
    await post('/api/connection', pairs);
    toast('Saved. Reconnecting…');
  } catch (e) { toast('Save failed: ' + e.message, false); }
}

let slotRows = [];
function esc(v) {
  return String(v ?? '').replace(/[&<>"']/g, c =>
    ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}
function renderSlots(tags) {
  slotRows = [];
  const tbody = $('tagrows');
  tbody.innerHTML = '';
  const slots = (tags && tags.length) ? tags : Array.from({length: SLOT_COUNT}, () => ({
    enabled: false, ns: 1, stringId: true, ident: '', name: ''
  }));
  slots.slice(0, SLOT_COUNT).forEach((t, i) => {
    const tr = document.createElement('tr');
    tr.innerHTML =
      '<td><input type="checkbox" data-f="enabled"' + (t.enabled ? ' checked' : '') + '></td>' +
      '<td><input type="number" data-f="ns" min="0" max="65535" value="' + esc(t.ns ?? 0) + '"></td>' +
      '<td><select data-f="type"><option value="string"' + (t.stringId ? ' selected' : '') + '>string</option>' +
      '<option value="numeric"' + (!t.stringId ? ' selected' : '') + '>numeric</option></select></td>' +
      '<td><input class="wide" data-f="ident" value="' + esc(t.ident) + '"></td>' +
      '<td><input class="wide" data-f="name" value="' + esc(t.name) + '"></td>' +
      '<td id="live-' + i + '" style="font-family:ui-monospace,Consolas,monospace">-</td>' +
      '<td id="stat-' + i + '"><span class="muted">-</span></td>';
    tbody.appendChild(tr);
    slotRows.push(tr);
  });
}

function slotFormData() {
  const pairs = [];
  slotRows.forEach((tr, i) => {
    const p = 'tag' + i;
    tr.querySelectorAll('[data-f]').forEach(inp => {
      if (inp.dataset.f === 'enabled') {
        if (inp.checked) pairs.push([p + '-enabled', '1']);
      } else {
        pairs.push([p + '-' + inp.dataset.f, inp.value]);
      }
    });
  });
  return pairs;
}

async function saveTags() {
  try {
    await post('/api/tags', slotFormData());
    toast('Tag table saved');
    loadTags();
  } catch (e) { toast('Save failed: ' + e.message, false); }
}

let liveCells = [];
async function loadTags() {
  let tags;
  try { tags = await api('/api/tags'); } catch (e) { return; }
  if (!slotRows.length && settingsCache) renderSlots(settingsCache.tags);
  tags.forEach((t, i) => {
    const live = $('live-' + i), stat = $('stat-' + i);
    if (!live) return;
    live.textContent = t.ok ? t.value : '—';
    stat.innerHTML = t.ok ? '<span class="muted">Good</span>' :
      '<span class="muted">' + (t.enabled ? 'no data' : 'off') + '</span>';
  });
}

async function doConnect() {
  try { await post('/api/connect'); toast('Connecting…'); loadStatus(); }
  catch (e) { toast('Command failed', false); }
}
async function doDisconnect() {
  try { await post('/api/disconnect'); toast('Disconnected'); loadStatus(); }
  catch (e) { toast('Command failed', false); }
}
async function reboot() {
  await post('/api/reboot');
  toast('Rebooting…');
  setTimeout(() => location.reload(), 4000);
}
async function factoryReset() {
  if (!confirm('Erase all settings and restart?')) return;
  await post('/api/factory-reset');
  toast('Factory reset…');
  setTimeout(() => location.reload(), 4000);
}

loadStatus();
loadSettings();
loadTags();
setInterval(loadStatus, 3000);
setInterval(loadTags, 2000);
</script>
</body>
</html>)rawliteral";

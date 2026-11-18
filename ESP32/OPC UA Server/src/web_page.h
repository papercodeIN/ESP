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
<title>ESP32 OPC UA Server</title>
<style>
:root{--bg:#0f1820;--card:#16222c;--line:#243544;--txt:#e6eef5;--dim:#8aa0b2;--accent:#31c48d;--warn:#f6a723;--bad:#ef5350}
*{box-sizing:border-box}
body{margin:0;font:15px/1.5 system-ui,"Segoe UI",Roboto,sans-serif;background:var(--bg);color:var(--txt)}
header{display:flex;align-items:center;gap:12px;padding:16px 20px;background:linear-gradient(180deg,#182634,#111c25);border-bottom:1px solid var(--line);position:sticky;top:0;z-index:5}
h1{font-size:17px;margin:0;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.pill{margin-left:auto;flex:none;font-size:12px;padding:4px 10px;border-radius:999px;background:#22323f;color:var(--dim)}
.pill.ok{background:rgba(49,196,141,.15);color:var(--accent)}
.pill.ap{background:rgba(246,167,35,.15);color:var(--warn)}
nav{display:flex;gap:6px;padding:10px 16px;overflow-x:auto;border-bottom:1px solid var(--line);background:#111c25;position:sticky;top:57px;z-index:4}
nav button{background:transparent;color:var(--dim);border:1px solid transparent;padding:8px 14px;border-radius:8px;cursor:pointer;font-size:14px;white-space:nowrap}
nav button.active{color:var(--txt);background:#1c2b37;border-color:var(--line)}
main{padding:16px;max-width:980px;margin:0 auto 40px}
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
td input{padding:5px 6px;font-size:13px;min-width:90px}
#toast{position:fixed;left:50%;bottom:24px;transform:translate(-50%,20px);background:#1f3340;border:1px solid var(--line);padding:10px 18px;border-radius:10px;opacity:0;transition:.25s;pointer-events:none;z-index:20;max-width:90vw}
#toast.show{opacity:1;transform:translate(-50%,0)}
.muted{color:var(--dim);font-size:12px}
.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:4px}
code{background:#0d151c;border:1px solid var(--line);padding:1px 6px;border-radius:5px;font-size:12px}
.endpoint{display:flex;align-items:center;gap:10px;background:#0d151c;border:1px solid var(--line);border-radius:10px;padding:12px 14px;font-family:ui-monospace,Consolas,monospace;font-size:15px;color:var(--accent);margin:6px 0 14px;overflow-x:auto}
.endpoint span{flex:1}
.badge{font-size:11px;padding:2px 8px;border-radius:999px;background:#22323f;color:var(--dim)}
.badge.rw{background:rgba(49,196,141,.15);color:var(--accent)}
.badge.ro{background:rgba(138,160,178,.15);color:var(--dim)}
</style>
</head>
<body>
<header><h1>ESP32 OPC UA Server</h1><span class="pill" id="conn">loading</span></header>
<nav>
<button class="active" data-tab="status">Status</button>
<button data-tab="network">Network</button>
<button data-tab="opcua">OPC UA</button>
<button data-tab="tags">Tags</button>
</nav>
<main>

<section id="tab-status" class="active">
  <div class="card">
    <h2>OPC UA endpoint</h2>
    <div class="endpoint"><span id="s-endpoint">-</span><button class="ghost small" onclick="copyEndpoint()">Copy</button></div>
    <div class="grid">
      <div>
        <div class="row"><span>Server state</span><span id="s-opcua">-</span></div>
        <div class="row"><span>Device name</span><span id="s-devname">-</span></div>
        <div class="row"><span>mDNS</span><span><a id="s-mdns" href="#">-</a></span></div>
      </div>
      <div>
        <div class="row"><span>Namespace</span><span>1 (string ids)</span></div>
        <div class="row"><span>Security policy</span><span>None</span></div>
        <div class="row"><span>Application URI</span><span>urn:esp32:opcua:server</span></div>
      </div>
    </div>
    <p class="muted">Connect any OPC UA client (UaExpert, Prosys, TIA, …) with security policy <code>None</code> and anonymous login. Tags live in namespace 1 with string identifiers, e.g. <code>ns=1;s=Temperature</code>.</p>
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
        <div class="row"><span>Firmware</span><span>ESP32 OPC UA Server 1.0</span></div>
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
        <div class="field"><label>Static IP</label><input id="n-ip" placeholder="e.g. 192.168.1.50"></div>
        <div class="field"><label>Gateway</label><input id="n-gw"></div>
        <div class="field"><label>Netmask</label><input id="n-mask" placeholder="255.255.255.0"></div>
        <div class="field"><label>Setup AP password</label><input id="n-appass" type="password"></div>
      </div>
    </div>
    <div class="actions">
      <button class="primary" onclick="saveSettings()">Save and restart</button>
      <button class="ghost" onclick="loadSettings()">Reload</button>
    </div>
    <p class="muted">Saving Wi-Fi changes restarts the device. If the station cannot connect afterwards, the setup AP <span id="n-aps">-</span> comes back automatically.</p>
  </div>
  <div class="card">
    <h2>Maintenance</h2>
    <div class="actions">
      <button class="ghost" onclick="reboot()">Reboot</button>
      <button class="danger" onclick="factoryReset()">Factory reset</button>
    </div>
  </div>
</section>

<section id="tab-opcua">
  <div class="card">
    <h2>Endpoint configuration</h2>
    <div class="grid">
      <div>
        <div class="field"><label>Device name (application name)</label><input id="u-devname"></div>
        <div class="field"><label>TCP port</label><input id="u-port" type="number" min="1" max="65535"></div>
      </div>
      <div>
        <div class="field"><label>Endpoint host override (blank = IP address)</label><input id="u-ephost" placeholder="e.g. esp32-opcua.local"></div>
        <p class="muted">Used when building <code>opc.tcp://host:port</code> in GetEndpoints. Leave blank to advertise the device IP.</p>
      </div>
    </div>
    <div class="actions">
      <button class="primary" onclick="saveOpcua()">Save and restart</button>
      <button class="ghost" onclick="loadSettings()">Reload</button>
    </div>
  </div>
  <div class="card">
    <h2>Tag addressing</h2>
    <p class="muted">All tags are published in <b>namespace 1</b> with <b>string identifiers</b> under the Objects folder. Write access follows the <i>Setpoint</i> and <i>RelayOutput</i> access levels; everything else is read-only. The method <code>ns=1;s=RebootDevice</code> restarts the device when called.</p>
  </div>
</section>

<section id="tab-tags">
  <div class="card">
    <h2>Live tags</h2>
    <table>
      <thead><tr><th>Node id (ns=1;s=…)</th><th>Name</th><th>Type</th><th>Value</th><th>Write</th></tr></thead>
      <tbody id="tagrows"><tr><td colspan="5" class="muted">loading…</td></tr></tbody>
    </table>
    <p class="muted">Values refresh every 2 s. Writes go straight into the same state that OPC UA clients read.</p>
  </div>
</section>

</main>
<div id="toast"></div>
<script>
const $ = id => document.getElementById(id);
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

async function loadStatus() {
  try {
    const s = await api('/api/status');
    $('conn').textContent = s.mode === 'setup-ap' ? 'setup AP' : (s.connected ? 'online' : 'offline');
    $('conn').className = 'pill ' + (s.mode === 'setup-ap' ? 'ap' : (s.connected ? 'ok' : ''));
    $('s-mode').textContent = s.mode === 'setup-ap' ? 'Setup AP' : 'Station';
    $('s-ssid').textContent = s.ssid || '-';
    $('s-ip').textContent = s.ip;
    $('s-rssi').textContent = s.connected ? s.rssi + ' dBm' : '-';
    $('s-uptime').textContent = fmtUptime(s.uptime);
    $('s-heap').textContent = fmtBytes(s.heap);
    $('s-endpoint').textContent = s.endpointUrl || 'starting…';
    $('s-opcua').textContent = s.opcuaRunning ? 'Running' : 'Stopped';
    $('s-devname').textContent = s.deviceName;
    const mdns = $('s-mdns');
    mdns.textContent = s.hostname + '.local';
    mdns.href = 'http://' + s.hostname + '.local';
    $('n-aps').textContent = s.mode === 'setup-ap' ? s.ssid : '';
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
  $('u-devname').value = s.deviceName;
  $('u-port').value = s.port;
  $('u-ephost').value = s.endpointHost;
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

async function saveOpcua() {
  const pairs = [
    ['deviceName', $('u-devname').value],
    ['port', $('u-port').value],
    ['endpointHost', $('u-ephost').value]
  ];
  try {
    await post('/api/settings', pairs);
    toast('Saved. Restarting…');
    setTimeout(() => location.reload(), 4000);
  } catch (e) { toast('Save failed: ' + e.message, false); }
}

function copyEndpoint() {
  const text = $('s-endpoint').textContent;
  if (navigator.clipboard) navigator.clipboard.writeText(text).then(() => toast('Endpoint copied'));
  else toast(text);
}

const tagInputs = {};
const tagValueCells = {};
let tagRowsBuilt = false;
function renderTags(tags) {
  const tbody = $('tagrows');
  if (tagRowsBuilt) {
    /* Refresh the value cells only — rebuilding the table would steal
     * focus from the write inputs every poll. */
    for (const t of tags) {
      const cell = tagValueCells[t.id];
      if (cell) cell.textContent = t.valid ? t.value : '-';
    }
    return;
  }
  tbody.innerHTML = '';
  for (const t of tags) {
    const tr = document.createElement('tr');
    const idCell = document.createElement('td');
    idCell.innerHTML = '<code>' + t.id + '</code>';
    const nameCell = document.createElement('td');
    nameCell.textContent = t.name;
    const typeCell = document.createElement('td');
    const badge = document.createElement('span');
    badge.className = 'badge ' + (t.writable ? 'rw' : 'ro');
    badge.textContent = t.type + (t.writable ? ' · RW' : ' · RO');
    typeCell.appendChild(badge);
    const valCell = document.createElement('td');
    valCell.textContent = t.valid ? t.value : '-';
    valCell.style.fontFamily = 'ui-monospace,Consolas,monospace';
    tagValueCells[t.id] = valCell;
    tr.append(idCell, nameCell, typeCell, valCell);

    const writeCell = document.createElement('td');
    if (t.writable) {
      const wrap = document.createElement('div');
      wrap.style.cssText = 'display:flex;gap:6px';
      const input = document.createElement('input');
      input.type = t.type === 'Boolean' ? 'text' : 'number';
      input.step = 'any';
      input.placeholder = t.type === 'Boolean' ? 'true / false' : 'value';
      if (!(t.id in tagInputs)) tagInputs[t.id] = t.value;
      input.value = tagInputs[t.id];
      input.oninput = () => tagInputs[t.id] = input.value;
      const btn = document.createElement('button');
      btn.className = 'ghost small';
      btn.textContent = 'Apply';
      btn.onclick = async () => {
        try {
          await post('/api/tags/write', [['id', t.id], ['value', tagInputs[t.id]]]);
          toast('Wrote ' + t.id);
        } catch (e) { toast('Write failed', false); }
      };
      wrap.append(input, btn);
      writeCell.appendChild(wrap);
    } else {
      writeCell.innerHTML = '<span class="muted">—</span>';
    }
    tr.appendChild(writeCell);
    tbody.appendChild(tr);
  }
  tagRowsBuilt = true;
}

async function loadTags() {
  try { renderTags(await api('/api/tags')); } catch (e) { /* transient */ }
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

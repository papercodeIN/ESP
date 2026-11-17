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
<title>BACnet/IP Device</title>
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
td input,td select{padding:5px 6px;font-size:13px;min-width:70px}
#toast{position:fixed;left:50%;bottom:24px;transform:translate(-50%,20px);background:#1f3340;border:1px solid var(--line);padding:10px 18px;border-radius:10px;opacity:0;transition:.25s;pointer-events:none;z-index:20;max-width:90vw}
#toast.show{opacity:1;transform:translate(-50%,0)}
.muted{color:var(--dim);font-size:12px}
.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:4px}
code{background:#0d151c;border:1px solid var(--line);padding:1px 6px;border-radius:5px;font-size:12px}
</style>
</head>
<body>
<header><h1 id="devname">BACnet/IP Device</h1><span class="pill" id="conn">loading</span></header>
<nav>
<button class="active" data-tab="status">Status</button>
<button data-tab="network">Network</button>
<button data-tab="device">BACnet</button>
<button data-tab="points">Points</button>
</nav>
<main>

<section id="tab-status" class="active">
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
        <div class="row"><span>Device instance</span><span id="s-instance">-</span></div>
        <div class="row"><span>BACnet port</span><span id="s-port">-</span></div>
        <div class="row"><span>Firmware</span><span id="s-fw">-</span></div>
        <div class="row"><span>Uptime</span><span id="s-uptime">-</span></div>
        <div class="row"><span>Free memory</span><span id="s-heap">-</span></div>
      </div>
    </div>
    <p class="muted" style="margin-top:12px">Open this page at <code id="s-url">-</code>. Discovery works from YABE and any BACnet/IP client on the same subnet.</p>
  </div>
  <div class="card">
    <h2>Live values</h2>
    <div id="live" class="muted">Loading...</div>
  </div>
  <div class="card">
    <h2>Maintenance</h2>
    <div class="actions">
      <button class="ghost" onclick="rebootDevice()">Restart device</button>
      <button class="danger" onclick="factoryReset()">Factory reset</button>
    </div>
    <p class="muted" style="margin-top:10px">Factory reset erases Wi-Fi and BACnet settings and starts the setup access point.</p>
  </div>
</section>

<section id="tab-network">
  <div class="card">
    <h2>Wi-Fi network</h2>
    <div class="field"><label>Network name (SSID)</label><input id="f-ssid" maxlength="32" autocomplete="off"></div>
    <div class="field"><label>Password</label><input id="f-pass" type="password" maxlength="64" autocomplete="new-password" placeholder="Leave empty to keep the current password"></div>
    <div class="field"><label>Host name (mDNS)</label><input id="f-host" maxlength="32"><span class="muted">The UI is also reachable at <code>http://HOSTNAME.local</code></span></div>
    <div class="inline"><input id="f-static" type="checkbox"><label for="f-static" style="margin:0">Use a fixed IP address</label></div>
    <div class="grid">
      <div class="field"><label>IP address</label><input id="f-ip" maxlength="15"></div>
      <div class="field"><label>Gateway</label><input id="f-gw" maxlength="15"></div>
      <div class="field"><label>Subnet mask</label><input id="f-mask" maxlength="15"></div>
      <div class="field"><label>DNS server</label><input id="f-dns" maxlength="15"></div>
    </div>
    <div class="field"><label>Setup access point password</label><input id="f-appass" maxlength="64" placeholder="Empty = open network"></div>
    <div class="actions"><button class="primary" onclick="saveNetwork()">Save and restart</button></div>
    <p class="muted" style="margin-top:10px">If the device cannot join the network it starts its own access point so you can always reach this page at <code>http://192.168.4.1</code>.</p>
  </div>
</section>

<section id="tab-device">
  <div class="card">
    <h2>BACnet identity</h2>
    <div class="grid">
      <div class="field"><label>Device instance</label><input id="f-instance" type="number" min="0" max="4194302"></div>
      <div class="field"><label>Device name</label><input id="f-name" maxlength="63"></div>
      <div class="field"><label>Location</label><input id="f-location" maxlength="31"></div>
      <div class="field"><label>Vendor ID</label><input id="f-vendorid" type="number" min="0" max="65535"></div>
      <div class="field"><label>Vendor name</label><input id="f-vendorname" maxlength="47"></div>
      <div class="field"><label>Model name</label><input id="f-model" maxlength="47"></div>
      <div class="field"><label>Firmware revision</label><input id="f-fw" maxlength="15"></div>
      <div class="field"><label>Application version</label><input id="f-app" maxlength="15"></div>
    </div>
    <div class="field"><label>Description</label><input id="f-desc" maxlength="79"></div>
  </div>
  <div class="card">
    <h2>Services</h2>
    <div class="grid">
      <div class="field"><label>BACnet/IP UDP port</label><input id="f-port" type="number" min="1" max="65535"></div>
      <div class="field"><label>Web interface port</label><input value="80" disabled></div>
    </div>
    <div class="inline"><input id="f-web" type="checkbox"><label for="f-web" style="margin:0">Enable the web interface on the local network</label></div>
    <div class="actions"><button class="primary" onclick="saveDevice()">Save and restart</button></div>
    <p class="muted" style="margin-top:10px">The device answers Who-Is, ReadProperty, ReadPropertyMultiple, WriteProperty and SubscribeCOV (change-of-value notifications). Change the UDP port only if your site requires it (default 47808 / 0xBAC0).</p>
  </div>
</section>

<section id="tab-points">
  <div class="card">
    <h2>Points</h2>
    <div class="muted" style="margin-bottom:10px">Commandable points accept a value and a priority (1 = highest, 16 = lowest). Release writes NULL to that priority and the point falls back to the next active priority or the relinquish default.</div>
    <div style="overflow-x:auto">
      <table>
        <thead><tr><th>Point</th><th>Name</th><th>Present value</th><th>Priority</th><th>Actions</th></tr></thead>
        <tbody id="pts"><tr><td colspan="5" class="muted">Loading...</td></tr></tbody>
      </table>
    </div>
  </div>
</section>

</main>
<div id="toast"></div>
<script>
var POINTS=[];
var POINTS_SIG="";
var UNITS={62:"\u00b0C",73:"s",95:"",98:"%",199:"dB"};

function $(s){return document.querySelector(s);}
function unit(u){return UNITS[u]!==undefined?UNITS[u]:"";}
function fmt(v,d){if(typeof v!=="number")return String(v);var f=Math.pow(10,d===undefined?1:d);return String(Math.round(v*f)/f);}
function esc(s){return String(s).replace(/[&<>"]/g,function(c){return {"&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;"}[c];});}
function toast(msg){var t=$("#toast");t.textContent=msg;t.className="show";clearTimeout(t._timer);t._timer=setTimeout(function(){t.className="";},2800);}

Array.prototype.forEach.call(document.querySelectorAll("nav button"),function(b){
  b.addEventListener("click",function(){
    Array.prototype.forEach.call(document.querySelectorAll("nav button"),function(x){x.classList.toggle("active",x===b);});
    Array.prototype.forEach.call(document.querySelectorAll("main section"),function(s){s.classList.toggle("active",s.id==="tab-"+b.dataset.tab);});
  });
});

function api(path,opts){
  return fetch(path,opts).then(function(r){
    if(!r.ok)throw new Error("HTTP "+r.status);
    return r.json();
  });
}

function loadStatus(){
  return api("/api/status").then(function(s){
    $("#devname").textContent=s.name+" \u00b7 device "+s.instance;
    var c=$("#conn");
    c.textContent=s.portal?"Setup AP":(s.connected?"Online":"Offline");
    c.className="pill "+(s.portal?"ap":(s.connected?"ok":""));
    $("#s-mode").textContent=s.mode;
    $("#s-ssid").textContent=s.ssid||"-";
    $("#s-ip").textContent=s.ip||"-";
    $("#s-rssi").textContent=s.connected?s.rssi+" dBm":"-";
    $("#s-instance").textContent=s.instance;
    $("#s-port").textContent=s.port+" (UDP)";
    $("#s-fw").textContent=s.firmware;
    $("#s-uptime").textContent=s.uptime+" s";
    $("#s-heap").textContent=Math.round(s.heap/1024)+" KB";
    $("#s-url").textContent=s.portal?"http://192.168.4.1/":"http://"+(s.ip||"device")+"/";
  });
}

function renderLive(){
  var box=$("#live");
  if(!POINTS.length){box.textContent="No points.";return;}
  box.innerHTML=POINTS.map(function(p){
    return '<div class="row"><span>'+p.k+':'+p.i+' \u00b7 '+esc(p.n)+'</span><span>'+fmt(p.v)+" "+unit(p.u)+'</span></div>';
  }).join("");
}

function renderPoints(){
  var body=$("#pts");
  if(!POINTS.length){body.innerHTML='<tr><td colspan="5" class="muted">No points.</td></tr>';return;}
  body.innerHTML="";
  POINTS.forEach(function(p){
    var tr=document.createElement("tr");
    var kind=p.k;
    var isAnalog=(kind==="AV"||kind==="AO");
    var html='<td>'+kind+':'+p.i+'</td><td>'+esc(p.n)+'</td>';
    if(p.c){
      if(isAnalog){
        html+='<td><input type="number" step="0.1" id="val-'+p.t+'-'+p.i+'" value="'+fmt(p.v,2)+'"></td>';
      }else{
        html+='<td><select id="val-'+p.t+'-'+p.i+'"><option value="1">Active (1)</option><option value="0">Inactive (0)</option></select></td>';
      }
      var opts="";
      for(var n=1;n<=16;n++){opts+='<option value="'+n+'"'+(n===8?" selected":"")+'>'+n+'</option>';}
      html+='<td><select id="pri-'+p.t+'-'+p.i+'">'+opts+'</select></td>';
      html+='<td><button class="ghost small" onclick="writePoint('+p.t+','+p.i+')">Write</button> '+
            '<button class="ghost small" onclick="releasePoint('+p.t+','+p.i+')">Release</button></td>';
    }else{
      html+='<td id="val-'+p.t+'-'+p.i+'">'+fmt(p.v)+" "+unit(p.u)+'</td><td class="muted">-</td><td class="muted">read-only</td>';
    }
    tr.innerHTML=html;
    body.appendChild(tr);
    if(p.c&&!isAnalog&&p.v===0){var s=document.getElementById("val-"+p.t+"-"+p.i);if(s)s.value="0";}
  });
}

/* Update present values in place so an in-progress edit is never wiped. */
function refreshPointValues(){
  POINTS.forEach(function(p){
    var el=document.getElementById("val-"+p.t+"-"+p.i);
    if(!el)return;
    if(p.c){
      if(el.tagName==="INPUT"){
        if(document.activeElement!==el){el.value=fmt(p.v,2);}
      }else{
        el.value=p.v?"1":"0";
      }
    }else{
      el.textContent=fmt(p.v)+" "+unit(p.u);
    }
  });
}

/* The table is rebuilt only when the point structure changes (names,
   commandable flags, priority arrays); plain value updates refresh in
   place so the user can type without the page clearing the field. */
function pointsSig(data){
  return JSON.stringify((data&&data.points?data.points:[]).map(function(p){
    var s=[p.t,p.i,p.k,p.n,p.u,p.c];
    if(p.c){s.push(p.rd);s.push((p.pa||[]).join(","));}
    return s;
  }));
}

function loadPoints(){
  return api("/api/points").then(function(data){
    POINTS=data.points||[];
    renderLive();
    var sig=pointsSig(data);
    if(sig!==POINTS_SIG){
      POINTS_SIG=sig;
      renderPoints();
    }else{
      refreshPointValues();
    }
  });
}

function writePoint(type,instance){
  var v=document.getElementById("val-"+type+"-"+instance);
  var p=document.getElementById("pri-"+type+"-"+instance);
  var body=new URLSearchParams({type:String(type),instance:String(instance),value:v?v.value:"",priority:String(p?p.value:8)});
  api("/api/points/write",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:body})
    .then(function(r){toast(r.ok?"Value written":"Write failed (class "+r.class+", code "+r.code+")");return loadPoints();});
}

function releasePoint(type,instance){
  var p=document.getElementById("pri-"+type+"-"+instance);
  var body=new URLSearchParams({type:String(type),instance:String(instance),rel:"1",priority:String(p?p.value:8)});
  api("/api/points/write",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:body})
    .then(function(r){toast(r.ok?"Priority released":"Release failed (code "+r.code+")");return loadPoints();});
}

function loadSettings(){
  return api("/api/settings").then(function(s){
    $("#f-ssid").value=s.ssid||"";
    $("#f-pass").value="";
    $("#f-pass").placeholder=s.hasPass?"Stored password - leave empty to keep":"No password stored";
    $("#f-host").value=s.hostname||"";
    $("#f-static").checked=!!s.static;
    $("#f-ip").value=s.ip||"";
    $("#f-gw").value=s.gw||"";
    $("#f-mask").value=s.mask||"";
    $("#f-dns").value=s.dns||"";
    $("#f-appass").value=s.apPass||"";
    $("#f-instance").value=s.instance;
    $("#f-name").value=s.name||"";
    $("#f-desc").value=s.desc||"";
    $("#f-location").value=s.location||"";
    $("#f-vendorid").value=s.vendorId;
    $("#f-vendorname").value=s.vendorName||"";
    $("#f-model").value=s.model||"";
    $("#f-fw").value=s.firmware||"";
    $("#f-app").value=s.appVersion||"";
    $("#f-port").value=s.port;
    $("#f-web").checked=!!s.web;
  });
}

function post(form,data){
  return api("/api/settings",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:new URLSearchParams(data)})
    .then(function(r){
      if(!r.ok){toast("Save failed");return;}
      if(r.reboot){toast("Saved - restarting...");}
      else{toast("Saved");}
      return r;
    })
    .then(function(r){if(r&&!r.reboot){return loadSettings();}});
}

function saveNetwork(){
  post("network",{
    ssid:$("#f-ssid").value,
    pass:$("#f-pass").value,
    hostname:$("#f-host").value,
    static:$("#f-static").checked?"1":"0",
    ip:$("#f-ip").value,
    gw:$("#f-gw").value,
    mask:$("#f-mask").value,
    dns:$("#f-dns").value,
    apPass:$("#f-appass").value
  });
}

function saveDevice(){
  post("device",{
    instance:$("#f-instance").value,
    name:$("#f-name").value,
    desc:$("#f-desc").value,
    location:$("#f-location").value,
    vendorId:$("#f-vendorid").value,
    vendorName:$("#f-vendorname").value,
    model:$("#f-model").value,
    fw:$("#f-fw").value,
    app:$("#f-app").value,
    port:$("#f-port").value,
    web:$("#f-web").checked?"1":"0"
  });
}

function rebootDevice(){
  if(!confirm("Restart the device?"))return;
  api("/api/reboot",{method:"POST"}).then(function(){toast("Restarting...");});
}

function factoryReset(){
  if(!confirm("Erase all settings and start the setup access point?"))return;
  api("/api/factory-reset",{method:"POST"}).then(function(){toast("Resetting...");});
}

loadStatus();loadSettings();loadPoints();
setInterval(loadStatus,3000);
setInterval(loadPoints,5000);
</script>
</body>
</html>)rawliteral";

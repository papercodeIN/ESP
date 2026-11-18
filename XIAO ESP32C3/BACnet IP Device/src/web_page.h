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
:root{--bg:#0a0f14;--panel:#10171f;--panel2:#0b1218;--line:#1e2a35;--line2:#2a3a49;--txt:#dde7ef;--dim:#8298aa;--faint:#5d7182;--accent:#35d0a5;--warn:#e8a33d;--bad:#e5534b;--mono:ui-monospace,"Cascadia Mono",Consolas,Menlo,monospace}
*{box-sizing:border-box}
body{margin:0;font:14px/1.45 system-ui,"Segoe UI",Roboto,sans-serif;background:var(--bg);color:var(--txt)}
.topbar{position:sticky;top:0;z-index:5;background:linear-gradient(180deg,#121b25,#0c131b);border-bottom:1px solid var(--line)}
header{display:flex;align-items:center;gap:16px;padding:10px 18px}
.brand{min-width:0}
.brand .tag{font-size:10px;letter-spacing:.16em;color:var(--faint);text-transform:uppercase;white-space:nowrap}
h1{font-size:15px;margin:2px 0 0;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.hmetrics{display:flex;gap:22px;margin-left:auto;flex:none}
.hm{text-align:right}
.hm .k{font-size:9.5px;letter-spacing:.12em;color:var(--faint);text-transform:uppercase}
.hm .v{font-family:var(--mono);font-size:13px;font-variant-numeric:tabular-nums;color:var(--txt);margin-top:1px}
.chip{flex:none;display:inline-flex;align-items:center;gap:7px;font-size:12px;padding:5px 12px;border-radius:999px;border:1px solid var(--line2);background:#152029;color:var(--dim)}
.chip.ok{border-color:rgba(53,208,165,.4);background:rgba(53,208,165,.08);color:var(--accent)}
.chip.ap{border-color:rgba(232,163,61,.4);background:rgba(232,163,61,.08);color:var(--warn)}
.chip.bad{border-color:rgba(229,83,75,.4);background:rgba(229,83,75,.08);color:var(--bad)}
.led{width:8px;height:8px;border-radius:50%;background:#3a4c5c;flex:none}
.led.ok{background:var(--accent);box-shadow:0 0 7px rgba(53,208,165,.75)}
.led.ap{background:var(--warn);box-shadow:0 0 7px rgba(232,163,61,.75)}
.led.bad{background:var(--bad);box-shadow:0 0 7px rgba(229,83,75,.75)}
nav{display:flex;gap:6px;padding:4px 14px;overflow-x:auto}
nav button{background:transparent;color:var(--dim);border:0;border-bottom:2px solid transparent;padding:8px 12px;cursor:pointer;font-size:13px;white-space:nowrap;letter-spacing:.02em}
nav button:hover{color:var(--txt)}
nav button.active{color:var(--txt);border-bottom-color:var(--accent)}
main{padding:16px 18px 30px;max-width:1120px;margin:0 auto}
section{display:none}
section.active{display:block}
.card{background:linear-gradient(180deg,#101820,#0d141b);border:1px solid var(--line);border-radius:10px;padding:14px 16px;margin-bottom:14px}
h2{display:flex;align-items:center;gap:8px;font-size:11px;text-transform:uppercase;letter-spacing:.14em;color:var(--dim);margin:0 0 10px;font-weight:600}
h2::before{content:"";width:3px;height:11px;background:var(--accent);border-radius:1px;flex:none}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:0 20px}
.row{display:flex;justify-content:space-between;align-items:baseline;gap:12px;padding:6px 0;border-bottom:1px solid #16212b;font-size:13px}
.row:last-child{border-bottom:0}
.row span:first-child{color:var(--dim)}
.row .v{font-family:var(--mono);font-variant-numeric:tabular-nums;font-size:12.5px;text-align:right;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;max-width:60%}
label{display:block;font-size:11px;color:var(--dim);margin-bottom:4px;letter-spacing:.04em}
input,select{width:100%;padding:8px 10px;border-radius:6px;border:1px solid var(--line);background:#0a1118;color:var(--txt);font-size:13px;font-family:inherit}
input:focus,select:focus{outline:0;border-color:var(--accent)}
input[type=checkbox]{width:auto;accent-color:var(--accent)}
.field{margin-bottom:10px}
.inline{display:flex;align-items:center;gap:8px;font-size:13px;color:var(--dim);margin-bottom:10px}
button{font-family:inherit}
button.primary{background:var(--accent);color:#04231a;border:0;padding:9px 16px;border-radius:6px;font-weight:600;cursor:pointer;font-size:13px}
button.primary:hover{filter:brightness(1.08)}
button.ghost{background:#16222d;color:var(--txt);border:1px solid var(--line2);padding:7px 13px;border-radius:6px;cursor:pointer;font-size:13px}
button.ghost:hover{border-color:#3a4f61}
button.danger{background:transparent;color:var(--bad);border:1px solid rgba(229,83,75,.5);padding:7px 13px;border-radius:6px;cursor:pointer;font-size:13px}
button.danger:hover{background:rgba(229,83,75,.08)}
button.small{padding:4px 9px;font-size:12px}
table{width:100%;border-collapse:collapse;font-size:13px}
th,td{padding:7px 8px;text-align:left;border-bottom:1px solid #16212b;white-space:nowrap}
th{color:var(--faint);font-weight:600;font-size:10.5px;text-transform:uppercase;letter-spacing:.1em}
tbody tr:hover{background:rgba(255,255,255,.02)}
td input,td select{padding:5px 7px;font-size:12.5px;min-width:70px}
#toast{position:fixed;left:50%;bottom:24px;transform:translate(-50%,20px);background:#17232e;border:1px solid var(--line2);padding:10px 18px;border-radius:8px;opacity:0;transition:.25s;pointer-events:none;z-index:20;max-width:90vw;font-size:13px}
#toast.show{opacity:1;transform:translate(-50%,0)}
.muted{color:var(--dim);font-size:12px}
.actions{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
code{background:#0a1118;border:1px solid var(--line);padding:1px 6px;border-radius:4px;font-size:11.5px;font-family:var(--mono);color:#b9cbda}
.stgrid{display:grid;grid-template-columns:minmax(0,5fr) minmax(0,7fr);gap:14px;align-items:start}
.stgrid .card{margin-bottom:0}
.two{display:grid;grid-template-columns:minmax(0,3fr) minmax(0,2fr);gap:14px;align-items:start}
.two .card{margin-bottom:0}
.maint{display:flex;align-items:center;justify-content:space-between;gap:14px;flex-wrap:wrap}
.maint .muted{margin:4px 0 0}
.tiles{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px}
.tile{background:#0b1218;border:1px solid var(--line);border-radius:8px;padding:10px 12px}
.tile .tk{display:flex;align-items:center;gap:7px;font-size:10.5px;letter-spacing:.1em;color:var(--faint);text-transform:uppercase;font-weight:600}
.tile .tv{font-family:var(--mono);font-size:19px;margin-top:6px;color:var(--txt);font-variant-numeric:tabular-nums;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.tile .tv .u{font-size:11.5px;color:var(--dim);margin-left:3px}
.tile .tn{font-size:11px;color:var(--dim);margin-top:3px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
@media(max-width:920px){.stgrid,.two{grid-template-columns:1fr}.hmetrics .hm.opt{display:none}}
@media(max-width:640px){.hmetrics{display:none}}
</style>
</head>
<body>
<div class="topbar">
<header>
  <div class="brand"><div class="tag">BACnet/IP Field Device</div><h1 id="devname">BACnet/IP Device</h1></div>
  <div class="hmetrics">
    <div class="hm"><div class="k">IP</div><div class="v" id="hm-ip">-</div></div>
    <div class="hm opt"><div class="k">Signal</div><div class="v" id="hm-rssi">-</div></div>
    <div class="hm opt"><div class="k">Uptime</div><div class="v" id="hm-up">-</div></div>
    <div class="hm opt"><div class="k">Memory</div><div class="v" id="hm-heap">-</div></div>
  </div>
  <span class="chip" id="conn">loading</span>
</header>
<nav>
<button class="active" data-tab="status">Status</button>
<button data-tab="network">Network</button>
<button data-tab="device">BACnet</button>
<button data-tab="gpio">GPIO</button>
<button data-tab="memory">Memory</button>
<button data-tab="points">Points</button>
</nav>
</div>
<main>

<section id="tab-status" class="active">
  <div class="stgrid">
    <div class="card">
      <h2>Device</h2>
      <div class="grid">
        <div>
          <div class="row"><span>Mode</span><span class="v" id="s-mode">-</span></div>
          <div class="row"><span>Wi-Fi</span><span class="v" id="s-ssid">-</span></div>
          <div class="row"><span>IP address</span><span class="v" id="s-ip">-</span></div>
          <div class="row"><span>Signal</span><span class="v" id="s-rssi">-</span></div>
          <div class="row"><span>Host name</span><span class="v" id="s-host">-</span></div>
        </div>
        <div>
          <div class="row"><span>Device instance</span><span class="v" id="s-instance">-</span></div>
          <div class="row"><span>BACnet port</span><span class="v" id="s-port">-</span></div>
          <div class="row"><span>Firmware</span><span class="v" id="s-fw">-</span></div>
          <div class="row"><span>Uptime</span><span class="v" id="s-uptime">-</span></div>
          <div class="row"><span>Free memory</span><span class="v" id="s-heap">-</span></div>
        </div>
      </div>
      <p class="muted" style="margin:10px 0 0">Discovery works from YABE and any BACnet/IP client on the same subnet. This page is also reachable at <code id="s-url">-</code></p>
    </div>
    <div class="card">
      <h2>Live values</h2>
      <div id="live" class="muted">Loading...</div>
    </div>
  </div>
  <div class="card maint">
    <div>
      <h2>Maintenance</h2>
      <p class="muted">Factory reset erases Wi-Fi and BACnet settings and starts the setup access point.</p>
    </div>
    <div class="actions">
      <button class="ghost" onclick="rebootDevice()">Restart device</button>
      <button class="danger" onclick="factoryReset()">Factory reset</button>
    </div>
  </div>
</section>

<section id="tab-network">
  <div class="card">
    <h2>Wi-Fi network</h2>
    <div class="grid">
      <div>
        <div class="field"><label>Network name (SSID)</label><input id="f-ssid" maxlength="32" autocomplete="off"></div>
        <div class="field"><label>Password</label><input id="f-pass" type="password" maxlength="64" autocomplete="new-password" placeholder="Leave empty to keep the current password"></div>
        <div class="field"><label>Host name (mDNS)</label><input id="f-host" maxlength="32"><span class="muted">The UI is also reachable at <code>http://HOSTNAME.local</code></span></div>
      </div>
      <div>
        <div class="inline"><input id="f-static" type="checkbox"><label for="f-static" style="margin:0">Use a fixed IP address</label></div>
        <div class="field"><label>IP address</label><input id="f-ip" maxlength="15"></div>
        <div class="field"><label>Gateway</label><input id="f-gw" maxlength="15"></div>
        <div class="field"><label>Subnet mask</label><input id="f-mask" maxlength="15"></div>
        <div class="field"><label>DNS server</label><input id="f-dns" maxlength="15"></div>
      </div>
    </div>
    <div class="field" style="max-width:420px"><label>Setup access point password</label><input id="f-appass" maxlength="64" placeholder="Empty = open network"></div>
    <div class="actions"><button class="primary" onclick="saveNetwork()">Save and restart</button></div>
    <p class="muted" style="margin:10px 0 0">If the device cannot join the network it starts its own access point so you can always reach this page at <code>http://192.168.4.1</code>.</p>
  </div>
</section>

<section id="tab-device">
  <div class="two">
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
      <div class="actions"><button class="primary" onclick="saveDevice()">Save and restart</button></div>
    </div>
    <div class="card">
      <h2>Services</h2>
      <div class="grid">
        <div class="field"><label>BACnet/IP UDP port</label><input id="f-port" type="number" min="1" max="65535"></div>
        <div class="field"><label>Web interface port</label><input value="80" disabled></div>
      </div>
      <div class="inline"><input id="f-web" type="checkbox"><label for="f-web" style="margin:0">Enable the web interface on the local network</label></div>
      <p class="muted" style="margin:0 0 12px">The device answers Who-Is, ReadProperty, ReadPropertyMultiple, WriteProperty and SubscribeCOV (change-of-value notifications). Change the UDP port only if your site requires it (default 47808 / 0xBAC0).</p>
    </div>
  </div>
</section>

<section id="tab-gpio">
  <div class="card">
    <h2>GPIO Point Mapping</h2>
    <div class="muted" style="margin-bottom:10px">Assign spare header pins to BACnet Binary Input (BI) or Binary Output (BO) points. Built-in points BO:0 (D0 / GPIO2) and BI:1 (D9 / BOOT) are not shown here. Changes require a device restart.</div>
    <div style="overflow-x:auto">
      <table>
        <thead><tr><th>Enable</th><th>Pin</th><th>Type</th><th>Instance</th><th>Name</th></tr></thead>
        <tbody id="gpio-rows"></tbody>
      </table>
    </div>
    <div class="actions" style="margin-top:12px"><button class="primary" onclick="saveGpio()">Save and restart</button></div>
  </div>
</section>

<section id="tab-memory">
  <div class="card">
    <h2>Memory Points</h2>
    <div class="muted" style="margin-bottom:10px">Create software-only BACnet AV/BV points. Definitions are stored in flash; persistent values are saved a few seconds after a change. Changes require a restart.</div>
    <div style="overflow-x:auto">
      <table>
        <thead><tr><th>Enable</th><th>Type</th><th>Instance</th><th>Name</th><th>Access</th><th>Storage</th><th>Default</th><th>Min</th><th>Max</th></tr></thead>
        <tbody id="memory-rows"></tbody>
      </table>
    </div>
    <div class="actions" style="margin-top:12px"><button class="primary" onclick="saveMemory()">Save and restart</button></div>
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
function fmtUptime(s){s=Math.max(0,Math.floor(s||0));var d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60),x=s%60,p=function(n){return(n<10?"0":"")+n;};return(d?d+"d ":"")+p(h)+":"+p(m)+":"+p(x);}
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
    var state=s.portal?"ap":(s.connected?"ok":"bad");
    c.innerHTML='<span class="led '+state+'"></span>'+(s.portal?"Setup AP":(s.connected?"Online":"Offline"));
    c.className="chip "+state;
    $("#hm-ip").textContent=s.ip||"-";
    $("#hm-rssi").textContent=s.connected?s.rssi+" dBm":"--";
    $("#hm-up").textContent=fmtUptime(s.uptime);
    $("#hm-heap").textContent=Math.round(s.heap/1024)+" KB";
    $("#s-mode").textContent=s.mode;
    $("#s-ssid").textContent=s.ssid||"-";
    $("#s-ip").textContent=s.ip||"-";
    $("#s-rssi").textContent=s.connected?s.rssi+" dBm":"-";
    $("#s-host").textContent=s.hostname?"http://"+s.hostname+".local":"-";
    $("#s-instance").textContent=s.instance;
    $("#s-port").textContent=s.port+" (UDP)";
    $("#s-fw").textContent=s.firmware;
    $("#s-uptime").textContent=fmtUptime(s.uptime);
    $("#s-heap").textContent=Math.round(s.heap/1024)+" KB";
    $("#s-url").textContent=s.portal?"http://192.168.4.1/":"http://"+(s.ip||"device")+"/";
  });
}

function renderLive(){
  var box=$("#live");
  if(!POINTS.length){box.className="muted";box.textContent="No points.";return;}
  box.className="tiles";
  box.innerHTML=POINTS.map(function(p){
    var binary=(p.k==="BI"||p.k==="BO"||p.k==="BV");
    var on=binary?(p.v!==0):true;
    var val=binary?(p.v?"ACTIVE":"INACTIVE"):fmt(p.v,1)+'<span class="u">'+unit(p.u)+'</span>';
    return '<div class="tile">'+
           '<div class="tk"><span class="led '+(on?"ok":"")+'"></span>'+p.k+':'+p.i+'</div>'+
           '<div class="tv">'+val+'</div>'+
           '<div class="tn" title="'+esc(p.n)+'">'+esc(p.n)+'</div>'+
           '</div>';
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
        html+='<td><select id="val-'+p.t+'-'+p.i+'"><option value="1"'+(p.v?' selected':'')+'>Active (1)</option><option value="0"'+(!p.v?' selected':'')+'>Inactive (0)</option></select></td>';
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
    if(p.c&&!isAnalog){var s=document.getElementById("val-"+p.t+"-"+p.i);if(s)s.value=p.v?"1":"0";}
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
        /* Do not replace a user's in-progress binary selection during the
         * five-second polling refresh. The next refresh after Write will
         * still synchronize it with the BACnet present value. */
        if(document.activeElement!==el){el.value=p.v?"1":"0";}
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
    $("#f-appass").value="";
    $("#f-appass").placeholder=s.hasApPass?"Stored password - leave empty to keep":"Empty = open setup AP";
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

var GPIO_POINTS=[];
var MEMORY_POINTS=[];

function loadMemory(){
  return api("/api/memory").then(function(data){
    MEMORY_POINTS=data.points||[];
    var body=$("#memory-rows");
    body.innerHTML="";
    MEMORY_POINTS.forEach(function(mp,i){
      var tr=document.createElement("tr");
      tr.innerHTML=
        '<td><input type="checkbox" id="me-'+i+'"'+(mp.enabled?" checked":"")+'></td>'+
        '<td><select id="mk-'+i+'"><option value="0"'+(mp.kind===0?" selected":"")+'>Binary (BV)</option><option value="1"'+(mp.kind===1?" selected":"")+'>Analog (AV)</option></select></td>'+
        '<td><input type="number" id="mi-'+i+'" min="0" max="4194302" value="'+mp.instance+'" style="width:100px"></td>'+
        '<td><input id="mn-'+i+'" maxlength="32" value="'+esc(mp.name||"")+'" placeholder="Memory point name"></td>'+
        '<td><select id="mw-'+i+'"><option value="0"'+(!mp.writable?" selected":"")+'>Read-only</option><option value="1"'+(mp.writable?" selected":"")+'>Writable</option></select></td>'+
        '<td><select id="mp-'+i+'"><option value="0"'+(!mp.persistent?" selected":"")+'>Volatile</option><option value="1"'+(mp.persistent?" selected":"")+'>Persistent</option></select></td>'+
        '<td><input type="number" step="0.1" id="md-'+i+'" value="'+mp.defaultValue+'" style="width:90px"></td>'+
        '<td><input type="number" step="0.1" id="mmin-'+i+'" value="'+mp.minValue+'" style="width:80px"></td>'+
        '<td><input type="number" step="0.1" id="mmax-'+i+'" value="'+mp.maxValue+'" style="width:80px"></td>';
      body.appendChild(tr);
    });
  });
}

function saveMemory(){
  var data={};
  MEMORY_POINTS.forEach(function(mp,i){
    data["enabled_"+i]=$("#me-"+i).checked?"1":"0";
    data["kind_"+i]=$("#mk-"+i).value;
    data["instance_"+i]=$("#mi-"+i).value;
    data["name_"+i]=$("#mn-"+i).value;
    data["writable_"+i]=$("#mw-"+i).value;
    data["persistent_"+i]=$("#mp-"+i).value;
    data["default_"+i]=$("#md-"+i).value;
    data["min_"+i]=$("#mmin-"+i).value;
    data["max_"+i]=$("#mmax-"+i).value;
  });
  fetch("/api/memory",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:new URLSearchParams(data)})
    .then(function(r){return r.json();})
    .then(function(r){toast(r.ok?"Saved - restarting...":(r.error||"Save failed"));})
    .catch(function(){toast("Save failed");});
}

function loadGpio(){
  return api("/api/gpio").then(function(data){
    GPIO_POINTS=data.points||[];
    var body=$("#gpio-rows");
    body.innerHTML="";
    GPIO_POINTS.forEach(function(gp,i){
      var tr=document.createElement("tr");
      tr.innerHTML=
        '<td><input type="checkbox" id="ge-'+i+'"'+(gp.enabled?" checked":"")+'></td>'+
        '<td><code>'+esc(gp.label)+' / GPIO'+gp.pin+'</code></td>'+
        '<td><select id="gk-'+i+'"><option value="0"'+(gp.kind===0?" selected":"")+'>Digital Input (BI)</option><option value="1"'+(gp.kind===1?" selected":"")+'>Digital Output (BO)</option></select></td>'+
        '<td><input type="number" id="gi-'+i+'" min="0" max="4194302" value="'+gp.instance+'" style="width:100px"></td>'+
        '<td><input id="gn-'+i+'" maxlength="32" value="'+esc(gp.name||"")+'" placeholder="Auto-generated"></td>';
      body.appendChild(tr);
    });
  });
}

function saveGpio(){
  var data={};
  GPIO_POINTS.forEach(function(gp,i){
    data["enabled_"+i]=$("#ge-"+i).checked?"1":"0";
    data["kind_"+i]=$("#gk-"+i).value;
    data["instance_"+i]=$("#gi-"+i).value;
    data["name_"+i]=$("#gn-"+i).value;
    data["activeLow_"+i]=gp.activeLow?"1":"0";
  });
  fetch("/api/gpio",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:new URLSearchParams(data)})
    .then(function(r){return r.json();})
    .then(function(r){
      if(!r.ok){toast(r.error||"Save failed");return;}
      toast("Saved - restarting...");
    })
    .catch(function(){toast("Saved - restarting...");});
}

loadStatus();loadSettings();loadPoints();loadGpio();loadMemory();
setInterval(loadStatus,3000);
setInterval(loadPoints,5000);
</script>
</body>
</html>)rawliteral";

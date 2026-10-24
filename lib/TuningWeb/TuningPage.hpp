/**
 * @file   TuningPage.hpp
 * @brief  The tuning web page served at "/". Self contained (no internet on the access point).
 */
#pragma once

static const char TUNING_PAGE[] = R"html(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0d1117">
<title>BiCopter Tuning</title>
<style>
:root{--bg:#0d1117;--panel:#161b22;--panel2:#1c232c;--line:#2a323d;--text:#e6edf3;--muted:#8b97a5;
--accent:#3b82f6;--accent2:#1d4ed8;--ok:#22c55e;--warn:#f59e0b;--bad:#ef4444;--roll:#60a5fa;--pitch:#f59e0b;
--sky1:#1e5fa8;--sky2:#5aa0e0;--gnd1:#8a5a2b;--gnd2:#5c3b1c;--sym:#facc15;--radius:14px}
@media (prefers-color-scheme:light){:root{--bg:#eef1f5;--panel:#fff;--panel2:#f5f7fa;--line:#dde2e9;--text:#111827;
--muted:#5b6574;--roll:#2563eb;--pitch:#d97706;--sym:#eab308}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
.num{font-variant-numeric:tabular-nums}
.muted{color:var(--muted)}
/* header */
header{position:sticky;top:0;z-index:5;background:color-mix(in srgb,var(--panel) 88%,transparent);
backdrop-filter:blur(10px);border-bottom:1px solid var(--line)}
.hbar{max-width:1180px;margin:0 auto;display:flex;align-items:center;gap:12px;padding:10px 16px}
.brand{display:flex;align-items:center;gap:10px;flex:1;min-width:0}
.logo{width:34px;height:34px;border-radius:9px;background:linear-gradient(135deg,var(--accent),#8b5cf6);display:grid;place-items:center}
.brand b{font-size:16px;display:block;line-height:1.1}.brand small{color:var(--muted);font-size:12px}
.chips{display:flex;gap:8px;align-items:center}
.chip{font-size:12px;font-weight:600;padding:4px 10px;border-radius:99px;background:var(--panel2);border:1px solid var(--line);white-space:nowrap}
.state.DISARMED{color:var(--ok);border-color:color-mix(in srgb,var(--ok) 45%,transparent)}
.state.ARMED{color:#fff;background:var(--bad);border-color:var(--bad)}
.state.FAILSAFE,.state.FAULT,.state.OFFLINE{color:var(--warn);border-color:color-mix(in srgb,var(--warn) 45%,transparent)}
.dot{width:8px;height:8px;border-radius:50%;background:var(--ok);display:inline-block;margin-right:6px;vertical-align:1px}
.dot.off{background:var(--bad)}
/* layout */
main{max-width:1180px;margin:0 auto;padding:14px 16px 110px;display:grid;gap:14px;grid-template-columns:1fr}
@media (min-width:900px){main{grid-template-columns:1.15fr 1fr}.span2{grid-column:1/-1}}
.card{background:var(--panel);border:1px solid var(--line);border-radius:var(--radius);padding:16px;min-width:0}
.card h2{margin:0;font-size:15px;display:flex;align-items:center;gap:8px}
.card h2 .tag{margin-left:auto;font-size:12px;font-weight:500;color:var(--muted)}
.head{display:flex;align-items:center;margin-bottom:12px}
.hint{font-size:13px;color:var(--muted);margin:0 0 12px}
.alert{border-radius:12px;padding:10px 14px;font-size:14px;display:none;align-items:center;gap:10px}
.alert.show{display:flex}
.alert.warn{background:color-mix(in srgb,var(--warn) 14%,transparent);border:1px solid color-mix(in srgb,var(--warn) 40%,transparent)}
.alert.bad{background:color-mix(in srgb,var(--bad) 14%,transparent);border:1px solid color-mix(in srgb,var(--bad) 40%,transparent)}
/* attitude */
.att{display:grid;grid-template-columns:minmax(0,260px) 1fr;gap:16px;align-items:center}
@media (max-width:560px){.att{grid-template-columns:1fr}.att svg.hz{max-width:260px;margin:0 auto}}
svg.hz{width:100%;height:auto;display:block}
.read{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.tile{background:var(--panel2);border:1px solid var(--line);border-radius:12px;padding:10px 12px}
.tile .k{font-size:12px;color:var(--muted);display:flex;align-items:center;gap:6px}
.tile .v{font-size:28px;font-weight:700;line-height:1.15}
.tile .s{font-size:12px;color:var(--muted)}
.sw{width:10px;height:3px;border-radius:2px;display:inline-block}
.kv{display:flex;justify-content:space-between;gap:8px;font-size:13px;padding:7px 0;border-top:1px solid var(--line)}
.kv:first-child{border-top:0}
canvas{width:100%;height:150px;display:block;margin-top:14px;background:var(--panel2);border:1px solid var(--line);border-radius:12px}
.legend{display:flex;gap:14px;font-size:12px;color:var(--muted);margin-top:6px}
/* buttons + inputs */
button{font:inherit;font-weight:600;font-size:14px;border:1px solid var(--line);background:var(--panel2);color:var(--text);
border-radius:10px;padding:9px 14px;cursor:pointer;transition:filter .15s,transform .05s}
button:hover{filter:brightness(1.12)}button:active{transform:scale(.97)}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff}
button.ghost{background:transparent}
button.danger{color:var(--bad)}
button:disabled{opacity:.4;pointer-events:none}
button.wide{width:100%;padding:12px}
input{font:inherit;color:var(--text);background:var(--bg);border:1px solid var(--line);border-radius:10px;padding:9px 10px;
min-width:0;width:100%;font-variant-numeric:tabular-nums;appearance:textfield;-moz-appearance:textfield}
input:focus{outline:2px solid color-mix(in srgb,var(--accent) 60%,transparent);border-color:var(--accent)}
input::-webkit-outer-spin-button,input::-webkit-inner-spin-button{-webkit-appearance:none;margin:0}
input.changed{border-color:var(--warn);box-shadow:0 0 0 1px var(--warn) inset}
label{display:block;font-size:12px;color:var(--muted);margin-bottom:5px}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px}
/* motors */
.motors{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.motor{background:var(--panel2);border:1px solid var(--line);border-radius:12px;padding:12px;text-align:center}
.motor svg{width:100%;max-width:170px;height:auto}
.motor .v{font-size:26px;font-weight:700}
.motor{min-width:0;padding:12px 10px}
.step{display:grid;grid-template-columns:repeat(4,1fr);gap:4px;margin-top:10px}
.step button{padding:8px 0;min-width:0;font-size:13px}
.step input{grid-column:1/-1;grid-row:1;text-align:center;padding:8px 2px}
/* pid */
.seg{display:grid;grid-template-columns:repeat(3,1fr);background:var(--panel2);border:1px solid var(--line);border-radius:11px;padding:3px;gap:3px}
.seg button{border:0;background:transparent;padding:8px;border-radius:8px;color:var(--muted)}
.seg button.on{background:var(--accent);color:#fff}
.seg button .d{width:6px;height:6px;border-radius:50%;background:var(--warn);display:none;margin-left:6px;vertical-align:2px}
.seg button.dirty .d{display:inline-block}
.axisnote{font-size:13px;color:var(--muted);margin:10px 0 4px}
.gain{display:grid;grid-template-columns:1fr minmax(0,130px) auto auto;gap:6px;align-items:center;padding:9px 0;border-top:1px solid var(--line)}
.gain:first-of-type{border-top:0}
.gain .t b{display:block;font-size:14px}.gain .t span{font-size:12px;color:var(--muted)}
.gain input{text-align:right}
.gain button{padding:8px 9px;font-size:12px}
/* log */
#log{list-style:none;margin:0;padding:0;max-height:180px;overflow:auto}
#log li{font-size:13px;padding:6px 0;border-top:1px solid var(--line);display:flex;gap:10px}
#log li:first-child{border-top:0}
#log time{color:var(--muted);font-variant-numeric:tabular-nums;flex:none}
/* save bar + toast */
#savebar{position:fixed;left:0;right:0;bottom:0;z-index:6;transform:translateY(110%);transition:transform .25s;
background:var(--panel);border-top:1px solid var(--line);padding:12px 16px calc(12px + env(safe-area-inset-bottom))}
#savebar.show{transform:none}
#savebar .in{max-width:1180px;margin:0 auto;display:flex;align-items:center;gap:12px}
#savebar .in span{flex:1;font-size:14px}
#toast{position:fixed;left:50%;top:70px;transform:translate(-50%,-10px);z-index:7;max-width:92%;padding:10px 16px;border-radius:12px;
background:#111827;color:#fff;font-size:14px;opacity:0;transition:.2s;pointer-events:none;box-shadow:0 8px 24px #0006}
#toast.show{opacity:1;transform:translate(-50%,0)}#toast.err{background:var(--bad)}
</style></head><body>
<header><div class="hbar">
 <div class="brand"><div class="logo"><svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round">
 <path d="M3 13h18"/><path d="M12 13v4"/><path d="M1 8h6M17 8h6"/><path d="M4 8v5M20 8v5"/></svg></div>
 <div><b>BiCopter</b><small>Tuning &amp; calibration</small></div></div>
 <div class="chips"><span class="chip num" id="hz"><span class="dot off" id="dot"></span>–</span><span class="chip state" id="state">…</span></div>
</div></header>

<main>
<div class="alert bad span2" id="armedAlert">⚠ <span><b>Armed.</b> Live view only, every change is refused until you disarm.</span></div>
<div class="alert warn span2" id="offAlert">⚠ <span>No connection to the board. Is your phone still on the <b>BiCopter</b> WiFi?</span></div>

<section class="card span2">
 <div class="head"><h2>Attitude <span class="tag" id="accTag"></span></h2></div>
 <div class="att">
  <svg class="hz" viewBox="-120 -120 240 240" aria-label="artificial horizon">
   <defs><clipPath id="clip"><circle r="104"/></clipPath>
    <linearGradient id="sky" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="var(--sky1)"/><stop offset="1" stop-color="var(--sky2)"/></linearGradient>
    <linearGradient id="gnd" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="var(--gnd1)"/><stop offset="1" stop-color="var(--gnd2)"/></linearGradient></defs>
   <circle r="118" fill="var(--panel2)" stroke="var(--line)"/>
   <g clip-path="url(#clip)"><g id="horizon">
    <rect x="-400" y="-400" width="800" height="400" fill="url(#sky)"/><rect x="-400" y="0" width="800" height="400" fill="url(#gnd)"/>
    <line x1="-400" x2="400" stroke="#fff" stroke-width="1.5"/><g id="ladder" stroke="#fff" fill="#fff" font-size="9" text-anchor="middle"></g>
   </g></g>
   <circle r="104" fill="none" stroke="var(--line)" stroke-width="2"/>
   <g id="rollScale" stroke="var(--text)" stroke-width="1.5"></g>
   <g id="rollPtr"><path d="M0 -102 l-6 11 h12z" fill="var(--sym)"/></g>
   <g fill="var(--sym)" stroke="#000" stroke-width="1">
    <rect x="-64" y="-2.5" width="128" height="5" rx="2"/><rect x="-74" y="-10" width="16" height="9" rx="2"/><rect x="58" y="-10" width="16" height="9" rx="2"/>
    <circle r="5"/></g>
  </svg>
  <div>
   <div class="read">
    <div class="tile"><div class="k"><span class="sw" style="background:var(--roll)"></span>Roll</div><div class="v num" id="roll">–</div><div class="s">+ = right side down</div></div>
    <div class="tile"><div class="k"><span class="sw" style="background:var(--pitch)"></span>Pitch</div><div class="v num" id="pitch">–</div><div class="s">+ = nose down</div></div>
   </div>
   <div style="margin-top:10px">
    <div class="kv"><span class="muted">Raw sensor (before level offset)</span><span class="num" id="raw">–</span></div>
    <div class="kv"><span class="muted">Gyro roll / pitch / yaw</span><span class="num" id="gyro">–</span></div>
    <div class="kv"><span class="muted">Throttle · ESC L / R</span><span class="num" id="esc">–</span></div>
    <div class="kv"><span class="muted">Hover balance (learned in flight)</span><span class="num" id="hover">–</span></div>
   </div>
  </div>
 </div>
 <canvas id="chart"></canvas>
 <div class="legend"><span><span class="sw" style="background:var(--roll)"></span> roll</span><span><span class="sw" style="background:var(--pitch)"></span> pitch</span><span style="margin-left:auto">last 15 s</span></div>
</section>

<section class="card">
 <div class="head"><h2>Level calibration</h2></div>
 <p class="hint">Stand the model on a flat surface (check with a bubble level), then press the button. The raw
 sensor angle becomes "level" and is saved straight away.</p>
 <button class="primary wide ctl" onclick="act('levelNow')">Set current attitude as level</button>
 <div class="row2" style="margin-top:14px">
  <div><label for="lvR">Roll offset (°)</label><input id="lvR" class="ctl" type="number" step="0.1" inputmode="decimal"></div>
  <div><label for="lvP">Pitch offset (°)</label><input id="lvP" class="ctl" type="number" step="0.1" inputmode="decimal"></div>
 </div>
 <div class="actions"><button class="ctl" onclick="sendLevel()">Apply offsets</button></div>
 <h2 style="margin:16px 0 4px;font-size:14px">Drift trim</h2>
 <p class="hint">Hovers moving <b>backwards</b>? Press "nose down". Moving <b>forward</b>? "nose up". 0.5&deg; per press,
 applied at once; press Save to flash when it stays in place. Sideways drift: use the roll buttons.</p>
 <div class="row2">
  <button class="ctl" onclick="nudgeLevel(1,+0.5)">&#9660; Nose down 0.5&deg;</button>
  <button class="ctl" onclick="nudgeLevel(1,-0.5)">&#9650; Nose up 0.5&deg;</button>
  <button class="ctl" onclick="nudgeLevel(0,-0.5)">&#9664; Roll left 0.5&deg;</button>
  <button class="ctl" onclick="nudgeLevel(0,+0.5)">Roll right 0.5&deg; &#9654;</button>
 </div>
</section>

<section class="card">
 <div class="head"><h2>Motor tilt <span class="tag">side view, nose →</span></h2></div>
 <p class="hint">Model level, sticks centred: trim each servo until its motor stands at 0°. Applies instantly.
 1° ≈ <span id="upd">10</span> µs.</p>
 <div class="motors" id="motors"></div>
</section>

<section class="card">
 <div class="head"><h2>PID gains</h2></div>
 <div class="seg" id="seg"></div>
 <p class="axisnote" id="axisNote"></p>
 <div id="gains"></div>
 <div class="actions"><button class="primary ctl" onclick="sendGains()">Apply all axes</button>
  <button class="ghost ctl" onclick="loadGains();toast('reloaded from board')">Revert</button></div>
</section>

<section class="card">
 <div class="head"><h2>Flight recorder <span class="tag" id="recTag">25 samples/s</span></h2></div>
 <p class="hint">Press Record, fly, land, press Stop: your phone saves a .csv file with angles, rates, servo tilt
 and throttle. Keep the screen on and stay on the BiCopter WiFi while recording.</p>
 <div class="read">
  <div class="tile"><div class="k">Duration</div><div class="v num" id="recTime">0:00</div></div>
  <div class="tile"><div class="k">Samples</div><div class="v num" id="recN">0</div><div class="s" id="recGap"></div></div>
 </div>
 <div class="actions"><button class="primary" id="recBtn" onclick="recToggle()">● Record</button>
  <button id="recSave" onclick="recDownload()" disabled>Download .csv</button></div>
</section>

<section class="card">
 <div class="head"><h2>Board</h2></div>
 <div class="kv"><span class="muted">Arming</span><span id="block">–</span></div>
 <div class="actions"><button class="primary ctl" onclick="act('save')">Save to flash</button>
  <button class="ctl" onclick="act('gyro')">Recalibrate gyro</button>
  <button class="ctl" onclick="act('hoverReset')">Reset hover balance</button>
  <button class="danger ctl" onclick="if(confirm('Reset level, trims and gains to the Config.hpp defaults?'))act('defaults')">Restore defaults</button></div>
 <h2 style="margin:16px 0 6px;font-size:13px" class="muted">Events</h2>
 <ul id="log"></ul>
</section>
</main>

<div id="savebar"><div class="in"><span>You have <b>unsaved changes</b>. They are lost at power off.</span>
 <button class="primary ctl" onclick="act('save')">Save to flash</button></div></div>
<div id="toast"></div>

<script>
const $=id=>document.getElementById(id),NS='http://www.w3.org/2000/svg';
const AX=['Roll','Pitch','Yaw'];
const AXNOTE=['Held by the two motors (differential thrust).','Held by tilting both servos together.','Held by tilting the servos in opposite directions.'];
const GF=[{k:'rateP',n:'Rate P',h:'response to rate error',s:'0.0001',max:.05},{k:'rateI',n:'Rate I',h:'removes steady drift',s:'0.0001',max:.05},
 {k:'rateD',n:'Rate D',h:'damps overshoot',s:'0.00001',max:.01},{k:'angleP',n:'Angle P',h:'self-level strength',s:'0.1',max:15}];
let S=null,W=null,axis=0,trim=[0,0],lastEv=-1,hist=[];
const f=(v,d=1)=>(v>=0?'+':'−')+Math.abs(v).toFixed(d);
const el=(t,a)=>{const e=document.createElementNS(NS,t);for(const k in a)e.setAttribute(k,a[k]);return e};
// --- horizon decorations
(()=>{const L=$('ladder');for(let p=-40;p<=40;p+=10){if(!p)continue;const y=-p*3,w=p%20?14:24;
 L.append(el('line',{x1:-w,x2:w,y1:y,y2:y}));for(const x of[-w-11,w+11]){const t=el('text',{x,y:y+3,stroke:'none'});t.textContent=Math.abs(p);L.append(t)}}
 const R=$('rollScale');for(const a of[-60,-45,-30,-20,-10,0,10,20,30,45,60]){const r=a*Math.PI/180,l=a%30?6:11;
 R.append(el('line',{x1:Math.sin(r)*104,y1:-Math.cos(r)*104,x2:Math.sin(r)*(104+l),y2:-Math.cos(r)*(104+l)}))}})();
// --- motors
$('motors').innerHTML=['Left','Right'].map((n,i)=>`<div class="motor"><label>${n} motor</label>
<svg viewBox="-80 -70 160 90"><path d="M-62 0 A62 62 0 0 1 62 0" fill="none" stroke="var(--line)" stroke-width="6" stroke-linecap="round"/>
${[-45,-30,-15,0,15,30,45].map(a=>{const r=a*Math.PI/180;return`<line x1="${Math.sin(r)*54}" y1="${-Math.cos(r)*54}" x2="${Math.sin(r)*46}" y2="${-Math.cos(r)*46}" stroke="var(--muted)"/>`}).join('')}
<line x1="0" y1="0" x2="0" y2="-66" stroke="var(--muted)" stroke-dasharray="3 3"/>
<g id="mg${i}"><rect x="-9" y="-30" width="18" height="30" rx="4" fill="var(--accent)"/><rect x="-2" y="-40" width="4" height="10" fill="var(--text)"/>
<rect x="-40" y="-44" width="80" height="4" rx="2" fill="var(--text)" opacity=".85"/></g><circle r="5" fill="var(--text)"/>
<text x="74" y="16" font-size="10" fill="var(--muted)" text-anchor="end">nose</text></svg>
<div class="v num" id="tilt${i}">–</div><div class="muted num" style="font-size:12px" id="srv${i}"></div>
<div class="step"><button class="ctl" onclick="nudge(${i},-5)">−5</button><button class="ctl" onclick="nudge(${i},-1)">−1</button>
<input id="tr${i}" class="ctl" type="number" step="1" inputmode="numeric" onchange="setTrim(${i},this.value)">
<button class="ctl" onclick="nudge(${i},1)">+1</button><button class="ctl" onclick="nudge(${i},5)">+5</button></div>
<div class="muted num" style="font-size:12px;margin-top:6px" id="trd${i}"></div></div>`).join('');
// --- pid
$('seg').innerHTML=AX.map((n,a)=>`<button onclick="pickAxis(${a})">${n}<span class="d"></span></button>`).join('');
$('gains').innerHTML=GF.map((g,j)=>`<div class="gain"><div class="t"><b id="gn${j}">${g.n}</b><span id="gh${j}">${g.h}</span></div>
<input id="g${j}" class="ctl" type="number" min="0" step="${g.s}" inputmode="decimal" oninput="editGain(${j},this.value)">
<button class="ctl" onclick="scaleGain(${j},0.9)">−10%</button><button class="ctl" onclick="scaleGain(${j},1.1)">+10%</button></div>`).join('');
function loadGains(){if(!S)return;W=GF.map(g=>S[g.k].slice());showAxis()}
function pickAxis(a){axis=a;showAxis()}
function showAxis(){if(!W)return;[...$('seg').children].forEach((b,a)=>{b.classList.toggle('on',a==axis);
 b.classList.toggle('dirty',GF.some((g,j)=>W[j][a]!=S[g.k][a]))});
 $('axisNote').textContent=AXNOTE[axis];$('gn3').textContent=axis==2?'Heading P':'Angle P';
 $('gh3').textContent=axis==2?'heading hold, 0 = off':'self-level strength';
 GF.forEach((g,j)=>{const i=$('g'+j);if(document.activeElement!==i)i.value=+W[j][axis].toFixed(6);i.classList.toggle('changed',W[j][axis]!=S[g.k][axis])})}
function editGain(j,v){const x=parseFloat(v);if(!isNaN(x)){W[j][axis]=x;showAxis()}}
function scaleGain(j,k){let v=W[j][axis]*k;if(v==0&&k>1)v=+GF[j].s;W[j][axis]=Math.min(GF[j].max,+v.toPrecision(3));showAxis()}
// --- chart
const cv=$('chart'),cx=cv.getContext('2d');
function css(n){return getComputedStyle(document.documentElement).getPropertyValue(n).trim()}
function draw(){const r=devicePixelRatio||1,w=cv.clientWidth,h=cv.clientHeight;if(cv.width!=w*r){cv.width=w*r;cv.height=h*r}
 cx.setTransform(r,0,0,r,0,0);cx.clearRect(0,0,w,h);
 let m=10;for(const p of hist)m=Math.max(m,Math.abs(p[0]),Math.abs(p[1]));m=Math.ceil(m/5)*5;
 const y=v=>h/2-v/m*(h/2-12),X=i=>i/(75-1)*(w-36)+30;
 cx.font='11px system-ui';cx.fillStyle=css('--muted');cx.strokeStyle=css('--line');cx.lineWidth=1;
 for(const v of[m,0,-m]){cx.beginPath();cx.moveTo(30,y(v));cx.lineTo(w-6,y(v));cx.setLineDash(v?[3,4]:[]);cx.stroke();cx.fillText((v>0?'+':'')+v+'°',2,y(v)+4)}
 cx.setLineDash([]);cx.lineWidth=2;cx.lineJoin='round';
 [['--roll',0],['--pitch',1]].forEach(([c,k])=>{cx.strokeStyle=css(c);cx.beginPath();
  hist.forEach((p,i)=>{const o=75-hist.length;i?cx.lineTo(X(i+o),y(p[k])):cx.moveTo(X(i+o),y(p[k]))});cx.stroke()})}
// --- render
function render(s){const first=!S;S=s;if(first){loadGains();fillForms()}
 $('state').textContent=s.state;$('state').className='chip state '+s.state;
 $('hz').innerHTML=`<span class="dot"></span>${s.loopHz} Hz`;$('offAlert').classList.remove('show');
 $('armedAlert').classList.toggle('show',s.armed);document.querySelectorAll('.ctl').forEach(b=>b.disabled=s.armed);
 $('roll').textContent=f(s.roll)+'°';$('pitch').textContent=f(s.pitch)+'°';
 const rr=Math.max(-90,Math.min(90,s.roll)),pp=Math.max(-40,Math.min(40,s.pitch));
 $('horizon').setAttribute('transform',`rotate(${-rr}) translate(0 ${-pp*3})`);$('rollPtr').setAttribute('transform',`rotate(${-rr})`);
 $('raw').textContent=`${f(s.rawRoll)}° / ${f(s.rawPitch)}°`;$('gyro').textContent=s.gyro.map(v=>f(v)).join(' / ')+' °/s';
 $('esc').textContent=`${Math.round(s.throttle*100)} % · ${s.escUs[0]} / ${s.escUs[1]} µs`;
 $('hover').textContent=s.hoverTilt==0&&s.hoverTrim==0?'not learned yet':
  `motors ${f(s.hoverTilt)}° → body ${f(s.hoverTrim)}°`+(Math.abs(s.hoverTilt)>6?' (move CG '+(s.hoverTilt>0?'forward':'back')+')':'');
 $('accTag').textContent=s.accelUsed?'accelerometer levelling':'accelerometer ignored (moving)';
 for(let i=0;i<2;i++){$('mg'+i).setAttribute('transform',`rotate(${Math.max(-60,Math.min(60,s.tiltDeg[i]))})`);
  $('tilt'+i).textContent=f(s.tiltDeg[i])+'°';$('srv'+i).textContent=`servo ${s.servoUs[i]} µs`;
  $('trd'+i).textContent=`trim ${s.trimUs[i]} µs ≈ ${f(s.trimDeg[i])}°`}
 $('upd').textContent=s.usPerDeg;$('block').textContent=s.armBlock||'ready to arm';
 $('savebar').classList.toggle('show',s.unsaved&&!s.armed);
 if(s.eventCount!=lastEv){lastEv=s.eventCount;const li=document.createElement('li');
  li.innerHTML=`<time>${new Date().toLocaleTimeString()}</time><span></span>`;li.lastChild.textContent=s.event;$('log').prepend(li);
  while($('log').children.length>20)$('log').lastChild.remove()}
 hist.push([s.roll,s.pitch]);if(hist.length>75)hist.shift();draw();showAxis()}
function fillForms(){$('lvR').value=S.level[0].toFixed(2);$('lvP').value=S.level[1].toFixed(2);
 trim=S.trimUs.slice();$('tr0').value=trim[0];$('tr1').value=trim[1]}
let fails=0;
async function poll(){try{const r=await fetch('/api/state',{cache:'no-store'});render(await r.json());fails=0}
 catch(e){if(++fails>2){$('state').textContent='OFFLINE';$('state').className='chip state OFFLINE';
  $('hz').innerHTML='<span class="dot off"></span>–';$('offAlert').classList.add('show')}}
 setTimeout(poll,200)}
// --- actions
function toast(t,err){const e=$('toast');e.textContent=t;e.className='show'+(err?' err':'');clearTimeout(e._t);e._t=setTimeout(()=>e.className='',2600)}
async function post(url,body){try{const r=await fetch(url,{method:'POST',body:new URLSearchParams(body)});const t=await r.text();toast(t,!r.ok);return r.ok}
 catch(e){toast('no connection to the board',true);return false}}
async function act(w){if(await post('/api/action',{do:w})&&(w=='defaults'||w=='levelNow'))setTimeout(()=>{fillForms();loadGains()},350)}
function sendLevel(){post('/api/level',{roll:$('lvR').value,pitch:$('lvP').value})}
// The controller holds (raw angle - offset) at 0, so offset + 0.5 makes it hold the nose 0.5 deg further down
async function nudgeLevel(i,d){if(!S)return;const v=[S.level[0],S.level[1]];v[i]=Math.round((v[i]+d)*10)/10;
 if(await post('/api/level',{roll:v[0],pitch:v[1]})){$('lvR').value=v[0].toFixed(2);$('lvP').value=v[1].toFixed(2)}}
function setTrim(i,v){trim[i]=Math.max(-300,Math.min(300,Math.round(+v||0)));$('tr'+i).value=trim[i];post('/api/trims',{left:trim[0],right:trim[1]})}
function nudge(i,d){setTrim(i,trim[i]+d)}
async function sendGains(){const b={};for(let a=0;a<3;a++){b['p'+a]=W[0][a];b['i'+a]=W[1][a];b['d'+a]=W[2][a];b['a'+a]=W[3][a]}
 if(await post('/api/gains',b))setTimeout(loadGains,350)}
// --- flight recorder
const REC_HEAD='seq,ms,state,throttle,roll_deg,pitch_deg,gyro_roll_dps,gyro_pitch_dps,gyro_yaw_dps,set_roll_dps,set_pitch_dps,'+
 'set_yaw_dps,cmd_roll,cmd_pitch,cmd_yaw,yaw_i,hover_tilt_deg,hover_trim_deg,servo_l_us,servo_r_us,esc_l_us,esc_r_us,accel_used\n';
let rec=null,recLast=0,recLost=0,recStart=0;
function recToggle(){if(rec&&rec.on){rec.on=false;$('recBtn').textContent='● Record';$('recBtn').className='primary';
  $('recSave').disabled=!rec.lines.length;if(rec.lines.length)recDownload();return}
 rec={on:true,lines:[]};recLast=0;recLost=0;recStart=Date.now();$('recBtn').textContent='■ Stop';$('recBtn').className='danger';
 $('recSave').disabled=true;recPoll()}
async function recPoll(){if(!rec||!rec.on)return;
 try{const t=await(await fetch('/api/log?since='+recLast,{cache:'no-store'})).text();
  for(const l of t.split('\n')){if(!l)continue;const q=+l.slice(0,l.indexOf(','));
   if(recLast&&q>recLast+1)recLost+=q-recLast-1;if(q>recLast){recLast=q;rec.lines.push(l)}}}catch(e){}
 const s=Math.floor((Date.now()-recStart)/1000);$('recTime').textContent=Math.floor(s/60)+':'+String(s%60).padStart(2,'0');
 $('recN').textContent=rec.lines.length;$('recGap').textContent=recLost?recLost+' samples missed':'';
 setTimeout(recPoll,200)}
function recDownload(){if(!rec||!rec.lines.length)return;const d=new Date(),p=n=>String(n).padStart(2,'0');
 const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([REC_HEAD+rec.lines.join('\n')+'\n'],{type:'text/csv'}));
 a.download=`bicopter_${d.getFullYear()}${p(d.getMonth()+1)}${p(d.getDate())}_${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}.csv`;
 document.body.append(a);a.click();a.remove();toast('flight log saved ('+rec.lines.length+' samples)')}
addEventListener('resize',draw);poll();
</script></body></html>)html";

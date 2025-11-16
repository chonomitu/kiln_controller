#pragma once

// Extracted from webserver.cpp – main UI HTML
static const char INDEX_HTML[] PROGMEM = R"HTML(

<!doctype html><html lang="pl"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>KilnCtrl</title>
<link rel="icon" href="/logo.png" type="image/png">
<style>

  :root{--bg:#0b0e12;--card:#12161d;--fg:#e8eef6;--mut:#9bb0c3;--ok:#1e8e3e;--bad:#c5221f;--acc:#48a1ff;--br:#253043}

  /* TŁO – logo jako tapeta całej strony */
  html,body{
    background:var(--bg) url("/logo.png") center center / cover no-repeat fixed;
    color:var(--fg);
    font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;
    margin:0;
  }

  /* Kontener z lekkim przyciemnieniem, żeby UI było czytelne na tle logotypu */
  .wrap{
    max-width:960px;
    margin:0 auto;
    padding:12px;
    background:rgba(11,14,18,0.82);
    border-radius:16px;
    box-shadow:0 0 24px rgba(0,0,0,0.6);
  }

  .topbar{
    display:flex;
    align-items:center;
    justify-content:space-between;
    gap:8px;
  }
  .logo-mini{
    width:32px;
    height:32px;
    object-fit:contain;
    filter:drop-shadow(0 0 6px rgba(0,0,0,.7));
  }


  h1{font-size:20px;margin:8px 0 10px}
  .row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}

  button,a.btn{
    padding:10px 14px;
    border:0;
    border-radius:10px;
    cursor:pointer;
    text-decoration:none;
    display:inline-block;
    position:relative;
  }
  .btn{background:#17202b;color:#d7e1ee;border:1px solid var(--br)}

  /* START / STOP – przyciski 3D */
  .start{background:var(--ok);color:#fff}
  .stop{background:var(--bad);color:#fff}
  .btn-3d{
    box-shadow:0 4px 0 #05070b,0 0 12px rgba(0,0,0,.6);
    transition:transform .1s,box-shadow .1s;
  }
  .btn-3d.pressed,
  .btn-3d:active{
    transform:translateY(2px);
    box-shadow:0 2px 0 #05070b inset,0 0 8px rgba(0,0,0,.8);
  }

 
  .card{
  background:rgba(0,0,0,0.15);  /* bardzo delikatne przyciemnienie pod tekst */
  border:1px solid rgba(37,48,67,0.8);
  border-radius:14px;
  padding:12px;
  margin-top:12px;
}

  .kv{display:grid;grid-template-columns:160px 1fr;gap:8px 10px;font-size:14px}
  .mut{color:var(--mut)}
  .bar{height:12px;background:#101824;border-radius:8px;overflow:hidden;margin-top:10px;border:1px solid #1f2a3a}
  .bar>i{display:block;height:100%;background:var(--acc);width:0%}
  code{background:#111926;padding:2px 6px;border-radius:6px}
  canvas{width:100%;height:220px;display:block;background:#0f141d;border-radius:12px}
  
  .badge{padding:6px 12px;border-radius:999px;border:1px solid #2a384f;background:#0f1724}
  .glow{color:#8bf7a8;text-shadow:0 0 8px #15ff62,0 0 16px #15ff62,0 0 24px #15ff62;font-weight:700}

  /* HEAT ON – czerwony neon (fizyczne grzanie / SSR) */
  .heater-off{color:#97a8bd}
  .heater-on{
    color:#ff6b81;
    text-shadow:
      0 0 4px #ff6b81,
      0 0 8px #ff6b81,
      0 0 14px #ff2b4a,
      0 0 22px #ff2b4a;
    font-weight:700;
    letter-spacing:0.05em;
  }

  /* Temperatura – niebieski neon, główna informacja */
  .temp-main{
    font-size:26px;
    font-weight:700;
    color:#8bf7ff;
    text-shadow:
      0 0 4px #46d9ff,
      0 0 10px #46d9ff,
      0 0 18px #1aa4ff;
    letter-spacing:0.03em;
  }
  .temp-unit{
    margin-left:4px;
    color:#b8c5d8;
    font-size:18px;
  }

  /* Zegar + ON/OFF sterownika + dodatkowy czas */
  .badge-clock{
    display:inline-flex;
    flex-direction:column;
    align-items:flex-start;
    gap:2px;
    padding:4px 10px;
    border-radius:999px;
    border:1px solid #2a384f;
    background:#0f1724;
    font-size:12px;
    cursor:pointer;
  }
  .badge-clock-main{
    display:flex;
    align-items:baseline;
    gap:6px;
  }
  .status-on{
    color:#ff6b81;
    text-shadow:
      0 0 4px #ff6b81,
      0 0 8px #ff6b81,
      0 0 14px #ff2b4a;
    font-weight:700;
    letter-spacing:0.06em;
  }
  .status-off{
    color:#7b8ba1;
    font-weight:600;
  }
  .badge-sub{
    font-size:11px;
    color:#9bb0c3;
  }

  /* Narzędzia – sekcja rozwijana na dole */
  .tools-header{
    cursor:pointer;
    justify-content:space-between;
  }
  .tools-header:hover{
    background:rgba(255,255,255,0.02);
  }
  .tools-body{
    display:none;
    margin-top:8px;
  }
  .tools-body.show{
    display:block;
  }

  /* Edytor presetów (sesyjny) */
  .preset-editor{
    margin-top:8px;
    padding:8px;
    border-radius:10px;
    background:#0f1724;
    border:1px dashed #2a384f;
    font-size:13px;
  }
  .preset-editor-row{
    display:flex;
    flex-wrap:wrap;
    gap:6px;
    align-items:center;
    margin-top:4px;
  }
  .preset-editor-row label{
    font-size:12px;
    color:var(--mut);
  }
  .preset-editor-row input[type=text]{
    flex:1;
    min-width:120px;
    background:#0b1320;
    color:var(--fg);
    border:1px solid #2a384f;
    border-radius:8px;
    padding:6px 8px;
    font-size:13px;
  }
  .preset-editor-row input[type=number],
  .preset-editor-row input[type=range]{
    background:#0b1320;
    color:var(--fg);
    border:1px solid #2a384f;
    border-radius:8px;
  }

  input[type=number],select{
    background:#0f1724;
    color:var(--fg);
    border:1px solid #2a384f;
    border-radius:10px;
    padding:10px 10px
  }
  table{width:100%;border-collapse:collapse;font-size:13px}
  th,td{border-bottom:1px solid #2a2f40;padding:8px 10px;text-align:left}
  th{color:#b8c5d8}
  @media (max-width:560px){
    .wrap{padding:10px}
    .row{gap:6px}
    button,.btn{flex:1}
    .kv{grid-template-columns:120px 1fr}
    canvas{height:200px}
  }
</style>
<div class="wrap">
  <div class="topbar">
    <h1>KilnCtrl</h1>
    <img src="/logo.png" alt="KilnCtrl" class="logo-mini">
  </div>


  <!-- GÓRNA BELKA: START / STOP + odśwież + zegar + ON/OFF sterownika + tryb -->
  <div class="row top-row">


  <!-- GÓRNA BELKA: START / STOP + odśwież + zegar + ON/OFF sterownika + tryb -->
  <div class="row top-row">
    <button class="start btn-3d" onclick="doStart()">START</button>
    <button class="stop btn-3d"  onclick="doStop()">STOP</button>
    <button class="btn" onclick="fetchState()">Odśwież</button>

    <span class="mut" style="margin-left:auto;display:flex;align-items:center;gap:8px">
      <span class="badge-clock" onclick="cycleTimerView()" title="kliknij, aby przełączyć widok czasu">
        <span class="badge-clock-main">
          <span id="clock">--:--:--</span>
          <span id="runStatus" class="status-off">OFF</span>
        </span>
        <span id="runExtra" class="badge-sub">piec zatrzymany</span>
      </span>
      <span>tryb: <b id="mode">?</b></span>
    </span>
  </div>

  <div class="card">
    <div class="kv">

      <div>Tryb pracy</div>
      <div class="row" style="gap:8px">
        <!-- przyciski szybkiej zmiany trybu -->
        <button class="btn" id="btnModeDyn"  onclick="applyModeDynamic()">Tryb dynamiczny</button>
        <button class="btn" id="btnModeProf" onclick="applyModeProfile()">Tryb profilu</button>

        <!-- ręczne zmiany etapów profilu -->
        <button class="btn" id="stepPrev" onclick="profileStepPrev()">◀ krok</button>
        <button class="btn" id="stepNext" onclick="profileStepNext()">krok ▶</button>

        <span class="mut">Etap (gdy profil): <b id="profInfo">–</b></span>
      </div>

      <div>Temperatura</div>
      <div>
        <span id="temp" class="temp-main">?</span>
        <span class="temp-unit">°C</span>
        <span id="sensorMsg" class="mut"></span>
      </div>

      <div>Setpoint</div>
      <div class="row" style="gap:8px">
        <span id="set">?</span> °C
        <span id="profileStepTime" class="mut" style="display:none;margin-left:8px"></span>
      </div>
      <div class="row" style="gap:8px" id="dynamicSetControls">
        <input id="newSet" type="number" step="1" inputmode="numeric" style="width:110px" placeholder="np. 200">
        <button class="btn" onclick="applySet()">Ustaw</button>
        <select id="presetSel" style="min-width:180px"></select>
        <button class="btn" onclick="applyPreset()">Zastosuj preset</button>
      </div>

      <div>Output</div>
      <div>
        <span id="out">?</span> %
        <span id="heaterTxt" class="heater-off">HEAT OFF</span>
      </div>

    </div>
  </div>

  <div class="bar"><i id="bar"></i></div>

  <div class="card">
    <div class="row" style="margin:0 0 6px;align-items:center">
      <b>Wykres</b>
      <span class="mut"> – wybierz okno:</span>
      <select id="winSel" onchange="changeWindow()">
        <option value="120">~2 min</option>
        <option value="300">~5 min</option>
        <option value="600">~10 min</option>
        <option value="1800">~30 min</option>
      </select>
    </div>
    <canvas id="chart"></canvas>
    <div class="mut" style="font-size:12px;margin-top:6px">
      Oś <b>Y (lewa)</b>: temperatura [°C] • Oś <b>Y (prawa)</b>: OUT [%] • Oś <b>X</b>: czas (ostatnie okno)
    </div>
  </div>

  <!-- NARZĘDZIA I KONFIGURACJA: schowane na dole w rozwijanej belce -->
  <div class="card">
    <div class="row tools-header" onclick="toggleTools()">
      <b>Narzędzia i konfiguracja</b>
      <span class="mut">kliknij, aby rozwinąć</span>
    </div>
    <div id="toolsBody" class="tools-body">
      <div class="kv" style="margin-top:8px">
        <div>Firmware</div>
        <div class="row" style="gap:8px">
          <a class="btn" href="/update">Firmware / OTA</a>
          <span class="mut">Wgraj nowy plik .bin przez przeglądarkę</span>
        </div>

        <div>Narzędzia</div>
        <div class="row" style="gap:8px">
          <a href="/export" class="btn">Eksport CSV</a>
          <a href="/pins.html" class="btn">Piny</a>
          <a href="/profile.html" class="btn">Profil</a>
        </div>

        <div>Presety</div>
        <div>
          <button class="btn" type="button" onclick="togglePresetEditor()">Edycja presetów</button>
          <div class="mut" style="font-size:12px;margin-top:4px">
            Zmień nazwy i temperatury presetów (0–2000°C). Docelowo można je zapisywać w pamięci sterownika.
          </div>
          <!-- Edytor presetów (UI, na razie sesyjny – backend do EEPROM zrobimy osobno) -->
          <div id="presetEditor" class="preset-editor" style="display:none">
            <div class="preset-editor-row">
              <label for="presetEditSel">Preset:</label>
              <select id="presetEditSel" style="min-width:140px" onchange="onPresetEditChange()"></select>
            </div>
            <div class="preset-editor-row">
              <label for="presetNameEdit">Nazwa</label>
              <input type="text" id="presetNameEdit" placeholder="np. Biskwit 950°C">
            </div>
            <div class="preset-editor-row">
              <label for="presetTempEdit">Temperatura (0–2000°C)</label>
              <input type="number" id="presetTempEdit" min="0" max="2000" step="1" style="width:80px">
              <input type="range" id="presetTempSlider" min="0" max="2000" step="10" style="flex:1" oninput="onPresetSliderChange()">
            </div>
            <div class="preset-editor-row">
              <button class="btn" type="button" onclick="savePresetEdit()">Zapisz preset</button>
              <span class="mut">Na stałe zapis w EEPROM zrobimy w logice ESP (osobna funkcja do wywołania z tego miejsca).</span>
            </div>
          </div>
        </div>

        <div>Sieć</div>
                        
        <div>Sieć</div>
        <div class="row" style="gap:8px">
          <a href="/wifi.html" class="btn">Konfiguracja WiFi (STA)</a>
          <a href="/mqtt.html" class="btn">Konfiguracja MQTT</a>
          <span class="mut" style="font-size:12px">
            Tryb AP tego sterownika działa zawsze jako baza – WiFi/MQTT są dodatkowymi opcjami podłączenia.
          </span>
        </div>

        <div></div>
        <div class="mut" style="font-size:12px">
          Temperatura sterownika: <code id="ctrlTemp">–</code> °C
        </div>

        <div class="mut" style="font-size:12px;margin-top:4px">
          AP: SSID <code id="netApSsid">-</code>,
          IP <code id="netApIp">-</code>,
          MAC <code id="netApMac">-</code><br>
          STA: <span id="netStaStatus">brak połączenia</span>,
          IP <code id="netStaIp">-</code>,
          MAC <code id="netStaMac">-</code>
        </div>

        <div>API</div>
        <div class="mut" style="font-size:12px">
          Endpointy dla integracji (np. z innym systemem): <code>/ping</code>, <code>/state</code>, <code>/status.json</code>,
          <code>/mode?m=dynamic|profile</code>, <code>/start</code>/<code>/stop</code>,
          <code>/set?sp=200</code>, <code>/presets</code>, <code>/preset?i=0</code>, <code>/export</code>.
        </div>
      </div>
    </div>
  </div>

      <!-- Certyfikat blockchain / NFT -->
  <div class="card" style="text-align:center;margin-top:8px">
    <div style="font-size:16px;font-weight:600;margin-bottom:4px;">
      Sterownik posiada certyfikat blockchain (NFT)
    </div>
    <div class="mut" style="font-size:13px;line-height:1.5;">
      Zrobione przez: <b>kyrson</b> dla <b>biroskop</b> 18.X 2025<br>
      Szczegóły certyfikatu:
      <a href="https://kyrson.pl" target="_blank" style="color:#6cf;text-decoration:none;">
        kyrson.pl
      </a>
    </div>
  </div>
  
</div>
<script>
let tBuf=[], oBuf=[]; 
let maxPts = 120; // domyślnie ~2 min (próbka/sek)
const DPR = Math.max(1, Math.min(3, window.devicePixelRatio||1));

const qs=s=>document.querySelector(s);
const show=(el,v)=>{ if(typeof el==='string') el=qs(el); if(!el)return; el.style.display=v?'':'none'; };

// TIMER / CZAS
let timerView = 0;        // 0 = od startu etapu, 1 = do końca etapu, 2 = koniec etapu o godzinie
let lastState = null;
let runStartClient = null;  // fallback dla trybu dynamicznego (czas od START w tej sesji)

// PRESETY (sesyjna kopia z /presets)
let presets = [];

function pushBuf(b,v){b.push(v); if(b.length>maxPts)b.shift();}
function changeWindow(){
  const v = parseInt(qs('#winSel').value||"120",10);
  maxPts = v;
  if(tBuf.length>maxPts) tBuf=tBuf.slice(tBuf.length-maxPts);
  if(oBuf.length>maxPts) oBuf=oBuf.slice(oBuf.length-maxPts);
  drawChart();
}

function resizeCanvas(){
  const c=qs('#chart'); const W=c.clientWidth||320; const H=c.clientHeight||220;
  c.width = Math.floor(W*DPR); c.height = Math.floor(H*DPR);
}

async function fetchState(){
  try{
    const r=await fetch('/state',{cache:'no-store'}); if(!r.ok) throw 0;
    const d=await r.json();
    lastState = d;

    qs('#mode').textContent   = d.mode || 'dynamic';

    const isProfile = d.mode === 'profile';

    const stepPrevBtn = qs('#stepPrev');
    const stepNextBtn = qs('#stepNext');
    if (stepPrevBtn && stepNextBtn) {
      stepPrevBtn.style.display = isProfile ? '' : 'none';
      stepNextBtn.style.display = isProfile ? '' : 'none';
    }

    // przełączanie UI dla trybu dynamicznego vs profilowego
    const dynControls = qs('#dynamicSetControls');
    const stepTimeEl  = qs('#profileStepTime');
    if (dynControls) {
      dynControls.style.display = isProfile ? 'none' : '';
    }
    if (stepTimeEl) {
      if (isProfile && typeof d.profile_elapsed === 'number' && typeof d.profile_remain === 'number') {
        const elapsed = d.profile_elapsed;
        const remain  = d.profile_remain;
        const total   = elapsed + remain;
        stepTimeEl.style.display = '';
        stepTimeEl.textContent = 'czas kroku: ' + formatDuration(total) + ' (pozostało: ' + formatDuration(remain) + ')';
      } else {
        stepTimeEl.style.display = 'none';
        stepTimeEl.textContent = '';
      }
    }

    const btnDyn  = qs('#btnModeDyn');
    const btnProf = qs('#btnModeProf');
    if (btnDyn && btnProf) {
      btnDyn.classList.toggle('glow', !isProfile);
      btnProf.classList.toggle('glow', isProfile);
    }

    qs('#temp').textContent   = Number.isFinite(d.temp)? d.temp.toFixed(1) : '?';
    qs('#set').textContent    = (d.set ?? 0).toFixed(0);
    qs('#out').textContent    = (d.out ?? 0).toFixed(0);
    qs('#bar').style.width    = Math.max(0,Math.min(100,d.out||0))+'%';

    const ctrlEl = qs('#ctrlTemp');
    if (ctrlEl) {
      if (typeof d.ctrl_temp === 'number' && Number.isFinite(d.ctrl_temp)) {
        ctrlEl.textContent = d.ctrl_temp.toFixed(1);
      } else {
        ctrlEl.textContent = '–';
      }
    }

    qs('#sensorMsg').textContent = d.sensor ? '' : ' (brak czujnika)';
    qs('#profInfo').textContent = (d.mode==='profile' && d.steps)
      ? ((d.step+1)+' / '+d.steps)
      : '–';

    const heaterSpan = qs('#heaterTxt');
    const heaterOn   = !!d.heater;
    heaterSpan.textContent = heaterOn ? 'HEAT ON' : 'HEAT OFF';
    heaterSpan.className   = heaterOn ? 'heater-on' : 'heater-off';

    const runOn      = !!d.active;
    const runSpan    = qs('#runStatus');
    if (runSpan) {
      runSpan.textContent = runOn ? 'ON' : 'OFF';
      runSpan.className   = runOn ? 'status-on' : 'status-off';
    }

    const btnStart = qs('.start');
    if (btnStart) {
      btnStart.classList.toggle('pressed', runOn);
    }

    if (runOn) {
      if (!isProfile && !runStartClient) {
        runStartClient = Date.now();
      }
    } else {
      runStartClient = null;
    }

    pushBuf(tBuf,+(d.temp??0)); pushBuf(oBuf,+(d.out??0));

    // ─────────────────────────────
    // Info o sieci z d.ap / d.sta
    // ─────────────────────────────
    if (d.ap) {
      const elApSsid = qs('#netApSsid');
      const elApIp   = qs('#netApIp');
      const elApMac  = qs('#netApMac');
      if (elApSsid) elApSsid.textContent = d.ap.ssid || '(AP)';
      if (elApIp)   elApIp.textContent   = d.ap.ip   || '-';
      if (elApMac)  elApMac.textContent  = d.ap.mac  || '-';
    }

    if (d.sta) {
      const elStaStatus = qs('#netStaStatus');
      const elStaIp     = qs('#netStaIp');
      const elStaMac    = qs('#netStaMac');

      const connected = !!d.sta.connected;
      const ssid      = d.sta.ssid || '';

      if (elStaStatus) {
        elStaStatus.textContent = connected
          ? ('połączono z ' + ssid)
          : 'brak połączenia';
      }
      if (elStaIp)  elStaIp.textContent  = d.sta.ip  || '-';
      if (elStaMac) elStaMac.textContent = d.sta.mac || '-';
    }

    drawChart();
    updateRunInfo();
  } catch(e){
    qs('#sensorMsg').textContent=' (błąd /state)';
  }
}

function drawChart(){
  const c=qs('#chart'); const x=c.getContext('2d');
  x.setTransform(1,0,0,1,0,0); x.clearRect(0,0,c.width,c.height);
  if(tBuf.length<2) return;

  const padL=40, padR=40, padB=22, padT=8;
  const W=c.width, H=c.height, PW=W-padL-padR, PH=H-padT-padB;
  const toX = i => padL + (i*(PW/(Math.max(1,maxPts-1))));
  const toYt = (v,tmin,tmax)=> padT + PH - ((v-tmin)/(tmax-tmin))*PH;
  const toYo = v => padT + PH - (v/100)*PH;

  let tmin=Math.min(...tBuf), tmax=Math.max(...tBuf);
  if(!isFinite(tmin)||!isFinite(tmax)){ tmin=0; tmax=1; }
  if(tmin===tmax){tmin-=1;tmax+=1}
  tmin-=2; tmax+=2;

  x.strokeStyle='#2a3a52'; x.lineWidth=1; x.globalAlpha=.5;
  x.beginPath();
  for(let i=0;i<=5;i++){ const y=padT + (PH*i/5); x.moveTo(padL,y); x.lineTo(W-padR,y); }
  for(let i=0;i<=6;i++){ const X=padL + (PW*i/6); x.moveTo(X,padT); x.lineTo(X,H-padB); }
  x.stroke(); x.globalAlpha=1;

  x.lineWidth=1.5; x.strokeStyle='#9dd1ff'; x.beginPath();
  for(let i=0;i<oBuf.length;i++){ const X=toX(i),Y=toYo(oBuf[i]); if(i===0)x.moveTo(X,Y); else x.lineTo(X,Y); }
  x.stroke();

  x.lineWidth=2; x.strokeStyle='#48a1ff'; x.beginPath();
  for(let i=0;i<tBuf.length;i++){ const X=toX(i),Y=toYt(tBuf[i],tmin,tmax); if(i===0)x.moveTo(X,Y); else x.lineTo(X,Y); }
  x.stroke();

  x.fillStyle='#b8c5d8'; x.font = (10*DPR)+'px sans-serif';
  x.textAlign='right'; x.textBaseline='middle';
  x.fillText(tmax.toFixed(0)+'°C', padL-4, padT);
  x.fillText(((tmin+tmax)/2).toFixed(0)+'°C', padL-4, padT+PH/2);
  x.fillText(tmin.toFixed(0)+'°C', padL-4, padT+PH);

  x.textAlign='left';
  x.fillText('100%', W-padR+4, padT);
  x.fillText('50%',  W-padR+4, padT+PH/2);
  x.fillText('0%',   W-padR+4, padT+PH);

  x.textAlign='center'; x.textBaseline='alphabetic';
  const win = parseInt(qs('#winSel').value||"120",10);
  let lbl = '~' + (win>=3600? (win/3600)+' h' : win>=60? (win/60)+' min' : win+' s');
  x.fillText('czas ('+lbl+')', padL+PW/2, H-4);
}

function doStart(){ fetch('/start').then(fetchState); }
function doStop(){  fetch('/stop').then(fetchState); }

function parseLocaleNumber(s){ if(typeof s!=='string') return NaN; s=s.replace(',', '.'); return Number(s); }
function applySet(){
  const v=parseLocaleNumber(qs('#newSet').value); if(!isFinite(v)) return;
  fetch('/set?sp='+encodeURIComponent(v)).then(fetchState);
}

async function loadPins(){
  try{
    const r=await fetch('/pins'); if(!r.ok) throw 0;
    const p=await r.json();
  }catch(e){}
}

async function loadPresets(){
  try{
    const r=await fetch('/presets'); if(!r.ok) throw 0;
    const a=await r.json();
    const selMain = document.getElementById('presetSel');
    const selEdit = document.getElementById('presetEditSel');
    presets = [];

    selMain.innerHTML='';
    if (selEdit) selEdit.innerHTML='';

    a.forEach((p,i)=>{
      presets.push({name:p.name, sp:p.sp});

      const o=document.createElement('option');
      o.value=i; o.textContent=`${p.name} (${p.sp.toFixed(0)}°C)`;
      selMain.appendChild(o);

      if (selEdit) {
        const oe=document.createElement('option');
        oe.value=i; oe.textContent=`${p.name} (${p.sp.toFixed(0)}°C)`;
        selEdit.appendChild(oe);
      }
    });

    if (selEdit && presets.length>0) {
      selEdit.value = '0';
      fillPresetEditorFromIndex(0);
    }
  }catch(e){}
}

function applyPreset(){
  const sel = document.getElementById('presetSel');
  if (!sel) return;
  const i = parseInt(sel.value,10);
  if (isNaN(i) || !presets[i]) return;
  const sp = presets[i].sp;
  fetch('/set?sp='+encodeURIComponent(sp)).then(fetchState);
}

function applyModeDynamic(){
  fetch('/mode/dynamic', {method:'POST'}).then(fetchState);
}

function applyModeProfile(){
  fetch('/mode/profile', {method:'POST'}).then(fetchState);
}

function profileStepPrev(){
  fetch('/profile/prev', {method:'POST'}).then(fetchState);
}

function profileStepNext(){
  fetch('/profile/next', {method:'POST'}).then(fetchState);
}

function updateClock(){
  const el = qs('#clock');
  if (!el) return;
  const d = new Date();
  const hh = String(d.getHours()).padStart(2,'0');
  const mm = String(d.getMinutes()).padStart(2,'0');
  const ss = String(d.getSeconds()).padStart(2,'0');
  el.textContent = `${hh}:${mm}:${ss}`;
}

function formatDuration(sec){
  if (!isFinite(sec)) return '—';
  sec = Math.max(0, Math.floor(sec));
  const h = Math.floor(sec/3600);
  const m = Math.floor((sec%3600)/60);
  const s = sec%60;
  if (h > 0) return `${h}h ${String(m).padStart(2,'0')}m`;
  return `${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}`;
}

function updateRunInfo(){
  const extra = qs('#runExtra');
  if (!extra) return;

  if (!lastState){
    extra.textContent = 'brak danych';
    return;
  }
  const d = lastState;
  const active = !!d.active;
  const isProfile = d.mode === 'profile';

  if (!active){
    extra.textContent = 'piec zatrzymany';
    return;
  }

  // PROFIL – używamy PROFILE_ELAPSED_SEC / PROFILE_REMAIN_SEC z ESP
  if (isProfile && typeof d.profile_elapsed === 'number' && typeof d.profile_remain === 'number'){
    const elapsed = d.profile_elapsed;
    const remain  = d.profile_remain;
    if (timerView === 0){
      extra.textContent = 'od startu etapu: ' + formatDuration(elapsed);
    } else if (timerView === 1){
      extra.textContent = 'do końca etapu: ' + formatDuration(remain);
    } else {
      const finish = new Date(Date.now() + remain*1000);
      const hh = String(finish.getHours()).padStart(2,'0');
      const mm = String(finish.getMinutes()).padStart(2,'0');
      extra.textContent = 'koniec etapu ok. ' + hh + ':' + mm;
    }
    return;
  }

  // DYNAMIC – proste „od START w tej sesji”
  if (!runStartClient){
    extra.textContent = 'od START w tej sesji: 00:00';
    return;
  }
  const now = Date.now();
  const sec = (now - runStartClient) / 1000;
  extra.textContent = 'od START (sesja): ' + formatDuration(sec);
}

function cycleTimerView(){
  timerView = (timerView + 1) % 3;
  updateRunInfo();
}

function toggleTools(){
  const body = qs('#toolsBody');
  if (!body) return;
  body.classList.toggle('show');
}

// --- Edycja presetów (UI, na razie sesyjna) ---
function togglePresetEditor(){
  const box = qs('#presetEditor');
  if (!box) return;
  const isShown = box.style.display !== 'none';
  box.style.display = isShown ? 'none' : '';
}

function fillPresetEditorFromIndex(i){
  if (!presets[i]) return;
  const p = presets[i];
  const nameEl = qs('#presetNameEdit');
  const tempEl = qs('#presetTempEdit');
  const slider = qs('#presetTempSlider');
  if (nameEl) nameEl.value = p.name || '';
  if (tempEl) tempEl.value = p.sp.toFixed(0);
  if (slider) slider.value = Math.max(0,Math.min(2000,Math.round(p.sp)));
}

function onPresetEditChange(){
  const sel = qs('#presetEditSel');
  if (!sel) return;
  const i = parseInt(sel.value,10);
  if (isNaN(i)) return;
  fillPresetEditorFromIndex(i);
}

function onPresetSliderChange(){
  const slider = qs('#presetTempSlider');
  const tempEl = qs('#presetTempEdit');
  if (!slider || !tempEl) return;
  tempEl.value = slider.value;
}

function savePresetEdit(){
  const sel = qs('#presetEditSel');
  const nameEl = qs('#presetNameEdit');
  const tempEl = qs('#presetTempEdit');
  if (!sel || !nameEl || !tempEl) return;
  const i = parseInt(sel.value,10);
  if (isNaN(i) || !presets[i]) return;

  let sp = parseFloat(String(tempEl.value).replace(',','.'));
  if (!isFinite(sp)) sp = presets[i].sp;
  sp = Math.max(0, Math.min(2000, sp));

  presets[i].name = nameEl.value || presets[i].name;
  presets[i].sp   = sp;

  const selMain = qs('#presetSel');
  const selEdit = qs('#presetEditSel');
  if (selMain){
    Array.from(selMain.options).forEach((o,idx)=>{
      if (presets[idx]){
        o.textContent = `${presets[idx].name} (${presets[idx].sp.toFixed(0)}°C)`;
      }
    });
  }
  if (selEdit){
    Array.from(selEdit.options).forEach((o,idx)=>{
      if (presets[idx]){
        o.textContent = `${presets[idx].name} (${presets[idx].sp.toFixed(0)}°C)`;
      }
    });
  }

  // Zapis do pamięci (LittleFS -> "/presets.json")
  fetch('/presets/save', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({presets})
  }).then(r=>{
    if (!r.ok) throw 0;
    console.log('[PRESETS] Zapisano w pamięci.');
  }).catch(()=>{
    console.warn('[PRESETS] Błąd zapisu w pamięci.');
  });
}

// --- Init ---
window.addEventListener('resize', ()=>{resizeCanvas(); drawChart();});
resizeCanvas(); 
changeWindow(); 

setInterval(()=>{
  updateClock();
  updateRunInfo();
},1000);

fetchState(); 
updateClock();
loadPins(); 
loadPresets();
setInterval(fetchState,1000);
</script>

)HTML";

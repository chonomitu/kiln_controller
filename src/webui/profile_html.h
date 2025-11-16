#pragma once

// Extracted from web_config.cpp – PROFILE_HTML UI HTML
static const char PROFILE_HTML[] PROGMEM = R"HTML(

<!doctype html><html lang="pl"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Program (profil) wypału</title>
<style>
  body{background:#0b0e12;color:#e8eef6;font-family:system-ui,Segoe UI,Roboto,Arial;margin:0}
  .wrap{max-width:880px;margin:0 auto;padding:14px}
  h1{font-size:20px;margin:8px 0 14px}
  .card{background:#12161d;border:1px solid #253043;border-radius:14px;padding:12px;margin:12px 0}
  table{width:100%;border-collapse:collapse}
  th,td{padding:8px 10px;border-bottom:1px solid #243045}
  th{color:#b8c5d8;text-align:left}
  input{background:#0f1724;color:#e8eef6;border:1px solid #2a384f;border-radius:10px;padding:8px 10px;width:110px}
  select{background:#0f1724;color:#e8eef6;border:1px solid #2a384f;border-radius:10px;padding:8px 10px}
  .row{display:flex;gap:8px;flex-wrap:wrap}
  .btn{padding:10px 14px;border:0;border-radius:10px;background:#17202b;color:#d7e1ee;border:1px solid #253043;cursor:pointer}
  .btn-danger{background:#3a1218;border-color:#5a1c2a;color:#ffb3c0}
  .btn-ghost{background:transparent;border-style:dashed}
  .mut{color:#9bb0c3}
</style>
<div class="wrap">
  <h1>Program (profil) wypału</h1>

  <div class="card">
    <div class="row" style="margin-bottom:8px">
      <div style="flex:1 1 180px">
        <div class="mut">Aktywny profil</div>
        <select id="profilesList"></select>
      </div>
      <div style="flex:1 1 180px">
        <div class="mut">Nazwa do zapisu</div>
        <input id="profileName" type="text" placeholder="np. Stoneware_1240C">
      </div>
    </div>
    <div class="row" style="margin-bottom:8px">
      <button class="btn" onclick="saveNamed()">Zapisz jako</button>
      <button class="btn" onclick="loadNamed()">Wczytaj</button>
      <button class="btn btn-danger" onclick="deleteNamed()">Usuń</button>
      <button class="btn btn-ghost" onclick="setModeDynamic()">Tryb dynamiczny</button>
      <button class="btn btn-ghost" onclick="setModeProfile()">Tryb profilowy</button>
      <span id="msgTop" class="mut"></span>
    </div>
  </div>

  <div class="card">
    <table id="tbl"><thead><tr><th>#</th><th>Temperatura (°C)</th><th>Czas trwania</th><th></th></tr></thead><tbody></tbody></table>
    <div class="row" style="margin-top:10px">
      <button class="btn" onclick="addRow()">Dodaj krok</button>
      <button class="btn" onclick="saveProfile()">Zapisz profil tymczasowy</button>
      <span id="msg" class="mut"></span>
    </div>
    <div class="mut" style="margin-top:8px">
      <b>„Zapisz tymczasowy profil”</b> – zapisuje aktualny program jako aktywny (używany przy następnym START).<br>
      <b>„Zapisz jako”</b> u góry – zapisuje program do pamięci nazwanych profili (lista rozwijana).<br>
      Czas podaj jako <b>min</b> lub z sufiksem: <code>h</code>, <code>m</code>, <code>s</code> (np. <code>10m</code>, <code>4h</code>).<br>
      Maks. 8 kroków. Zakres temperatur: 0–2000&nbsp;°C. Tryb profilowy pokazuje na OLED <code>minęło / zostało</code> dla bieżącego etapu.
    </div>

  </div>

  <div class="card">
    <a href="/" class="btn">← Panel</a>
  </div>
</div>
<script>
let data=[];

function fmtRow(i,step){
  return `<tr>
  <td>${i+1}</td>
  <td><input type="number" step="1" min="0" max="2000" value="${step.targetC||0}" data-k="t"></td>
  <td><input type="text" value="${step.h||""}" placeholder="np. 10m, 30m, 4h" data-k="h"></td>
  <td><button class="btn" onclick="delRow(${i})">Usuń</button></td>
</tr>`;
}

function toSec(txt){
  if(!txt) return 0;
  txt = String(txt).trim().toLowerCase();
  if(txt.endsWith("h")) return parseFloat(txt)*3600;
  if(txt.endsWith("m")) return parseFloat(txt)*60;
  if(txt.endsWith("s")) return parseFloat(txt);
  return parseFloat(txt)*60; // domyślnie minuty
}

async function loadActiveProfile(){
  const r=await fetch('/profile.json');
  if(!r.ok) return;
  const d=await r.json();
  data = (d.steps||[]).map(s=>({
    targetC: s.targetC||0,
    h: (s.holdSec? (Math.round(s.holdSec/60))+'m': '0m')
  }));
  render();
}

function render(){
  const tb=document.querySelector('#tbl tbody'); tb.innerHTML="";
  data.forEach((s,i)=>tb.insertAdjacentHTML("beforeend", fmtRow(i,s)));
}

function addRow(){ if(data.length<8){ data.push({targetC:200,h:"10m"}); render(); } }
function delRow(i){ data.splice(i,1); render(); }

async function saveProfile(){
  const rows=[...document.querySelectorAll('#tbl tbody tr')];
  const steps=[];
  for(let i=0;i<rows.length;i++){
    const t = parseFloat(rows[i].querySelector('input[data-k="t"]').value||"0");
    const h = rows[i].querySelector('input[data-k="h"]').value||"0m";
    const sec = toSec(h);
    if(!(isFinite(t) && isFinite(sec) && sec>=0 && t>=0 && t<=2000)) { setMsg("Błędne wartości (0–2000°C, czas >=0).", true); return; }
    steps.push({targetC: Math.round(t), holdSec: Math.round(sec)});
  }
  const r=await fetch('/profile/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({steps})});
  if(r.ok){ setMsg("Zapisano aktywny profil. Aktywne od następnego START.", false); await refreshProfiles(); }
  else{ setMsg("Błąd zapisu.", true); }
}

function setMsg(txt,err){
  const m=document.getElementById('msg'); m.textContent=txt; m.style.color=err?"#ff8383":"#7cf09f";
}
function setMsgTop(txt,err){
  const m=document.getElementById('msgTop'); m.textContent=txt; m.style.color=err?"#ff8383":"#7cf09f";
}

// ── manager profili nazwanych ────────────────────────────────────────────────
async function refreshProfiles(){
  const r = await fetch('/profiles/list');
  if(!r.ok) return;
  const d = await r.json();
  const sel = document.getElementById('profilesList');
  sel.innerHTML = "";
  (d.profiles||[]).forEach(name=>{
    const opt = document.createElement('option');
    opt.value = name;
    opt.textContent = name;
    sel.appendChild(opt);
  });
}

function currentProfileName(){
  const sel = document.getElementById('profilesList');
  return sel.value || "";
}

async function saveNamed(){
  const nameInput = document.getElementById('profileName');
  const name = (nameInput.value || "").trim();
  if(!name){ setMsgTop("Podaj nazwę profilu.", true); return; }

  const rows=[...document.querySelectorAll('#tbl tbody tr')];
  const steps=[];
  for(let i=0;i<rows.length;i++){
    const t = parseFloat(rows[i].querySelector('input[data-k="t"]').value||"0");
    const h = rows[i].querySelector('input[data-k="h"]').value||"0m";
    const sec = toSec(h);
    if(!(isFinite(t) && isFinite(sec) && sec>=0 && t>=0 && t<=2000)) { setMsgTop("Błędne wartości (0–2000°C, czas >=0).", true); return; }
    steps.push({targetC: Math.round(t), holdSec: Math.round(sec)});
  }

  const r = await fetch('/profiles/save?name='+encodeURIComponent(name),{
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({steps})
  });
  if(r.ok){
    setMsgTop("Zapisano profil \""+name+"\" i ustawiono jako aktywny.", false);
    await refreshProfiles();
  } else {
    setMsgTop("Błąd zapisu profilu.", true);
  }
}

async function loadNamed(){
  const name = currentProfileName();
  if(!name){ setMsgTop("Wybierz profil z listy.", true); return; }
  const r = await fetch('/profiles/load?name='+encodeURIComponent(name), {method:'POST'});
  if(r.ok){
    setMsgTop("Wczytano profil \""+name+"\".", false);
    await loadActiveProfile();
  } else {
    setMsgTop("Nie udało się wczytać profilu.", true);
  }
}

async function deleteNamed(){
  const name = currentProfileName();
  if(!name){ setMsgTop("Wybierz profil do usunięcia.", true); return; }
  if(!confirm("Usunąć profil \""+name+"\"?")) return;
  const r = await fetch('/profiles/delete?name='+encodeURIComponent(name), {method:'POST'});
  if(r.ok){
    setMsgTop("Usunięto profil.", false);
    await refreshProfiles();
  } else {
    setMsgTop("Błąd usuwania profilu.", true);
  }
}

// ── tryb pracy ────────────────────────────────────────────────────────────────
async function setModeDynamic(){
  const r = await fetch('/profile/mode/dynamic',{method:'POST'});
  if(r.ok){
    setMsgTop("Tryb dynamiczny: sterujesz temperaturą na żywo.", false);
  } else {
    setMsgTop("Nie udało się przełączyć na dynamiczny.", true);
  }
}

async function setModeProfile(){
  const r = await fetch('/profile/mode/profile',{method:'POST'});
  if(r.ok){
    setMsgTop("Tryb profilowy: używany będzie zapisany program.", false);
  } else {
    setMsgTop("Brak profilu lub błąd przełączenia.", true);
  }
}

// init
(async function(){
  await refreshProfiles();
  await loadActiveProfile();
})();
</script>

)HTML";

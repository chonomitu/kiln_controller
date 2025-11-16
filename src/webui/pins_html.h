#pragma once

// Extracted from web_config.cpp – PINS_HTML UI HTML
static const char PINS_HTML[] PROGMEM = R"HTML(

<!doctype html><html lang="pl"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Konfiguracja pinów</title>
<style>
  body{background:#0b0e12;color:#e8eef6;font-family:system-ui,Segoe UI,Roboto,Arial;margin:0}
  .wrap{max-width:880px;margin:0 auto;padding:14px}
  h1{font-size:20px;margin:8px 0 14px}
  .card{background:#12161d;border:1px solid #253043;border-radius:14px;padding:12px;margin:12px 0}
  table{width:100%;border-collapse:collapse}
  th,td{padding:8px 10px;border-bottom:1px solid #243045}
  th{color:#b8c5d8;text-align:left}
  select{background:#0f1724;color:#e8eef6;border:1px solid #2a384f;border-radius:10px;padding:8px 10px}
  .row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
  .btn{padding:10px 14px;border:0;border-radius:10px;background:#17202b;color:#d7e1ee;border:1px solid #253043;cursor:pointer}
  .ok{color:#7cf09f} .bad{color:#ff8383}
  .mut{color:#9bb0c3}
  .chip{display:inline-block;padding:4px 8px;border-radius:999px;background:#0f1724;border:1px solid #2a384f;font-size:12px}
  .mut-small{color:#9bb0c3;font-size:12px}
</style>
<div class="wrap">
  <h1>Konfiguracja pinów</h1>
  <div class="card">
    <div class="mut-small" style="margin-bottom:8px">
      Domyślnie sterownik używa sprawdzonego układu pinów dla NodeMCU v3
      (<span class="chip">I2C: D2/D1</span>,
       <span class="chip">SPI: D5/D6/D7/D8</span>,
       <span class="chip">SSR: D4</span>,
       <span class="chip">BUZZ: D0</span>,
       <span class="chip">LED: D3</span>).
      Możesz jednym kliknięciem przywrócić ten układ lub włączyć ręczną edycję.
    </div>
    <table id="pinsTable">
      <thead>
        <tr><th>Funkcja</th><th>GPIO</th><th>Etykieta</th></tr>
      </thead>
      <tbody></tbody>
    </table>
    <div class="row" style="margin-top:10px">
      <button class="btn" onclick="savePins()">Zapisz</button>
      <button class="btn" type="button" onclick="applyDefaultPins()">Ustaw domyślne piny</button>
      <label class="mut-small" style="display:flex;align-items:center;gap:4px">
        <input type="checkbox" id="advancedPins" onchange="toggleAdvancedPins()">
        Ręczna edycja pinów
      </label>
      <span id="msg" class="mut-small"></span>
    </div>
    <div class="mut" style="margin-top:8px">
      Kolizje: jeśli jeden GPIO jest przypisany do 2+ funkcji, wiersze zostaną podświetlone na
      <span class="bad">czerwono</span>.
    </div>
  </div>
  <div class="card">
    <a href="/" class="btn">← Panel</a>
  </div>
</div>
<script>
// Role logiczne – muszą odpowiadać CFG.pins.*
const ROLES = ["I2C_SDA","I2C_SCL","SPI_MOSI","SPI_MISO","SPI_SCK","SPI_CS","SSR","LED","BUZZ"];

// Dostępne GPIO (w tym -1 = Brak)
const GPIOS = [-1,16,5,4,0,2,14,12,13,15,3,1];

// Rekomendowany układ pinów dla NodeMCU v3 (tryb "na sztywno")
const DEFAULT_PINS = {
  I2C_SDA: 4,    // D2
  I2C_SCL: 5,    // D1
  SPI_MOSI: 13,  // D7
  SPI_MISO: 12,  // D6
  SPI_SCK: 14,   // D5
  SPI_CS: 15,    // D8
  SSR: 2,        // D4
  BUZZ: 16,      // D0
  LED: 0         // D3 (GPIO0) – LED statusowy
};

function lab(g){
  g = Number(g);
  if (g === -1 || Number.isNaN(g)) return "Brak";
  const map = {
    16:"D0", 5:"D1", 4:"D2", 0:"D3", 2:"D4",
    14:"D5",12:"D6",13:"D7",15:"D8",
    3:"RX(D9)", 1:"TX(D10)"
  };
  return map[g] || ("GPIO " + g);
}

function opt(g, v){
  const sel = (Number(g) === Number(v)) ? " selected" : "";
  const extra = (g >= 0) ? ` (GPIO ${g})` : "";
  return `<option value="${g}"${sel}>${lab(g)}${extra}</option>`;
}

function row(role, val){
  const v = (typeof val === "number") ? val : -1;
  const opts = GPIOS.map(g => opt(g, v)).join("");
  return `<tr data-role="${role}"><td>${role}</td><td><select onchange="recalc()">${opts}</select></td><td class="lab">${lab(v)}</td></tr>`;
}

async function load(){
  const r = await fetch('/pins/json');
  const d = await r.json();
  const tb = document.querySelector('#pinsTable tbody');
  tb.innerHTML = "";
  ROLES.forEach(role => {
    const has = Object.prototype.hasOwnProperty.call(d, role);
    const val = has ? d[role] : -1;
    tb.insertAdjacentHTML("beforeend", row(role, val));
  });
  recalc();

  // domyślnie: tryb "na sztywno" – bez ręcznej edycji
  const adv = document.getElementById('advancedPins');
  if (adv) {
    adv.checked = false;
  }
  toggleAdvancedPins();

  const msg = document.getElementById('msg');
  if (msg) {
    msg.textContent = "Tryb domyślny: piny zablokowane. Kliknij „Ustaw domyślne piny”, a potem „Zapisz”.";
    msg.style.color = "#9bb0c3";
  }
}

function recalc(){
  const rows = [...document.querySelectorAll('#pinsTable tbody tr')];
  const used = {};

  // przelicz ile razy występuje każdy GPIO (oprócz -1 = Brak)
  rows.forEach(tr => {
    const sel = tr.querySelector('select');
    const g = Number(sel.value);
    const labEl = tr.querySelector('.lab');
    labEl.textContent = lab(g);
    if (g >= 0){
      used[g] = (used[g] || 0) + 1;
    }
  });

  // wiersze z kolizją (g >= 0 i użyte >1) na czerwono
  rows.forEach(tr => {
    const g = Number(tr.querySelector('select').value);
    tr.style.color = (g >= 0 && used[g] > 1) ? "#ff8383" : "";
  });
}

// Ustaw rekomendowany układ pinów
function applyDefaultPins(){
  const rows = [...document.querySelectorAll('#pinsTable tbody tr')];
  rows.forEach(tr => {
    const role = tr.getAttribute('data-role');
    const g = DEFAULT_PINS[role];
    if (typeof g === "number") {
      const sel = tr.querySelector('select');
      if (sel) {
        sel.value = String(g);
      }
      const labEl = tr.querySelector('.lab');
      if (labEl) labEl.textContent = lab(g);
    }
  });
  recalc();

  const msg = document.getElementById('msg');
  if (msg){
    msg.textContent = "Ustawiono rekomendowany układ pinów. Kliknij „Zapisz”, aby zapisać w sterowniku.";
    msg.style.color = "#9bb0c3";
  }
}

// Włącz/wyłącz ręczną edycję (tryb zaawansowany)
function toggleAdvancedPins(){
  const adv = document.getElementById('advancedPins');
  const enabled = adv && adv.checked;
  const rows = [...document.querySelectorAll('#pinsTable tbody tr')];
  rows.forEach(tr => {
    const sel = tr.querySelector('select');
    if (sel) sel.disabled = !enabled;
  });

  const msg = document.getElementById('msg');
  if (msg){
    if (!enabled){
      msg.textContent = "Tryb domyślny: piny zablokowane. Zaznacz „Ręczna edycja pinów”, aby je zmienić.";
      msg.style.color = "#9bb0c3";
    } else {
      msg.textContent = "Tryb zaawansowany: możesz zmieniać piny. Pamiętaj o zapisaniu konfiguracji.";
      msg.style.color = "#9bb0c3";
    }
  }
}

async function savePins(){
  const rows = [...document.querySelectorAll('#pinsTable tbody tr')];
  const body = {};
  let bad = false;

  rows.forEach(tr => {
    const role = tr.getAttribute('data-role');
    const g = Number(tr.querySelector('select').value);
    body[role] = g;
  });

  // sprawdź kolizje (ignorujemy -1 = Brak)
  const counts = {};
  Object.values(body).forEach(g => {
    g = Number(g);
    if (g >= 0){
      counts[g] = (counts[g] || 0) + 1;
    }
  });
  Object.keys(counts).forEach(k => {
    if (counts[k] > 1) bad = true;
  });

  const msg = document.getElementById('msg');
  if (bad){
    msg.textContent = "Kolizje pinów – popraw zanim zapiszesz.";
    msg.style.color = "#ff8383";
    return;
  }

  msg.textContent = "Zapisuję…";
  msg.style.color = "#9bb0c3";

  const r = await fetch('/pins/save', {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify(body)
  });
  if (r.ok){
    msg.textContent = "Zapisano. Restart magistral wykonany.";
    msg.style.color = "#7cf09f";
  } else {
    msg.textContent = "Błąd zapisu.";
    msg.style.color = "#ff8383";
  }
}

load();
</script>

)HTML";

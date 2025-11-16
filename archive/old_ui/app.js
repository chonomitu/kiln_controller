
const q = s=>document.querySelector(s);
async function api(path,opts){ const r=await fetch(path,opts); try{ return await r.json(); }catch(e){ return {}; } }

function drawChart(dataT,dataD){
  const el=q('#chart'); if(!el) return; const ctx=el.getContext('2d');
  const W=el.clientWidth, H=240; el.width=W*devicePixelRatio; el.height=H*devicePixelRatio; ctx.setTransform(devicePixelRatio,0,0,devicePixelRatio,0,0);
  ctx.fillStyle='#0c0c0c'; ctx.fillRect(0,0,W,H);
  if(!dataT.length) return; const tmin=Math.floor(Math.min(...dataT)), tmax=Math.ceil(Math.max(...dataT));
  const dmax=100, m={l:40,r:40,t:10,b:24}; const X0=m.l,X1=W-m.r,Y0=H-m.b,Y1=m.t; const w=X1-X0,h=Y0-Y1;
  const px=i=>X0 + i*(w/Math.max(1,dataT.length-1)); const pyT=v=>Y0 - ((v-tmin)/Math.max(1,(tmax-tmin)))*h; const pyD=v=>Y0 - (v/dmax)*h;
  ctx.strokeStyle='#222'; ctx.lineWidth=1; for(let k=0;k<=5;k++){ const v=tmin + k*(tmax-tmin)/5; const y=pyT(v); ctx.beginPath(); ctx.moveTo(X0,y); ctx.lineTo(X1,y); ctx.stroke(); ctx.fillStyle='#bbb'; ctx.textAlign='right'; ctx.textBaseline='middle'; ctx.fillText(v.toFixed(0)+'°C', X0-6,y); }
  const xTicks=Math.min(10,dataT.length-1); for(let k=0;k<=xTicks;k++){ const i=Math.round(k*(dataT.length-1)/xTicks), x=px(i); ctx.beginPath(); ctx.moveTo(x,Y0); ctx.lineTo(x,Y1); ctx.stroke(); }
  for(let i=0;i<dataD.length;i++){ const x=px(i)-3, hh=(dataD[i]/100)*h; ctx.fillStyle='#1e88e5'; ctx.fillRect(x,Y0-hh,6,hh); }
  ctx.strokeStyle='#66bb6a'; ctx.lineWidth=2; ctx.beginPath(); for(let i=0;i<dataT.length;i++){ const x=px(i), y=pyT(dataT[i]); if(i==0) ctx.moveTo(x,y); else ctx.lineTo(x,y); } ctx.stroke();
  ctx.fillStyle='#bbb'; ctx.textAlign='center'; ctx.fillText('próbki', (X0+X1)/2, H-4);
}

async function refresh(){
  const s=await api('/api/state'); if(!s) return;
  if(q('#mode')) q('#mode').value = s.mode;
  if(q('#setpoint')) q('#setpoint').value = s.setpoint.toFixed(0);
  if(q('#tNow')) q('#tNow').textContent = s.temp.toFixed(1);
  if(q('#sNow')) q('#sNow').textContent = s.setpoint.toFixed(0);
  if(q('#oNow')) q('#oNow').textContent = s.out.toFixed(0);
  if(q('#maxNow')) q('#maxNow').textContent = s.maxTempC.toFixed(0);
  if(q('#sampleNow')) q('#sampleNow').textContent = s.sampleSec;
  const ind=q('#heatInd'); if(ind){ ind.classList.toggle('on', !!s.active); ind.classList.toggle('off', !s.active); ind.textContent = s.active? 'HEATING' : 'OFF'; }

  window.__tbuf = (window.__tbuf||[]).concat([s.temp]); if(window.__tbuf.length>120) window.__tbuf.shift();
  window.__dbuf = (window.__dbuf||[]).concat([s.out]);  if(window.__dbuf.length>120) window.__dbuf.shift();
  drawChart(window.__tbuf, window.__dbuf);

  const setBox=q('#setpointBox'); if(setBox) setBox.style.display = (Number(s.mode)===0)? 'block':'none';

  if(s.profile && s.profile.steps){
    const box=q('#steps'); if(box){ box.innerHTML=''; s.profile.steps.forEach((st,i)=>{ const div=document.createElement('div'); div.style.margin='8px 0'; div.innerHTML=`<label>Etap ${i+1} – T[°C]</label><input data-i=${i} class="t" type=number step=1 value="${st.t}"><label>Czas [min]</label><input data-i=${i} class="m" type=number step=1 value="${(st.sec/60)|0}">`; box.appendChild(div); }); }
  }
}

const btnStart=q('#btnStart'); if(btnStart) btnStart.onclick=()=>api('/api/start',{method:'POST'}).then(refresh);
const btnStop =q('#btnStop');  if(btnStop)  btnStop.onclick =()=>api('/api/stop',{method:'POST'}).then(refresh);
const modeSel=q('#mode'); if(modeSel) modeSel.onchange=()=> api('/api/mode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mode:+q('#mode').value,set:+q('#setpoint').value})}).then(refresh);
const setIn =q('#setpoint'); if(setIn) setIn.onchange=()=> api('/api/mode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mode:+q('#mode').value,set:+q('#setpoint').value})}).then(refresh);

const addBtn=q('#addStep'); if(addBtn) addBtn.onclick=()=> api('/api/profile/add',{method:'POST'}).then(refresh);
const saveBtn=q('#saveProfile'); if(saveBtn) saveBtn.onclick=()=>{ const T=[...document.querySelectorAll('#steps .t')].map(e=>+e.value); const M=[...document.querySelectorAll('#steps .m')].map(e=>+e.value); const steps=T.map((t,i)=>({t,sec:(M[i]|0)*60})); return api('/api/profile/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({steps})}).then(refresh); };
const loadBtn=q('#loadProfile'); if(loadBtn) loadBtn.onclick=()=>{ const name=q('#profName').value.trim(); if(!name) return; api('/api/profile/load?name='+encodeURIComponent(name)).then(refresh); };

function options(el,val,taken){ el.innerHTML=''; const P=[{label:'D0 (16)',val:16},{label:'D1 (5)',val:5},{label:'D2 (4)',val:4},{label:'D3 (0)',val:0},{label:'D4 (2)',val:2},{label:'D5 (14)',val:14},{label:'D6 (12)',val:12},{label:'D7 (13)',val:13},{label:'D8 (15)',val:15},{label:'LED_BUILTIN (2)',val:2}]; P.forEach(p=>{ const o=document.createElement('option'); o.value=p.val; o.textContent=p.label + (taken.has(p.val)?' (zajęty)':''); if(p.val==val) o.selected=true; if(taken.has(p.val)&&p.val!=val) o.disabled=true; el.appendChild(o); }); }

async function refreshConfig(){ const s=await api('/api/state'); if(!s||!s.pins) return; const t=new Set(); const px=s.pins; [px.SSR,px.LED,px.BUZZ,px.BTN_A,px.BTN_B,px.I2C_SDA,px.I2C_SCL,px.SPI_CS,px.SPI_MOSI,px.SPI_MISO,px.SPI_SCK].forEach(v=>{ if(typeof v==='number'&&v>=0) t.add(v); });
  ['pSSR','pBUZZ','pLED','pBA','pBB','pCS','pMOSI','pMISO','pSCK'].forEach(id=>{ const el=q('#'+id); if(el) options(el, s.pins[id.replace('p','')], t); });
  if(q('#pSDA')){ q('#pSDA').innerHTML='<option value="'+s.pins.I2C_SDA+'">D5 ('+s.pins.I2C_SDA+')</option>'; }
  if(q('#pSCL')){ q('#pSCL').innerHTML='<option value="'+s.pins.I2C_SCL+'">D6 ('+s.pins.I2C_SCL+')</option>'; }
  if(q('#kp')) q('#kp').value=s.pid.Kp; if(q('#ki')) q('#ki').value=s.pid.Ki; if(q('#kd')) q('#kd').value=s.pid.Kd; if(q('#win')) q('#win').value=s.pid.windowMs; if(q('#samp')) q('#samp').value=s.sampleSec; if(q('#maxT')) q('#maxT').value=s.maxTempC;
  if(q('#staEn')) q('#staEn').checked = !!(s.net && s.net.staEnabled);
}
const savePins=q('#savePins'); if(savePins) savePins.onclick=()=>{ const body={SSR:+q('#pSSR').value,BUZZ:+q('#pBUZZ').value,LED:+q('#pLED').value,BTN_A:+q('#pBA').value,BTN_B:+q('#pBB').value,I2C_SDA:14,I2C_SCL:12,SPI_CS:+q('#pCS').value,SPI_MOSI:+q('#pMOSI').value,SPI_MISO:+q('#pMISO').value,SPI_SCK:+q('#pSCK').value}; return api('/api/pins',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}).then(refreshConfig); };
const savePID=q('#savePID'); if(savePID) savePID.onclick=()=>{ const body={Kp:+q('#kp').value,Ki:+q('#ki').value,Kd:+q('#kd').value,windowMs:+q('#win').value,sampleSec:+q('#samp').value,maxTempC:+q('#maxT').value}; return api('/api/pid',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}).then(refreshConfig); };

setInterval(()=>{ if(q('#chart')) refresh(); else refreshConfig(); }, 2000);
refresh();

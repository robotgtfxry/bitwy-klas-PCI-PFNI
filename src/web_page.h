#pragma once

#include <Arduino.h>

// Panel konfiguracyjny serwowany przez mastera pod http://192.168.4.1/
static const char PANEL_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bitwy Klas – przyciski</title>
<style>
:root{--bg:#f3f4f6;--card:#fff;--text:#16181d;--muted:#6b7280;--line:#e3e5e8;--accent:#2563eb;--ok:#15803d;--bad:#dc2626;--gold:#d97706;--press:#7c3aed}
@media (prefers-color-scheme:dark){:root{--bg:#0e1014;--card:#171a20;--text:#e8eaed;--muted:#9aa0a8;--line:#2a2e36;--accent:#60a5fa;--ok:#4ade80;--bad:#f87171;--gold:#fbbf24;--press:#a78bfa}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
header{position:sticky;top:0;z-index:2;display:flex;align-items:center;justify-content:space-between;gap:12px;padding:12px 16px;background:var(--card);border-bottom:1px solid var(--line)}
h1{margin:0;font-size:17px}
h2{margin:0 0 12px;font-size:15px}
.conn{display:flex;align-items:center;gap:6px;font-size:13px;color:var(--muted)}
.dot{width:9px;height:9px;border-radius:50%;background:var(--bad)}
.dot.ok{background:var(--ok)}
main{max-width:980px;margin:0 auto;padding:16px;display:grid;grid-template-columns:minmax(0,1fr);gap:16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px}
.status{text-align:center;transition:box-shadow .2s}
.status.win{box-shadow:inset 0 0 0 3px var(--gold)}
.label{font-size:13px;color:var(--muted);text-transform:uppercase;letter-spacing:.06em}
.big{font-size:44px;font-weight:700;margin:4px 0;font-variant-numeric:tabular-nums;word-break:break-word}
.sub{color:var(--muted);min-height:1.45em}
.btns{display:flex;flex-wrap:wrap;gap:8px}
button{font:inherit;padding:9px 14px;border:1px solid var(--line);border-radius:9px;background:var(--card);color:var(--text);cursor:pointer}
button:hover:not(:disabled){border-color:var(--muted)}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff}
button.on{background:var(--press);border-color:var(--press);color:#fff}
button:disabled{opacity:.4;cursor:default}
.nodes{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:12px}
.node{transition:box-shadow .12s,opacity .2s}
.node.off{opacity:.5}
.node.pressed{box-shadow:inset 0 0 0 3px var(--press)}
.node.winner{box-shadow:inset 0 0 0 3px var(--gold)}
.nm{font-size:18px;font-weight:650;word-break:break-word}
.mac{font:12px ui-monospace,Consolas,monospace;color:var(--muted)}
.badges{display:flex;flex-wrap:wrap;gap:6px;margin:8px 0}
.badge{font-size:11px;padding:1px 8px;border:1px solid var(--line);border-radius:99px;color:var(--muted)}
.badge.ok{color:var(--ok);border-color:currentColor}
.badge.bad{color:var(--bad);border-color:currentColor}
.badge.m{color:var(--accent);border-color:currentColor}
.badge.p{color:#fff;background:var(--press);border-color:var(--press)}
.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;margin:6px 0 12px;font-size:12px;color:var(--muted)}
.stats b{display:block;font-size:18px;color:var(--text);font-variant-numeric:tabular-nums}
.node .btns button{padding:6px 10px;font-size:13px}
.row{display:flex;flex-wrap:wrap;gap:12px;align-items:end}
label{display:grid;gap:4px;font-size:13px;color:var(--muted)}
input{width:130px;font:inherit;padding:8px 10px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--text)}
.scroll{overflow-x:auto}
table{width:100%;border-collapse:collapse;font-size:14px}
th,td{padding:6px 8px;text-align:left;border-bottom:1px solid var(--line);white-space:nowrap}
th{font-size:12px;font-weight:500;color:var(--muted)}
.num{font-variant-numeric:tabular-nums}
.empty{color:var(--muted);font-size:14px}
</style>
</head>
<body>
<header>
  <h1>Bitwy Klas – przyciski</h1>
  <div class="conn"><span class="dot" id="dot"></span><span id="conn">łączenie…</span></div>
</header>
<main>
  <section class="card status" id="status">
    <div class="label" id="stLabel">—</div>
    <div class="big" id="stBig">—</div>
    <div class="sub" id="stSub"></div>
  </section>

  <section class="card">
    <div class="btns">
      <button class="primary" id="bStart">Start</button>
      <button id="bNext">Następna runda</button>
      <button id="bStop">Stop</button>
      <button id="bTest">Tryb testu</button>
      <button id="bReset">Reset wyników</button>
    </div>
  </section>

  <section>
    <h2 id="nodesTitle">Przyciski</h2>
    <div class="nodes" id="nodes"></div>
  </section>

  <section class="card">
    <h2>Ustawienia</h2>
    <div class="row">
      <label>Czas świecenia zwycięzcy [s]<input id="cLock" type="number" min="1" max="30" step="0.5"></label>
      <label>Przycisków do startu gry<input id="cExp" type="number" min="2" max="10" step="1"></label>
      <button id="bCfg">Zapisz</button>
    </div>
  </section>

  <section class="card">
    <h2>Historia rund</h2>
    <div id="hist" class="scroll"><div class="empty">Brak rozegranych rund.</div></div>
  </section>
</main>
<script>
const $=id=>document.getElementById(id);
let ws=null,S=null,rxAt=0;

function send(o){if(ws&&ws.readyState===1)ws.send(JSON.stringify(o))}
function connect(){
  ws=new WebSocket(`ws://${location.host}/ws`);
  ws.onopen=()=>{$('dot').classList.add('ok');$('conn').textContent='połączono'};
  ws.onclose=()=>{$('dot').classList.remove('ok');$('conn').textContent='rozłączono – ponawiam…';setTimeout(connect,1000)};
  ws.onmessage=e=>{
    const m=JSON.parse(e.data);
    if(m.t==='s'){S=m;rxAt=performance.now();render()}
    else if(m.t==='h')renderHist(m.items);
  };
}
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function fmt(ms){ms=Math.max(0,ms);const m=Math.floor(ms/60000),s=Math.floor(ms/1000)%60,t=Math.floor(ms/100)%10;return `${m}:${String(s).padStart(2,'0')}.${t}`}

function renderStatus(){
  if(!S)return;
  const dt=performance.now()-rxAt;
  let label,big,sub;
  switch(S.st){
    case 0:label='Oczekiwanie na przyciski';big=`${S.online} / ${S.expected}`;sub='Gra ruszy sama, gdy połączą się wszystkie przyciski (albo kliknij Start).';break;
    case 1:{
      label=`Runda ${S.round}`;
      const left=S.elapsed<0?S.startsIn-dt:-(S.elapsed+dt);
      if(left>0){big='Uwaga…';sub='Start rundy za chwilę'}
      else{big=fmt(-left);sub='Czekam na pierwsze kliknięcie'}
      break}
    case 2:{
      label=`Runda ${S.round} – wygrywa`;big=S.winnerName;
      const r=S.reaction!=null?`czas reakcji ${S.reaction.toFixed(1)} ms · `:'';
      sub=`${r}nowa runda za ${Math.max(0,(S.lockLeft-dt)/1000).toFixed(1)} s`;break}
    case 3:label='Tryb testu';big='Test';sub='Wciśnij dowolny przycisk – zaświeci jego dioda i podświetli się karta.';break;
    default:label='Gra zatrzymana';big='Stop';sub='Kliknij Start, aby rozpocząć nową rundę.';
  }
  $('status').classList.toggle('win',S.st===2);
  $('stLabel').textContent=label;$('stBig').textContent=big;$('stSub').textContent=sub;
}

function nodeEl(id){
  let el=$('nodes').querySelector(`[data-id="${id}"]`);
  if(el)return el;
  el=document.createElement('div');el.className='card node';el.dataset.id=id;
  el.innerHTML=`<div class="nm"></div><div class="mac"></div><div class="badges"></div>
  <div class="stats"><div><b class="wins"></b>wygrane</div><div><b class="q"></b>łącze</div><div><b class="rtt"></b>opóźnienie</div></div>
  <div class="btns"><button data-a="identify">Identyfikuj</button><button data-a="name">Zmień nazwę</button><button data-a="enable"></button></div>`;
  $('nodes').appendChild(el);return el;
}

function render(){
  renderStatus();
  const ids=new Set();
  for(const n of S.nodes){
    ids.add(n.id);
    const el=nodeEl(n.id);
    el.querySelector('.nm').textContent=n.name;
    el.querySelector('.mac').textContent=n.id;
    let b='';
    if(n.self)b+='<span class="badge m">MASTER</span>';
    b+=n.on?'<span class="badge ok">online</span>':'<span class="badge bad">offline</span>';
    if(!n.en)b+='<span class="badge bad">wyłączony z gry</span>';
    if(n.down)b+='<span class="badge p">wciśnięty</span>';
    el.querySelector('.badges').innerHTML=b;
    el.querySelector('.wins').textContent=n.wins;
    el.querySelector('.q').textContent=n.on?`${n.q}%`:'—';
    el.querySelector('.rtt').textContent=n.self?'—':(n.on&&n.rtt?`${(n.rtt/1000).toFixed(2)} ms`:'—');
    el.classList.toggle('off',!n.on);
    el.classList.toggle('winner',n.win);
    el.classList.toggle('pressed',!n.win&&(n.down||(n.press>=0&&n.press<600)));
    const en=el.querySelector('[data-a="enable"]');en.textContent=n.en?'Wyłącz z gry':'Włącz do gry';
    el.querySelectorAll('button').forEach(x=>x.disabled=!n.on);
  }
  $('nodes').querySelectorAll('.node').forEach(el=>{if(!ids.has(el.dataset.id))el.remove()});
  $('nodesTitle').textContent=`Przyciski (${S.online} online)`;

  const playing=S.st===1||S.st===2;
  $('bStart').disabled=playing;
  $('bStop').disabled=S.st===4;
  const t=$('bTest');t.classList.toggle('on',S.st===3);t.textContent=S.st===3?'Zakończ test':'Tryb testu';
  if(document.activeElement!==$('cLock'))$('cLock').value=S.lockMs/1000;
  if(document.activeElement!==$('cExp'))$('cExp').value=S.expected;
}

function renderHist(items){
  if(!items.length){$('hist').innerHTML='<div class="empty">Brak rozegranych rund.</div>';return}
  $('hist').innerHTML='<table><thead><tr><th>Runda</th><th>Zwycięzca</th><th>Reakcja</th><th>2. miejsce</th><th>Różnica</th></tr></thead><tbody>'+
    items.map(i=>`<tr><td class="num">${i.r}</td><td>${esc(i.w)}</td><td class="num">${i.rt.toFixed(1)} ms</td>`+
      `<td>${i.s!=null?esc(i.s):'—'}</td><td class="num">${i.m!=null?'+'+i.m.toFixed(2)+' ms':'—'}</td></tr>`).join('')+
    '</tbody></table>';
}

function trimName(v){const a=Array.from(v.trim());const enc=new TextEncoder();while(a.length&&enc.encode(a.join('')).length>20)a.pop();return a.join('')}

$('nodes').addEventListener('click',e=>{
  const b=e.target.closest('button');if(!b||!S)return;
  const id=b.closest('.node').dataset.id,n=S.nodes.find(x=>x.id===id);if(!n)return;
  const a=b.dataset.a;
  if(a==='identify')send({c:'identify',id});
  else if(a==='enable')send({c:'enable',id,on:!n.en});
  else if(a==='name'){
    const v=prompt('Nowa nazwa przycisku (np. Klasa 1A):',n.name);
    if(v===null)return;const name=trimName(v);if(name)send({c:'name',id,name});
  }
});
$('bStart').onclick=()=>send({c:'start'});
$('bNext').onclick=()=>send({c:'next'});
$('bStop').onclick=()=>send({c:'stop'});
$('bTest').onclick=()=>send({c:'test',on:!(S&&S.st===3)});
$('bReset').onclick=()=>{if(confirm('Wyzerować wygrane i historię rund?'))send({c:'reset'})};
$('bCfg').onclick=()=>{
  const lockMs=Math.round(parseFloat($('cLock').value)*1000),expected=parseInt($('cExp').value,10);
  if(!(lockMs>=1000&&lockMs<=30000)){alert('Czas świecenia: od 1 do 30 s');return}
  if(!(expected>=2&&expected<=10)){alert('Liczba przycisków: od 2 do 10');return}
  send({c:'cfg',lockMs,expected});document.activeElement.blur();
};
(function tick(){renderStatus();requestAnimationFrame(tick)})();
connect();
</script>
</body>
</html>
)rawliteral";

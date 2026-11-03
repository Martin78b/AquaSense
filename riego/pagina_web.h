// Página web: una sola página con CSS y JavaScript embebidos, sin recursos externos.
#pragma once

#include <Arduino.h>

const char PAGINA[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Riego</title>
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 256 256'%3E%3Cdefs%3E%3CclipPath id='g'%3E%3Cpath d='M128 18C152 60 212 104 212 156A84 84 0 0 1 44 156C44 104 104 60 128 18Z'/%3E%3C/clipPath%3E%3C/defs%3E%3Cpath d='M128 18C152 60 212 104 212 156A84 84 0 0 1 44 156C44 104 104 60 128 18Z' fill='%234a6fa5'/%3E%3Cg clip-path='url(%23g)'%3E%3Cpath d='M30 206Q79 192 128 204T226 200V260H30Z' fill='%238a6a4f'/%3E%3C/g%3E%3Cpath d='M128 206V130' stroke='%23dff0e3' stroke-width='9' stroke-linecap='round'/%3E%3Cpath d='M128 164C102 164 84 148 80 120C106 118 126 134 128 164Z' fill='%23dff0e3'/%3E%3Cpath d='M128 144C130 114 152 94 184 94C182 124 158 144 128 144Z' fill='%239fd3aa'/%3E%3C/svg%3E">
<style>
:root{color-scheme:light;--fondo:#eef3ef;--hoja:#2f5d3a;--hortensia:#4a6fa5;--hortensia-claro:#dfe7f3;--tierra:#8a6a4f;--tarjeta:#fff;--texto:#1d2a20;--suave:#5d6b61;--linea:#cfd9d1;--alerta:#a8442a}
@media(prefers-color-scheme:dark){:root:not([data-tema="claro"]){--fondo:#141a16;--hoja:#7fbf8e;--hortensia:#8fb0e0;--hortensia-claro:#24304a;--tierra:#c49a76;--tarjeta:#1d2520;--texto:#e6ece7;--suave:#9fb0a4;--linea:#33413a;--alerta:#e58a6c;color-scheme:dark}}
:root[data-tema="oscuro"]{--fondo:#141a16;--hoja:#7fbf8e;--hortensia:#8fb0e0;--hortensia-claro:#24304a;--tierra:#c49a76;--tarjeta:#1d2520;--texto:#e6ece7;--suave:#9fb0a4;--linea:#33413a;--alerta:#e58a6c;color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:var(--fondo);color:var(--texto);line-height:1.45}
main{max-width:620px;margin:auto;padding:16px 14px 40px}
h1{font-size:26px;margin:4px 0 0;color:var(--hoja);letter-spacing:-.5px;display:flex;align-items:center;gap:8px}
.logo{width:36px;height:36px;flex:0 0 auto}
h2{font-size:17px;margin:26px 0 10px;color:var(--hoja)}
.suave{color:var(--suave);font-size:14px}
.maceta{background:var(--tarjeta);border:1px solid var(--linea);border-left:6px solid var(--hortensia);border-radius:0 12px 12px 0;padding:12px 14px;margin-bottom:10px}
.maceta.activa{border-left-color:var(--hoja)}
.nombre{font-size:18px;font-weight:600}
.humedad{position:relative;height:14px;background:var(--hortensia-claro);border-radius:7px;margin:8px 0 4px;overflow:hidden}
.humedad .agua{height:100%;background:var(--hortensia)}
.humedad .umbral{position:absolute;top:0;bottom:0;width:2px;background:var(--tierra)}
.panel{background:var(--tarjeta);border:1px solid var(--linea);border-radius:12px;padding:10px 14px;margin-bottom:10px}
.aviso{background:var(--hortensia-claro);color:var(--hortensia);border-radius:12px;padding:10px 14px;margin-bottom:10px;font-size:14px}
.campo{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:7px 0;border-bottom:1px solid var(--linea);font-size:15px}
.campo:last-child{border-bottom:0}
.campo.vertical{flex-direction:column;align-items:stretch;gap:6px}
input{font:inherit;padding:6px 8px;border:1px solid var(--linea);border-radius:6px;background:var(--fondo);color:var(--texto);width:100px}
input[type=checkbox]{width:22px;height:22px;flex:0 0 auto;accent-color:var(--hoja)}
input.ancho{width:150px}
input.largo{width:100%}
button{font:inherit;font-weight:600;border:0;border-radius:8px;padding:10px 16px;background:var(--hoja);color:var(--tarjeta);cursor:pointer}
button.chico{padding:6px 10px;font-size:13px;background:var(--hortensia-claro);color:var(--hortensia)}
button.detener{background:var(--alerta)}
button:disabled{opacity:.45;cursor:default}
button:focus-visible,input:focus-visible{outline:3px solid var(--hortensia);outline-offset:2px}
.fila{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.msg{margin-top:10px;font-size:14px;min-height:1.2em}
.msg.error{color:var(--alerta)}
.pestanas{display:flex;flex-wrap:wrap;gap:6px;margin:14px 0 0}
.pestanas button{background:transparent;color:var(--suave);border:1px solid var(--linea);border-radius:20px;padding:7px 14px}
.pestanas button.sel{background:var(--hoja);color:var(--tarjeta);border-color:var(--hoja)}
.tab{display:none}
.tab.sel{display:block}
</style>
</head>
<body><main>
<div class="fila" style="justify-content:space-between"><h1><svg class="logo" viewBox="0 0 256 256" aria-hidden="true"><defs><clipPath id="gota-logo"><path d="M128 18C152 60 212 104 212 156A84 84 0 0 1 44 156C44 104 104 60 128 18Z"/></clipPath></defs><path d="M128 18C152 60 212 104 212 156A84 84 0 0 1 44 156C44 104 104 60 128 18Z" fill="#4a6fa5"/><g clip-path="url(#gota-logo)"><path d="M30 206Q79 192 128 204T226 200V260H30Z" fill="#8a6a4f"/><path d="M78 132C80 108 96 84 112 62" fill="none" stroke="#fff" stroke-opacity=".35" stroke-width="9" stroke-linecap="round"/></g><path d="M128 206V130" stroke="#dff0e3" stroke-width="9" stroke-linecap="round"/><path d="M128 164C102 164 84 148 80 120C106 118 126 134 128 164Z" fill="#dff0e3"/><path d="M128 144C130 114 152 94 184 94C182 124 158 144 128 144Z" fill="#9fd3aa"/></svg>Riego</h1><button class="chico" id="tema" onclick="cambiarTema()">Tema: automático</button></div>
<div class="suave" id="hora">Conectando con el ESP32…</div>
<nav class="pestanas" id="pestanas"><button data-t="estado" class="sel" onclick="mostrar('estado')">Estado</button><button data-t="general" onclick="mostrar('general')">Horarios</button><button data-t="telegram" onclick="mostrar('telegram')">Notificaciones</button><button data-t="sistema" onclick="mostrar('sistema')">Sistema</button></nav>

<section class="tab sel" id="t-estado">
<h2>Macetas</h2>
<div id="pausaBox"></div>
<div id="estado"></div>
<div class="fila"><button class="detener" onclick="detener()">Detener riego</button><button class="chico" id="btnPausa" onclick="pausar()">Pausar riego automático</button></div>
</section>

<section class="tab" id="t-general">
<h2>Horarios</h2>
<div class="panel"><div class="suave" style="padding:4px 0 6px">Se usan para las macetas que no tienen el sensor activado.</div><div id="horarios"></div></div>
<h2>Seguridad</h2>
<div class="panel"><div class="campo"><span>Tiempo máximo de un riego (s)</span><input type="number" id="maxs" min="1" max="300"></div></div>
</section>

<section class="tab" id="t-telegram">
<h2>Notificaciones y comandos</h2>
<div class="aviso">Para crear tu bot, abrí Telegram, buscá @BotFather y mandale /newbot. Al terminar te da un token: pegalo acá, activá Telegram y guardá. Después escribile cualquier mensaje a tu bot y acá va a aparecer tu ID de chat para autorizarlo.</div>
<div class="panel">
<div class="campo"><label class="fila" for="tgOn">Activar Telegram</label><input type="checkbox" id="tgOn"></div>
<div class="campo vertical"><label for="tgTok">Token del bot</label><input class="largo" type="password" id="tgTok" autocomplete="off" placeholder="123456789:AAH4k2xYz..."></div>
<div class="campo vertical"><label for="tgChat">ID del chat autorizado</label><input class="largo" id="tgChat" inputmode="numeric" placeholder="123456789"></div>
<div class="campo"><span>Conexión</span><span id="tgEstado" class="suave" style="text-align:right">…</span></div>
<div id="tgDesc"></div>
</div>
<h2>Avisos</h2>
<div class="panel">
<div class="campo"><label for="nRie">Cada vez que se riega una maceta</label><input type="checkbox" id="nRie"></div>
<div class="campo"><label for="nMax">Si una maceta con sensor llega al máximo de riegos del día</label><input type="checkbox" id="nMax"></div>
<div class="campo"><label for="nIni">Cuando se enciende el sistema, por ejemplo después de un corte de luz</label><input type="checkbox" id="nIni"></div>
<div class="campo"><label for="nRes">Resumen diario a las</label><span class="fila" style="flex-wrap:nowrap"><input type="time" id="rHora"><input type="checkbox" id="nRes"></span></div>
</div>
<h2>Comandos</h2>
<div class="panel">
<div class="campo"><label for="tgCmd">Aceptar comandos desde Telegram</label><input type="checkbox" id="tgCmd"></div>
<div class="suave" style="padding:8px 0">/estado muestra cómo están las macetas. /regar 1, /regar kiwi o /regar todas riegan con la cantidad configurada. /detener corta el riego en curso. /pausa y /reanudar frenan o retoman el riego automático. /horarios lista los horarios y /ayuda muestra los comandos. Solo responde a tu chat autorizado.</div>
</div>
<div class="fila"><button class="chico" onclick="probarTg()">Enviar mensaje de prueba</button></div>
<div class="msg" id="msgTg"></div>
</section>

<section class="tab" id="t-sistema">
<h2>Sistema</h2>
<div class="panel">
<div class="campo"><span>Versión instalada</span><span style="font-weight:600" id="sisVer">v1.0.0</span></div>
<div class="campo"><span>Repositorio GitHub</span><span class="suave" id="sisRepo">Martin78b/AquaSense</span></div>
<div class="campo"><span>Red WiFi conectada</span><span class="suave" id="sisWifi">…</span></div>
<div class="campo"><span>Memoria libre (Heap)</span><span class="suave" id="sisHeap">…</span></div>
</div>

<h2>Red WiFi (Memoria NVS)</h2>
<div class="panel">
<div class="suave" style="padding:4px 0 10px">Los datos de tu WiFi quedan guardados permanentemente en la memoria NVS del ESP32 y sobreviven a cualquier actualización OTA. Si cambiás de red o router, podés actualizarlos acá:</div>
<div class="campo vertical">
<label for="wfSsid">Nombre de red (SSID)</label>
<input class="largo" id="wfSsid" placeholder="MiRedWiFi">
</div>
<div class="campo vertical" style="border-bottom:none">
<label for="wfPass">Contraseña de red</label>
<input class="largo" type="password" id="wfPass" placeholder="Nueva clave (dejá vacío para no cambiar)">
</div>
<div class="fila" style="margin-top:10px">
<button class="chico" onclick="guardarWifi()">Guardar WiFi y conectar</button>
</div>
<div class="msg" id="msgWifi"></div>
</div>

<h2>Actualizar desde GitHub Releases</h2>
<div class="panel">
<div class="suave" style="padding:4px 0 10px">Verifica si hay una nueva versión publicada en GitHub y la descarga e instala automáticamente por WiFi.</div>
<div class="fila"><button class="chico" id="btnCheckGh" onclick="verificarGh()">Buscar actualización en GitHub</button></div>
<div id="ghRes" style="margin-top:10px"></div>
</div>

<h2>Actualizar desde archivo local (.bin)</h2>
<div class="panel">
<div class="suave" style="padding:4px 0 10px">Instalá directamente un archivo <code>firmware.bin</code> compilado en tu computadora (desde Arduino IDE o PlatformIO).</div>
<div class="campo vertical" style="border-bottom:none">
<input type="file" id="archivoBin" accept=".bin" style="width:100%">
<div class="fila" style="margin-top:10px">
<button id="btnSubirBin" onclick="subirBinLocal()">Subir e instalar firmware</button>
</div>
<div id="progresoLocal" style="display:none;margin-top:12px;width:100%">
<div class="humedad"><div class="agua" id="barraProgreso" style="width:0%"></div></div>
<div class="suave" id="txtProgreso" style="margin-top:4px">Subiendo 0%…</div>
</div>
</div>
</div>
<div class="msg" id="msgSis"></div>
</section>

<div id="config"></div>

<div id="guardarBox" style="display:none;margin-top:16px">
<button onclick="guardar()">Guardar ajustes</button>
<div class="suave" style="margin-top:6px">Guarda los cambios de todas las pestañas.</div>
<div class="msg" id="msg"></div>
</div>
</main>
<script>
let est=null,cfg=null,pest='estado';
const TEMAS=['auto','claro','oscuro'],NOMBRE_TEMA={auto:'automático',claro:'claro',oscuro:'oscuro'};
let tema='auto';
try{tema=localStorage.getItem('tema')||'auto';}catch(e){}
function aplicarTema(){
  if(tema==='auto')document.documentElement.removeAttribute('data-tema');
  else document.documentElement.setAttribute('data-tema',tema);
  document.getElementById('tema').textContent='Tema: '+NOMBRE_TEMA[tema];
}
function cambiarTema(){tema=TEMAS[(TEMAS.indexOf(tema)+1)%3];try{localStorage.setItem('tema',tema);}catch(e){}aplicarTema();}
aplicarTema();

const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const dos=n=>String(n).padStart(2,'0');

function mostrar(id){
  pest=id;
  document.querySelectorAll('.tab').forEach(t=>t.classList.toggle('sel',t.id==='t-'+id));
  document.querySelectorAll('#pestanas button').forEach(b=>b.classList.toggle('sel',b.dataset.t===id));
  $('guardarBox').style.display=(id==='estado'||id==='sistema')?'none':'block';
  if(id==='sistema')cargarInfoSistema();
}
function pintarPestanas(){
  const lista=[['estado','Estado'],['general','Horarios'],['telegram','Notificaciones'],['sistema','Sistema']].concat(cfg.macetas.map((m,i)=>['m'+i,m.nombre]));
  $('pestanas').innerHTML=lista.map(([id,n])=>`<button data-t="${id}" onclick="mostrar('${id}')">${esc(n)}</button>`).join('');
  mostrar(pest);
}
function irA(el){const t=el.closest('.tab');if(t)mostrar(t.id.slice(2));el.focus();}

async function cargarEstado(){
  try{const r=await fetch('/api/estado');est=await r.json();pintarEstado();}
  catch(e){$('hora').textContent='Sin conexión con el ESP32. Revisá que estés en la misma red WiFi.';}
}
function pintarEstado(){
  $('hora').textContent='Hora del sistema: '+est.hora+'. Señal WiFi: '+est.rssi+' dBm';
  $('pausaBox').innerHTML=est.pausa?'<div class="aviso">El riego automático está en pausa. Los riegos manuales siguen funcionando.</div>':'';
  $('btnPausa').textContent=est.pausa?'Reanudar riego automático':'Pausar riego automático';
  $('tgEstado').textContent=est.tgEstado;
  $('tgDesc').innerHTML=est.tgDesconocido
    ?`<div class="campo"><span class="suave">Te escribió un chat sin autorizar, con el ID ${esc(est.tgDesconocido)}</span><button class="chico" onclick="autorizar('${esc(est.tgDesconocido)}')">Autorizar</button></div>`:'';
  $('estado').innerHTML=est.macetas.map((m,i)=>{
    const enCurso=est.regando===i;
    const umbral=cfg?cfg.macetas[i].umbral:0;
    const hum=m.sensor
      ?`<div class="humedad"><div class="agua" style="width:${m.pct}%"></div><div class="umbral" style="left:${umbral}%"></div></div><div class="suave">Humedad ${m.pct} %, riega por debajo de ${umbral} % (lectura ${m.raw})</div>`
      :`<div class="suave">Riego por horario. Lectura del sensor: ${m.raw}</div>`;
    const ahora=enCurso?`Regando, faltan ${est.restante} s`:(m.pendiente?'En espera para regar':`Último riego: ${esc(m.ultimo)}`);
    return `<div class="maceta${enCurso?' activa':''}"><div class="nombre">${esc(m.nombre)}</div>${hum}
<div class="suave">Hoy: ${m.mlHoy} ml en total, ${m.riegosHoy} riegos automáticos</div><div class="suave">${ahora}</div>
<div class="fila" style="margin-top:10px"><button ${enCurso||m.pendiente?'disabled':''} onclick="regar(${i})">Regar ${m.ml} ml</button></div></div>`;
  }).join('');
}
async function regar(i){await fetch('/api/regar',{method:'POST',body:new URLSearchParams({m:i})});cargarEstado();}
async function detener(){await fetch('/api/detener',{method:'POST'});cargarEstado();}
async function pausar(){await fetch('/api/pausa',{method:'POST',body:new URLSearchParams({on:est&&est.pausa?0:1})});cargarEstado();}
function autorizar(id){$('tgChat').value=id;aviso('ID cargado. Guardá los ajustes para autorizarlo.',false);}

async function probarTg(){
  const el=$('msgTg');el.className='msg';el.textContent='Enviando…';
  try{const r=await fetch('/api/telegram/prueba',{method:'POST'});el.textContent=await r.text();el.className='msg'+(r.ok?'':' error');}
  catch(e){el.textContent='No se pudo conectar con el ESP32.';el.className='msg error';}
}

async function cargarConfig(){
  const r=await fetch('/api/config');cfg=await r.json();
  $('maxs').value=cfg.maxs;
  $('horarios').innerHTML=cfg.horarios.map((h,j)=>`<div class="campo"><label class="fila"><input type="checkbox" id="ha${j}" ${h.a?'checked':''}>Horario ${j+1}</label><input type="time" id="ht${j}" value="${dos(h.h)}:${dos(h.m)}"></div>`).join('');
  const t=cfg.tg;
  $('tgOn').checked=t.on;$('tgTok').value='';
  $('tgTok').placeholder=t.tokenGuardado?'Token guardado. Dejalo vacío para no cambiarlo.':'123456789:AAH4k2xYz...';
  $('tgChat').value=t.chat;$('tgCmd').checked=t.cmd;$('nRie').checked=t.riego;$('nMax').checked=t.max;
  $('nIni').checked=t.ini;$('nRes').checked=t.res;$('rHora').value=dos(t.rh)+':'+dos(t.rm);
  $('config').innerHTML=cfg.macetas.map((m,i)=>`<section class="tab" id="t-m${i}"><h2>Ajustes de ${esc(m.nombre)}</h2><div class="panel">
<div class="campo"><span>Nombre</span><input class="ancho" id="n${i}" maxlength="20" value="${esc(m.nombre)}"></div>
<div class="campo"><span>Agua por riego (ml)</span><input type="number" id="ml${i}" min="10" max="3000" value="${m.ml}"></div>
<div class="campo"><span>Caudal medido de la bomba (ml/s)</span><input type="number" step="0.1" id="q${i}" min="1" max="200" value="${m.caudal}"></div>
<div class="campo"><label class="fila" for="s${i}">Regar según el sensor</label><input type="checkbox" id="s${i}" ${m.sensor?'checked':''}></div>
<div class="campo"><span>Regar si la humedad baja de (%)</span><input type="number" id="u${i}" min="0" max="100" value="${m.umbral}"></div>
<div class="campo"><span>Lectura con tierra seca</span><span class="fila"><input type="number" id="as${i}" min="0" max="4095" value="${m.seco}"><button class="chico" onclick="tomar('as',${i})">Usar actual</button></span></div>
<div class="campo"><span>Lectura con tierra mojada</span><span class="fila"><input type="number" id="am${i}" min="0" max="4095" value="${m.mojado}"><button class="chico" onclick="tomar('am',${i})">Usar actual</button></span></div>
<div class="campo"><span>Espera mínima entre riegos (min)</span><input type="number" id="e${i}" min="0" max="1440" value="${m.espera}"></div>
<div class="campo"><span>Máximo de riegos automáticos por día</span><input type="number" id="x${i}" min="0" max="24" value="${m.maxDia}"></div>
</div></section>`).join('');
  pintarPestanas();
  if(est)pintarEstado();
}
function tomar(p,i){if(est){$(p+i).value=est.macetas[i].raw;limpiar();}}
function limpiar(){$('msg').textContent='';$('msg').className='msg';}
function aviso(t,error){$('msg').textContent=t;$('msg').className='msg'+(error?' error':'');}

async function guardar(){
  const p=new URLSearchParams();let err='';
  const num=(id,min,max,nombre)=>{const el=$(id),v=Number(el.value);
    if(!err&&(el.value===''||isNaN(v)||v<min||v>max)){err=`Revisá "${nombre}": tiene que estar entre ${min} y ${max}.`;irA(el);}
    return v;};
  p.set('maxs',num('maxs',1,300,'Tiempo máximo de un riego'));
  cfg.horarios.forEach((h,j)=>{const t=$('ht'+j).value,activo=$('ha'+j).checked;
    if(!err&&activo&&!t){err=`Completá la hora del horario ${j+1}.`;irA($('ht'+j));}
    const [hh,mm]=(t||'00:00').split(':');p.set('ha'+j,activo?1:0);p.set('hh'+j,Number(hh));p.set('hm'+j,Number(mm));});

  const on=$('tgOn').checked,tok=$('tgTok').value.trim(),chat=$('tgChat').value.trim(),rh=$('rHora').value;
  if(!err&&on&&!tok&&!cfg.tg.tokenGuardado){err='Para activar Telegram, pegá el token de tu bot.';irA($('tgTok'));}
  if(!err&&tok&&!/^\d+:[\w-]{20,}$/.test(tok)){err='El token no tiene el formato esperado. Copialo completo desde @BotFather.';irA($('tgTok'));}
  if(!err&&chat&&!/^-?\d{3,20}$/.test(chat)){err='El ID del chat tiene que ser un número, por ejemplo 123456789.';irA($('tgChat'));}
  if(!err&&$('nRes').checked&&!rh){err='Elegí la hora del resumen diario.';irA($('rHora'));}
  const [rH,rM]=(rh||'21:00').split(':');
  p.set('tgOn',on?1:0);if(tok)p.set('tgTok',tok);p.set('tgChat',chat);
  p.set('tgCmd',$('tgCmd').checked?1:0);p.set('nRie',$('nRie').checked?1:0);p.set('nMax',$('nMax').checked?1:0);
  p.set('nIni',$('nIni').checked?1:0);p.set('nRes',$('nRes').checked?1:0);p.set('rH',Number(rH));p.set('rM',Number(rM));

  cfg.macetas.forEach((m,i)=>{const n=$('n'+i).value.trim();
    if(!err&&!n){err=`La maceta ${i+1} necesita un nombre.`;irA($('n'+i));}
    p.set('n'+i,n);
    p.set('ml'+i,num('ml'+i,10,3000,'Agua por riego'));
    p.set('q'+i,num('q'+i,1,200,'Caudal medido'));
    p.set('s'+i,$('s'+i).checked?1:0);
    p.set('u'+i,num('u'+i,0,100,'Humedad mínima'));
    const s=num('as'+i,0,4095,'Lectura con tierra seca'),w=num('am'+i,0,4095,'Lectura con tierra mojada');
    if(!err&&s===w){err=`En ${n}, las lecturas con tierra seca y mojada tienen que ser distintas.`;irA($('am'+i));}
    p.set('as'+i,s);p.set('am'+i,w);
    p.set('e'+i,num('e'+i,0,1440,'Espera mínima'));
    p.set('x'+i,num('x'+i,0,24,'Máximo de riegos por día'));});
  if(err){aviso(err,true);return;}
  try{const r=await fetch('/api/config',{method:'POST',body:p});
    if(r.ok){aviso('Ajustes guardados.',false);cargarConfig();}else aviso('El ESP32 rechazó los ajustes. Volvé a intentarlo.',true);}
  catch(e){aviso('No se pudo conectar con el ESP32. Los ajustes no se guardaron.',true);}
}

async function cargarInfoSistema(){
  try{
    const r=await fetch('/api/ota/info');
    const d=await r.json();
    $('sisVer').textContent='v'+d.version;
    $('sisRepo').textContent=d.repo;
    $('sisHeap').textContent=Math.round(d.heap/1024)+' KB';
    if(d.wifiSsid) $('wfSsid').value=d.wifiSsid;
    if(d.wifiIp) $('sisWifi').textContent=d.wifiSsid+' ('+d.wifiIp+')';
  }catch(e){}
}

async function guardarWifi(){
  const s=$('wfSsid').value.trim(), p=$('wfPass').value;
  if(!s){alert('Ingresá el nombre de la red WiFi.');return;}
  if(!confirm(`¿Guardar red WiFi "${s}" en NVS y reiniciar el ESP32 para conectar?`)) return;
  const res=$('msgWifi');res.textContent='Guardando y reiniciando…';res.className='msg';
  try{
    const body=new URLSearchParams({ssid:s,pass:p});
    const r=await fetch('/api/wifi',{method:'POST',body});
    if(r.ok){
      esperarReinicio('Nueva red WiFi guardada.');
    }else{
      res.textContent=await r.text();res.className='msg error';
    }
  }catch(e){
    res.textContent='Error al conectar con el ESP32.';res.className='msg error';
  }
}

async function verificarGh(){
  const res=$('ghRes');res.innerHTML='<div class="suave">Consultando GitHub Releases…</div>';
  try{
    const r=await fetch('/api/ota/check');
    const d=await r.json();
    if(!d.ok){
      res.innerHTML=`<div class="msg error">No se pudo verificar: ${esc(d.error)}</div>`;
      return;
    }
    if(!d.nueva){
      res.innerHTML=`<div class="aviso" style="margin-top:6px">✅ AquaSense está al día. Tenés instalada la versión más reciente (${esc(d.actual)}).</div>`;
    }else{
      res.innerHTML=`<div class="panel" style="background:var(--fondo);margin-top:6px">
        <div style="font-weight:600;color:var(--hoja)">🚀 ¡Nueva versión disponible: ${esc(d.disponible)}!</div>
        <div class="suave" style="margin:6px 0">${esc(d.notas||'Sin notas de versión')}</div>
        <div class="fila" style="margin-top:8px">
          <button onclick="instalarGh('${esc(d.disponible)}')">Instalar ${esc(d.disponible)} desde GitHub</button>
        </div>
      </div>`;
    }
  }catch(e){
    res.innerHTML='<div class="msg error">Error al conectar con el ESP32.</div>';
  }
}

async function instalarGh(ver){
  if(!confirm(`¿Iniciar actualización a ${ver}? Se detendrá cualquier riego en curso y el ESP32 se reiniciará al finalizar.`)) return;
  const res=$('ghRes');
  res.innerHTML='<div class="aviso">⏳ Descargando e instalando firmware desde GitHub… La placa se reiniciará en unos 20-30 segundos. No cortes la energía.</div>';
  try{
    await fetch('/api/ota/github',{method:'POST'});
    esperarReinicio('Actualización desde GitHub iniciada.');
  }catch(e){
    res.innerHTML='<div class="msg error">Error de conexión al iniciar actualización.</div>';
  }
}

function subirBinLocal(){
  const file=$('archivoBin').files[0];
  if(!file){alert('Seleccioná un archivo .bin primero.');return;}
  if(!confirm(`¿Instalar ${file.name}? Se detendrá cualquier riego en curso y el ESP32 se reiniciará.`)) return;

  const xhr=new XMLHttpRequest();
  const form=new FormData();
  form.append('update',file);

  $('progresoLocal').style.display='block';
  $('btnSubirBin').disabled=true;

  xhr.upload.addEventListener('progress',e=>{
    if(e.lengthComputable){
      const p=Math.round((e.loaded/e.total)*100);
      $('barraProgreso').style.width=p+'%';
      $('txtProgreso').textContent='Subiendo '+p+'% ('+Math.round(e.loaded/1024)+' KB / '+Math.round(e.total/1024)+' KB)...';
    }
  });

  xhr.onload=()=>{
    if(xhr.status===200){
      $('txtProgreso').textContent='¡Subida completa! Reiniciando ESP32...';
      esperarReinicio('Firmware subido con éxito.');
    }else{
      $('progresoLocal').style.display='none';
      $('btnSubirBin').disabled=false;
      alert('Error en actualización: '+xhr.responseText);
    }
  };

  xhr.onerror=()=>{
    $('progresoLocal').style.display='none';
    $('btnSubirBin').disabled=false;
    alert('Error de conexión durante la subida.');
  };

  xhr.open('POST','/api/ota/upload');
  xhr.send(form);
}

function esperarReinicio(motivo){
  $('hora').textContent=motivo+' Esperando que el ESP32 vuelva a estar en línea…';
  let intentos=0;
  const timer=setInterval(async()=>{
    intentos++;
    try{
      const r=await fetch('/api/estado');
      if(r.ok){
        clearInterval(timer);
        alert('AquaSense reiniciado correctamente.');
        location.reload();
      }
    }catch(e){
      if(intentos>35){
        clearInterval(timer);
        $('hora').textContent='Demoró más de lo esperado. Si la red cambió o tardó, recargá la página manualmente.';
      }
    }
  },2000);
}

document.addEventListener('input',limpiar);
cargarConfig().then(cargarEstado);
setInterval(cargarEstado,3000);
</script>
</body></html>
)rawliteral";

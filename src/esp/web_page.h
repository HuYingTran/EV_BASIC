// Trang điều khiển + màn hình trạng thái trên điện thoại (nạp vào flash bằng PROGMEM).
#pragma once
#include <pgmspace.h>

const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="vi"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Xe điện DATN</title>
<style>
:root{--bg:#0f1418;--card:#1a2228;--line:#2d3640;--fg:#e6edf3;--mute:#8b98a5;--ok:#2ea043;--warn:#d29922;--bad:#e5534b;--acc:#388bfd}
*{box-sizing:border-box}
body{margin:0;font:15px system-ui,sans-serif;background:var(--bg);color:var(--fg);user-select:none;-webkit-user-select:none}
main{max-width:520px;margin:auto;padding:12px 16px 24px;display:grid;grid-template-columns:minmax(0,1fr);gap:10px}
.row{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px}
.card{background:var(--card);border-radius:10px;padding:10px}
.lbl{color:var(--mute);font-size:12px}
.val{font-size:22px;font-weight:600;font-variant-numeric:tabular-nums}
#warn{font-weight:600;min-height:40px;display:flex;align-items:center}
.bars{display:flex;gap:10px;align-items:flex-end;height:110px;margin-top:6px}
.bar{flex:1;height:100%;display:flex;flex-direction:column;justify-content:flex-end;align-items:center;gap:2px}
.bar i{display:block;width:100%;border-radius:6px 6px 0 0;background:var(--ok);transition:height .15s}
.bar b{font-variant-numeric:tabular-nums}
button{font:inherit;font-weight:700;border:0;border-radius:10px;padding:14px 6px;color:#fff;background:#30363d}
button.on{background:var(--acc)}
#stop{background:var(--bad)}
#es{background:#5a1d1b;padding:18px}
#es.on{background:var(--bad);outline:3px solid #fff}
#pad{position:relative;width:240px;height:240px;margin:4px auto;border-radius:50%;background:var(--card);border:2px solid var(--line);touch-action:none}
#knob{position:absolute;width:80px;height:80px;border-radius:50%;background:var(--acc);left:78px;top:78px}
#knob.auto{background:var(--ok);transition:left .2s linear,top .2s linear}
#dir{text-align:center;font-weight:600;min-height:20px}
input[type=range]{width:100%}
</style></head><body><main>
<div class="row">
 <div class="card"><div class="lbl">Pin</div><div class="val" id="bat">--</div><div class="lbl" id="vb">-- V</div></div>
 <div class="card"><div class="lbl">Tốc độ</div><div class="val" id="spd">--</div><div class="lbl">km/h</div></div>
 <div class="card"><div class="lbl">Chế độ</div><div class="val" id="md" style="font-size:17px">--</div><div class="lbl" id="lk">Đang kết nối…</div></div>
</div>
<div id="warn" class="card">Đang kết nối…</div>
<div class="card"><div class="lbl">Khoảng cách vật cản (mm)</div><div class="bars">
 <div class="bar"><b id="dl">-</b><i id="bl"></i><span class="lbl">Trái</span></div>
 <div class="bar"><b id="dc">-</b><i id="bc"></i><span class="lbl">Giữa</span></div>
 <div class="bar"><b id="dr">-</b><i id="br"></i><span class="lbl">Phải</span></div>
</div></div>
<div class="row">
 <button id="b1" onclick="setMode(1)">LÁI TAY</button>
 <button id="b2" onclick="setMode(2)">TỰ HÀNH</button>
 <button id="stop" onclick="setMode(0)">DỪNG</button>
</div>
<div id="pad"><div id="knob"></div></div>
<div id="dir"></div>
<div class="card"><span class="lbl">Tốc độ tối đa: <b id="vmv">60</b> %</span>
 <input id="vm" type="range" min="10" max="100" value="60"></div>
<button id="es">DỪNG KHẨN CẤP</button>
</main><script>
const $=id=>document.getElementById(id);
const MODES=['DỪNG','LÁI TAY','TỰ HÀNH'];
const WARN=[[1,'DỪNG KHẨN CẤP','bad'],[256,'LỖI CẢM BIẾN GY-530','bad'],
 [64,'KẸT: không tìm được hướng thoát','bad'],[16,'DỪNG: vật cản quá gần','bad'],[4,'Uno mất lệnh điều khiển','warn'],
 [32,'TTC < 1,5 s: đang giảm tốc','warn'],[8,'Giảm tốc theo khoảng cách','warn'],[512,'Remote đang ghi đè tự hành','warn'],
 [128,'Đã khóa: bấm DỪNG rồi chọn chế độ','warn']];
let ws,mode=0,thr=0,st=0,vmax=60,es=0,last=0;
function connect(){
 ws=new WebSocket('ws://'+location.hostname+':81/');
 ws.onmessage=e=>{last=Date.now();show(JSON.parse(e.data))};
 ws.onclose=()=>setTimeout(connect,1000);
}
connect();
setInterval(()=>{
 if(ws.readyState===1)ws.send(`C,${mode},${thr},${st},${vmax},${es}`);
 if(Date.now()-last>1000){$('lk').textContent='Mất WiFi';$('warn').textContent='Mất kết nối với xe';$('warn').style.background='var(--warn)';follow(null)}
},100);
function setMode(m){mode=m;if(m===0){es=0;$('es').classList.remove('on')}paint()}
function paint(){$('b1').classList.toggle('on',mode===1);$('b2').classList.toggle('on',mode===2)}
$('vm').oninput=e=>{vmax=+e.target.value;$('vmv').textContent=vmax};
$('es').onclick=()=>{es=es?0:1;$('es').classList.toggle('on',!!es);if(es)mode=0;paint()};
function bar(k,d){
 $('d'+k).textContent=d>=4000?'>4000':d;
 const b=$('b'+k);b.style.height=Math.max(4,Math.min(d,2000)/2000*80)+'px';
 b.style.background=d<400?'var(--bad)':d<1500?'var(--warn)':'var(--ok)';
}
function show(t){
 $('bat').textContent=t.p+'%';$('vb').textContent=(t.v/1000).toFixed(1)+' V';
 $('spd').textContent=(Math.abs(t.s)*0.0036).toFixed(1);
 $('md').textContent=MODES[t.m]||'?';
 $('lk').textContent=t.u?'Uno: OK':'Uno: mất tín hiệu';
 bar('l',t.l);bar('c',t.c);bar('r',t.r);
 const w=t.u?WARN.find(x=>t.f&x[0]):[0,'Không nhận được dữ liệu từ ATmega','bad'];
 const el=$('warn');el.textContent=w?w[1]:'Bình thường';
 el.style.background=w?(w[2]==='bad'?'var(--bad)':'var(--warn)'):'var(--card)';
 follow(t);
}
const pad=$('pad'),knob=$('knob');let act=false;
// Tự hành: núm chỉ HIỂN THỊ chuyển động thực của xe, không đổi thr/st nên không tạo lệnh ghi đè.
// pd là PWM bánh sau đã bù vùng chết; trừ PWM_MIN_PCT (config.h) để lấy lại lệnh %, rồi phóng đại
// vì tự hành chỉ chạy tối đa 36 %. sa là góc lái ước lượng (±100 = hết cỡ) -> trục ngang của núm.
const PWM_MIN=39,GAIN=2.5;
const cmdOf=p=>p?Math.sign(p)*Math.max(0,Math.abs(p)-PWM_MIN)/(1-PWM_MIN/100):0;
function moveLabel(V,S){
 const lai=Math.abs(S)<=10?'bánh thẳng':(Math.abs(S)>=90?'lái hết cỡ ':'lái ')+(S>0?'phải':'trái');
 if(!V)return 'Đang đứng yên, '+lai;
 return (V<0?'Đang lùi, ':'Đang tiến, ')+lai;
}
function follow(t){
 if(act)return;
 const auto=!!t&&!!t.u&&t.m===2;
 let x=0,y=0,txt='';
 if(auto){
  const V=cmdOf(t.pd),S=t.sa,m=80;
  x=S/100*m;y=-V*GAIN/100*m;
  const d=Math.hypot(x,y);
  if(d>m){x*=m/d;y*=m/d}
  txt=moveLabel(V,S);
 }
 knob.classList.toggle('auto',auto);
 knob.style.left=(78+x)+'px';knob.style.top=(78+y)+'px';
 $('dir').textContent=txt;
}
function mv(e){
 const r=pad.getBoundingClientRect(),R=r.width/2,m=R-40;
 let x=e.clientX-r.left-R,y=e.clientY-r.top-R;const d=Math.hypot(x,y);
 if(d>m){x*=m/d;y*=m/d}
 knob.style.left=(x+R-40-2)+'px';knob.style.top=(y+R-40-2)+'px';
 st=Math.round(x/m*100);thr=Math.round(-y/m*100);
}
pad.onpointerdown=e=>{act=true;knob.classList.remove('auto');$('dir').textContent='';pad.setPointerCapture(e.pointerId);mv(e)};
pad.onpointermove=e=>{if(act)mv(e)};
pad.onpointerup=pad.onpointercancel=()=>{act=false;thr=st=0;knob.style.left=knob.style.top='78px'};
</script></body></html>)HTML";

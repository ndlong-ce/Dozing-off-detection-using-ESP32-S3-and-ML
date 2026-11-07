#pragma once
// Trang web (HTML + JS) hien camera, trang thai mat, o mat CNN nhin thay.
// Chi duoc include boi web_server.cpp.

static const char INDEX_HTML[] = R"rawliteral(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Eye Monitor</title>
<style>
body{font-family:system-ui,sans-serif;background:#111;color:#eee;text-align:center;margin:0;padding:12px}
img{width:100%;max-width:640px;border-radius:8px;background:#000}
#s{margin:12px auto;padding:14px;max-width:640px;border-radius:8px;font-size:1.3em;font-weight:bold;background:#2a7}
#s.alarm{background:#d22}
#s.none{background:#555}
#d{color:#aaa;font-family:monospace}
</style></head><body>
<h2>ESP32-S3 Eye Monitor</h2>
<img id="v" alt="camera">
<div id="s" class="none">...</div>
<div id="d"></div>
<div style="margin-top:10px;color:#aaa">Ô mắt CNN nhìn thấy (trái | phải)</div>
<img id="c" alt="crops" style="image-rendering:pixelated;width:512px;max-width:100%;aspect-ratio:2/1;background:#000;margin-top:4px">
<script>
document.getElementById('v').src = 'http://' + location.hostname + ':81/stream';
async function poll(){
  try{
    const j = await (await fetch('/status')).json();
    const s = document.getElementById('s');
    if(!j.face){ s.textContent='Không thấy khuôn mặt'; s.className='none'; }
    else if(j.alarm){ s.textContent='NHẮM MẮT - CẢNH BÁO! (LED bật' + (j.buzz ? ' + BUZZER' : '') + ')'; s.className='alarm'; }
    else { s.textContent = j.p_closed>0.15 ? 'Mắt đang nhắm...' : 'Mắt mở'; s.className=''; }
    document.getElementById('d').textContent =
      'P(closed)='+j.p_closed.toFixed(2)+'  cnt='+j.cnt+'  FPS='+j.fps.toFixed(1);
    document.getElementById('c').src = '/crops?' + Date.now();
  }catch(e){}
  setTimeout(poll, 300);
}
poll();
</script></body></html>)rawliteral";

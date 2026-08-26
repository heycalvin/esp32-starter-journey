#include "web_server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "bsp_sensor.h"
#include "bsp_led.h"
#include "bsp_sdcard.h"
#include "sys_ota.h"
#include "sys_fsm.h"
#include "net_manager.h"
#include "file_reader.h"

static const char *TAG = "WEB_SERVER";
static httpd_handle_t s_server = NULL;

/* 📱 强制门户配网 HTML 页面 (用于 AP 配网模式) */
static const char HTML_PORTAL_PAGE[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>ESP32 智能中控配网</title>"
"<style>"
"body{background:#0f172a;color:#f8fafc;font-family:-apple-system,sans-serif;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:90vh;}"
".card{background:#1e293b;border:1px solid #334155;border-radius:20px;padding:28px;width:100%;max-width:380px;text-align:center;box-shadow:0 20px 40px rgba(0,0,0,0.5);}"
"h2{color:#38bdf8;margin:0 0 8px 0;}"
"p{color:#94a3b8;font-size:14px;margin-bottom:20px;}"
"label{font-size:13px;color:#94a3b8;margin:12px 0 6px 0;display:block;text-align:left;font-weight:600;}"
"select,input{width:100%;box-sizing:border-box;background:#0f172a;border:1px solid #475569;color:#fff;padding:12px;border-radius:10px;font-size:15px;outline:none;margin-bottom:8px;}"
"select:focus,input:focus{border-color:#38bdf8;}"
".btn{width:100%;background:#0ea5e9;color:#fff;border:none;padding:14px;font-size:16px;font-weight:bold;border-radius:10px;cursor:pointer;margin-top:16px;}"
".btn:hover{background:#0284c7;}"
".refresh{font-size:12px;color:#38bdf8;cursor:pointer;text-align:right;display:block;margin-bottom:4px;}"
"</style></head><body>"
"<div class='card'>"
"<h2>⚡ ESP32 智能开箱配网</h2>"
"<p>请点选您家中的 Wi-Fi 并输入密码</p>"
"<form action='/save' method='POST'>"
"<div style='display:flex;justify-content:space-between;align-items:center;'>"
"<label style='margin:0;'>周围 Wi-Fi 列表</label><span class='refresh' onclick='scanWiFi()'>🔄 刷新</span>"
"</div>"
"<select id='wifi_select' onchange='selectSSID()'><option value=''>📡 正在扫描周围 Wi-Fi...</option></select>"
"<label>Wi-Fi 名称 (SSID)</label><input id='ssid_input' name='ssid' placeholder='可下拉选择或手动输入' required>"
"<label>Wi-Fi 密码</label><input type='password' name='password' placeholder='请输入密码'>"
"<button type='submit' class='btn'>保存并连接网络</button>"
"</form></div>"
"<script>"
"function scanWiFi(){"
"  let s=document.getElementById('wifi_select'); s.innerHTML='<option>📡 正在扫描周围 Wi-Fi...</option>';"
"  fetch('/api/scan').then(r=>r.json()).then(list=>{"
"    s.innerHTML='<option value=\"\">-- 请在下方点选您的 Wi-Fi --</option>';"
"    list.forEach(w=>{"
"      let opt=document.createElement('option');"
"      opt.value=w.ssid; opt.innerText='📶 '+w.ssid+' ('+w.rssi+' dBm)';"
"      s.appendChild(opt);"
"    });"
"  }).catch(()=>{s.innerHTML='<option>⚠️ 扫描失败，请手动输入</option>';});"
"}"
"function selectSSID(){"
"  let v=document.getElementById('wifi_select').value;"
"  if(v) document.getElementById('ssid_input').value=v;"
"}"
"scanWiFi();"
"</script></body></html>";

/* 💻 现代化轻奢 Web 控制台 & 多图批量处理无线传府中枢 (用于 STA 模式) */
static const char HTML_CONSOLE_PAGE[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>ESP32 智能中枢 Web 控制台</title>"
"<style>"
"body{background:#0b1329;color:#f8fafc;font-family:-apple-system,sans-serif;margin:0;padding:16px;line-height:1.5;}"
".container{max-width:680px;margin:0 auto;}"
".header{display:flex;justify-content:space-between;align-items:center;margin-bottom:16px;border-bottom:1px solid #1e293b;padding-bottom:12px;}"
"h1{color:#38bdf8;font-size:20px;margin:0;display:flex;align-items:center;gap:8px;}"
".tag{background:#1e293b;color:#10b981;font-size:12px;padding:4px 10px;border-radius:20px;border:1px solid #334155;}"
".card{background:#131c38;border:1px solid #1e293b;border-radius:16px;padding:18px;margin-bottom:16px;box-shadow:0 8px 24px rgba(0,0,0,0.4);}"
".card h3{margin:0 0 12px 0;font-size:16px;color:#94a3b8;display:flex;align-items:center;gap:6px;}"
".grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;}"
".stat{background:#0b1329;padding:12px;border-radius:10px;border:1px solid #1e293b;}"
".stat .lbl{color:#64748b;font-size:12px;margin-bottom:2px;}"
".stat .val{color:#38bdf8;font-size:16px;font-weight:bold;}"
"input,button{box-sizing:border-box;border-radius:8px;font-size:14px;outline:none;}"
"input[type=text]{width:100%;background:#0b1329;border:1px solid #334155;color:#fff;padding:10px;margin-bottom:10px;}"
"input[type=text]:focus{border-color:#38bdf8;}"
".btn{background:#0284c7;color:#fff;border:none;padding:12px 16px;cursor:pointer;font-weight:600;transition:0.2s;width:100%;border-radius:8px;}"
".btn:hover{background:#0369a1;}"
".btn:disabled{background:#334155;color:#64748b;cursor:not-allowed;}"
".btn-green{background:#059669;}"
".btn-green:hover{background:#047857;}"
".btn-amber{background:#d97706;}"
".btn-amber:hover{background:#b45309;}"
".file-box{border:2px dashed #334155;border-radius:10px;padding:16px;text-align:center;background:#0b1329;margin-bottom:10px;cursor:pointer;}"
".file-box:hover{border-color:#38bdf8;}"
".prog-section{display:none;background:#0b1329;border:1px solid #1e293b;border-radius:10px;padding:12px;margin-top:12px;}"
".prog-row{margin-bottom:8px;}"
".prog-header{display:flex;justify-content:space-between;font-size:12px;color:#94a3b8;margin-bottom:4px;}"
".progress{height:7px;background:#1e293b;border-radius:4px;overflow:hidden;}"
".progress-bar{height:100%;width:0%;background:#38bdf8;transition:0.1s;}"
".progress-bar-green{background:#10b981;}"
".queue-grid{display:grid;grid-template-columns:repeat(auto-fill, minmax(70px, 1fr));gap:8px;margin-top:12px;max-height:160px;overflow-y:auto;}"
".queue-item{position:relative;background:#0b1329;border:1px solid #334155;border-radius:6px;overflow:hidden;aspect-ratio:240/280;}"
".queue-item img{width:100%;height:100%;object-fit:cover;}"
".queue-badge{position:absolute;bottom:0;left:0;right:0;background:rgba(15,23,42,0.85);font-size:10px;text-align:center;padding:2px;color:#38bdf8;}"
".toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#10b981;color:#fff;padding:10px 20px;border-radius:30px;font-size:14px;box-shadow:0 10px 25px rgba(0,0,0,0.5);display:none;z-index:999;}"
"</style></head><body>"
"<div class='container'>"
"<div class='header'>"
"<h1>⚡ ESP32 智能中枢控制台</h1>"
"<span class='tag' id='status_tag'>● 在线运行</span>"
"</div>"

"<!-- 1. 硬件遥测与控制 -->"
"<div class='card'>"
"<h3>📊 实时系统运维指标</h3>"
"<div class='grid'>"
"<div class='stat'><div class='lbl'>室内温湿度 (NTC/DHT)</div><div class='val' id='temp_val'>--.- °C / --.- %</div></div>"
"<div class='stat'><div class='lbl'>内存架构 (SRAM / PSRAM)</div><div class='val' id='mem_val'>-- KB / -- MB</div></div>"
"<div class='stat'><div class='lbl'>系统运行时间 (Uptime)</div><div class='val' id='uptime_val'>--:--:--</div></div>"
"<div class='stat'><div class='lbl'>板载智能照明 (LED2)</div><div class='val' id='led_val'>关闭</div></div>"
"</div>"
"<button class='btn btn-amber' style='margin-top:10px;' onclick='toggleLED()'>💡 远程开关板载智能灯</button>"
"</div>"

"<!-- 2. 小区高精度地理位置与天气配置 -->"
"<div class='card'>"
"<h3>📍 小区社区位置 & 天气同步设置</h3>"
"<p style='color:#64748b;font-size:12px;margin:-4px 0 10px 0;'>可手动输入您所在的小区名称，或点击一键获取当前位置并同步到开发板屏幕</p>"
"<input type='text' id='loc_input' placeholder='例如：深圳市南山区 · 科技园社区附近'>"
"<input type='text' id='weather_input' placeholder='例如：☀️ 晴朗 26°C · 空气优 · 适宜出行'>"
"<div style='display:flex;gap:8px;'>"
"<button class='btn btn-green' onclick='getBrowserGeo()'>📍 获取手机/电脑定位</button>"
"<button class='btn' onclick='saveLocation()'>💾 同步到开发板屏幕</button>"
"</div>"
"</div>"

"<!-- 3. 多图批量无线传输中枢 (支持多选 + 三级进度条) -->"
"<div class='card'>"
"<h3>🖼️ 批量无线传图 (自动裁剪 240×280 & 三级进度)</h3>"
"<p style='color:#38bdf8;font-size:12px;margin:-4px 0 10px 0;'>✨ 支持一次性多选多张图片，自动以 240×280 满屏黄金比例裁剪并极速流水线上传！</p>"
"<div class='file-box' onclick=\"document.getElementById('photo_files').click()\">"
"<span id='photo_select_title'>📁 点击选择多张照片 (支持批量多选)</span>"
"<input type='file' id='photo_files' accept='image/*' multiple style='display:none;' onchange=\"handleBatchPhotoSelect(this)\">"
"</div>"

"<!-- 批量缩略图队列 -->"
"<div class='queue-grid' id='photo_queue_grid'></div>"

"<!-- 三级精细进度指示面板 -->"
"<div class='prog-section' id='batch_prog_panel'>"
"<!-- 1. 批次总进度 -->"
"<div class='prog-row'>"
"<div class='prog-header'><span>📊 批次总进度</span><span id='total_prog_txt'>0 / 0 (0%)</span></div>"
"<div class='progress'><div class='progress-bar progress-bar-green' id='total_bar'></div></div>"
"</div>"
"<!-- 2. 当前图片智能处理进度 (Canvas 裁剪 & 压缩) -->"
"<div class='prog-row'>"
"<div class='prog-header'><span id='curr_process_txt'>⚡ 图片智能处理</span><span id='curr_process_pct'>0%</span></div>"
"<div class='progress'><div class='progress-bar' id='process_bar'></div></div>"
"</div>"
"<!-- 3. 当前图片网络上传进度 -->"
"<div class='prog-row'>"
"<div class='prog-header'><span id='curr_upload_txt'>🚀 网络无线上传</span><span id='curr_upload_pct'>0%</span></div>"
"<div class='progress'><div class='progress-bar' id='upload_bar' style='background:#f59e0b;'></div></div>"
"</div>"
"</div>"

"<button class='btn' id='btn_batch_upload' style='margin-top:10px;' onclick=\"startBatchUploadPipeline()\" disabled>🚀 开始批量流水线上传</button>"
"</div>"

"<!-- 4. 无线传书 (上传 novel.txt) -->"
"<div class='card'>"
"<h3>📖 无线传书 (上传电子小说到 TF卡)</h3>"
"<p style='color:#64748b;font-size:12px;margin:-4px 0 10px 0;'>选择电脑/手机上的 .txt 纯文本小说，自动保存至 TF卡 /sdcard/novel.txt</p>"
"<div class='file-box' onclick=\"document.getElementById('novel_file').click()\">"
"<span id='novel_name'>📁 点击选择小说文本文件 (.txt)</span>"
"<input type='file' id='novel_file' accept='.txt' style='display:none;' onchange=\"handleFileSelect(this, 'novel_name')\">"
"</div>"
"<div class='progress' id='novel_prog' style='display:none;'><div class='progress-bar' id='novel_bar'></div></div>"
"<button class='btn' onclick=\"uploadNovelFile()\">🚀 立即无线传输小说</button>"
"</div>"

"</div>"
"<div class='toast' id='toast'></div>"

"<script>"
"function showToast(msg){\n"
"  let t=document.getElementById('toast'); t.innerText=msg; t.style.display='block';\n"
"  setTimeout(()=>{t.style.display='none';}, 3500);\n"
"}\n"
"function refreshStatus(){\n"
"  fetch('/api/status').then(r=>r.json()).then(d=>{\n"
"    document.getElementById('temp_val').innerText = d.temp.toFixed(1)+' °C / '+d.humi.toFixed(1)+' %';\n"
"    document.getElementById('mem_val').innerText = Math.round(d.heap/1024)+' KB / '+(d.psram/(1024*1024)).toFixed(2)+' MB';\n"
"    document.getElementById('uptime_val').innerText = d.uptime || '--:--:--';\n"
"    document.getElementById('led_val').innerText = d.led ? '🟢 开启' : '⚪ 关闭';\n"
"    if(d.loc && !document.getElementById('loc_input').value) document.getElementById('loc_input').value = d.loc;\n"
"    if(d.weather && !document.getElementById('weather_input').value) document.getElementById('weather_input').value = d.weather;\n"
"  }).catch(()=>{});\n"
"}\n"
"setInterval(refreshStatus, 2000); refreshStatus();\n"

"function toggleLED(){\n"
"  fetch('/api/control', {method:'POST', body:'toggle'}).then(()=>{\n"
"    showToast('💡 已切换板载灯状态'); refreshStatus();\n"
"  });\n"
"}\n"

"function saveLocation(){\n"
"  let loc = document.getElementById('loc_input').value.trim();\n"
"  let w = document.getElementById('weather_input').value.trim();\n"
"  if(!loc) return alert('请输入位置信息');\n"
"  fetch('/api/location', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({loc:loc, weather:w})})\n"
"  .then(r=>r.json()).then(()=>{\n"
"    showToast('📍 位置与天气已成功同步至 ESP32 屏幕！');\n"
"  });\n"
"}\n"

"function getBrowserGeo(){\n"
"  if(!navigator.geolocation) return alert('当前浏览器不支持定位');\n"
"  showToast('🔍 正在获取手机/电脑高精度定位...');\n"
"  navigator.geolocation.getCurrentPosition(pos=>{\n"
"    let lat = pos.coords.latitude.toFixed(4);\n"
"    let lng = pos.coords.longitude.toFixed(4);\n"
"    document.getElementById('loc_input').value = '📍 经纬度: ['+lng+', '+lat+'] 附近社区';\n"
"    showToast('✅ 已获取经纬度坐标，点击保存即可同步！');\n"
"  }, err=>{\n"
"    showToast('⚠️ 定位权限未允许，请手动输入小区');\n"
"  });\n"
"}\n"

"function handleFileSelect(input, targetId){\n"
"  if(input.files && input.files[0]){\n"
"    document.getElementById(targetId).innerText = '📄 已选: '+input.files[0].name+' ('+(input.files[0].size/1024).toFixed(1)+' KB)';\n"
"  }\n"
"}\n"

"function uploadNovelFile(){\n"
"  let file = document.getElementById('novel_file').files[0];\n"
"  if(!file) return alert('请先选择小说文本文件');\n"
"  let prog = document.getElementById('novel_prog'); let bar = document.getElementById('novel_bar');\n"
"  prog.style.display='block'; bar.style.width='0%';\n"
"  let xhr = new XMLHttpRequest();\n"
"  xhr.open('POST', '/api/upload/novel', true);\n"
"  xhr.upload.onprogress = e=>{ if(e.lengthComputable) bar.style.width = Math.round((e.loaded/e.total)*100)+'%'; };\n"
"  xhr.onload = ()=>{\n"
"    prog.style.display='none';\n"
"    if(xhr.status==200){ showToast('🎉 小说上传成功，开发板已即刻同步！'); }\n"
"    else{ alert('❌ 上传失败，请检查 TF卡'); }\n"
"  };\n"
"  xhr.send(file);\n"
"}\n"

"/* ================= 批量图片预处理与流水线上传引擎 ================= */\n"
"let s_selected_files = [];\n"
"let s_processed_queue = [];\n"

"function handleBatchPhotoSelect(input){\n"
"  if(!input.files || input.files.length === 0) return;\n"
"  s_selected_files = Array.from(input.files);\n"
"  document.getElementById('photo_select_title').innerText = '📁 已选择 '+s_selected_files.length+' 张照片 (点击可重新选择)';\n"
"  let grid = document.getElementById('photo_queue_grid');\n"
"  grid.innerHTML = '';\n"
"  s_selected_files.forEach((f, idx)=>{\n"
"    let item = document.createElement('div');\n"
"    item.className = 'queue-item'; item.id = 'q_item_'+idx;\n"
"    item.innerHTML = '<img id=\"q_img_'+idx+'\" src=\"\"><div class=\"queue-badge\" id=\"q_badge_'+idx+'\">⏳ 等待</div>';\n"
"    grid.appendChild(item);\n"
"    // 快速生成初始缩略预览\n"
"    let r = new FileReader();\n"
"    r.onload = e => { document.getElementById('q_img_'+idx).src = e.target.result; };\n"
"    r.readAsDataURL(f);\n"
"  });\n"
"  document.getElementById('btn_batch_upload').disabled = false;\n"
"  document.getElementById('batch_prog_panel').style.display = 'block';\n"
"  document.getElementById('total_prog_txt').innerText = '0 / '+s_selected_files.length+' (0%)';\n"
"  document.getElementById('total_bar').style.width = '0%';\n"
"}\n"

"// Canvas 240x280 智能裁剪与压缩函数\n"
"function processSingleImage(file, onProgress){\n"
"  return new Promise((resolve, reject)=>{\n"
"    let r = new FileReader();\n"
"    r.onload = function(e){\n"
"      let img = new Image();\n"
"      img.onload = function(){\n"
"        let targetW = 240, targetH = 280;\n"
"        let canvas = document.createElement('canvas');\n"
"        canvas.width = targetW; canvas.height = targetH;\n"
"        let ctx = canvas.getContext('2d');\n"
"        let scale = Math.max(targetW / img.width, targetH / img.height);\n"
"        let renderW = img.width * scale, renderH = img.height * scale;\n"
"        let offsetX = (targetW - renderW) / 2, offsetY = (targetH - renderH) / 2;\n"
"        ctx.fillStyle = '#000000'; ctx.fillRect(0, 0, targetW, targetH);\n"
"        ctx.drawImage(img, offsetX, offsetY, renderW, renderH);\n"
"        if(onProgress) onProgress(80);\n"
"        canvas.toBlob(blob => {\n"
"          if(onProgress) onProgress(100);\n"
"          resolve({blob: blob, dataUrl: canvas.toDataURL('image/jpeg', 0.85)});\n"
"        }, 'image/jpeg', 0.85);\n"
"      };\n"
"      img.onerror = reject;\n"
"      img.src = e.target.result;\n"
"    };\n"
"    r.readAsDataURL(file);\n"
"  });\n"
"}\n"

"// 单张图片网络上传函数 (支持精确 onprogress 汇报)\n"
"function uploadSingleBlob(blob, filename, onUploadProgress){\n"
"  return new Promise((resolve, reject)=>{\n"
"    let xhr = new XMLHttpRequest();\n"
"    xhr.open('POST', '/api/upload/photo?name='+encodeURIComponent(filename), true);\n"
"    xhr.upload.onprogress = e => {\n"
"      if(e.lengthComputable && onUploadProgress){\n"
"        let pct = Math.round((e.loaded / e.total) * 100);\n"
"        onUploadProgress(pct);\n"
"      }\n"
"    };\n"
"    xhr.onload = () => {\n"
"      if(xhr.status === 200) resolve();\n"
"      else reject(new Error('Upload failed'));\n"
"    };\n"
"    xhr.onerror = reject;\n"
"    xhr.send(blob);\n"
"  });\n"
"}\n"

"// 异步流水线调度器：逐张进行 Canvas 预处理 + 上传\n"
"async function startBatchUploadPipeline(){\n"
"  if(s_selected_files.length === 0) return alert('请先选择照片');\n"
"  let btn = document.getElementById('btn_batch_upload');\n"
"  btn.disabled = true; btn.innerText = '⏳ 正在流水线传输中...';\n"
"  let total = s_selected_files.length;\n"
"  let successCount = 0;\n"

"  for(let i=0; i<total; i++){\n"
"    let f = s_selected_files[i];\n"
"    let badge = document.getElementById('q_badge_'+i);\n"
"    let imgElem = document.getElementById('q_img_'+i);\n"
"    \n"
"    // 1. 更新当前处理状态\n"
"    badge.innerText = '⚡ 处理中'; badge.style.color = '#38bdf8';\n"
"    document.getElementById('curr_process_txt').innerText = '⚡ 处理 ['+(i+1)+'/'+total+']: '+f.name;\n"
"    document.getElementById('process_bar').style.width = '20%';\n"
"    document.getElementById('curr_process_pct').innerText = '20%';\n"

"    // 执行 240x280 Canvas 智能裁剪\n"
"    let res = await processSingleImage(f, pct => {\n"
"      document.getElementById('process_bar').style.width = pct+'%';\n"
"      document.getElementById('curr_process_pct').innerText = pct+'%';\n"
"    });\n"
"    imgElem.src = res.dataUrl;\n"
"    badge.innerText = '🚀 上传中'; badge.style.color = '#f59e0b';\n"

"    // 2. 执行网络上传\n"
"    let fname = 'photo_' + (Date.now() + i).toString().slice(-6) + '.jpg';\n"
"    document.getElementById('curr_upload_txt').innerText = '🚀 上传 ['+(i+1)+'/'+total+']: '+fname;\n"
"    document.getElementById('upload_bar').style.width = '0%';\n"
"    document.getElementById('curr_upload_pct').innerText = '0%';\n"

"    try {\n"
"      await uploadSingleBlob(res.blob, fname, pct => {\n"
"        document.getElementById('upload_bar').style.width = pct+'%';\n"
"        document.getElementById('curr_upload_pct').innerText = pct+'%';\n"
"        badge.innerText = '🚀 '+pct+'%';\n"
"      });\n"
"      successCount++;\n"
"      badge.innerText = '✅ 成功'; badge.style.color = '#10b981';\n"
"    } catch(err){\n"
"      badge.innerText = '❌ 失败'; badge.style.color = '#ef4444';\n"
"    }\n"

"    // 3. 更新总进度条\n"
"    let totalPct = Math.round(((i + 1) / total) * 100);\n"
"    document.getElementById('total_bar').style.width = totalPct+'%';\n"
"    document.getElementById('total_prog_txt').innerText = (i + 1)+' / '+total+' ('+totalPct+'%)';\n"
"  }\n"

"  btn.disabled = false; btn.innerText = '🚀 开始批量流水线上传';\n"
"  showToast('🎉 全部 '+successCount+' 张 240×280 满屏壁纸批量上传成功！');\n"
"}\n"
"</script></body></html>";

static const char HTML_SAVE_SUCCESS[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>配置已保存</title>"
"<style>body{background:#0f172a;color:#f8fafc;font-family:-apple-system,sans-serif;padding:40px 20px;text-align:center;}"
".card{background:#1e293b;border-radius:16px;padding:30px;max-width:360px;margin:0 auto;box-shadow:0 10px 30px rgba(0,0,0,0.5);}"
"h2{color:#10b981;}</style></head><body>"
"<div class='card'><h2>🎉 Wi-Fi 凭证已保存！</h2>"
"<p>ESP32 正在自动连接您的路由器并校准时间...</p>"
"<p style='color:#94a3b8;font-size:13px;'>请观察中控台屏幕，设备将自动进入正常工作状态。</p></div>"
"</body></html>";

/* URL 解码辅助函数 */
static void url_decode(char *dst, const char *src, size_t dst_len)
{
    char a, b;
    size_t written = 0;
    while (*src && written < dst_len - 1) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
        written++;
    }
    *dst = '\0';
}

/* 1. 根路径 GET / ➔ 根据当前模式返回配网页或智能控制台 */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    if (net_manager_is_provisioning()) {
        return httpd_resp_send(req, HTML_PORTAL_PAGE, HTTPD_RESP_USE_STRLEN);
    } else {
        return httpd_resp_send(req, HTML_CONSOLE_PAGE, HTTPD_RESP_USE_STRLEN);
    }
}

/* 2. 扫描周围 Wi-Fi 列表 JSON API */
static esp_err_t scan_get_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_config = { .show_hidden = false };
    esp_wifi_scan_start(&scan_config, true);

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > 10) ap_count = 10;

    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_records) {
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    }

    char *json = malloc(1024);
    if (!json) {
        if (ap_records) free(ap_records);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int offset = snprintf(json, 1024, "[");
    for (int i = 0; i < ap_count && offset < 950; i++) {
        if (strlen((char *)ap_records[i].ssid) == 0) continue;
        offset += snprintf(json + offset, 1024 - offset,
                           "%s{\"ssid\":\"%s\",\"rssi\":%d}",
                           (i > 0) ? "," : "", (char *)ap_records[i].ssid, ap_records[i].rssi);
    }
    snprintf(json + offset, 1024 - offset, "]");

    if (ap_records) free(ap_records);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ret;
}

/* 3. 保存配网表单 POST /save */
static esp_err_t save_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char raw_ssid[64] = {0};
    char raw_pass[64] = {0};
    char ssid[33] = {0};
    char pass[65] = {0};

    char *p_ssid = strstr(buf, "ssid=");
    if (p_ssid) {
        p_ssid += 5;
        char *p_end = strchr(p_ssid, '&');
        if (p_end) strncpy(raw_ssid, p_ssid, p_end - p_ssid);
        else strncpy(raw_ssid, p_ssid, sizeof(raw_ssid) - 1);
    }

    char *p_pass = strstr(buf, "password=");
    if (p_pass) {
        p_pass += 9;
        char *p_end = strchr(p_pass, '&');
        if (p_end) strncpy(raw_pass, p_pass, p_end - p_pass);
        else strncpy(raw_pass, p_pass, sizeof(raw_pass) - 1);
    }

    url_decode(ssid, raw_ssid, sizeof(ssid));
    url_decode(pass, raw_pass, sizeof(pass));

    ESP_LOGI(TAG, "📥 收到用户配网数据: SSID=[%s]", ssid);
    net_manager_save_credentials(ssid, pass);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_SAVE_SUCCESS, HTTPD_RESP_USE_STRLEN);

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

/* 4. 实时状态监控 JSON API GET /api/status */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    bsp_sensor_data_t data;
    bsp_sensor_read_all(&data);

    char uptime[32] = {0};
    char loc[64] = {0};
    char weather[64] = {0};
    net_manager_get_uptime_str(uptime, sizeof(uptime));
    net_manager_get_location_str(loc, sizeof(loc));
    net_manager_get_weather_str(weather, sizeof(weather));

    char json_response[512];
    snprintf(json_response, sizeof(json_response),
             "{\"temp\":%.1f,\"humi\":%.1f,\"heap\":%lu,\"psram\":%lu,\"led\":%d,\"uptime\":\"%s\",\"loc\":\"%s\",\"weather\":\"%s\"}",
             data.ntc_temperature, data.dht_humidity,
             (unsigned long)data.free_heap_bytes, (unsigned long)data.free_psram_bytes,
             bsp_led_get_state() ? 1 : 0, uptime, loc, weather);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

/* 5. 小区地理位置与天气修改 API POST /api/location */
static esp_err_t location_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        char *p_loc = strstr(buf, "\"loc\":\"");
        if (p_loc) {
            p_loc += 7;
            char *p_end = strchr(p_loc, '\"');
            if (p_end) {
                *p_end = '\0';
                net_manager_set_location_str(p_loc);
                ESP_LOGI(TAG, "📍 [Web 位置同步] 已更新当前社区位置: %s", p_loc);
                *p_end = '\"';
            }
        }
        char *p_w = strstr(buf, "\"weather\":\"");
        if (p_w) {
            p_w += 11;
            char *p_end = strchr(p_w, '\"');
            if (p_end) {
                *p_end = '\0';
                net_manager_set_weather_str(p_w);
                ESP_LOGI(TAG, "⛅ [Web 天气同步] 已更新实时天气: %s", p_w);
            }
        }
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
}

/* 6. 远程控制 API POST /api/control */
static esp_err_t control_post_handler(httpd_req_t *req)
{
    char buf[64];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        if (strstr(buf, "toggle")) {
            bsp_led_toggle();
        }
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "{\"result\":\"ok\"}");
}

/* 7. 无线传书 API POST /api/upload/novel (流式写入 /sdcard/novel.txt) */
static esp_err_t upload_novel_post_handler(httpd_req_t *req)
{
    if (!bsp_sdcard_is_mounted()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "TF Card Not Ready");
        return ESP_FAIL;
    }

    FILE *f = fopen("/sdcard/novel.txt", "wb");
    if (!f) {
        ESP_LOGE(TAG, "❌ 无法打开 /sdcard/novel.txt 进行写入");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[1024];
    int remaining = req->content_len;
    ESP_LOGI(TAG, "📖 [无线传书] 开始接收 novel.txt (总大小: %d 字节)", remaining);

    while (remaining > 0) {
        int to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
        int received = httpd_req_recv(req, buf, to_read);
        if (received <= 0) {
            fclose(f);
            ESP_LOGE(TAG, "❌ 接收小说数据中断");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        fwrite(buf, 1, received, f);
        remaining -= received;
    }
    fclose(f);

    file_reader_rescan_chapters();
    ESP_LOGI(TAG, "🎉 [无线传书] novel.txt 写入完成并重新索引章节！");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
}

/* 8. 无线传图 API POST /api/upload/photo (流式写入 /sdcard/photos/) */
static esp_err_t upload_photo_post_handler(httpd_req_t *req)
{
    if (!bsp_sdcard_is_mounted()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "TF Card Not Ready");
        return ESP_FAIL;
    }

    char query[128] = {0};
    char filename[64] = "uploaded_photo.jpg";
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[64];
        if (httpd_query_key_value(query, "name", val, sizeof(val)) == ESP_OK) {
            url_decode(filename, val, sizeof(filename));
        }
    }

    char file_path[128];
    snprintf(file_path, sizeof(file_path), "/sdcard/photos/%s", filename);
    FILE *f = fopen(file_path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "❌ 无法打开 %s 写入图片", file_path);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[1024];
    int remaining = req->content_len;
    ESP_LOGI(TAG, "🖼️ [批量传图] 开始写入 %s (大小: %d 字节)", file_path, remaining);

    while (remaining > 0) {
        int to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
        int received = httpd_req_recv(req, buf, to_read);
        if (received <= 0) {
            fclose(f);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        fwrite(buf, 1, received, f);
        remaining -= received;
    }
    fclose(f);

    file_reader_rescan_photos();
    ESP_LOGI(TAG, "🎉 [批量传图] 图片 %s 上传成功并已动态重新索引相册！", file_path);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
}

/* 9. OTA 固件上传 */
static esp_err_t ota_post_handler(httpd_req_t *req)
{
    char buf[1024];
    int remaining = req->content_len;
    ESP_LOGI(TAG, "📦 收到 Web OTA 固件上传请求 (总大小: %d 字节)", remaining);

    sys_fsm_handle_event(HUB_FSM_EVT_START_OTA, 0);
    esp_err_t err = sys_ota_begin_upgrade(remaining);
    if (err != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (remaining > 0) {
        int to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
        int received = httpd_req_recv(req, buf, to_read);
        if (received <= 0) {
            ESP_LOGE(TAG, "❌ 接收 OTA 固件数据中断");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        sys_ota_write_chunk(buf, received);
        remaining -= received;
    }

    httpd_resp_sendstr(req, "{\"status\":\"OTA_SUCCESS_REBOOTING\"}");
    sys_ota_finish_and_reboot();
    return ESP_OK;
}

/* =====================================================================
 * 10. 文件管理器 API: GET /api/files  返回 TF 卡文件列表 JSON
 * ===================================================================== */
static esp_err_t files_list_get_handler(httpd_req_t *req)
{
    if (!bsp_sdcard_is_mounted()) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_sendstr(req, "{\"error\":\"SD card not mounted\"}");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr_chunk(req, "{\"files\":[");

    const char *dirs[] = {"/sdcard/photos", "/sdcard", NULL};
    const char *dir_names[] = {"photos", "root", NULL};
    bool first = true;
    char item_buf[700];

    for (int di = 0; dirs[di] != NULL; di++) {
        DIR *dir = opendir(dirs[di]);
        if (!dir) continue;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char full_path[300];
            snprintf(full_path, sizeof(full_path), "%s/%s", dirs[di], entry->d_name);
            struct stat st;
            long fsize = 0;
            if (stat(full_path, &st) == 0) fsize = st.st_size;
            snprintf(item_buf, sizeof(item_buf),
                "%s{\"name\":\"%s\",\"dir\":\"%s\",\"path\":\"%s\",\"size\":%ld}",
                first ? "" : ",",
                entry->d_name, dir_names[di], full_path, fsize);
            httpd_resp_sendstr_chunk(req, item_buf);
            first = false;
        }
        closedir(dir);
    }

    httpd_resp_sendstr_chunk(req, "]}");
    return httpd_resp_sendstr_chunk(req, NULL);
}

/* =====================================================================
 * 11. 文件管理器 API: DELETE /api/file?path=<path>  删除指定文件
 * ===================================================================== */
static esp_err_t file_delete_handler(httpd_req_t *req)
{
    char query[256] = {0};
    char path[128] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "path", path, sizeof(path));
    }
    if (strlen(path) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing path");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    if (remove(path) == 0) {
        ESP_LOGI(TAG, "[FileManager] 删除文件: %s", path);
        file_reader_rescan_photos();
        return httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    } else {
        return httpd_resp_sendstr(req, "{\"status\":\"error\",\"msg\":\"delete failed\"}");
    }
}

/* =====================================================================
 * 12. 文件管理器 API: GET /download?path=<path>  下载指定文件
 * ===================================================================== */
static esp_err_t file_download_get_handler(httpd_req_t *req)
{
    char query[256] = {0};
    char path[128] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "path", path, sizeof(path));
    }
    if (strlen(path) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing path");
        return ESP_FAIL;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char disp_hdr[200];
    // 提取文件名设置下载标题
    const char *fname = strrchr(path, '/');
    fname = fname ? fname + 1 : path;
    snprintf(disp_hdr, sizeof(disp_hdr), "attachment; filename=\"%s\"", fname);
    httpd_resp_set_hdr(req, "Content-Disposition", disp_hdr);
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t web_server_init(void)
{
    if (s_server) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    config.stack_size = 10240;

    if (httpd_start(&s_server, &config) == ESP_OK) {
        // 门户 / 控制台主路由
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
        httpd_register_uri_handler(s_server, &root_uri);

        httpd_uri_t detect1_uri = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = root_get_handler };
        httpd_register_uri_handler(s_server, &detect1_uri);

        httpd_uri_t detect2_uri = { .uri = "/generate_204", .method = HTTP_GET, .handler = root_get_handler };
        httpd_register_uri_handler(s_server, &detect2_uri);

        httpd_uri_t scan_uri = { .uri = "/api/scan", .method = HTTP_GET, .handler = scan_get_handler };
        httpd_register_uri_handler(s_server, &scan_uri);

        httpd_uri_t save_uri = { .uri = "/save", .method = HTTP_POST, .handler = save_post_handler };
        httpd_register_uri_handler(s_server, &save_uri);

        // 状态监控与控制
        httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler };
        httpd_register_uri_handler(s_server, &status_uri);

        httpd_uri_t loc_uri = { .uri = "/api/location", .method = HTTP_POST, .handler = location_post_handler };
        httpd_register_uri_handler(s_server, &loc_uri);

        httpd_uri_t ctrl_uri = { .uri = "/api/control", .method = HTTP_POST, .handler = control_post_handler };
        httpd_register_uri_handler(s_server, &ctrl_uri);

        // 无线文件传输 (小说 & 图片)
        httpd_uri_t novel_uri = { .uri = "/api/upload/novel", .method = HTTP_POST, .handler = upload_novel_post_handler };
        httpd_register_uri_handler(s_server, &novel_uri);

        httpd_uri_t photo_uri = { .uri = "/api/upload/photo", .method = HTTP_POST, .handler = upload_photo_post_handler };
        httpd_register_uri_handler(s_server, &photo_uri);

        httpd_uri_t ota_uri = { .uri = "/api/ota", .method = HTTP_POST, .handler = ota_post_handler };
        httpd_register_uri_handler(s_server, &ota_uri);

        // 文件管理器 API
        httpd_uri_t files_uri = { .uri = "/api/files", .method = HTTP_GET, .handler = files_list_get_handler };
        httpd_register_uri_handler(s_server, &files_uri);

        httpd_uri_t file_del_uri = { .uri = "/api/file", .method = HTTP_DELETE, .handler = file_delete_handler };
        httpd_register_uri_handler(s_server, &file_del_uri);

        httpd_uri_t download_uri = { .uri = "/download", .method = HTTP_GET, .handler = file_download_get_handler };
        httpd_register_uri_handler(s_server, &download_uri);

        ESP_LOGI(TAG, "🌐 [服务层] 嵌入式轻奈 Web 控制台、文件管理器、OTA 空投启动 (端口: 80)！");
        return ESP_OK;
    }
    return ESP_FAIL;
}

void web_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}

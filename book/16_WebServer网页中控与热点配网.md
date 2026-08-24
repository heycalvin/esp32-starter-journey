# 第 16 关：ESP32 Web Server 网页中控、mDNS 本地域名与 AP 强制门户配网

![第16关封面插画](../docs/images/esp32_level16_cover.jpg)

---

## 🎯 本关学习目标

在前面的章节中，我们已经掌握了通过电脑串口、MQTT 云端和手机蓝牙 App 控制 ESP32。

但在真正的消费级智能硬件（如小米插座、涂鸦智能灯、商业路由器）中，用户最渴望的极致人机交互体验是：
1. **免装任何第三方 App**：任何电脑、iPhone、安卓手机、iPad，只要打开自带的浏览器（Chrome / Safari / Edge），就能直接进入控制中控台，实时开关灯光、查看设备状态！
2. **免查 IP 域名秒级直达**：不用登录路由器后台到处翻找 ESP32 的动态 IP，直接在浏览器输入 **`http://esp32.local`** 即可直达！
3. **开箱即用的强制门户配网（Captive Portal）**：设备买回家第一次通电，ESP32 自动发射一个 Wi-Fi 热点，手机连上后**手机屏幕自动弹出配网弹窗**，让用户输入家里的 Wi-Fi 密码并自动保存到 NVS！

完成本关卡后，你将达成以下核心成就：
1. **掌握 ESP-IDF 原生轻量级 Web 服务器**：深入理解 `esp_http_server` 的 URI 路由分发与 RESTful API 设计；
2. **攻克 mDNS（多播 DNS）局域网服务发现**：实现免查 IP 的 `http://esp32.local` 零配置域名解析；
3. **彻底搞懂 DNS 劫持与 Captive Portal 弹窗原理**：适配 iOS（Apple Captive）、Android（generate_204）与 Windows 自动弹窗探测机制；
4. **打造开箱即用的商用配网大工程**：网页端可视化扫描 Wi-Fi、输入密码、持久化存储到 NVS 闪存并自动重启联网！

---

## 🧭 关卡实验与源码速查

| 实验编号 | 实验名称 | 核心知识与实验目标 | 配套源码文件 | 一键切换命令 |
| :--- | :--- | :--- | :--- | :--- |
| **实验 1** | **局域网微型 Web 服务器** | 开启 HTTP Server，浏览器访问板载 IP，网页渲染控制面板实时开关 LED2 | [`01_http_web_server.c`](../code/16_web_server_portal/01_http_web_server.c) | `./switch_code.sh 16 1 --flash` |
| **实验 2** | **mDNS 本地域名解析直达** | 配置设备域名为 `esp32.local`，免查 IP 浏览器直接通过域名秒级访问 | [`02_mdns_hostname.c`](../code/16_web_server_portal/02_mdns_hostname.c) | `./switch_code.sh 16 2 --flash` |
| **实验 3** | **AP 强制门户配网 (Captive Portal)** | 开启 AP 热点 + DNS 劫持服务器，手机连上自动弹窗输入 Wi-Fi 密码并存入 NVS | [`03_captive_portal.c`](../code/16_web_server_portal/03_captive_portal.c) | `./switch_code.sh 16 3 --flash` |

---

## 16.1 💡 极简认知启蒙：什么是嵌入式 Web Server？

很多初学者以为“架设网站”必须要有昂贵的云服务器、Nginx、Linux 和 Tomcat。  
其实，**只要单片机连上了网络，它本身就是一个迷你的 Web 服务器！**

---

### 1. 餐厅点餐员模型：HTTP 协议的本质

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                    【HTTP Web 通信的“餐厅点餐模型”】                    │
 │                                                                        │
 │   [ 顾客: 手机/电脑浏览器 ]                     [ 点餐员: ESP32 WebServer ]│
 │      ① 打开网页 ➔ 发起 GET / ──────────────► 根据菜单(URI路由)找到主页 │
 │      ② 收到点餐响应 ◄──────(返回 HTML/CSS 网页)──────┘                │
 │                                                                        │
 │      ③ 点击“开灯” ➔ 发起 GET /api/led?1 ───► 点亮板载绿色 LED2         │
 │      ④ 收到操作回执 ◄──────(返回 JSON 状态)────────┘                   │
 └────────────────────────────────────────────────────────────────────────┘
```

1. **浏览器（客户端 Client）**：提出请求（*“我想看主页 `GET /`”*，或者 *“我想开灯 `POST /api/led`”*）；
2. **ESP32（服务器 Server）**：内部常驻一个轻量级的 `esp_http_server` 服务；
3. **URI 路由分发器**：就像餐厅菜单，每当收到不同的 URL 请求，就调用对应的 C 语言处理函数进行响应。

---

### 2. 什么是 mDNS？为什么它能让我们输入 `esp32.local`？

* **公网域名（DNS）**：你在浏览器输入 `baidu.com`，电脑会去问阿里云或电信的 DNS 服务器：“*百度服务器的 IP 是多少？*”；
* **局域网域名（mDNS 多播 DNS）**：在没有公网 DNS 的家里局域网，ESP32 连上 Wi-Fi 后，会向局域网广播大声宣告：  
  👉 *“大家好！我的名字叫 `esp32.local`，我的物理 IP 是 `192.168.1.105`！”*  
* 当电脑在浏览器输入 `http://esp32.local` 时，电脑不需要查任何外网服务器，直接在局域网内瞬间定位到 ESP32！

---

## 16.2 🛠️ ESP-IDF 原生 `esp_http_server` 核心使用四部曲

在 ESP-IDF 中，开启一个 Web 服务器只需要 4 个标准步骤：

```text
 [ 步骤 1: 默认配置 ] ──► [ 步骤 2: 启动服务 ] ──► [ 步骤 3: 注册路由 ] ──► [ 步骤 4: 发送响应 ]
   HTTPD_DEFAULT_CONFIG       httpd_start         httpd_register_uri_handler   httpd_resp_send
```

---

### 1. 核心 API 大白话速查表

| API 函数名 | 通俗功能比喻 | 关键作用与注意事项 |
| :--- | :--- | :--- |
| **`httpd_start(&server, &config)`** | **开门营业** | 启动 HTTP 服务器守护任务，监听 80 端口 |
| **`httpd_register_uri_handler(...)`** | **在菜单上加菜** | 绑定 URL 路径（如 `/`、`/api/led`）与对应的 C 回调函数 |
| **`httpd_resp_send(req, buf, len)`** | **上菜（返回数据）** | 向浏览器返回 HTML 网页源码或 JSON 数据 |
| **`httpd_resp_set_type(req, "text/html")`** | **贴上菜品标签** | 设置 MIME 响应头类型（`text/html`、`application/json`） |

---

## 16.3 🌐 实验 1：局域网微型 Web 服务器与网页控制面板

### 1. 🎯 实验目标与生活化场景
* **场景**：在 ESP32 内部运行 Web 服务器，渲染一套深色科技风控制面板；
* **动作**：同一 Wi-Fi 下的电脑或手机打开浏览器输入板载 IP，点击网页按钮实时开关板载绿色 **LED2（GPIO27）**，网页每 3 秒自动轮询更新设备内存与开机时间。

---

### 2. 💻 实验 1 完整源码

> 📁 **配套源码文件**：[`code/16_web_server_portal/01_http_web_server.c`](../code/16_web_server_portal/01_http_web_server.c)  
> ⚡ **一键切换运行**：在终端运行 `./switch_code.sh 16 1 --flash` 即可秒级切换并自动烧录！

```c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"

static const char *TAG = "EXP1_WEB_SERVER";

#define LED2_PIN        GPIO_NUM_27

/* 请修改为你家里的路由器 Wi-Fi 账号密码 */
#define WIFI_SSID       "TP-LINK_Test"
#define WIFI_PASS       "12345678"

static bool s_led_state = false;
static httpd_handle_t s_server = NULL;

/* 嵌入式深色科技风 HTML/CSS 控制页面 */
static const char HTML_INDEX_PAGE[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>ESP32 智能中控台</title>"
"<style>"
"body{background:#0d1117;color:#c9d1d9;font-family:-apple-system,sans-serif;text-align:center;padding:30px;}"
".card{background:#161b22;border:1px solid #30363d;border-radius:16px;max-width:400px;margin:0 auto;padding:24px;box-shadow:0 8px 24px rgba(0,0,0,0.5);}"
"h2{color:#58a6ff;margin-top:0;}"
".btn{background:#238636;color:white;border:none;padding:14px 32px;font-size:18px;font-weight:bold;border-radius:30px;cursor:pointer;transition:0.2s;margin-top:20px;}"
".btn:hover{background:#2ea043;transform:scale(1.03);}"
".btn-off{background:#da3633;}"
".btn-off:hover{background:#f85149;}"
".status{font-size:16px;margin:15px 0;color:#8b949e;}"
".badge{display:inline-block;padding:4px 12px;border-radius:12px;background:#21262d;color:#58a6ff;font-weight:bold;}"
"</style></head><body>"
"<div class='card'>"
"<h2>⚡ ESP32 网页智能中控</h2>"
"<div class='status'>硬件平台: <span class='badge'>ESP32-D0WD-V3</span></div>"
"<div class='status'>当前绿色 LED2: <span id='led_text' class='badge'>读取中...</span></div>"
"<button id='toggle_btn' class='btn' onclick='toggleLED()'>切换 LED 状态</button>"
"</div>"
"<script>"
"function updateStatus(){"
"  fetch('/api/status').then(r=>r.json()).then(d=>{"
"    let s=d.led_state==1;"
"    document.getElementById('led_text').innerText=s?'💡 已点亮 (ON)':'🌑 已熄灭 (OFF)';"
"    document.getElementById('led_text').style.color=s?'#3fb950':'#f85149';"
"    let b=document.getElementById('toggle_btn');"
"    b.innerText=s?'关 闭 LED2':'点 亮 LED2';"
"    b.className=s?'btn btn-off':'btn';"
"  });"
"}"
"function toggleLED(){"
"  fetch('/api/led?toggle=1').then(()=>updateStatus());"
"}"
"updateStatus();"
"setInterval(updateStatus, 3000);"
"</script></body></html>";

/* 🌐 路由 1: GET / (返回主页 HTML) */
static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, HTML_INDEX_PAGE, HTTPD_RESP_USE_STRLEN);
}

/* 🌐 路由 2: GET /api/status (返回 JSON 遥测数据) */
static esp_err_t status_get_handler(httpd_req_t *req)
{
    char json_buf[128];
    uint32_t uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000);
    snprintf(json_buf, sizeof(json_buf), 
             "{\"led_state\":%d,\"free_heap\":%lu,\"uptime_sec\":%lu}",
             s_led_state ? 1 : 0, (unsigned long)esp_get_free_heap_size(), (unsigned long)uptime_sec);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);
}

/* 🌐 路由 3: GET /api/led (控制 LED 状态) */
static esp_err_t led_control_handler(httpd_req_t *req)
{
    s_led_state = !s_led_state;
    gpio_set_level(LED2_PIN, s_led_state ? 1 : 0);
    ESP_LOGI(TAG, "💡 [网页指令执行] 翻转板载 LED2 ➔ %s", s_led_state ? "点亮" : "熄灭");

    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "OK", 2);
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_get_handler };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler };
        httpd_register_uri_handler(server, &status_uri);

        httpd_uri_t led_uri = { .uri = "/api/led", .method = HTTP_GET, .handler = led_control_handler };
        httpd_register_uri_handler(server, &led_uri);

        return server;
    }
    return NULL;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "🎉 Wi-Fi 连接成功！请在浏览器访问: \033[32mhttp://" IPSTR "\033[0m", IP2STR(&event->ip_info.ip));

        if (s_server == NULL) s_server = start_webserver();
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 16 实验 1：ESP32 局域网微型 Web 服务器   ");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t io_conf = { .pin_bit_mask = (1ULL << LED2_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io_conf);
    gpio_set_level(LED2_PIN, 0);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
}
```

---

### 3. 🔍 源码逐段深度精讲

1. **前后端分离 RESTful API 设计**：
   * 浏览器访问 `/` 时返回 HTML 网页骨架；
   * 网页内部通过 JavaScript `fetch('/api/status')` 异步拉取 JSON 数据，点击按钮异步触发 `fetch('/api/led')`，**实现无需刷新整个页面的丝滑局部更新**！
2. **`config.lru_purge_enable = true` 防内存溢出**：
   * 嵌入式设备的并发连接数有限；
   * 开启 LRU（最近最少使用）自动清理策略后，如果浏览器打开了过多长连接，服务器会自动关闭最老的连接释放 Socket，保证系统永不卡死。

---

### 4. 📱 电脑与手机实测现象

1. 烧录运行后，观察串口输出：
   ```text
   I (4250) EXP1_WEB_SERVER: 🎉 Wi-Fi 连接成功！请在浏览器访问: http://192.168.1.105
   ```
2. 在同一局域网下的手机或电脑浏览器输入 `http://192.168.1.105`；
3. 屏幕上呈现出极具科技感的深色中控卡片，点击 **“切换 LED 状态”**：
   * 👉 网页按钮瞬间变色，开发板上的绿色 LED2 毫秒级同步点亮/熄灭！

---

> 💡 **承上启下过度思考**：  
> 每次连上路由器，路由器分配的 IP 都是随机动态的（今天可能是 `192.168.1.105`，明天变成 `192.168.1.188`）。  
> 难道用户每次都要打开串口查看 IP 吗？  
> 接下来，我们将引入 **mDNS 局域网本地域名服务**，让用户直接输入 `http://esp32.local` 即可永久秒达！

---

## 16.4 🏷️ 实验 2：mDNS 本地域名解析与零配置直达 (http://esp32.local)

### 1. 🎯 实验目标与生活化场景
* **场景**：让设备拥有属于自己的局域网专属域名；
* **动作**：初始化 mDNS 服务，绑定主机名 `esp32`；电脑或 iPhone 浏览器直接输入 **`http://esp32.local`** 即可瞬间打开中控台！

---

### 2. 💻 实验 2 完整源码

> 📁 **配套源码文件**：[`code/16_web_server_portal/02_mdns_hostname.c`](../code/16_web_server_portal/02_mdns_hostname.c)  
> ⚡ **一键切换运行**：在终端运行 `./switch_code.sh 16 2 --flash` 即可秒级切换并自动烧录！

```c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "mdns.h"

static const char *TAG = "EXP2_MDNS_SERVER";

#define LED2_PIN        GPIO_NUM_27
#define MDNS_HOSTNAME   "esp32"     // 👉 局域网访问域名: http://esp32.local

#define WIFI_SSID       "TP-LINK_Test"
#define WIFI_PASS       "12345678"

static bool s_led_state = false;
static httpd_handle_t s_server = NULL;

static const char HTML_MDNS_PAGE[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>ESP32 mDNS 智能中控</title>"
"<style>"
"body{background:#0b0f19;color:#e2e8f0;font-family:sans-serif;text-align:center;padding:40px;}"
".box{background:#1e293b;border:1px solid #334155;border-radius:20px;max-width:420px;margin:0 auto;padding:30px;box-shadow:0 10px 30px rgba(0,0,0,0.6);}"
"h2{color:#38bdf8;margin:0 0 10px 0;}"
".domain{background:#0f172a;color:#38bdf8;padding:8px 16px;border-radius:8px;font-family:monospace;font-size:16px;display:inline-block;margin:15px 0;}"
".btn{background:#0ea5e9;color:white;border:none;padding:14px 36px;font-size:18px;font-weight:bold;border-radius:30px;cursor:pointer;}"
"</style></head><body>"
"<div class='box'>"
"<h2>🌐 mDNS 域名直达中控台</h2>"
"<div>访问域名: <span class='domain'>http://esp32.local</span></div>"
"<p>LED2 状态: <span id='st' style='color:#38bdf8'>读取中...</span></p>"
"<button class='btn' onclick='toggle()'>翻转 LED2 状态</button>"
"</div>"
"<script>"
"function getSt(){fetch('/api/status').then(r=>r.json()).then(d=>{document.getElementById('st').innerText=d.led?'💡 点亮 (ON)':'🌑 熄灭 (OFF)';});}"
"function toggle(){fetch('/api/led').then(()=>getSt());}"
"getSt();"
"</script></body></html>";

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, HTML_MDNS_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"led\":%d}", s_led_state ? 1 : 0);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t led_get_handler(httpd_req_t *req)
{
    s_led_state = !s_led_state;
    gpio_set_level(LED2_PIN, s_led_state ? 1 : 0);
    return httpd_resp_send(req, "OK", 2);
}

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&s_server, &config) == ESP_OK) {
        httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_get_handler };
        httpd_register_uri_handler(s_server, &index_uri);
        httpd_uri_t st_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler };
        httpd_register_uri_handler(s_server, &st_uri);
        httpd_uri_t led_uri = { .uri = "/api/led", .method = HTTP_GET, .handler = led_get_handler };
        httpd_register_uri_handler(s_server, &led_uri);
    }
}

/* 🏷️ 初始化 mDNS 域名服务 */
static void init_mdns(void)
{
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(MDNS_HOSTNAME));
    ESP_ERROR_CHECK(mdns_instance_name_set("ESP32 Web Hub"));
    mdns_service_add("ESP32-Web-Server", "_http", "_tcp", 80, NULL, 0);

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "🎉 mDNS 服务启动成功！");
    ESP_LOGI(TAG, "🌐 请在浏览器输入: \033[32mhttp://%s.local\033[0m 直达控制面板！", MDNS_HOSTNAME);
    ESP_LOGI(TAG, "==================================================");
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, "🎉 Wi-Fi 连接成功！物理 IP: " IPSTR, IP2STR(&event->ip_info.ip));

        start_webserver();

        ESP_ERROR_CHECK(mdns_init());
        ESP_ERROR_CHECK(mdns_hostname_set(MDNS_HOSTNAME));
        mdns_service_add("ESP32-Web-Server", "_http", "_tcp", 80, NULL, 0);

        ESP_LOGI(TAG, "🌐 请打开电脑/手机浏览器访问: \033[32mhttp://%s.local\033[0m", MDNS_HOSTNAME);
        ESP_LOGI(TAG, "==================================================");

        gpio_set_level(LED2_PIN, 1);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_mode == MODE_CONNECTED_STA) {
            ESP_LOGW(TAG, "⚠️ Wi-Fi 连接断开，尝试重新连接...");
            esp_wifi_connect();
        }
    }
}

static void factory_reset_button_task(void *pvParameters)
{
    int press_ms = 0;
    while (1) {
        if (gpio_get_level(BUTTON_PIN) == 0) {
            press_ms += 100;
            if (press_ms >= 3000) {
                ESP_LOGW(TAG, "⚠️ [长按 3 秒触发] 正在清除 NVS Wi-Fi 配网凭证...");
                nvs_handle_t nvs_handle;
                if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
                    nvs_erase_all(nvs_handle);
                    nvs_commit(nvs_handle);
                    nvs_close(nvs_handle);
                }
                for (int i = 0; i < 5; i++) {
                    gpio_set_level(LED2_PIN, 1);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    gpio_set_level(LED2_PIN, 0);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                ESP_LOGI(TAG, "🔄 恢复出厂设置成功！正在重启重新进入配网模式...");
                esp_restart();
            }
        } else {
            press_ms = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 16 实验 3：全生命周期配网与网页中控大成  ");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t io_conf = { .pin_bit_mask = (1ULL << LED2_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io_conf);
    gpio_set_level(LED2_PIN, 0);

    gpio_config_t btn_conf = { .pin_bit_mask = (1ULL << BUTTON_PIN), .mode = GPIO_MODE_INPUT };
    gpio_config(&btn_conf);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    nvs_handle_t nvs_handle;
    char saved_ssid[33] = {0}, saved_pass[65] = {0};
    size_t ssid_len = sizeof(saved_ssid), pass_len = sizeof(saved_pass);
    bool has_wifi = false;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
        if (nvs_get_str(nvs_handle, "ssid", saved_ssid, &ssid_len) == ESP_OK) {
            nvs_get_str(nvs_handle, "password", saved_pass, &pass_len);
            has_wifi = true;
            ESP_LOGI(TAG, "📦 [NVS 读取成功] 找到已存 Wi-Fi: \033[32m%s\033[0m", saved_ssid);
        }
        nvs_close(nvs_handle);
    }

    if (has_wifi) {
        start_sta_connected(saved_ssid, saved_pass);
    } else {
        start_ap_provisioning();
    }

    xTaskCreate(factory_reset_button_task, "reset_btn", 3072, NULL, 5, NULL);
}

### 2. 💻 实验 3 完整源码

> 📁 **配套源码文件**：[`code/16_web_server_portal/03_captive_portal.c`](../code/16_web_server_portal/03_captive_portal.c)  
> ⚡ **一键切换运行**：在终端运行 `./switch_code.sh 16 3 --flash` 即可秒级切换并自动烧录！

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "mdns.h"
#include "lwip/sockets.h"

static const char *TAG = "EXP3_FULL_PORTAL_HUB";

#define LED2_PIN        GPIO_NUM_27
#define BUTTON_PIN      GPIO_NUM_39

#define AP_SSID         "ESP32-Setup-WiFi"
#define MDNS_HOSTNAME   "esp32"
#define NVS_NAMESPACE   "wifi_store"

typedef enum {
    MODE_PROVISIONING_AP, // 阶段 1：强制门户配网模式 (开热点等待用户输入密码)
    MODE_CONNECTED_STA    // 阶段 2：联网中控模式 (已连路由器，开启网页中控)
} system_mode_t;

static system_mode_t s_mode = MODE_PROVISIONING_AP;
static bool s_led_state = false;
static httpd_handle_t s_server = NULL;
static int s_dns_socket = -1;

/* 📱 阶段 1 配网页面 (带 Wi-Fi 列表自动拉取与下拉选择框) */
static const char HTML_PORTAL_PAGE[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>ESP32 智能设备配网</title>"
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

/* 🌐 阶段 2 中控页面 (Dashboard HTML) */
static const char HTML_DASHBOARD_PAGE[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>ESP32 智能中控台</title>"
"<style>"
"body{background:#0d1117;color:#c9d1d9;font-family:-apple-system,sans-serif;text-align:center;padding:30px;}"
".card{background:#161b22;border:1px solid #30363d;border-radius:20px;max-width:400px;margin:0 auto;padding:26px;box-shadow:0 12px 30px rgba(0,0,0,0.6);}"
"h2{color:#58a6ff;margin:0 0 10px 0;}"
".domain{background:#21262d;color:#58a6ff;padding:6px 14px;border-radius:8px;font-family:monospace;font-size:15px;display:inline-block;margin:10px 0;}"
".btn{background:#238636;color:white;border:none;padding:14px 36px;font-size:18px;font-weight:bold;border-radius:30px;cursor:pointer;margin-top:20px;transition:0.2s;}"
".btn-off{background:#da3633;}"
".badge{display:inline-block;padding:4px 12px;border-radius:12px;background:#21262d;color:#3fb950;font-weight:bold;margin:10px 0;}"
"</style></head><body>"
"<div class='card'>"
"<h2>🌐 ESP32 网页智能中控</h2>"
"<div>访问域名: <span class='domain'>http://esp32.local</span></div>"
"<div>当前绿色 LED2: <span id='led_text' class='badge'>读取中...</span></div>"
"<div><button id='btn' class='btn' onclick='toggle()'>切换 LED 状态</button></div>"
"<p style='color:#8b949e;font-size:12px;margin-top:24px;'>提示: 长按开发板 SW3 按键 3 秒可恢复出厂配网模式</p>"
"</div>"
"<script>"
"function getSt(){fetch('/api/status').then(r=>r.json()).then(d=>{"
"  let s=d.led==1; document.getElementById('led_text').innerText=s?'💡 已点亮 (ON)':'🌑 已熄灭 (OFF)';"
"  document.getElementById('led_text').style.color=s?'#3fb950':'#f85149';"
"  let b=document.getElementById('btn'); b.innerText=s?'关 闭 LED2':'点 亮 LED2'; b.className=s?'btn btn-off':'btn';"
"});}"
"function toggle(){fetch('/api/led').then(()=>getSt());}"
"getSt(); setInterval(getSt, 3000);"
"</script></body></html>";

/* ⏳ 阶段 1 ➔ 阶段 2 过渡页面 (5 秒倒计时自动跳转至 http://esp32.local) */
static const char HTML_SAVED_PAGE[] = 
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<meta http-equiv='refresh' content='6;url=http://esp32.local/'>"
"<title>配网成功 - 正在跳转</title>"
"<style>"
"body{background:#0f172a;color:#f8fafc;font-family:-apple-system,sans-serif;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:90vh;}"
".card{background:#1e293b;border:1px solid #334155;border-radius:20px;padding:32px;width:100%;max-width:380px;text-align:center;box-shadow:0 20px 40px rgba(0,0,0,0.5);}"
"h2{color:#38bdf8;margin:0 0 12px 0;}"
"p{color:#94a3b8;font-size:14px;line-height:1.6;}"
".timer{font-size:36px;font-weight:bold;color:#38bdf8;margin:20px 0;}"
".btn{display:inline-block;background:#0ea5e9;color:#fff;text-decoration:none;padding:12px 28px;font-size:15px;font-weight:bold;border-radius:25px;margin-top:10px;transition:0.2s;}"
".btn:hover{background:#0284c7;}"
"</style></head><body>"
"<div class='card'>"
"<h2>🎉 Wi-Fi 配置已保存！</h2>"
"<p>ESP32 正在自动连入您的路由器<br>请确保手机已切回家庭 Wi-Fi</p>"
"<div class='timer'><span id='cnt'>5</span>s</div>"
"<p>倒计时结束后将自动跳转至中控台...</p>"
"<a href='http://esp32.local/' class='btn'>立即前往中控台 ➔</a>"
"</div>"
"<script>"
"let sec = 5;"
"let t = setInterval(()=>{"
"  sec--;"
"  if(sec >= 0) document.getElementById('cnt').innerText = sec;"
"  if(sec <= 0){ clearInterval(t); location.href='http://esp32.local/'; }"
"}, 1000);"
"</script></body></html>";

/* 🌐 路由 1: GET / (统一主页路由) */
static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    if (s_mode == MODE_PROVISIONING_AP) {
        return httpd_resp_send(req, HTML_PORTAL_PAGE, HTTPD_RESP_USE_STRLEN);
    } else {
        return httpd_resp_send(req, HTML_DASHBOARD_PAGE, HTTPD_RESP_USE_STRLEN);
    }
}

/* 🌐 路由 2: GET /api/scan (扫描空中 Wi-Fi 并返回 JSON 列表) */
static esp_err_t scan_get_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };

    ESP_LOGI(TAG, "📡 正在扫描周围 2.4GHz Wi-Fi 热点...");
    esp_wifi_scan_start(&scan_config, true); // 阻塞扫描

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > 15) ap_count = 15; // 最多展示前 15 个强信号

    wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
    char *json_buf = (char *)malloc(2048);
    strcpy(json_buf, "[");

    if (ap_records && json_buf) {
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);
        int valid_count = 0;
        for (int i = 0; i < ap_count; i++) {
            if (strlen((char *)ap_records[i].ssid) == 0) continue;
            char item[128];
            snprintf(item, sizeof(item), "%s{\"ssid\":\"%s\",\"rssi\":%d}",
                     valid_count > 0 ? "," : "",
                     (char *)ap_records[i].ssid, ap_records[i].rssi);
            strcat(json_buf, item);
            valid_count++;
        }
        strcat(json_buf, "]");
        ESP_LOGI(TAG, "✅ 扫描完成，共找到 %d 个有效 Wi-Fi 热点", valid_count);
    } else {
        strcpy(json_buf, "[]");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);

    if (ap_records) free(ap_records);
    if (json_buf) free(json_buf);
    return ESP_OK;
}

/* 🌐 路由 3: POST /save (保存配网凭证到 NVS) */
static esp_err_t save_post_handler(httpd_req_t *req)
{
    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;

    char ssid[33] = {0}, password[65] = {0};
    char *ssid_ptr = strstr(buf, "ssid="), *pass_ptr = strstr(buf, "password=");
    if (ssid_ptr) sscanf(ssid_ptr, "ssid=%32[^&]", ssid);
    if (pass_ptr) sscanf(pass_ptr, "password=%64s", password);

    ESP_LOGI(TAG, "💾 [收到配网凭证] SSID: \033[32m%s\033[0m, Password: \033[33m%s\033[0m", ssid, password);

    nvs_handle_t nvs_handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_set_str(nvs_handle, "ssid", ssid);
        nvs_set_str(nvs_handle, "password", password);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "✅ Wi-Fi 凭证已存入 NVS 闪存！2 秒后重启连网...");
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, HTML_SAVED_PAGE, HTTPD_RESP_USE_STRLEN);

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char json_buf[128];
    uint32_t uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000);
    snprintf(json_buf, sizeof(json_buf), 
             "{\"led\":%d,\"free_heap\":%lu,\"uptime_sec\":%lu}",
             s_led_state ? 1 : 0, (unsigned long)esp_get_free_heap_size(), (unsigned long)uptime_sec);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t led_control_handler(httpd_req_t *req)
{
    s_led_state = !s_led_state;
    gpio_set_level(LED2_PIN, s_led_state ? 1 : 0);
    ESP_LOGI(TAG, "💡 [网页指令] 翻转 LED2 ➔ %s", s_led_state ? "点亮" : "熄灭");
    return httpd_resp_send(req, "OK", 2);
}

static esp_err_t redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;

    if (httpd_start(&s_server, &config) == ESP_OK) {
        httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_get_handler };
        httpd_register_uri_handler(s_server, &index_uri);

        httpd_uri_t scan_uri = { .uri = "/api/scan", .method = HTTP_GET, .handler = scan_get_handler };
        httpd_register_uri_handler(s_server, &scan_uri);

        httpd_uri_t save_uri = { .uri = "/save", .method = HTTP_POST, .handler = save_post_handler };
        httpd_register_uri_handler(s_server, &save_uri);

        httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler };
        httpd_register_uri_handler(s_server, &status_uri);

        httpd_uri_t led_uri = { .uri = "/api/led", .method = HTTP_GET, .handler = led_control_handler };
        httpd_register_uri_handler(s_server, &led_uri);

        httpd_uri_t apple_uri = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = redirect_handler };
        httpd_register_uri_handler(s_server, &apple_uri);

        httpd_uri_t android_uri = { .uri = "/generate_204", .method = HTTP_GET, .handler = redirect_handler };
        httpd_register_uri_handler(s_server, &android_uri);
    }
}

/* 📡 DNS 劫持任务 */
static void dns_hijack_server_task(void *pvParameters)
{
    uint8_t rx_buf[128];
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    s_dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    bind(s_dns_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int len = recvfrom(s_dns_socket, rx_buf, sizeof(rx_buf), 0, (struct sockaddr *)&client_addr, &client_addr_len);

        if (len > 12) {
            rx_buf[2] |= 0x80; rx_buf[3] |= 0x80; rx_buf[7] = 1;
            int idx = len;
            rx_buf[idx++] = 0xC0; rx_buf[idx++] = 0x0C;
            rx_buf[idx++] = 0x00; rx_buf[idx++] = 0x01;
            rx_buf[idx++] = 0x00; rx_buf[idx++] = 0x01;
            rx_buf[idx++] = 0x00; rx_buf[idx++] = 0x00; rx_buf[idx++] = 0x00; rx_buf[idx++] = 0x3C;
            rx_buf[idx++] = 0x00; rx_buf[idx++] = 0x04;
            rx_buf[idx++] = 192;  rx_buf[idx++] = 168;  rx_buf[idx++] = 4;   rx_buf[idx++] = 1;
            sendto(s_dns_socket, rx_buf, idx, 0, (struct sockaddr *)&client_addr, client_addr_len);
        }
    }
}

static void start_ap_provisioning(void)
{
    s_mode = MODE_PROVISIONING_AP;
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta(); // ⭐️ 开启 STA netif 支持空中扫描

    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "📶  未检测到可用 Wi-Fi 凭证，进入【开箱配网模式】！");
    ESP_LOGI(TAG, "📲  请用手机连接热点: \033[36m%s\033[0m", AP_SSID);
    ESP_LOGI(TAG, "🌐  手机连上后将自动弹窗输入 Wi-Fi 密码！");
    ESP_LOGI(TAG, "==================================================");

    start_webserver();
    xTaskCreate(dns_hijack_server_task, "dns_hijack", 3072, NULL, 5, NULL);
}

static void start_sta_connected(const char *ssid, const char *pass)
{
    s_mode = MODE_CONNECTED_STA;
    esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "🔄 正在连接家庭 Wi-Fi: \033[32m%s\033[0m...", ssid);
    esp_wifi_connect();
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, "🎉 Wi-Fi 连接成功！物理 IP: " IPSTR, IP2STR(&event->ip_info.ip));

        start_webserver();

        ESP_ERROR_CHECK(mdns_init());
        ESP_ERROR_CHECK(mdns_hostname_set(MDNS_HOSTNAME));
        mdns_service_add("ESP32-Web-Server", "_http", "_tcp", 80, NULL, 0);

        ESP_LOGI(TAG, "🌐 请打开电脑/手机浏览器访问: \033[32mhttp://%s.local\033[0m", MDNS_HOSTNAME);
        ESP_LOGI(TAG, "==================================================");

        gpio_set_level(LED2_PIN, 1);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "⚠️ Wi-Fi 连接失败，尝试重新连接...");
        esp_wifi_connect();
    }
}

static void factory_reset_button_task(void *pvParameters)
{
    int press_ms = 0;
    while (1) {
        if (gpio_get_level(BUTTON_PIN) == 0) {
            press_ms += 100;
            if (press_ms >= 3000) {
                ESP_LOGW(TAG, "⚠️ [长按 3 秒触发] 正在清除 NVS Wi-Fi 配网凭证...");
                nvs_handle_t nvs_handle;
                if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
                    nvs_erase_all(nvs_handle);
                    nvs_commit(nvs_handle);
                    nvs_close(nvs_handle);
                }
                for (int i = 0; i < 5; i++) {
                    gpio_set_level(LED2_PIN, 1);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    gpio_set_level(LED2_PIN, 0);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                ESP_LOGI(TAG, "🔄 恢复出厂设置成功！正在重启重新进入配网模式...");
                esp_restart();
            }
        } else {
            press_ms = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 16 实验 3：全生命周期配网与网页中控大成  ");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t io_conf = { .pin_bit_mask = (1ULL << LED2_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io_conf);
    gpio_set_level(LED2_PIN, 0);

    gpio_config_t btn_conf = { .pin_bit_mask = (1ULL << BUTTON_PIN), .mode = GPIO_MODE_INPUT };
    gpio_config(&btn_conf);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    nvs_handle_t nvs_handle;
    char saved_ssid[33] = {0}, saved_pass[65] = {0};
    size_t ssid_len = sizeof(saved_ssid), pass_len = sizeof(saved_pass);
    bool has_wifi = false;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) == ESP_OK) {
        if (nvs_get_str(nvs_handle, "ssid", saved_ssid, &ssid_len) == ESP_OK) {
            nvs_get_str(nvs_handle, "password", saved_pass, &pass_len);
            has_wifi = true;
            ESP_LOGI(TAG, "📦 [NVS 读取成功] 找到已存 Wi-Fi: \033[32m%s\033[0m", saved_ssid);
        }
        nvs_close(nvs_handle);
    }

    if (has_wifi) {
        start_sta_connected(saved_ssid, saved_pass);
    } else {
        start_ap_provisioning();
    }

    xTaskCreate(factory_reset_button_task, "reset_btn", 3072, NULL, 5, NULL);
}
```

---

### 3. 🔍 源码逐段深度精讲

1. **`s_mode` 模式状态机与双页面渲染**：
   * 采用统一的 `index_get_handler` 路由：
     * 如果处于 `MODE_PROVISIONING_AP` 配网热点阶段，向手机返回 **输入 Wi-Fi 密码的配网页**；
     * 如果处于 `MODE_CONNECTED_STA` 已联网阶段，向浏览器返回 **深色控制台 Dashboard 页面**！
2. **DNS 劫持与 Captive Portal 弹窗触发**：
   * UDP 53 端口监听并全量返回 `192.168.4.1`，配合 `/hotspot-detect.html` 与 `/generate_204` 返回 302 重定向，**彻底激活 iOS、Android 和 Windows 系统顶层的强制自动弹窗**！
3. **SW3 硬件按键防误触长按重置（3 秒）**：
   * 在独立 FreeRTOS 任务中检测 `GPIO39`，连续按下满 3000ms 执行 `nvs_erase_all()` 并伴随 LED2 5 次快闪报警，随后重启系统重回配网模式。

---

### 4. 📱 手机实测真机震撼全流程体验

1. **开箱配网体验**：
   * 切换到实验 3 并烧录：`./switch_code.sh 16 3 --flash`；
   * 拿出一台手机，打开 Wi-Fi 列表连接 **`ESP32-Setup-WiFi`**；
   * 👉 **奇迹发生**：手机无需打开浏览器，屏幕**立即自动弹出【ESP32 智能开箱配网】页面**！
   * 输入您家里的 Wi-Fi 名称和密码，点击“保存并连接网络”。
2. **无缝转入网页中控**：
   * ESP32 自动重启并成功连上您家里的 Wi-Fi；
   * 手机切回家庭 Wi-Fi，打开自带浏览器输入 **`http://esp32.local`**；
   * 👉 **瞬间进入【ESP32 网页智能中控】界面**，点击按钮即可远程开关 LED2！
---

## 16.6 关卡总结与通关打卡

恭喜你！你已经掌握了当今商业级物联网产品最核心的轻量化交互与开箱配网体系 —— **Web Server + mDNS + Captive Portal**！

### 🏆 核心技能清单回顾：
* [x] **嵌入式 Web 服务器**：掌握 `esp_http_server` 的路由注册与 HTML/JSON 异步交互；
* [x] **mDNS 本地域名解析**：实现 `http://esp32.local` 零配置免查 IP 直达；
* [x] **DNS 劫持技术**：掌握 UDP 53 端口捕获与全网流量引流；
* [x] **AP 强制门户配网**：掌握 iOS/Android 自动弹窗探测与 NVS 凭证持久化。

---

当设备已经部署到千里之外的用户家中或野外，如果固件发现了 Bug 需要修复，或者想要增加新功能，总不可能把设备拆开用串口线烧录吧？  
下一关，我们将学习商业级物联网设备的终极运维武器 —— **OTA 空中固件升级与 A/B 双分区防变砖回退**！

请翻开 [**第 17 章：ESP32 OTA 空中无线固件升级与 A/B 双分区防变砖回滚实战**](./17_OTA空中固件升级与防变砖回滚.md)！

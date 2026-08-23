# 第 12 关：让 ESP32 上网——Wi-Fi、网络时间与互联网天气

![第12关封面插画](../docs/images/esp32_level12_cover.jpg)

这一关，我们把 ESP32 从一台只会做本地事情的小机器，变成一只会“上网问问题”的小助手：它先连家里的路由器，再向时间服务器问现在几点，最后向天气服务器问北京现在的天气。

> 本关的天气和时间显示在**串口日志**中，不会自动画到 LCD。把数据放到屏幕上，是后续把“联网”和“界面”组合起来的练习。

---

## 12.1 这一关你会完成什么

完成三个实验后，你可以解释并亲手验证这条链路：

```text
ESP32 ──Wi-Fi──> 家用路由器 ──> 互联网
  │                 │              │
  │                 └─ 分配 IP      ├─ 时间服务器（SNTP）
  │                                └─ 天气服务器（HTTP）
  │
  └─ 把收到的文字（JSON）拆成气温、风速和天气状态
```

| 实验 | 先做什么 | 成功时你看到什么 |
| --- | --- | --- |
| 1 | ESP32 连接 2.4 GHz Wi-Fi | 串口打印路由器分配的 IP 地址 |
| 2 | 向网络时间服务器对时 | 串口每秒打印一次北京时间 |
| 3 | 请求天气接口并解析 JSON | 串口每分钟打印气温、风速和天气 |

---

## 12.2 开始前：只做这四件事

1. 不需要新增杜邦线。Wi-Fi 是 ESP32 芯片内置的无线功能。
2. 准备一个 **2.4 GHz** Wi-Fi 名称和密码。本项目的 ESP32-WROOM-32E 不支持直接连接 5 GHz Wi-Fi。
3. 手机热点也可以，但要在热点设置中明确开启 2.4 GHz；只写“自动频段”的热点可能让 ESP32 看不到。
4. 打开实验源码，把下面两行改成自己的网络信息。不要把真实密码提交到 Git 仓库。

```c
#define EXAMPLE_WIFI_SSID "YOUR_WIFI_SSID"
#define EXAMPLE_WIFI_PASS "YOUR_WIFI_PASSWORD"
```

源码和本章是一一对应的：

| 实验 | 完整源码 | 切换并烧录 |
| --- | --- | --- |
| 1 | [`01_wifi_sta_connect.c`](../code/12_wifi_weather/01_wifi_sta_connect.c) | `./switch_code.sh 12 1 --flash` |
| 2 | [`02_sntp_time_sync.c`](../code/12_wifi_weather/02_sntp_time_sync.c) | `./switch_code.sh 12 2 --flash` |
| 3 | [`03_http_weather_clock.c`](../code/12_wifi_weather/03_http_weather_clock.c) | `./switch_code.sh 12 3 --flash` |

> [!TIP]
> 先只运行实验 1。不要一上来就运行天气程序；网络、时间、天气三件事一起出错时，最难判断是哪一步有问题。

---

## 12.3 先有地图：数据从哪里来，最后去了哪里？

把网络想成寄快递。

```text
你（ESP32） ──> 小区前台（路由器） ──> 快递网络（互联网） ──> 商家（服务器）
       └──── 前台先给你一个房间号：IP 地址 ────┘
```

这里有六个词，先记住它们分别解决什么问题：

| 名词 | 生活中的感觉 | 在本关里的作用 |
| --- | --- | --- |
| Wi-Fi | 你和路由器之间的无线通道 | 让 ESP32 能找到家里的路由器 |
| STA | 一台主动去连 Wi-Fi 的设备 | ESP32 本关扮演的角色，和手机相似 |
| IP 地址 | 小区里的门牌号 | 路由器用它把网络数据送回 ESP32 |
| DNS | 把“店名”查成“街道门牌” | 把 `api.open-meteo.com` 查成服务器 IP |
| HTTP | 点单时的一问一答 | ESP32 向天气服务发出 GET 请求并接收回答 |
| JSON | 有标签的快递清单 | 服务器返回的文字，里面写着温度、风速等数据 |

### 第一层：直觉

连上 Wi-Fi 不等于已经能访问天气网站。它更像是你的手机已经连上家中路由器；路由器还要给手机分配一个局域网地址，手机才知道“我是谁、回信该送到哪里”。

### 第二层：为什么要等到拿到 IP？

ESP32 连路由器时，先完成无线认证；随后 DHCP（可以理解为路由器的“自动发门牌号”服务）才会给它 IP、网关和 DNS 等信息。

```text
1. ESP32：我要连这个 Wi-Fi
2. 路由器：密码正确，可以进来
3. 路由器：你的 IP 是 192.168.x.x，出门请走我，查名字请问 DNS
4. ESP32：现在才开始问天气和时间
```

在 ESP-IDF 中，第 3 步完成会产生 `IP_EVENT_STA_GOT_IP` 事件。**只有收到它，才启动 SNTP、HTTP 等网络业务。**

### 第三层：真实产品还会多什么？

家用小实验可以把 Wi-Fi 名称和密码写在源码宏里。真实设备通常会使用配网页面或蓝牙配网，把凭据存到 NVS，并增加断线退避、证书校验、远程日志和升级机制。本关先只掌握最小、可观察的链路。

---

## 12.4 为什么不用 `while` 空转等待？——事件和事件组

Wi-Fi 连接需要时间，但 ESP32 不该像站在门口反复喊“连上了吗？”那样空转耗电。

本关用了两个小工具：

```text
系统事件循环：像广播站
  “Wi-Fi 已启动！”
  “Wi-Fi 断开！”
  “已经拿到 IP！”

事件组：像两张便签
  [已连接并拿到 IP]  或  [重试次数用完]
```

事件回调函数负责听广播、贴便签；`app_main()` 用 `xEventGroupWaitBits()` 等待两种结果之一。它不是空循环；Wi-Fi 驱动和系统事件任务仍会继续工作。

```c
if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    ESP_LOGI(TAG, "已从路由器拿到 IP：" IPSTR,
             IP2STR(&event->ip_info.ip));
}
```

掉线时，示例最多重试 5 次。达到上限后，它会设置失败便签并明确停止后续业务，而不是“永远卡在等待网络”。

---

## 12.5 实验 1：先拿到一个 IP 地址

### 运行

1. 打开 [`01_wifi_sta_connect.c`](../code/12_wifi_weather/01_wifi_sta_connect.c)，填写 Wi-Fi 名称和密码。
2. 在项目根目录运行：

   ```bash
   ./switch_code.sh 12 1 --flash
   ```

3. 打开串口监视器，等待连接结果。

### 成功时应该看到

```text
I (....) WIFI_STA: Wi-Fi 已启动，正在向路由器发起连接...
I (....) WIFI_STA: 已从路由器拿到 IP：192.168.1.123
I (....) WIFI_STA: 网络准备完成：下一步可以进行 DNS、HTTP 或 SNTP 请求。
```

IP 的具体数字会不同，这是正常的。它由你的路由器分配。

### 读懂最关键的三块代码

**1. 选择 STA 模式。** STA（Station）就是“客户端”。本关 ESP32 像手机一样加入已有路由器；AP（热点）则是 ESP32 自己创建一个 Wi-Fi，让手机来连它。

```c
ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
ESP_ERROR_CHECK(esp_wifi_start());
```

**2. 连接断开时有限重试。** `s_retry_count` 是计数器。密码错、信号弱或路由器关机时，它最多再试 5 次。

```c
if (s_retry_count < EXAMPLE_WIFI_MAX_RETRY) {
    s_retry_count++;
    ESP_ERROR_CHECK(esp_wifi_connect());
} else {
    xEventGroupSetBits(s_wifi_event_group, WIFI_FAILED_BIT);
}
```

**3. 等的是“成功或失败”，不是无期限的希望。**

```c
EventBits_t bits = xEventGroupWaitBits(
    s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
    pdFALSE, pdFALSE, portMAX_DELAY);
```

当 `WIFI_CONNECTED_BIT` 出现，说明已经有 IP；当 `WIFI_FAILED_BIT` 出现，说明本次连接失败，应先排查网络再继续。

---

## 12.6 实验 2：向网络问“现在几点”

断电后的 ESP32 不知道现实世界的日期和时间。联网后，它可以使用 SNTP（Simple Network Time Protocol，简单网络时间协议）向时间服务器询问标准时间。

```text
ESP32：现在几点？
时间服务器：这是 UTC 时间戳
ESP32：我在中国，用时区规则显示为 UTC+8 的北京时间
```

### 运行与成功标准

填写 Wi-Fi 信息后运行：

```bash
./switch_code.sh 12 2 --flash
```

成功日志类似：

```text
I (....) SNTP_CLOCK: 已拿到 IP，可以开始网络授时。
I (....) SNTP_CLOCK: 正在等待网络时间服务器响应，最长等待 10 秒...
I (....) SNTP_CLOCK: 已收到授时响应，系统时间已校准。
I (....) SNTP_CLOCK: 北京时间：2026-08-22 10:30:45
```

### 先分清两件事：对时与显示时区

服务器提供的是 UTC（协调世界时）。`TZ` 和 `tzset()` 只告诉 C 库“把时间显示成哪个时区”，它们不是把服务器给的时间硬加 8 小时。

```c
setenv("TZ", "CST-8", 1);
tzset();

esp_sntp_config_t config =
    ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
ESP_ERROR_CHECK(esp_netif_sntp_init(&config));

if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
    ESP_LOGW(TAG, "等待授时超时。请检查网络是否能访问时间服务器。");
}
```

`esp_netif_sntp_sync_wait()` 是这次改造最重要的一处：程序确实等到收到授时响应，才说“已校准”。网络延迟、服务器状态和本地时钟都会影响精度，因此不要把它理解为“毫秒级绝对准确”。

---

## 12.7 实验 3：向天气服务请求一份 JSON

HTTP 就是一次短暂的“一问一答”。浏览器访问网页时也在做类似的事，只是浏览器把结果排成漂亮页面；ESP32 先把原始文字交给你看。

```text
ESP32  ── GET /v1/forecast?... ──> Open-Meteo 天气服务
ESP32  <── HTTP 200 + JSON ─────  Open-Meteo 天气服务
```

本关请求的北京坐标是 `39.9042, 116.4074`。请求中只要三项“当前天气”：

```c
#define WEATHER_API_URL \
    "http://api.open-meteo.com/v1/forecast?latitude=39.9042&longitude=116.4074" \
    "&current=temperature_2m%2Cwind_speed_10m%2Cweather_code"
```

`%2C` 是逗号的 URL 写法，所以它的实际含义是：`temperature_2m,wind_speed_10m,weather_code`。

### 运行与成功标准

```bash
./switch_code.sh 12 3 --flash
```

成功日志类似：

```text
I (....) WEATHER_CLOCK: 已拿到 IP，可以访问网络服务。
I (....) WEATHER_CLOCK: 网络时间已同步。
I (....) WEATHER_CLOCK: 北京当前天气：多云，气温 26.4 °C，风速 11.2 km/h
I (....) WEATHER_CLOCK: 北京时间：2026-08-22 10:30:45
```

天气会变化，数字和“晴朗/多云/下雨”等文字不保证与示例完全相同。

### JSON 是什么？——带标签的收据

服务器回答大致像这样：

```json
{
  "current": {
    "temperature_2m": 26.4,
    "wind_speed_10m": 11.2,
    "weather_code": 2
  }
}
```

不要把 JSON 当成“固定第几行的文字”。它像一张有项目名的收据：先找到 `current` 这一栏，再按名字找 `temperature_2m`、`wind_speed_10m` 和 `weather_code`。

```c
cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
cJSON *temperature = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");

if (!cJSON_IsNumber(temperature)) {
    ESP_LOGE(TAG, "JSON 字段不完整或类型不对，本次不显示天气。");
}
```

为什么多了一步 `cJSON_IsNumber()`？因为服务器可能临时返回错误 JSON、字段可能不存在，或者接口将来改变。直接访问 `temperature->valuedouble` 就像没打开包裹先伸手拿东西，容易崩溃。

每次成功解析后，必须释放整棵 JSON 树：

```c
cJSON_Delete(root);
```

### 缓冲区：给快递准备多大的收件箱？

示例准备了 1024 字节的 `s_response_buffer`。HTTP 回答可能分多次到达，`HTTP_EVENT_ON_DATA` 每收到一段就追加；如果总数据超过收件箱，代码会记录错误并放弃本次解析，避免写坏内存。

```c
if ((size_t)event->data_len > free_space) {
    s_response_too_large = true;
    return ESP_FAIL;
}
```

### 一个重要的安全边界

这份代码使用 **HTTP**，是为了把“请求—响应—JSON”讲清楚，避免第一次接触网络时同时学习 TLS 证书。

HTTP 内容可能被篡改或窥视。真实产品访问天气、账号、控制指令等服务时，应改用 **HTTPS**，并启用服务器证书校验；不能把本实验的明文 HTTP 当作生产方案。

---

## 12.8 三层理解：为什么物联网要这样分步骤？

### 第一层：把事情一件件做完

- 先连 Wi-Fi；
- 再等路由器分配 IP；
- 再对时；
- 最后请求天气。

每一层失败，都能从串口看到它停在哪一层。

### 第二层：资源有限也要可靠

ESP32 的内存和任务数有限。事件驱动避免空循环占用 CPU；事件组把成功和失败变成明确状态；固定大小缓冲区防止网络数据无限吃内存；检查 JSON 类型避免异常数据让程序访问空指针。

### 第三层：工业产品的选择

| 场景 | 常见做法 | 本关与它的关系 |
| --- | --- | --- |
| 一次性取得天气 | HTTPS + REST/HTTP | 本关先学习 HTTP 的核心模型 |
| 持续上报传感器 | MQTT 长连接 | 下一关会学习 |
| 用户配置家中 Wi-Fi | SoftAP/蓝牙配网 + NVS | 本关只把密码写在示例宏中 |
| 远程控制设备 | HTTPS/MQTT + 身份认证和权限 | 不能只凭拿到 IP 就认为安全 |

---

## 12.9 主动回忆：不用看上文，试着回答

1. ESP32 已经显示“Wi-Fi 已启动”，为什么还不能立刻请求天气？
2. `IP_EVENT_STA_GOT_IP` 说明路由器完成了哪件事？
3. `TZ=CST-8` 是向服务器请求东八区时间，还是改变本地显示方式？
4. HTTP 返回 `200` 后，为什么还要检查 `cJSON_IsNumber()`？
5. 为什么 1024 字节缓冲区满了以后，宁可放弃这次解析也不能继续写？

参考答案：1）还没有 IP；2）分配了网络地址和网络参数；3）改变本地显示方式；4）HTTP 成功不保证字段完整或类型正确；5）继续写会越界并破坏内存。

---

## 12.10 常见问题

| 现象 | 最可能的原因 | 先这样做 |
| --- | --- | --- |
| 连续打印“第 x/5 次重连” | 密码、SSID 不对，或热点是 5 GHz | 用手机确认名称和密码；确认热点开启 2.4 GHz |
| 一直没有“已从路由器拿到 IP” | 路由器拒绝接入、信号太弱或 DHCP 异常 | 把开发板靠近路由器；重启热点；先运行实验 1 单独排查 |
| 显示已拿到 IP，但 SNTP 超时 | 能进局域网，不代表能访问时间服务器 | 检查路由器是否能上网、是否有访客网络隔离或防火墙限制 |
| HTTP 请求失败 | DNS、外网、服务器或网络拦截有问题 | 先确认实验 2 能对时；记录完整 `esp_err_to_name` 日志 |
| 返回 HTTP 不是 200 | 接口地址、网络或服务端暂时异常 | 查看状态码；稍后重试；不要在代码里假装成功 |
| JSON 字段不完整或类型不对 | 服务端返回内容不是预期天气数据 | 打印/保存响应做分析；不要删除 `cJSON_IsNumber()` 检查 |
| 程序编译通过，却没看到真实天气 | 没有烧录、没有填写 Wi-Fi 或没连接串口 | 运行 `--flash`，再观察实际串口日志 |

> [!IMPORTANT]
> `idf.py build` 通过，**不等于已经在真机上联网成功**。构建只证明源码能被编译和链接；是否拿到 IP、是否能对时、是否能访问天气接口，必须以你自己的开发板串口日志为准。

---

## 12.11 动手练习

1. **观察断线重连**：实验 1 成功后，临时关掉手机热点或路由器 Wi-Fi。观察重试日志，再恢复网络，思考为什么本例达到 5 次就停止。
2. **更换城市**：把实验 3 的经纬度改成你所在城市的经纬度。只改 `latitude` 和 `longitude`，先不要修改 JSON 字段名。
3. **新增一项数据**：阅读天气服务文档，添加一个当前字段，例如 `relative_humidity_2m`。依次完成：修改 URL、查看 JSON、用 `cJSON_IsNumber()` 检查、再打印结果。
4. **思考题**：如果天气服务突然返回 2 KB 数据，本例会怎样？如果产品需要支持更大的数据，你会增大静态缓冲区，还是改成分段解析？分别有什么代价？

---

## 12.12 本关小结与下一关

你已经完成了一条最小互联网链路：

```text
Wi-Fi STA → DHCP 获得 IP → SNTP 对时 → HTTP 请求 → JSON 安全解析
```

下一关会学习 MQTT。HTTP 更像“每次想问就打一次电话”；MQTT 更像“长期保持一个消息频道”，适合设备持续上报状态和接收控制命令。

继续阅读：[第 13 关：ESP32 MQTT 协议接入与阿里云 IoT 实战](./13_MQTT协议接入与阿里云IoT实战.md)。

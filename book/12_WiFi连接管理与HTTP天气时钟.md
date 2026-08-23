# 第 12 关：让 ESP32 上网——Wi-Fi、网络时间与互联网天气

![第12关封面插画](../docs/images/esp32_level12_cover.jpg)

这一关，我们把 ESP32 从一台只会做本地事情的小机器，变成一只会“上网问问题”的小助手：它先连家里的路由器，再向时间服务器问现在几点，最后向天气服务器问北京现在的天气。

> 本关的天气和时间显示在**串口日志**中，不会自动画到 LCD。把数据放到屏幕上，是后续把“联网”和“界面”组合起来的练习。

---

## 12.1 这一关你会完成什么

完成四个实验后，你可以解释并亲手验证这条链路：

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
| 3 | 请求天气接口并解析 JSON | 串口每分钟打印气温、风速和天气（明文 HTTP） |
| 4 | 挂载 CA 根证书包进行 HTTPS 请求 | 串口打印通过 TLS 证书校验获取的加密天气数据 |

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
| 4 | [`04_https_weather_ssl.c`](../code/12_wifi_weather/04_https_weather_ssl.c) | `./switch_code.sh 12 4 --flash` |

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

## 12.4 📚 核心机制拆解：为什么不用 `while` 死等？——FreeRTOS 事件组与事件循环深度解密

很多初学者在写联网程序时，最直觉的想法是写一个全局变量和死循环：
```c
// ❌ 初学者容易写的“死等轮询”（极度浪费资源与电量）：
while (g_wifi_connected == false) {
    vTaskDelay(pdMS_TO_TICKS(100)); // 不停醒来反复问“连上了吗？”
}
```
但在真正的嵌入式操作系统（FreeRTOS）与工业级物联网设备中，这种做法存在巨大缺陷：
1. **CPU 白白空转**：频繁唤醒查询会占用 CPU 运算资源，导致设备发热并消耗宝贵电量；
2. **多状态难以协同**：如果既想等“连接成功”，又想等“重试超时失败”，还要等“Wi-Fi 密码错误”，用简单的布尔变量会写出极度混乱的 `if-else`；
3. **缺少原子性保护**：多任务并发读写全局变量容易引发竞态冲突（Race Condition）。

为了优雅、低功耗、高可靠地解决这个问题，ESP32 采用了一对黄金搭档：**【ESP-IDF 系统事件循环（广播站）】 + 【FreeRTOS 事件组（多色任务指示牌）】**！

---

### 1. 核心心智模型：广播站与多色指示牌

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │            【ESP32 Wi-Fi 异步事件通知与 FreeRTOS 事件组工作全景】         │
 │                                                                        │
 │  ① 发生物理事件               ② 听广播的值班员                  ③ 多色任务指示牌 (事件组) │
 │  ┌──────────────┐             ┌────────────────┐              ┌────────────────┐       │
 │  │ Wi-Fi 驱动层 │ ──广播通知─►│ wifi_event_    │ ──举起绿旗──►│ [Bit 0] 拿到 IP │       │
 │  │ (拿到 IP 啦) │  (ESP-IDF)  │ handler 回调   │              │ [Bit 1] 连接失败│       │
 │  └──────────────┘             └────────────────┘              └────────────────┘       │
 │                                                                       ▲                │
 │                                                                 ④ 瞬间唤醒             │
 │                                                                       │                │
 │                                                               ┌────────────────┐       │
 │                                                               │ app_main() 任务│       │
 │                                                               │ (深度休眠 0%CPU)│       │
 │                                                               └────────────────┘       │
 └────────────────────────────────────────────────────────────────────────┘
```

* 📢 **ESP-IDF 系统事件循环（像广播站）**：
  Wi-Fi 底层驱动在后台默默工作，每当状态改变（如“Wi-Fi 启动了”、“掉线了”、“成功从路由器获取到 IP 了”），它就在系统总线里大喊一声广播；
* 👷 **事件回调函数（像听广播的值班员）**：
  它订阅了广播。当听到“拿到 IP”时，就跑到门口指示牌上把 **绿旗（Bit 0）** 插上；如果重试 5 次依然失败，就把 **红旗（Bit 1）** 插上；
* 🪧 **FreeRTOS 事件组（像多色任务指示牌）**：
  主程序 `app_main()` 发起联网后，**立刻进入深度阻塞休眠（CPU 占用率为 0%）**。一旦绿旗或红旗被插上，FreeRTOS 调度器以微秒级的速度精准把主程序唤醒！

---

### 2. FreeRTOS 事件组四大核心 API 字典（保姆级拆解）

事件组本质上是一个由 24 个二进制位（Bit 0 ~ Bit 23）组成的无符号整数，每一位都可以代表一个独立的物理状态（0 表示未发生，1 表示已发生）。

```c
#define WIFI_CONNECTED_BIT   BIT0  // (1 << 0) 二进制: 0000 0001 (代表成功拿到 IP)
#define WIFI_FAILED_BIT      BIT1  // (1 << 1) 二进制: 0000 0010 (代表重试超限失败)
```

| 函数原型 | 生活比喻 | 核心功能与参数深度剖析 |
| :--- | :--- | :--- |
| **`xEventGroupCreate()`** | **在门口钉一块全新的空白指示牌** | **创建事件组对象**。<br>• 在 FreeRTOS 堆区分配一块内存，所有 24 个标志位默认初始化为 0；<br>• 返回 `EventGroupHandle_t` 句柄指针，供后续所有任务共享。 |
| **`xEventGroupSetBits(group, bits)`** | **插上一面指定颜色的旗子** | **将指定的标志位置 1（触发事件）**。<br>• `group`：要操作的事件组句柄；<br>• `bits`：要置 1 的位掩码（如 `WIFI_CONNECTED_BIT`）；<br>• **关键特性**：一旦置位，如果有任务正在等待该位，FreeRTOS 会立即将其从阻塞队列中唤醒！ |
| **`xEventGroupClearBits(group, bits)`** | **拔掉旗子，准备迎接下一次任务** | **将指定的标志位清零（复位）**。<br>• 常用于在断线重连前，将之前的成功/失败标志清除。 |
| 🚨 **`xEventGroupWaitBits(...)`** | **在指示牌前深度休眠，见旗即醒** | **阻塞等待指定的事件发生（核心函数）**。<br>（参数详细拆解见下方专属表格）。 |

---

### 3. 🚨 重点难点突破：`xEventGroupWaitBits` 五大参数全景解密

这是整个嵌入式多任务同步中最强大也最常用的函数，它的原型如下：

```c
EventBits_t xEventGroupWaitBits(
    EventGroupHandle_t xEventGroup,       // 参数 1：等待哪块指示牌
    const EventBits_t  uxBitsToWaitFor,   // 参数 2：你关注哪些小旗子？
    const BaseType_t   xClearOnExit,      // 参数 3：醒来后要不要顺手把旗子拔掉？
    const BaseType_t   xWaitForAllBits,   // 参数 4：是“只要一面旗(OR)”还是“必须全齐(AND)”？
    TickType_t         xTicksToWait       // 参数 5：最长愿意等多久？
);
```

#### 🔍 各参数实战选项与应用场景：

| 参数名称 | 选项与常用值 | 通俗含义与设计权衡 |
| :--- | :--- | :--- |
| **`uxBitsToWaitFor`** | `WIFI_CONNECTED_BIT \| WIFI_FAILED_BIT` | **关注的目标位掩码**。<br>我们既关心“连接成功”，也关心“连接失败”，所以用按位或 `\|` 把它们组合起来一起监听。 |
| **`xClearOnExit`** | `pdFALSE` 或 `pdTRUE` | **退出时自动清除**。<br>• `pdFALSE`：醒来后保留指示牌上的标志（方便后续其他模块也能查看状态）；<br>• `pdTRUE`：一旦被唤醒，系统自动把这几个位清零。 |
| **`xWaitForAllBits`** | ⭐️ `pdFALSE` (或运算逻辑)<br>————<br>`pdTRUE` (与运算逻辑) | **多事件逻辑关系（极度重要）**：<br>• **`pdFALSE`（逻辑或 OR）**：**“只要其中任意一个旗子立起来就醒”**（本关使用！因为成功和失败是二选一，谁先发生就先处理谁）；<br>• **`pdTRUE`（逻辑与 AND）**：**“必须所有关注的旗子全部立起来才醒”**（例如：“Wi-Fi已连 + 时间已对齐 + 传感器就绪”三者齐全才开始上传数据）。 |
| **`xTicksToWait`** | `portMAX_DELAY` 或 `pdMS_TO_TICKS(10000)` | **最大等待超时时间**：<br>• `portMAX_DELAY`：永久阻塞休眠，直到有关注的事件发生才醒来；<br>• `pdMS_TO_TICKS(10000)`：最多等 10 秒，10 秒到了即使没事件也强制醒来。 |
| **返回值 `EventBits_t`** | `EventBits_t bits` | **被唤醒瞬间指示牌的具体状态快照**。<br>通过 `if (bits & WIFI_CONNECTED_BIT)` 判断到底是因为成功还是因为失败被唤醒的！ |

#### 💡 实战代码范例（一目了然）：

```c
// 1. 创建事件组
s_wifi_event_group = xEventGroupCreate();

// 2. 主任务在此深度阻塞休眠，等待连接成功或彻底失败
EventBits_t bits = xEventGroupWaitBits(
    s_wifi_event_group,
    WIFI_CONNECTED_BIT | WIFI_FAILED_BIT, // 监听成功与失败两面旗
    pdFALSE,                              // 退出时不清除标志
    pdFALSE,                              // pdFALSE 代表 OR 逻辑，任意一个发生即唤醒
    portMAX_DELAY                         // 永久等待事件通知
);

// 3. 醒来后精准判断结果：
if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "🎉 收到成功通知：已拿到 IP，立刻开始后续天气业务！");
} else if (bits & WIFI_FAILED_BIT) {
    ESP_LOGE(TAG, "❌ 收到失败通知：超过最大重试次数，终止联网操作！");
}
```

---

### 4. 🌐 Wi-Fi 底层架构与初始化“经典七步曲”流水线（保姆级拆解）

在 `wifi_init_sta()` 函数中，有几行看起来神秘又标准的代码，它们是整个 ESP-IDF 联网体系的基石：

```c
ESP_ERROR_CHECK(esp_netif_init());
ESP_ERROR_CHECK(esp_event_loop_create_default());
esp_netif_create_default_wifi_sta();

wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
ESP_ERROR_CHECK(esp_wifi_init(&init_config));
```

为什么连个 Wi-Fi 需要调用这么多初始化函数？我们用“给电脑联网”的生活场景来一一对应：

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                    【ESP32 Wi-Fi 联网初始化经典七步曲】                 │
 │                                                                        │
 │  ① nvs_flash_init()                 ──► 唤醒 Flash 硬盘 (读取射频校准参数) │
 │  ② esp_netif_init()                 ──► 安装 TCP/IP 协议栈 (邮政通信系统)  │
 │  ③ esp_event_loop_create_default()  ──► 架设系统广播大喇叭 (事件总线)      │
 │  ④ esp_netif_create_default_wifi_sta() ──► 插上名为“STA”的无线网卡        │
 │  ⑤ esp_wifi_init(&init_config)      ──► 加载 Wi-Fi 芯片底层硬件驱动与显存  │
 │  ⑥ esp_event_handler_instance_register() ──► 值班员拿起对讲机听广播通知   │
 │  ⑦ esp_wifi_start()                 ──► 给天线通电，正式发射无线电波连路由 │
 └────────────────────────────────────────────────────────────────────────┘
```

#### 🔍 核心系统函数功能解密字典：

| 函数原型 | 生活比喻 | 核心功能与参数深度剖析 |
| :--- | :--- | :--- |
| **`nvs_flash_init()`** | **唤醒 Flash 硬盘抽屉** | **初始化非易失性存储（NVS）**。<br>ESP32 的 Wi-Fi 射频天线在出厂时校准的功率参数存放在 NVS 分区中。如果 NVS 没初始化，Wi-Fi 驱动直接报错崩溃！ |
| **`esp_netif_init()`** | **安装 TCP/IP 邮局系统** | **初始化底层网络协议栈（LwIP 引擎）**。<br>单片机原本不懂什么是 IP 地址、什么是 TCP 三次握手、什么是 UDP。调用该函数会启动轻量级嵌入式 TCP/IP 协议栈（LwIP - Lightweight IP），赋予单片机处理互联网数据包的基础能力。 |
| **`esp_event_loop_create_default()`** | **架设全厂广播大喇叭** | **创建默认系统事件循环总线**。<br>在后台启动一个专属任务，专门负责派发全局系统事件（Wi-Fi 启动、掉线、拿到 IP、蓝牙连接等）。 |
| **`esp_netif_create_default_wifi_sta()`** | **插上一张无线网卡** | **创建默认 Wi-Fi Station 网络接口（Netif）**。<br>ESP32 既能当客户端（STA 连路由器），又能当热点（AP 供别人连），还能插有线网口（Ethernet）。该函数会生成一个标准的 STA 客户端网卡，并自动把它与 LwIP 协议栈和 DHCP 客户端绑定。 |
| **`esp_wifi_init(&cfg)`** | **安装 Wi-Fi 硬件驱动程序** | **初始化 Wi-Fi 控制器硬件与资源**。<br>`cfg` 使用宏 `WIFI_INIT_CONFIG_DEFAULT()`，为 Wi-Fi 驱动分配 FreeRTOS 任务优先级、DMA 接收队列、环形缓冲区（RingBuffer）和加密计算核心。 |
| **`esp_event_handler_instance_register(...)`** | **给值班员调好对讲机频道** | **向广播总线订阅感兴趣的事件**。<br>告诉广播站：“每当发生 `WIFI_EVENT`（底层连接事件）或 `IP_EVENT_STA_GOT_IP`（拿到IP事件）时，立刻呼叫我的 `wifi_event_handler` 回调函数！” |
| **`esp_wifi_set_mode(WIFI_MODE_STA)`** | **选择网卡工作模式** | 将 Wi-Fi 模式设定为 `WIFI_MODE_STA`（客户端模式）。 |
| **`esp_wifi_set_config(WIFI_IF_STA, &cfg)`** | **输入 Wi-Fi 账号密码** | 将目标路由器的 SSID、密码以及加密方式（如 WPA2）写入 Wi-Fi 驱动。 |
| **`esp_wifi_start()`** | **给天线通电，开始工作** | **正式启动 Wi-Fi 射频硬件**。<br>芯片天线通电发射 2.4GHz 无线电信号，并向广播站发出 `WIFI_EVENT_STA_START` 事件，触发自动连接！ |

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

## 12.7 实验 3：IP 自动地理定位与动态天气请求（两段式物联网流水线）

在很多初级教程中，天气程序的经纬度通常是**写死在代码里**的（例如固定写死北京 `39.9042, 116.4074`）。  
但真实的智能音箱、天气时钟或车载终端是**随人移动的** —— 用户把设备带到深圳、上海或纽约，时钟怎么能自适应显示当地天气？

本实验将带你实现一套工业界标准的**“两段式物联网自动化流水线”**：**【IP 定位 ➔ 动态组装 ➔ 当地天气】**！

---

### 1. 核心流程：两段式级联请求工作全景

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │            【ESP32 IP 自动地理定位与动态天气请求级联流水线】             │
 │                                                                        │
 │  【第一阶段：IP 物理位置探测】                                          │
 │  ESP32 ──GET /json/ (带公网源IP)──► IP-API 免费定位服务                 │
 │  ESP32 ◄──返回 JSON: {"city":"Shenzhen","lat":22.54,"lon":114.05}───── │
 │    │                                                                   │
 │    ▼ [cJSON 提取经纬度与城市名]                                         │
 │                                                                        │
 │  【第二阶段：动态组装与天气获取】                                       │
 │  ESP32 ──拼接生成 URL: https://api.open-meteo.com/v1/forecast?         │
 │          latitude=22.54&longitude=114.05&current=... ──► 天气服务器     │
 │  ESP32 ◄──返回当地实时温度、风速与天气代码 JSON ─────────────────────── │
 └────────────────────────────────────────────────────────────────────────┘
```

---

### 2. 运行与成功验证

1. 打开 [`03_http_weather_clock.c`](../code/12_wifi_weather/03_http_weather_clock.c)，填写你的 Wi-Fi 账号与密码；
2. 在项目根目录下一键切换并烧录：

```bash
./switch_code.sh 12 3 --flash
```

3. 打开串口监视器，观察自动定位与动态天气获取的完整链路日志：

```text
I (....) WEATHER_CLOCK: 已拿到 IP，可以访问网络服务。
I (....) WEATHER_CLOCK: 网络时间已同步。
I (....) WEATHER_CLOCK: 🔍 正在通过当前 IP 查询物理地理位置...
I (....) WEATHER_CLOCK: --------------------------------------------------
I (....) WEATHER_CLOCK: 📍 [IP 地理定位成功]
I (....) WEATHER_CLOCK:    • 所在国家: China
I (....) WEATHER_CLOCK:    • 省份地区: Guangdong
I (....) WEATHER_CLOCK:    • 当前城市: Shenzhen
I (....) WEATHER_CLOCK:    • 经纬坐标: 纬度 22.5431, 经度 114.0579
I (....) WEATHER_CLOCK: --------------------------------------------------
I (....) WEATHER_CLOCK: 🔗 动态生成的当地天气 URL: http://api.open-meteo.com/v1/forecast?latitude=22.5431&longitude=114.0579...
I (....) WEATHER_CLOCK: ==================================================
I (....) WEATHER_CLOCK:  🌤️ [Guangdong - Shenzhen 实时天气报告]: 
I (....) WEATHER_CLOCK:     - 天气状况: 晴朗 ☀️
I (....) WEATHER_CLOCK:     - 当前气温: 28.5 ℃
I (....) WEATHER_CLOCK:     - 当前风速: 8.2 km/h
I (....) WEATHER_CLOCK: ==================================================
I (....) WEATHER_CLOCK: 🕒 当前时间：2026-08-23 10:30:45 (下次天气更新: 60秒后)
```

---

### 3. 核心技术点 1：IP 定位 JSON 数据的深度解析

当 ESP32 访问 `http://ip-api.com/json/` 时，服务器会根据发起 TCP 连接的**公网 IP**，自动查表并返回 JSON：

```json
{
  "status": "success",
  "country": "China",
  "regionName": "Guangdong",
  "city": "Shenzhen",
  "lat": 22.5431,
  "lon": 114.0579
}
```

#### 🛡️ 为什么解析时需要防崩溃校验？
在 C 语言中解析 JSON，必须时刻防范“字段缺失”或“服务器返回空值”，否则会直接导致单片机访问空指针崩溃（Panic 重启）！

```c
// 1. 检查 status 是否为 "success"
cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "status");
if (!cJSON_IsString(status) || strcmp(status->valuestring, "success") != 0) {
    ESP_LOGE(TAG, "IP 定位接口返回异常");
    cJSON_Delete(root);
    return false;
}

// 2. 安全提取经纬度浮点数
cJSON *lat = cJSON_GetObjectItemCaseSensitive(root, "lat");
cJSON *lon = cJSON_GetObjectItemCaseSensitive(root, "lon");
if (cJSON_IsNumber(lat) && cJSON_IsNumber(lon)) {
    s_current_location.lat = lat->valuedouble;
    s_current_location.lon = lon->valuedouble;
}

// 3. 必须释放整个 JSON 树内存！
cJSON_Delete(root);
```

---

### 4. 核心技术点 2：动态字符串组装（`snprintf` 安全拼接）

在获取到当前的 `lat` 与 `lon` 后，使用 C 标准库 `snprintf` 动态拼接出目标天气接口：

```c
char weather_url[256];
snprintf(weather_url, sizeof(weather_url),
         "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
         "&current=temperature_2m%%2Cwind_speed_10m%%2Cweather_code",
         s_current_location.lat, s_current_location.lon);
```

> [!NOTE]
> 为什么格式化字符串里写的是 `%%2C`？  
> 因为 `%2C` 是 URL 编码中的“逗号（`,`）”。而在 `printf`/`snprintf` 中，单个 `%` 会被当成格式化转义符，所以必须用两个 `%%` 来输出一个原生的 `%`！

---

### 5. 缓冲区复用与内存保护

本实验准备了 2048 字节的通用收发缓冲区 `s_response_buffer`：
1. **先给第一阶段（IP 定位）使用**：接收到约 300 字节的定位 JSON 并解析完毕；
2. **重置清零 `s_buffer_length = 0`**；
3. **给第二阶段（天气查询）复用**：接收天气响应，既节约了单片机宝贵的 RAM，又防止多次 `malloc` 产生内存碎片！

```c
if ((size_t)event->data_len > free_space) {
    s_response_too_large = true;
    ESP_LOGE(TAG, "响应超过缓冲区容量，放弃本次解析！");
    return ESP_FAIL;
}
```

---

## 12.8 实验 4：进阶实战 —— HTTPS 加密通信与 SSL/TLS 证书校验（安全防线）

在了解了 HTTP 的基础模型后，我们正式迈入工业级安全通信的世界：**HTTPS（HTTP over TLS/SSL）**。

### 1. 生活比喻：明信片 vs 绝密防伪保险箱

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                    【HTTP 明文 vs HTTPS 证书加密通信对比】               │
 │                                                                        │
 │  【HTTP 明文通信】（像寄明信片）：                                       │
 │  ESP32 ──"北京气温是多少？"──► 路由器 ──► 互联网 ──► 服务器               │
 │            ▲ 任何路人（黑客）都能偷看，甚至悄悄涂改成 "50℃"！            │
 │                                                                        │
 │  【HTTPS 加密通信】（像绝密防伪保险箱）：                                │
 │  ESP32 ──"🔒 对称密钥加密密文 (a9F8#x...)"──► 互联网 ──► 服务器        │
 │            ▲ 必须先查验服务器的【权威公证处防伪证书 (CA)】，确认身份无误；│
 │            ▲ 链路全程 AES-256 加密，路人拦截后只是一堆乱码！            │
 └────────────────────────────────────────────────────────────────────────┘
```

---

### 2. 核心原理：TLS 握手四步曲与证书链

HTTPS 之所以兼具**“高安全性”**与**“极快速度”**，是因为它巧妙结合了两种加密算法：
1. **非对称加密（RSA / ECC）**：计算量大、安全性极高，专门用于**开工前互相核验身份和交换临时会话密钥**；
2. **对称加密（AES-256）**：计算量极小、速度飞快，后续的所有天气数据传输全部使用该临时密钥加密。

```text
① ESP32                ── "Client Hello (支持哪些加密算法？)" ──► 服务器
② ESP32                ◄── "Server Hello + 数字证书 (包含公钥)" ── 服务器
③ ESP32 (查验 CA 证书) ── "生成随机对称密钥，用服务器公钥加密后发送" ──► 服务器 (私钥解密)
④ ESP32 (AES 加密传输) ◄════════ 双向安全加密数据通道建立完毕 ════════► 服务器 (AES 解密)
```

> 💡 **ESP32 硬件加速黑科技**：
> ESP32 芯片内部集成了专用的 **RSA 4096、AES-256、SHA-512 硬件加密加速引擎**，因此在执行复杂的数论模幂与哈希运算时，速度比普通单片机快数倍，完全不拖慢系统！

---

### 3. ESP-IDF 的三层证书实战段位（推荐全球根证书包）

在单片机上做 HTTPS 校验，通常有三种做法：

| 段位与做法 | 典型代码 | 优缺点与适用场景 |
| :--- | :--- | :--- |
| **🥇 黄金段位（强烈推荐）**<br>**ESP-IDF 全球根证书包** | `.crt_bundle_attach = esp_crt_bundle_attach` | **【本实验采用】**<br>• 内置 Mozilla 认证的全球权威根证书库（Let's Encrypt, DigiCert, GlobalSign 等）；<br>• **无需手动下载/维护 `.pem` 证书文件**，自动验证全球 99.9% 的公网 HTTPS 域名；<br>• 占用极小 Flash 压缩存储，永不轻易失效！ |
| **🥈 白银段位**<br>**手动嵌入单个 PEM 证书** | `.cert_pem = server_root_cert_pem` | • 将某个特定服务器的 CA 根证书文本硬编码编译进固件；<br>• 适用于公司私有云、内网自建 CA 服务器；<br>• 缺点：一旦网站证书换届或根证书过期，单片机必须重新刷机升级固件。 |
| **🥉 青铜段位（不防伪）**<br>**跳过证书校验** | `skip_cert_common_name_check = true` | • 仅进行传输层加密（防偷看），但不核验对方是不是假冒钓鱼服务器（不防中间人劫持）；<br>• 仅限研发初期快速临时调试。 |

---

### 4. 运行与验证

1. 打开 [`04_https_weather_ssl.c`](../code/12_wifi_weather/04_https_weather_ssl.c)，填写你的 Wi-Fi 账号与密码；
2. 在项目根目录下执行一行命令切换并烧录：

```bash
./switch_code.sh 12 4 --flash
```

3. 打开串口监视器，观察通过 TLS 证书握手获取的天气日志：

```text
I (....) HTTPS_WEATHER: ✅ 已拿到 IP，可以安全发起 HTTPS 请求。
I (....) HTTPS_WEATHER: ⏰ 网络时间已成功同步 (UTC+8)。
I (....) HTTPS_WEATHER: 🌐 正在发起 TLS 握手与 HTTPS 天气请求...
I (....) HTTPS_WEATHER: 🔒 HTTPS 请求成功 (HTTP 200, 接收 312 字节密文并解密)
I (....) HTTPS_WEATHER: ==================================================
I (....) HTTPS_WEATHER:  🔒 [HTTPS 安全获取] 北京实时气象报告: 
I (....) HTTPS_WEATHER:     - 天气状况: 晴朗 ☀️
I (....) HTTPS_WEATHER:     - 当前气温: 26.8 ℃
I (....) HTTPS_WEATHER:     - 当前风速: 9.4 km/h
I (....) HTTPS_WEATHER: ==================================================
```

---

### 5. 关键代码对比：从 HTTP 到 HTTPS 到底改了什么？

你会惊讶地发现，有了 ESP-IDF 强大的 MbedTLS 封装，从明文 HTTP 升级到安全 HTTPS **仅仅只需要修改 2 处配置**：

```diff
+// 1. 引入 ESP-IDF 根证书包头文件
+#include "esp_crt_bundle.h"

-// 2. 旧版明文 HTTP 接口
-#define WEATHER_API_URL "http://api.open-meteo.com/v1/forecast?..."
+// 2. 升级为 HTTPS 安全接口
+#define WEATHER_HTTPS_URL "https://api.open-meteo.com/v1/forecast?..."

 esp_http_client_config_t config = {
-    .url = WEATHER_API_URL,
+    .url = WEATHER_HTTPS_URL,
     .event_handler = http_event_handler,
-    .timeout_ms = 5000,
+    .crt_bundle_attach = esp_crt_bundle_attach, // 👈 核心：启用全局权威 CA 证书包自动校验！
+    .timeout_ms = 10000,                        // TLS 握手需要更多交互时间，适当放宽超时
 };
```

---

## 12.9 三层理解：为什么物联网要这样分步骤？

### 第一层：把事情一件件做完

- 先连 Wi-Fi；
- 再等路由器分配 IP；
- 再对时；
- 再发起 TLS 握手与 CA 证书校验；
- 最后请求天气并解析 JSON。

每一层失败，都能从串口看到它停在哪一层。

### 第二层：资源有限也要可靠

ESP32 的内存和任务数有限。事件驱动避免空循环占用 CPU；事件组把成功和失败变成明确状态；硬件加速引擎分担加密算力；全局根证书包避免占用宝贵的 RAM 内存；固定大小缓冲区防止网络数据无限吃内存。

### 第三层：工业产品的选择

| 场景 | 常见做法 | 本关与它的关系 |
| --- | --- | --- |
| 一次性取得天气 | HTTPS + REST/HTTP | 本关已完成明文与 HTTPS 证书校验两种实战 |
| 持续上报传感器 | MQTT / MQTTS (TLS加密) | 下一关会深入学习 |
| 用户配置家中 Wi-Fi | SoftAP/蓝牙配网 + NVS | 本关只把密码写在示例宏中 |
| 远程控制设备 | HTTPS/MQTT + 双向证书认证 (mTLS) | 工业核心资产普遍采用双向证书认证 |

---

## 12.10 主动回忆：不用看上文，试着回答

1. ESP32 已经显示“Wi-Fi 已启动”，为什么还不能立刻请求天气？
2. `IP_EVENT_STA_GOT_IP` 说明路由器完成了哪件事？
3. `TZ=CST-8` 是向服务器请求东八区时间，还是改变本地显示方式？
4. 明文 HTTP 与 HTTPS 相比，主要存在什么安全风险？
5. 为什么推荐在 ESP-IDF 中使用 `esp_crt_bundle_attach` 而不是手动下载单个 `.pem` 证书？

参考答案：1）还没有分配到 IP 地址；2）分配了局域网 IP、网关与 DNS；3）改变本地时区显示方式；4）明文传输容易被嗅探窃听或中间人篡改；5）全局证书包涵盖 Mozilla 认证的权威 CA 库，无需手动维护，避免证书到期失效。

---

## 12.11 常见问题

| 现象 | 最可能的原因 | 先这样做 |
| --- | --- | --- |
| 连续打印“第 x/5 次重连” | 密码、SSID 不对，或热点是 5 GHz | 用手机确认名称和密码；确认热点开启 2.4 GHz |
| 一直没有“已从路由器拿到 IP” | 路由器拒绝接入、信号太弱或 DHCP 异常 | 把开发板靠近路由器；重启热点；先运行实验 1 单独排查 |
| 显示已拿到 IP，但 SNTP 超时 | 能进局域网，不代表能访问时间服务器 | 检查路由器是否能上网、是否有访客网络隔离或防火墙限制 |
| HTTPS 握手失败 (`ESP_ERR_MBEDTLS_...`) | 系统时间未同步（证书时间校验失败）或网络波动 | 确保实验 2 先完成 SNTP 对时；适当增加 `timeout_ms` |
| HTTP 请求失败 | DNS、外网、服务器或网络拦截有问题 | 先确认实验 2 能对时；记录完整 `esp_err_to_name` 日志 |
| JSON 字段不完整或类型不对 | 服务端返回内容不是预期天气数据 | 打印/保存响应做分析；不要删除 `cJSON_IsNumber()` 检查 |
| 程序编译通过，却没看到真实天气 | 没有烧录、没有填写 Wi-Fi 或没连接串口 | 运行 `--flash`，再观察实际串口日志 |

> [!IMPORTANT]
> `idf.py build` 通过，**不等于已经在真机上联网成功**。构建只证明源码能被编译和链接；是否拿到 IP、是否能对时、是否能访问天气接口，必须以你自己的开发板串口日志为准。

---

## 12.12 动手练习

1. **观察断线重连**：实验 1 成功后，临时关掉手机热点或路由器 Wi-Fi。观察重试日志，再恢复网络，思考为什么本例达到 5 次就停止。
2. **体验安全加密**：对比运行实验 3（HTTP）与实验 4（HTTPS），观察串口日志中 TLS 握手过程和数据传输的安全性。
3. **更换城市**：把实验 4 的经纬度改成你所在城市的经纬度。只改 `latitude` 和 `longitude`，先不要修改 JSON 字段名。
4. **新增一项数据**：阅读天气服务文档，添加一个当前字段，例如 `relative_humidity_2m`。依次完成：修改 URL、查看 JSON、用 `cJSON_IsNumber()` 检查、再打印结果。

---

## 12.13 本关小结与下一关

你已经完成了一条现代化工业级安全互联网链路：

```text
Wi-Fi STA → DHCP 获得 IP → SNTP 对时 → TLS 握手 & CA 根证书校验 → HTTPS 安全请求 → JSON 安全解析
```

下一关会学习 MQTT。HTTP 更像“每次想问就打一次电话”；MQTT 更像“长期保持一个消息频道”，适合设备持续上报状态和接收控制命令。

继续阅读：[第 13 关：ESP32 MQTT 协议接入与阿里云 IoT 实战](./13_MQTT协议接入与阿里云IoT实战.md)。

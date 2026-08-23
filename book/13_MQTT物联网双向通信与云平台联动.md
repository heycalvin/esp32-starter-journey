# 第 13 关：ESP32 MQTT 物联网双向通信与云平台联动实战 (手机远程控制)

![第13关封面插画](../docs/images/esp32_level13_cover.jpg)

---

## 🎯 本关学习目标

在前一关中，我们用 HTTP 协议成功抓取了互联网天气预报。但是，如果你想开发一个**智能家居插座**、**远程温湿度监控器**或**智能防盗门锁**：
* 难道手机要每隔 0.1 秒不停地向 ESP32 发送 HTTP 请求询问“门锁状态”？（这会把单片机累死，电量和网络带宽瞬间耗尽）；
* 当你在外面用手机点击“开灯”时，家里的单片机躲在路由器防火墙（NAT）内，手机根本无法直接通过局域网 IP 找到它！

本关我们将学习当今**整个全球物联网（IoT）产业事实上的国际标准通信协议 —— MQTT（Message Queuing Telemetry Transport）**！

完成本关卡后，你将达成以下核心成就：
1. **搞懂 MQTT 发布/订阅（Pub/Sub）通信模型**：用“小区快递代收驿站”大白话彻底理解 Broker、Topic、Payload 与 NAT 内网穿透；
2. **掌握 QoS 服务质量与长连接保活**：搞懂 QoS 0/1/2 差异、心跳包（Keep-Alive）与遗嘱消息（LWT）；
3. **掌握 ESP-IDF `mqtt_client` 驱动框架**：事件驱动状态机、断线自动重连与主题订阅分发；
4. **掌握工业级设备遥测属性（Telemetry）上报**：将温湿度、内存占用、开机时长打包为 JSON 发送至云端；
5. **打造手机远程控制闭环中枢**：手机/电脑发送指令 ➔ ESP32 毫秒级执行开灯 ➔ 立即回传 ACK 确认报文！

---

## 🧭 关卡实验与源码速查

| 实验编号 | 实验名称 | 核心知识与实验目标 | 配套源码文件 | 一键切换命令 |
| :--- | :--- | :--- | :--- | :--- |
| **实验 1** | **MQTT 客户端 Pub/Sub 基础** | 连接公共 Broker（broker.emqx.io），订阅测试主题，实现自发自收 | [`01_mqtt_pubsub.c`](../code/13_mqtt_iot/01_mqtt_pubsub.c) | `./switch_code.sh 13 1 --flash` |
| **实验 2** | **设备遥测数据定时 JSON 上报** | 定时（每5秒）采集系统运行时间、剩余内存并组装 JSON 遥测报文上报 | [`02_telemetry_upload.c`](../code/13_mqtt_iot/02_telemetry_upload.c) | `./switch_code.sh 13 2 --flash` |
| **实验 3** | **手机远程控制中枢与双向 ACK** | 手机/MQTTX 发送指令控制板载 LED2（GPIO27），ESP32 秒级响应并回传 ACK | [`03_remote_control_hub.c`](../code/13_mqtt_iot/03_remote_control_hub.c) | `./switch_code.sh 13 3 --flash` |

---

# 🌟 主题一：MQTT 核心架构与发布/订阅通信模型（为什么它是物联网之王？）

## 13.1 为什么智能家居不能用 HTTP？MQTT 的“小区快递驿站”比喻

很多初学者总以为：“手机想控制单片机，直接往单片机发个请求不就行了吗？”  
但在现实物理世界中，这条路根本走不通：

```text
 ❌ 手机想直接找单片机（HTTP 模式）：
 手机 (公司) ────想直接发指令────► [ 🔒 小区保安大门 / 防火墙 NAT ] ─── 进不去！───► 单片机 (家里)
```

由于家庭路由器有**防火墙（NAT 保护机制）**，家里的单片机只有局域网私有 IP，外网手机根本找不到它，也进不去大门！

为了解决这个问题，全球物联网工程师在公网上建了一个 24 小时营业的**【快递代收驿站（Broker）】**：

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                    【MQTT 快递代收驿站工作全景图】                     │
 │                                                                        │
 │  ① 单片机主动出门登记：                                               │
 │  单片机 (家里) ──主动走出门──► [ 🏢 快递驿站 (Broker) ]                 │
 │     "站长，如果有贴着【A号货架-客厅灯】标签的包裹，请立即呼我！(订阅)"  │
 │                                                                        │
 │  ② 手机在公司远程寄件：                                               │
 │  手机 (公司) ──寄出一个包裹──► [ 🏢 快递驿站 (Broker) ]                 │
 │     • 货架标签 (Topic)："A号货架-客厅灯" (发布)                        │
 │     • 包裹内容 (Payload)：装一张纸条写着 {"power": "ON"}               │
 │                                                                        │
 │  ③ 驿站秒级推送：                                                     │
 │  驿站站长一看来了【A号货架-客厅灯】包裹 ──► 顺着刚才保持的通道弹窗通知   │
 │  单片机收到纸条 ──► 啪！绿色 LED 灯瞬间点亮！                         │
 └────────────────────────────────────────────────────────────────────────┘
```

---

### 📋 快递驿站体系与 MQTT 核心术语 1-对-1 字典：

| 快递代收驿站体系 | MQTT 物联网术语 | 它是干什么的？（秒懂设计） |
| :--- | :--- | :--- |
| **🏢 小区门口的快递代收驿站** | **Broker（MQTT 代理服务器）** | 公网上的云端中转站（如 `broker.emqx.io`、阿里云 IoT）。所有手机和单片机都连着它。 |
| **🏷️ 快递箱上贴的【货架标签】** | **Topic（主题，如 `home/living/led`）** | 一个虚拟的信息频道，用正斜杠 `/` 分层，用于精准区分包裹是给谁的。 |
| **📝 在驿站登记：“有这标签通知我”** | **Subscribe（订阅）** | 单片机开机连上网后，告诉驿站：“我关注了 `home/living/led` 这个标签，有新包裹立刻呼我！” |
| **📦 往驿站投递一个带标签的包裹** | **Publish（发布）** | 无论你在世界任何地方，手机 App 向驿站投递一个包裹，上面贴着标签 `home/living/led`。 |
| **📄 包裹箱子里装的具体纸条/货物** | **Payload（载荷）** | 真正传输的数据内容（通常是 JSON 字符串，如 `{"cmd":"set_led","state":1}`）。 |
| **⚡ 驿站站长主动呼叫单片机取件** | **Message Delivery（即时下发）** | 驿站 Broker 收到包裹后，在 **10 毫秒内**顺着长连接推送到单片机，单片机瞬间把灯点亮！ |

---

## 13.2 MQTT 三大核心设计：为什么它是为嵌入式单片机量身定制？

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                      【MQTT 协议的三大核心杀手锏】                     │
 │                                                                        │
 │  1. 极致轻量：报文头部最小仅 2 字节（HTTP 动辄数百字节 Headers）；      │
 │  2. NAT 穿透：单片机主动向公网建长连接，外网手机无需穿透家庭路由器； │
 │  3. 毫秒级被动唤醒：单片机平时挂起休眠，云端推来指令时微秒级唤醒中断。 │
 └────────────────────────────────────────────────────────────────────────┘
```

### 1. Topic（主题）的层级命名规范
Topic 是一个用正斜杠 `/` 分隔的字符串，就像电脑里的文件夹路径一样清晰：
* `esp32_journey/device_01/telemetry` ➔ 设备 01 的遥测数据上报频道；
* `esp32_journey/device_01/command` ➔ 设备 01 的控制指令下发频道；
* `esp32_journey/device_01/ack` ➔ 设备 01 的执行结果确认频道。

### 2. 通配符订阅（支持多设备批量管理）：
* `+`（单层通配符）：`esp32_journey/+/telemetry` ➔ 可以同时接收 `device_01`、`device_02`、`device_03` 的上报数据；
* `#`（多层通配符）：`esp32_journey/#` ➔ 订阅该前缀下的所有消息。

---

## 13.3 什么是 QoS？为什么有 0、1、2 三种服务质量等级？

MQTT 允许你在发送消息时指定“可靠性级别”：

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                      【MQTT QoS 服务质量等级对比】                      │
 │                                                                        │
 │  • QoS 0（最多发一次，At most once）：                                 │
 │    发送方只管发出去，不要求确认（像普通平信，网络丢包就算了）。        │
 │    👉 适用场景：每秒 10 次的高频传感器温湿度波形（丢一帧完全无所谓）。  │
 │                                                                        │
 │  • QoS 1（至少送达一次，At least once - 最推荐 👍）：                   │
 │    发送方发出后，接收方必须回复 PUBACK 确认包；若没收到就自动重发。     │
 │    👉 适用场景：开关灯、门锁开关、报警通知（确保绝不丢消息）。          │
 │                                                                        │
 │  • QoS 2（确保只送达一次，Exactly once）：                             │
 │    严格四次握手（PUBLISH ➔ PUBREC ➔ PUBREL ➔ PUBCOMP），绝不重复。      │
 │    👉 适用场景：金融计费、医疗遥测（单片机资源有限，一般较少使用）。    │
 └────────────────────────────────────────────────────────────────────────┘
```

---

## 13.4 🏢 什么是“公共免费测试 MQTT 代理服务器”？（为什么选 broker.emqx.io？）

很多小白刚看到代码里的 `mqtt://broker.emqx.io:1883` 时，心里直犯嘀咕：  
*“这台服务器是谁的？我没花钱买云服务器，也没注册账号，为什么填上它就能直接联网通信？”*

---

### 1. 生活比喻：城市中心广场上的【公共免费大黑板】

* **如果要自己建 MQTT 服务器**：你需要花钱买阿里云/腾讯云 Linux 云主机、配置域名、开放防火墙端口、安装 EMQX 或 Mosquitto 软件，对于刚入坑单片机的小白来说门槛极高；
* **公共免费测试 Broker（如 `broker.emqx.io`）**：就像国际知名的开源物联网团队（EMQ 映驰科技）在公网机房里，专门为全世界开发者搭建的一座**永久免费、不设门禁、24小时开放的公共大驿站**！
* **核心优势**：
  1. ⚡ **零门槛开箱即用**：不需要注册账号、不需要充值、不需要输入用户名密码；
  2. 🌍 **全球多节点加速**：无论你在中国、欧洲还是北美，都能就近连接，网络超低延迟；
  3. 🛠️ **全协议生态支持**：支持 TCP、TLS 加密、WebSocket 网页端全方位连入。

---

### 2. 核心参数解密：端口（Port）到底怎么选？

在连接 Broker 时，你会看到不同的端口号，它们分别代表不同的安全通道：

| 端口号 | 协议通道 | 通俗比喻 | 适用场景 |
| :--- | :--- | :--- | :--- |
| **`1883`** | **TCP 明文连接** | **普通平信通道**（无加密，单片机算力消耗最低，速度最快） | **【本关实验采用】**，最适合小白快速上手验证逻辑！ |
| **`8883`** | **MQTTS（TLS/SSL 加密）** | **绝密押运通道**（结合 CA 证书全程 AES 加密，防窃听） | 工业量产级高安全物联网产品。 |
| **`8083` / `8084`** | **WebSocket 网页通道** | **浏览器专用通道** | 前端网页、H5 页面直接连接 MQTT 发送控制指令。 |

---

### 3. 🚨 重点避坑：公共 Broker 的“防撞车黄金命名法则”

因为这是**全世界开发者共享的公共服务器**，如果大家都把主题写成 `test` 或 `led`：
* 印度的一位工程师往 `test` 发送了一个 `1`；
* 你的 ESP32 订阅了 `test`，你的台灯就会莫名其妙自动亮起！

```text
 ❌ 错误命名（极易撞车）：
    "test"
    "esp32/led"

 ✅ 黄金防撞车命名（加入你的专属唯一标识）：
    "esp32_journey/<你的专属名字或随机ID>/command"
    "esp32_journey/<你的专属名字或随机ID>/telemetry"
```

---

### 4. 真实工业界的“段位进阶全景”

| 段位与场景 | 选用方案 | 特点与运维成本 |
| :--- | :--- | :--- |
| **🥉 第一阶（学习与原型验证）** | **公共免费 Broker**（`broker.emqx.io` / `test.mosquitto.org`） | 0 成本、免运维，适合学习实验。 |
| **🥈 第二阶（极客与家庭内网）** | **本地局域网自建 Broker**（树莓派/NAS 安装 `Mosquitto` 或 `EMQX 开源版`） | 即使家里断外网也能本地智能联动（如 Home Assistant），数据 100% 私密。 |
| **🥇 第三阶（百万级商业量产）** | **云厂商 IoT 平台**（阿里云 IoT 物联网平台 / 腾讯云 IoT / AWS IoT Core） | 一机一密身份认证、设备影子、海量高并发高可用、大数据存储与规则引擎流转。 |

---

# 🛠️ 主题二：ESP-IDF `mqtt_client` 驱动框架与 API 字典

ESP-IDF 官方内置了工业级成熟的 `mqtt_client` 组件（基于 FreeRTOS 异步任务与 Socket 封装）。

## 13.5 核心 API 函数功能字典

| 函数原型 | 生活比喻 | 核心功能与参数深度剖析 |
| :--- | :--- | :--- |
| **`esp_mqtt_client_init(&config)`** | **登记注册一部 MQTT 专属对讲机** | **创建 MQTT 客户端句柄**。<br>• `config.broker.address.uri`：填入 Broker 地址（如 `mqtt://broker.emqx.io:1883`）；<br>• 返回 `esp_mqtt_client_handle_t` 句柄指针。 |
| **`esp_mqtt_client_register_event(...)`** | **给对讲机设置专职值班员** | **注册全局 MQTT 事件回调函数**。<br>将网络连接、断开、接收数据、错误等事件统一分发给 `mqtt_event_handler`。 |
| **`esp_mqtt_client_start(client)`** | **对讲机开机并开始呼叫服务器** | **启动 MQTT 后台守护任务**。<br>在后台自动进行 TCP 三次握手与 MQTT CONNECT 报文握手，支持自动断线重连！ |
| **`esp_mqtt_client_subscribe(client, topic, qos)`** | **对讲机调到指定频道（点击关注）** | **向 Broker 订阅指定主题**。<br>• `topic`：目标主题字符串；<br>• `qos`：服务质量等级（通常填 0 或 1）。 |
| **`esp_mqtt_client_publish(client, topic, data, len, qos, retain)`** | **对着频道大喊广播一条消息** | **向指定主题发布数据**。<br>• `topic`：发布的目标主题；<br>• `data`：正文内容（字符串或二进制）；<br>• `len`：内容长度（传 0 则自动按 `strlen` 计算）；<br>• `qos`：0 或 1；<br>• `retain`：是否设为保留消息（0=否）。 |

---

### 💡 深度延伸：我们当前使用的是 MQTT 3.1.1 还是 5.0？

> 📌 **版本演进史速查**：MQTT 官方历史上只有 **v3.1 ➔ v3.1.1 ➔ v5.0**（没有 3.3 版本）。

1. **默认版本**：
   在 ESP-IDF 中，如果不做特殊指定，`mqtt_client` 默认使用的是当今全球物联网应用最广泛的 **MQTT v3.1.1**。
2. **如何开启 MQTT v5.0？**：
   ESP-IDF v5/v6 完全原生支持 MQTT 5.0。如果你想体验 5.0 的高级特性，只需在初始化结构体中指定：
   ```c
   esp_mqtt_client_config_t mqtt_cfg = {
       .broker.address.uri = "mqtt://broker.emqx.io:1883",
       .session.protocol_ver = MQTT_PROTOCOL_V_5, // 👈 切换为 MQTT 5.0 标准
   };
   ```
3. **v3.1.1 与 v5.0 核心特性对比表**：

| 特性维度 | MQTT v3.1.1（经典稳健之王 👑） | MQTT v5.0（现代化进阶之选 🚀） |
| :--- | :--- | :--- |
| **发布时间** | 2014 年（OASIS / ISO 国际标准） | 2019 年（全面现代化升级） |
| **云端兼容性** | 全球 100% 物联网平台/设备无条件支持 | 主流云平台与 EMQX 已全面支持 |
| **头部元数据** | 无（只有 Topic 和 Payload） | **支持 User Properties**（类似 HTTP Headers 键值对） |
| **报错机制** | 简单应答（只回复成功与否） | **丰富原因码（Reason Codes）**（精确返回为何拒绝连接/订阅） |
| **消息时效控制** | 无 | **支持消息过期时间（Message Expiry）**（离线过久指令自动作废） |
| **流量极致压缩** | 每次传输完整字符串 Topic | **支持主题别名（Topic Alias）**（用 2 字节数字代替长字符串主题） |
| **单片机选型建议** | **初学者与常规嵌入式首选 👍**（结构极简，开销极小） | 复杂大型 IoT 系统、微服务架构与高级网关推荐 |

---

## 13.6 MQTT 事件状态机（`mqtt_event_handler` 拆解）

```c
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "🎉 成功连入 MQTT Broker！");
            // ⭐️ 必须在连接成功后，才执行主题订阅
            esp_mqtt_client_subscribe(s_mqtt_client, "esp32/cmd", 1);
            break;

        case MQTT_EVENT_DATA:
            // ⭐️ 收到别人发来的包裹数据 (Payload)
            // 注意：event->topic 和 event->data 不是以 '\0' 结尾的字符串！
            // 打印时必须使用 %.*s 并指定长度：
            ESP_LOGI(TAG, "📩 收到主题 [%.*s] 消息 ➔ %.*s", 
                     event->topic_len, event->topic, event->data_len, event->data);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "⚠️ MQTT 服务器断开连接，后台驱动将自动重连...");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "❌ MQTT 协议层发生错误");
            break;

        default:
            break;
    }
}
```

> [!WARNING]
> **嵌入式经典避坑指南：`event->data` 并非以 `\0` 结尾！**  
> 在 `MQTT_EVENT_DATA` 中收到的数据片段是直接从 TCP 缓冲区截取的，末尾没有 C 语言字符串的结束符 `\0`。如果要把它当字符串传给 `cJSON_Parse`，**必须先分配内存或拷贝到临时数组中并手动补上 `\0`**：
> ```c
> char json_buf[512];
> int len = event->data_len < sizeof(json_buf) - 1 ? event->data_len : sizeof(json_buf) - 1;
> memcpy(json_buf, event->data, len);
> json_buf[len] = '\0'; // 👈 必须手动补 0！
> ```

---

# 🚀 主题三：三大实战实验逐级通关（源码 + 验证）

---

## 13.7 实验 1：MQTT 客户端 Pub/Sub 基础 (Hello MQTT)

连接到全球公共测试 Broker（`broker.emqx.io`），订阅自身测试主题，实现自发自收环回验证！

> 📁 **配套源码文件**：[`code/13_mqtt_iot/01_mqtt_pubsub.c`](../code/13_mqtt_iot/01_mqtt_pubsub.c)  
> ⚡ **一键切换运行**：在终端运行 `./switch_code.sh 13 1 --flash` 即可秒级切换并自动烧录！

### 1. 核心流程
1. Wi-Fi 连接并取得 IP；
2. 启动 MQTT 客户端连接 `mqtt://broker.emqx.io:1883`；
3. 触发 `MQTT_EVENT_CONNECTED` ➔ 订阅 `esp32_journey/hello/test` ➔ 发布一条 `"Hello World from ESP32!"`；
4. Broker 接收并原样推送回给 ESP32 ➔ 触发 `MQTT_EVENT_DATA` 并打印日志！

### 2. 成功运行日志：
```text
I (....) EXP1_MQTT_PUBSUB: 📡 正在连接 Wi-Fi...
I (....) EXP1_MQTT_PUBSUB: ✅ Wi-Fi 已就绪！
I (....) EXP1_MQTT_PUBSUB: 🚀 正在启动 MQTT 客户端连接: mqtt://broker.emqx.io:1883
I (....) EXP1_MQTT_PUBSUB: 🎉 [MQTT 状态] 成功连入 MQTT 云端 Broker!
I (....) EXP1_MQTT_PUBSUB: 📥 成功订阅主题: esp32_journey/hello/test
I (....) EXP1_MQTT_PUBSUB: 📤 已发送测试消息 ➔ Hello World from ESP32!
I (....) EXP1_MQTT_PUBSUB: 📩 [收到下行消息] 主题: esp32_journey/hello/test
I (....) EXP1_MQTT_PUBSUB:    正文内容: Hello World from ESP32!
```

---

## 13.8 实验 2：设备遥测数据定时 JSON 上报 (Telemetry Upload)

在工业物联网中，设备需要定时把运行健康状态和传感器读数打包上报。本实验每隔 5 秒将 ESP32 的 **剩余内存（Free Heap）**、**开机运行时长（Uptime）** 和 **温度** 组装成 JSON 报文推送至云端：

> 📁 **配套源码文件**：[`code/13_mqtt_iot/02_telemetry_upload.c`](../code/13_mqtt_iot/02_telemetry_upload.c)  
> ⚡ **一键切换运行**：在终端运行 `./switch_code.sh 13 2 --flash` 即可秒级切换并自动烧录！

### 1. 核心代码解析：cJSON 动态打包与安全释放

```c
static void telemetry_timer_task(void *arg)
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000)); // 每 5 秒上报一次

        if (!s_mqtt_connected) continue;

        uint32_t free_heap = esp_get_free_heap_size();
        int64_t uptime_sec = esp_timer_get_time() / 1000000;
        float mock_temp = 26.5f + (float)(rand() % 20) / 10.0f;

        // 1. 创建根 JSON 对象
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "device_id", "esp32_starter_01");
        cJSON_AddNumberToObject(root, "uptime_sec", (double)uptime_sec);
        cJSON_AddNumberToObject(root, "free_heap", (double)free_heap);
        cJSON_AddNumberToObject(root, "temperature", (double)mock_temp);

        // 2. 导出为紧凑型 JSON 字符串
        char *json_str = cJSON_PrintUnformatted(root);
        if (json_str != NULL) {
            ESP_LOGI(TAG, "📤 [定时遥测上报] ➔ %s", json_str);
            esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC_TELEMETRY, json_str, 0, 1, 0);
            
            // 3. 释放 cJSON 打印字符串内存
            free(json_str);
        }

        // 4. 释放 JSON 树结构体
        cJSON_Delete(root);
    }
}
```

### 2. 成功运行日志：
```text
I (....) EXP2_TELEMETRY: ✅ Wi-Fi 连接成功！
I (....) EXP2_TELEMETRY: 🎉 MQTT 连接成功，遥测通道已就绪！
I (....) EXP2_TELEMETRY: 📤 [定时遥测上报] ➔ {"device_id":"esp32_starter_01","uptime_sec":5,"free_heap":235120,"temperature":27.1}
I (....) EXP2_TELEMETRY: 📤 [定时遥测上报] ➔ {"device_id":"esp32_starter_01","uptime_sec":10,"free_heap":235088,"temperature":28.3}
```

---

## 13.9 实验 3：综合大工程 —— 手机远程控制中枢与双向联动 (Remote Control Hub)

打造真正的双向物联网中枢：使用手机/电脑通过 MQTT 下发 JSON 指令控制开发板上的 **绿色 LED2（GPIO27）**，开发板执行后立即回传包含执行结果的 **ACK 应答报文**！

> 📁 **配套源码文件**：[`code/13_mqtt_iot/03_remote_control_hub.c`](../code/13_mqtt_iot/03_remote_control_hub.c)  
> ⚡ **一键切换运行**：在终端运行 `./switch_code.sh 13 3 --flash` 即可秒级切换并自动烧录！

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                  【手机与 ESP32 远程双向控制闭环流转】                 │
 │                                                                        │
 │  1. 手机 App ──(Publish: {"cmd":"set_led","state":1})──► [ Broker ]   │
 │                                                               │        │
 │  2. ESP32   ◄───────(下发指令: TOPIC_CMD)─────────────────────┘        │
 │     │                                                                  │
 │     ├─ 解析 JSON ➔ gpio_set_level(GPIO27, 1) 点亮绿色 LED2             │
 │     │                                                                  │
 │  3. ESP32   ──(Publish: {"cmd":"set_led","success":true})──► [ Broker ]│
 │                                                               │        │
 │  4. 手机 App ◄───────(接收确认: TOPIC_ACK)────────────────────┘        │
 └────────────────────────────────────────────────────────────────────────┘
```

### 1. 📋 控制指令协议清单与 ACK 回执字典（手机端可下发的 3 大指令）

手机或电脑上位机向主题 `esp32_journey/device_01/command` 发布 JSON 指令，ESP32 会在毫秒级执行并通过 `esp32_journey/device_01/ack` 回传执行结果：

| 指令名称 (`cmd`) | 手机下发的 JSON 报文 | ESP32 执行动作 | ESP32 回传的 ACK 确认报文 |
| :--- | :--- | :--- | :--- |
| **💡 点亮绿色 LED2** | `{"cmd": "set_led", "state": 1}` | 将 GPIO27 拉高点亮 LED2 | `{"cmd":"set_led","success":true,"message":"LED Turned ON","led_state":true}` |
| **🌑 熄灭绿色 LED2** | `{"cmd": "set_led", "state": 0}` | 将 GPIO27 拉低熄灭 LED2 | `{"cmd":"set_led","success":true,"message":"LED Turned OFF","led_state":false}` |
| **📊 查询当前运行状态** | `{"cmd": "get_status"}` | 不动硬件，仅读取当前 LED 开关状态 | `{"cmd":"get_status","success":true,"message":"LED is ON","led_state":true}` |
| **🔄 远程软重启单片机** | `{"cmd": "reboot"}` | 先回传 ACK，延时 1 秒后调用 `esp_restart()` | `{"cmd":"reboot","success":true,"message":"System will reboot in 1s","led_state":...}` |
| **⚠️ 非法未知指令（容错）** | `{"cmd": "play_music"}` | 不执行动作，返回错误提示 | `{"cmd":"play_music","success":false,"message":"Unknown Command","led_state":...}` |

---

### 2. 核心源码剖析：指令分发与硬件控制实现
```c
static void handle_downlink_command(const char *payload, int len)
{
    char json_buf[512] = {0};
    if (len >= sizeof(json_buf)) len = sizeof(json_buf) - 1;
    strncpy(json_buf, payload, len);

    cJSON *root = cJSON_Parse(json_buf);
    if (!root) {
        ESP_LOGE(TAG, "❌ JSON 格式错误");
        send_ack_response("unknown", false, "JSON 语法解析错误");
        return;
    }

    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (cmd && cmd->valuestring) {
        if (strcmp(cmd->valuestring, "set_led") == 0) {
            cJSON *state = cJSON_GetObjectItem(root, "state");
            if (state) {
                s_led_status = (state->valueint != 0);
                gpio_set_level(LED2_PIN, s_led_status ? 1 : 0);
                ESP_LOGI(TAG, "💡 成功执行开关灯 ➔ %s", s_led_status ? "点亮 (ON)" : "熄灭 (OFF)");
                send_ack_response("set_led", true, s_led_status ? "LED Turned ON" : "LED Turned OFF");
            }
        } else if (strcmp(cmd->valuestring, "get_status") == 0) {
            send_ack_response("get_status", true, s_led_status ? "LED is ON" : "LED is OFF");
        } else if (strcmp(cmd->valuestring, "reboot") == 0) {
            send_ack_response("reboot", true, "System will reboot in 1s");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        } else {
            send_ack_response(cmd->valuestring, false, "Unknown Command");
        }
    }
    cJSON_Delete(root);
}
```

---

# 📱 主题四：使用 MQTTX 上位机 / 手机 App 联动测试指南

为了在手机或电脑上亲身体验远程控制，我们推荐使用全球最流行的开源跨平台 MQTT 调试工具 —— **MQTTX**（支持 Windows / Mac / Linux / iOS / Android）。

## 13.10 配合 MQTTX 进行 3 步可视化实战

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                        【MQTTX 联动配置三步法】                        │
 │                                                                        │
 │  步骤 1：新建连接 ➔ Name 随意，Host 填 `broker.emqx.io`，Port 填 `1883` │
 │  步骤 2：添加订阅 ➔ 点击 `+ New Subscription`，订阅以下两个主题：      │
 │           • `esp32_journey/device_01/telemetry` (查看遥测数据流)       │
 │           • `esp32_journey/device_01/ack`       (接收控制确认回复)     │
 │  步骤 3：发送控制 ➔ 向 `esp32_journey/device_01/command` 发送 JSON：    │
 │           {"cmd": "set_led", "state": 1}                              │
 └────────────────────────────────────────────────────────────────────────┘
```

### 观察现象：
1. 发送 `{"cmd": "set_led", "state": 1}` ➔ **开发板上的绿色 LED2 瞬间亮起**，MQTTX 收到 ACK 消息：
   ```json
   {"cmd":"set_led","success":true,"message":"LED 已点亮","led_state":true}
   ```
2. 发送 `{"cmd": "set_led", "state": 0}` ➔ **开发板上的绿色 LED2 瞬间熄灭**，MQTTX 收到 ACK 消息：
   ```json
   {"cmd":"set_led","success":true,"message":"LED 已熄灭","led_state":false}
   ```

---

## 13.11 🌟 工业进阶：从通用 MQTT 到商业云平台（阿里云 IoT / 腾讯云 / AWS）

很多初学者可能会好奇：  
*“我们本关用的是开源标准的 MQTT Broker（`broker.emqx.io`），那国内常说的**阿里云 IoT 物联网平台**、**腾讯云 IoT** 又是什么关系呢？”*

### 1. 核心真理：商业云平台的底层 100% 依然是标准 MQTT！
无论是阿里云、腾讯云还是亚马逊 AWS IoT，它们与单片机通信的**底层通信协议就是我们今天学的 MQTT**！

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │            【通用开源 MQTT 与商业云平台（阿里云 IoT）的核心区别】        │
 │                                                                        │
 │  1. 通用公共 Broker（本关实验）：                                      │
 │     • 免注册、无账号密码、Topic 随意起名；                              │
 │     • 适合学习、实验原型验证、极客内网环境。                          │
 │                                                                        │
 │  2. 阿里云 IoT / 商业级平台（工业量产）：                              │
 │     • 加了一层【设备三元组安全认证】（一机一密，防止设备被伪造）；      │
 │     • 规定了一套【物模型 Alink JSON 格式】（标准化命名与云端解析）。     │
 └────────────────────────────────────────────────────────────────────────┘
```

---

### 2. 阿里云 IoT 是如何使用 MQTT 协议的？（两大规矩）

如果你将来要把 ESP32 接入真实的阿里云 IoT 物联网平台，只需要遵守以下两条规矩：

#### ① 规矩一：使用“设备三元组”计算 MQTT 登录密码
在阿里云控制台创建设备后，会拿到三个参数（俗称**设备三元组**）：
* `ProductKey`（产品密钥，如 `a1b2c3d4`）
* `DeviceName`（设备名称，如 `esp32_dev01`）
* `DeviceSecret`（设备私钥，如 `xxxxxx`）

在单片机中，使用 HMAC-SHA256 算法对三元组计算哈希签名，生成 MQTT 的 `username` 和 `password`，填入 `esp_mqtt_client_config_t` 中，即可安全登录阿里云！

#### ② 规矩二：遵循标准物模型（Alink JSON）Topic 格式
阿里云对 Topic 和 JSON 正文格式做出了严格规范，例如：
* **属性上报 Topic**：`/sys/${ProductKey}/${DeviceName}/thing/event/property/post`
* **属性上报 JSON**：
  ```json
  {
    "id": "128",
    "version": "1.0",
    "params": {
      "temperature": 26.5,
      "LightStatus": 1
    },
    "method": "thing.event.property.post"
  }
  ```

---

### 3. 为什么本关先从开源通用 MQTT 入门？
* 如果一上来就让新手去注册阿里云账号、实名认证、在控制台点几十次鼠标建产品、手写哈希签名算法，**90% 的初学者会被繁琐的网页操作直接劝退**；
* 物联网的核心是 **“理解 Pub/Sub 发布订阅、NAT 穿透、长连接心跳与 QoS 机制”**。
* 只要打通了本关的标准 MQTT，未来对接阿里云 IoT、腾讯云 IoT、华为云 IoT 还是开源的 Home Assistant 智能家居，**底层代码结构 99% 完全通用**！

---

## 13.12 关卡总结与通关打卡

太震撼了！你已经完全掌握了现代物联网的通信神经中枢 —— **MQTT 协议**！

### 🏆 核心技能清单回顾：
* [x] **MQTT 架构与心智模型**：搞懂 Pub/Sub 发布订阅机制、Broker 云端邮局与“小区快递代收驿站”生动比喻；
* [x] **公共测试 Broker**：搞懂 `broker.emqx.io` 的免注册机制、1883/8883 端口区别与防撞车黄金命名法则；
* [x] **NAT 内网穿透与长连接**：理解为什么 MQTT 能让外网手机毫秒级穿透家庭路由器控制开发板；
* [x] **QoS 服务质量**：掌握 QoS 0/1/2 的设计权衡与场景选型；
* [x] **ESP-IDF `mqtt_client` 框架**：掌握事件状态机、异步回调与断线自动重连；
* [x] **工业级双向物联网控制中枢**：完成遥测数据 JSON 上报、手机远程控制与 ACK 回执双向闭环；
* [x] **商业云平台全景认知**：搞懂通用 MQTT 到阿里云/腾讯云 IoT（设备三元组与物模型）的演进桥梁。

---

现在，ESP32 已经具备了连接广域互联网（Wi-Fi + MQTT）的强大能力。但在近场无网络环境（如用手机靠近门锁自动开门、免配网近距离调试设备），**BLE 低功耗蓝牙** 则是不可替代的神器！

请翻开 [**第 14 章：ESP32 BLE 低功耗蓝牙 GATT 广播与手机 App 透传控制**](./14_BLE低功耗蓝牙GATT与手机App透传控制.md)！



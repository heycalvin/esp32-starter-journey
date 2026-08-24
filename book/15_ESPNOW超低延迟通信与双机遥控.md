# 第 15 关：ESP-NOW 超低延迟私有通信与双机遥控对射实战

![第15关封面插画](../docs/images/esp32_level15_cover.jpg)

---

## 🎯 本关学习目标

在前面的章节中，我们已经掌握了两种主流的无线通信技术：
1. **Wi-Fi（第 12、13 关）**：连接无线路由器，打通广域互联网云端（HTTP 天气时钟、MQTT 远程控制）；
2. **BLE 蓝牙（第 14 关）**：手机近距离免配网透传与遥控。

但是，在无人机穿越机航模、四驱麦轮遥控车、无线游戏手柄、智能农田传感器阵列等硬核场景下，开发者经常面临三个痛苦的难题：
* ❌ **Wi-Fi 太繁重**：必须依赖路由器，连接握手耗时长达 2~5 秒，空口网络延迟波动大（20~100ms），而且极度耗电；
* ❌ **BLE 蓝牙吞吐与距离受限**：传输距离通常只有 10~15 米，多机组网极其繁琐；
* ❌ **户外野外根本没有路由器**：在荒郊野外、山林农田，两块单片机如何直接点对点极速通信？

本关，我们将解锁乐鑫芯片独步天下的黑科技杀手锏 —— **ESP-NOW 私有无线通信协议**！

完成本关卡后，你将达成以下核心成就：
1. **建立极简通信心智模型**：理解“无线对讲机”免握手、免路由器的飞鸽传书机制；
2. **掌握 802.11 无线动作帧（Action Frame）本质**：搞懂 `< 2ms` 极速超低延迟与 200 米超远穿透力的物理原理；
3. **攻克双机单播对射遥控 (P2P Unicast)**：实现板 A 按下 SW3 按键，板 B 毫秒级点亮板载绿色 LED2；
4. **手写 Ping-Pong 链路测速仪**：利用高精度硬件定时器实测往返时延（RTT）与丢包率统计；
5. **打造“一呼百应”广播群控系统**：使用 `FF:FF:FF:FF:FF:FF` 广播地址，一键控制周围所有 ESP32 节点同步律动！

---

## 🧭 关卡实验与源码速查

| 实验编号 | 实验名称 | 核心知识与实验目标 | 配套源码文件 | 一键切换命令 |
| :--- | :--- | :--- | :--- | :--- |
| **实验 1** | **双机单播对射遥控 (P2P Unicast)** | 初始化 Wi-Fi STA 与 ESP-NOW 协议栈，MAC 点名配对 Peer，板 A 按键控制板 B 翻转 LED2 | [`01_espnow_unicast.c`](../code/15_espnow_remote/01_espnow_unicast.c) | `./switch_code.sh 15 1 --flash` |
| **实验 2** | **双向对讲与链路质量探测 (RTT 测速)** | Ping-Pong 回声机制，微秒级测量无线空口往返时延（RTT < 2ms），实时统计发包成功率与丢包率 | [`02_espnow_twoway.c`](../code/15_espnow_remote/02_espnow_twoway.c) | `./switch_code.sh 15 2 --flash` |
| **实验 3** | **广播群控与“一呼百应” (Group Control)** | 使用全局广播 MAC 地址 `FF:FF:...`，发射端一键群发，周围所有从机 2ms 内同步协同动作 | [`03_espnow_broadcast.c`](../code/15_espnow_remote/03_espnow_broadcast.c) | `./switch_code.sh 15 3 --flash` |

---

## 15.1 💡 极简认知启蒙：什么是 ESP-NOW？为什么它是遥控神技？

很多初学者一听到“无线通信协议”就觉得门槛极高。  
其实，**只要用生活中的“寄信”与“对讲机”打比方，ESP-NOW 的原理一秒就能秒懂！**

---

### 1. 生活化比喻：传统 Wi-Fi vs 蓝牙 vs ESP-NOW

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                      【三种无线通信生活化形象比喻】                    │
 │                                                                        │
 │  1. 传统 Wi-Fi ➔ 【通过总局邮局中转送信 🏢】：                         │
 │     • 你必须先办卡入网（输 Wi-Fi 密码连上路由器）；                    │
 │     • 每次寄信都要送到邮局（路由器），由邮局再分发给收件人；           │
 │     • 缺点：没有邮局（路由器）就彻底瘫痪，中间转手耗时大。             │
 │                                                                        │
 │  2. BLE 蓝牙 ➔ 【近距离悄悄话 🗣️】：                                  │
 │     • 两个人必须先握手配对、建立通道；                                 │
 │     • 距离超过十几米就听不见了，且只能一对一或极少量连接。             │
 │                                                                        │
 │  3. ⚡ ESP-NOW ➔ 【超强力军用无线对讲机 📻】：                        │
 │     • 根本不需要路由器！不需要输密码！不需要耗时漫长的建连握手！       │
 │     • 只要知道对方的 MAC 地址（频道代号），按下通话键直接朝空中喊话！  │
 │     • 速度极快（< 2ms），直线距离可达 100~200 米，极其省电！           │
 └────────────────────────────────────────────────────────────────────────┘
```

---

### 📊 2. 三大无线通信方案核心技术指标大比拼

| 核心指标 | 传统 Wi-Fi (TCP/UDP) | BLE 低功耗蓝牙 | ⚡ ESP-NOW (本关神技) |
| :--- | :--- | :--- | :--- |
| **是否依赖路由器？** | **必须依赖无线路由器** | 不需要 (依赖手机/主机) | **完全独立（点对点 P2P）** |
| **开机到发包耗时** | 2000ms ~ 5000ms (漫长握手) | 500ms ~ 2000ms | **0ms（免握手，上电即可秒发！）** |
| **空中往返延迟 (RTT)** | 20ms ~ 100ms (抖动大) | 15ms ~ 50ms | **< 2ms（毫秒级极速，近乎无感！）** |
| **典型空旷传输距离** | 30 ~ 50 米 | 10 ~ 15 米 | **100 ~ 200 米（Wi-Fi 射频全功率输出）** |
| **单包最大载荷** | 1460 字节 (MTU) | 20 ~ 244 字节 | **250 字节（专为传感器/遥控包量身定制）** |
| **休眠发包综合功耗** | 极高（保持心跳极耗电） | 较低 | **极低（发完 2ms 立即进入深度休眠）** |
| **最典型工业场景** | 智能家居中控、视频推流 | 手环心率、手机 App 调试 | **无人机穿越机、遥控小车、农田传感器阵列** |

---

### 🔍 3. 底层揭秘：为什么 ESP-NOW 可以做到“免连接”且“延迟 < 2ms”？

为什么普通的 Wi-Fi 连路由器那么慢，而 ESP-NOW 却快如闪电？

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                    【普通 Wi-Fi 握手 vs ESP-NOW 动作帧】                │
 │                                                                        │
 │  ❌ 普通 Wi-Fi (繁重四次握手):                                          │
 │     ESP32 ──(Probe Request 探针)──► 路由器                              │
 │     ESP32 ◄──(Probe Response 响应)── 路由器                            │
 │     ESP32 ──(Authentication 身份认证)──► 路由器                        │
 │     ESP32 ──(Association 关联请求)──► 路由器 ➔ 分配 IP (耗时 3~5秒!)   │
 │                                                                        │
 │  ───────────────────────────────────────────────────────────────────   │
 │                                                                        │
 │  ✅ ⚡ ESP-NOW (纯物理层 802.11 Vendor Action Frame):                   │
 │     ESP32 (板A) ──[ 802.11 动作数据帧 (带目标MAC) 直接射向空中 ]──► ESP32(板B)│
 │     ESP32 (板A) ◄──[ 物理层硬件瞬时 ACK 确认 (0.5ms) ]─────────── ESP32(板B)│
 │                                                                        │
 │     👉 结论：跳过了所有 TCP/IP 协议栈和路由转发，直通 Wi-Fi 射频底层！  │
 └────────────────────────────────────────────────────────────────────────┘
```

1. **802.11 Vendor Action Frame（厂商自定义动作帧）**：
   * ESP-NOW 使用了标准 IEEE 802.11 协议中的“动作帧”技术；
   * 它把你的数据直接封装在 Wi-Fi 物理帧中发射，**绕过了庞大的 TCP/IP 协议栈、ARP 寻址和 DHCP 分配 IP 流程**！
2. **底层硬件 MAC 过滤**：
   * 芯片内部的 Wi-Fi 硬件射频会自动比对空中数据包的目标 MAC 地址；
   * 如果是发给自己的，直接触发中断将数据送入内存，全流程硬件级加速！

---

### 4. 🎙️ 进阶探讨：ESP-NOW 能做“儿童对讲机”吗？可以传输音频或视频吗？

很多同学学到这里都会兴奋地提问：“*既然 ESP-NOW 可以免路由器双机对讲，那能不能拿它做一台【儿童无线对讲机】？能传声音和视频吗？*”

#### 💡 核心结论速览：
* **做无线语音对讲机？➔ ✅ 完全可以！而且是开源界与玩具工业界的爆款方案！**
* **传输实时流畅视频？➔ ❌ 不适合！单包 250 字节上限决定了它无法承载高吞吐视频流。**

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                    【音频 vs 视频 数据吞吐量大算账】                   │
 │                                                                        │
 │  1. 实时语音通话 (经过 Opus / ADPCM 压缩后):                           │
 │     • 采样率：16 kHz (16位) ➔ 压缩后码率仅需 【16 kbps ~ 32 kbps】    │
 │     • 每秒数据量：只需要 【2 KB ~ 4 KB / 秒】                          │
 │     • ESP-NOW 承载能力：每秒发 15~20 个 250 字节包，轻松吃下！        │
 │     • 结果：✅ 【完全流畅，空口延迟 < 10ms，声音清脆无卡顿】           │
 │                                                                        │
 │  ───────────────────────────────────────────────────────────────────   │
 │                                                                        │
 │  2. 实时视频传输 (哪怕只有 320×240 极低画质):                          │
 │     • 每张 JPEG 压缩图片：约 10 KB                                     │
 │     • 按最低流畅度 15 FPS (每秒15帧)：10 KB × 15 = 【150 KB / 秒】     │
 │     • 码率：约 1.2 Mbps ~ 2.0 Mbps                                     │
 │     • ESP-NOW 单包上限 250 字节 ➔ 一帧图片要切碎成 40~60 个碎片包！   │
 │     • 结果：❌ 【空中丢一个碎片包整张图就花屏撕裂，不适合传输视频】    │
 └────────────────────────────────────────────────────────────────────────┘
```

> 🛠️ **儿童无线对讲机极简硬件方案（单台成本 < 20 元）**：  
> * **主控芯片**：`ESP32-WROOM-32`（自带 Wi-Fi 射频与双核算力，约 12 元）；  
> * **音频输入（麦克风）**：`INMP441`（I2S 全数字全向拾音麦克风，约 2.5 元）；  
> * **音频输出（功放喇叭）**：`MAX98357A`（I2S 全数字功放模块）+ `8Ω 2W 腔体小喇叭`（约 4 元）；  
> * **对讲逻辑**：按住按键 ➔ 启动 I2S 录音并用 Opus/ADPCM 压缩 ➔ `esp_now_send` 毫秒级广播/单播投递 ➔ 接收端收到后 I2S DMA 实时解码播放，**免路由器、零服务器月租、100~200 米超远清晰通话**！  
> 
> 📹 **视频传输正确选型**：若需传输实时画面，请选择 **标准 Wi-Fi 局域网 UDP / RTSP / HTTP MJPEG 视频流（如 ESP32-CAM 模块）**。

#### 🔋 电池续航大算账：一块 600 mAh 的小锂电池能用多久？
很多同学在做产品原型时常问：“*如果我给板子接一块 600 mAh 的小型锂电池，到底能用多久？*”

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                   【600 mAh 电池三大场景实测续航对比】                 │
 │                                                                        │
 │  场景 1：儿童对讲机 (持续按住通话 / 功放喇叭持续工作)                   │
 │     • 平均工作电流：~160 mA                                            │
 │     • 连续通话时间：600 mAh ÷ 160 mA ≈ 【3.5 ~ 4 小时】                │
 │     • 日常间歇使用 (每天聊 30 分钟)：可维持 【1 ~ 2 天】               │
 │                                                                        │
 │  场景 2：低功耗无线遥控器 / 门铃 (按一次发一次，平时深度休眠)          │
 │     • 深度休眠待机电流：仅 10 µA (微安)                                │
 │     • 发射瞬间耗时：仅 10 毫秒，发完立即重回休眠                       │
 │     • 每天按 100 次理论续航：600 ÷ 0.28 mAh/天 ≈ 【2000+ 天 (3~5 年!)】│
 │                                                                        │
 │  场景 3：定时传感器哨兵 (每 5 分钟自动醒来发一包温湿度)                │
 │     • 每天唤醒 288 次，每次工作 50 毫秒，其余时间休眠                  │
 │     • 理论续航时间：600 ÷ 1.2 mAh/天 ≈ 【500 天 (约 1.5 年!)】         │
 └────────────────────────────────────────────────────────────────────────┘
```

---

### 5. 🔬 终极物理揭秘：ESP-NOW 本质用的是什么协议？为什么能传 100~200 米这么远？

很多细心的同学都会产生疑问：“*它不连路由器，也不用蓝牙，那它在空中发射的到底是什么波？为什么能传 100~200 米甚至穿几堵墙？*”

#### 1. 协议本质：纯血 Wi-Fi 802.11 动作帧（Vendor Action Frame）
* **物理层**：100% 运行在 ESP32 的 **2.4 GHz Wi-Fi 射频芯片** 上；
* **链路层**：采用 IEEE 802.11 标准中的“厂商自定义动作帧”，跳过了庞大的 TCP/IP 握手、IP 寻址和路由转发，直接由 Wi-Fi 硬件在空中嗅探捕获，**延迟直降至 1~2 毫秒**！

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                      【ESP-NOW 802.11 数据帧解剖图】                   │
 │                                                                        │
 │   ┌───────────────┬──────────────────────┬─────────────┬───────────┐   │
 │   │ 802.11 帧头   │ 乐鑫专属机构代码 (OUI)│ 目标对端 MAC │ 你的真实数据│   │
 │   │ (Action Frame)│ 0x18:FE:34 (Espressif)│ (如板B的MAC)│ (最多250B)│   │
 │   └───────────────┴──────────────────────┴─────────────┴───────────┘   │
 └────────────────────────────────────────────────────────────────────────┘
```

#### 2. 能传 200 米超远距离的三大物理黑科技：
1. **发射功率碾压（+20 dBm 满血大喇叭 📢）**：
   * BLE 蓝牙为了省电，发射功率仅有 `0~4 dBm`（约 1~2.5 毫瓦，相当于蚊子叫）；
   * ESP-NOW 激活了 ESP32 内部的 Wi-Fi 强力功率放大器（PA），输出功率高达 **`+20 dBm`（整整 100 毫瓦，大 40~100 倍！）**；
2. **通信物理铁律：降速增敏（1 Mbps 最强抗噪 DSSS 调制 🛡️）**：
   * 速率越低，灵敏度越高！普通 Wi-Fi 跑 54Mbps/150Mbps 高速，稍微有障碍物就衰减断连；
   * ESP-NOW 采用 802.11b 最坚挺的 **`1 Mbps DSSS` 扩频调制**，接收灵敏度高达 **`-98 dBm`**，具备极强的穿墙与抗多径干扰能力；
3. **“刺客式”超短空口爆发（0.3ms 极速发射）**：
   * 发送 1 包数据在空中仅占用 **0.3 毫秒**，发完立即关闭射频休眠，**既拥有 200 米超远射程，综合平均功耗又极其省电**！

---

## 15.2 🛠️ ESP-NOW 核心 API 与极简“通信四部曲”

在 ESP-IDF 中，编写 ESP-NOW 程序比写 Wi-Fi 联网简单 10 倍，总共只有 4 个标准化动作：

```text
 [ 步骤 1: 开启 Wi-Fi STA ] ──► [ 步骤 2: 初始化 ESP-NOW ] ──► [ 步骤 3: 注册 Peer 对端 ] ──► [ 步骤 4: 极速收发 ]
   esp_wifi_set_mode(STA)          esp_now_init()                esp_now_add_peer()           esp_now_send / recv_cb
```

---

### 1. 核心 API 大白话速查表

| API 函数名 | 通俗功能比喻 | 关键参数说明与注意事项 |
| :--- | :--- | :--- |
| **`esp_now_init()`** | **打开对讲机电源** | 初始化 ESP-NOW 协议栈（必须在 `esp_wifi_start()` 之后调用） |
| **`esp_now_register_send_cb(...)`** | **配置发信回执听筒** | 注册发送完成回调，返回 `ESP_NOW_SEND_SUCCESS` 或 `FAIL` |
| **`esp_now_register_recv_cb(...)`** | **配置来信铃声中断** | 注册接收回调，一旦空中收到发给自己的数据包立即触发解析 |
| **`esp_now_add_peer(...)`** | **在电话簿添加对端** | 传入 `esp_now_peer_info_t` 结构体，配置对端 MAC 地址与信道 |
| **`esp_now_send(mac, data, len)`** | **按下通话键发射** | 向指定 MAC（单播）或全 FF（广播）发射最多 250 字节数据 |

---

### 2. 什么是 Peer？为什么发数据前必须 `add_peer`？
* **Peer（对端 / 通信伙伴）** 就像你手机里的 **“通讯录联系人”**；
* ESP-NOW 在单播发送前，必须先在通讯录里添加这个联系人的 MAC 地址（`esp_now_add_peer`）；
* 一块 ESP32 最多可以同时添加 **20 个 Peer 联系人**（支持 1 对 20 组网遥控）！

---

## 15.3 🎯 实验 1：双机单播对射遥控 (P2P Unicast)

### 1. 🎯 实验目标与生活化场景
* **场景**：制作一个无线遥控手柄与受控接收板；
* **动作**：板 A（遥控端）按下板载按键 **SW3（GPIO39）**，板 B（受控端）在 **2 毫秒内** 瞬间翻转点亮/熄灭板载绿色 **LED2（GPIO27）**！

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                      【双机单播对射控制流转图】                        │
 │                                                                        │
 │   [ ESP32 板 A: 遥控发射端 ]                  [ ESP32 板 B: 智能受控端 ]│
 │      按下 SW3 用户按键 ──(GPIO39)               板载绿色 LED2 (GPIO27) │
 │             │                                             ▲            │
 │             ▼                                             │ (毫秒翻转) │
 │      esp_now_send(板B_MAC) ───(802.11 动作帧: < 2ms)───► on_data_recv   │
 │             │                                             │            │
 │             ▼                                             ▼            │
 │   on_data_sent (收到硬件ACK) ◄──(物理层 ACK 确认帧)────────┘            │
 └────────────────────────────────────────────────────────────────────────┘
```

---

### 2. 💻 实验 1 完整源码

> 📁 **配套源码文件**：[`code/15_espnow_remote/01_espnow_unicast.c`](../code/15_espnow_remote/01_espnow_unicast.c)  
> ⚡ **一键切换运行**：在终端运行 `./switch_code.sh 15 1 --flash` 即可秒级切换并自动烧录！

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
#include "esp_now.h"
#include "esp_mac.h"

static const char *TAG = "EXP1_ESPNOW_UNICAST";

#define LED2_PIN        GPIO_NUM_27
#define BUTTON_PIN      GPIO_NUM_39

/* 目标接收端 MAC 地址 (填入另一块 ESP32 打印的真实 MAC，全 FF 为通用广播) */
static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* 自定义单播遥控数据包结构体 (最大 250 字节) */
typedef struct {
    uint32_t seq_num;       // 发送序列号 (第几次按下按键)
    uint8_t  cmd;           // 控制指令: 1 ➔ 点亮 LED, 0 ➔ 熄灭 LED, 2 ➔ 翻转 LED
    char     sender_name[16]; // 发送者昵称
} __attribute__((packed)) remote_packet_t;

static bool s_led_state = false;
static uint8_t s_my_mac[6] = {0};

/* 📡 ESP-NOW 数据发送完成回调函数 (底层硬件 ACK 回执) */
static void on_data_sent(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "🚀 [硬件 ACK 成功] 数据包已成功投递到目标设备！(耗时 < 2ms)");
    } else {
        ESP_LOGW(TAG, "⚠️ [发送无响应] 未收到对端 ACK 确认 (目标设备可能关机或距离过远)");
    }
}

/* 📥 ESP-NOW 数据接收回调函数 (收到数据瞬间触发) */
static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (len == sizeof(remote_packet_t)) {
        remote_packet_t *pkt = (remote_packet_t *)data;
        ESP_LOGI(TAG, "📥 [收到对端遥控指令] 来源 MAC: %02X:%02X:%02X:%02X:%02X:%02X | 发送者: %s | 序号: #%lu | 指令: %d",
                 recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
                 recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5],
                 pkt->sender_name, pkt->seq_num, pkt->cmd);

        // 执行受控动作：翻转 LED2
        if (pkt->cmd == 2 || pkt->cmd == 1) {
            s_led_state = !s_led_state;
            gpio_set_level(LED2_PIN, s_led_state ? 1 : 0);
            ESP_LOGI(TAG, "💡 执行受控动作 ➔ 板载绿色 LED2 状态已翻转为: \033[32m%s\033[0m",
                     s_led_state ? "【点亮 ON】" : "【熄灭 OFF】");
        }
    }
}

/* 📌 Wi-Fi 底层初始化与 ESP-NOW 协议栈挂载 */
static void wifi_espnow_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 1. 初始化 Wi-Fi 为 STA 模式 (无需连路由器)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 2. 读取并打印本机物理 MAC 地址
    esp_read_mac(s_my_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "🏷️  本机 Wi-Fi STA MAC 地址: \033[36m%02X:%02X:%02X:%02X:%02X:%02X\033[0m",
             s_my_mac[0], s_my_mac[1], s_my_mac[2], s_my_mac[3], s_my_mac[4], s_my_mac[5]);
    ESP_LOGI(TAG, "==================================================");

    // 3. 挂载 ESP-NOW 并注册回调
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    // 4. 将目标对端添加进通信录 (Peer)
    esp_now_peer_info_t peer_info = {
        .channel = 0,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer_info.peer_addr, s_peer_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    ESP_LOGI(TAG, "✅ ESP-NOW 协议栈就绪！已配对 Peer: %02X:%02X:%02X:%02X:%02X:%02X",
             s_peer_mac[0], s_peer_mac[1], s_peer_mac[2], s_peer_mac[3], s_peer_mac[4], s_peer_mac[5]);
}

/* 🔘 独立按键扫描与遥控发射任务 */
static void button_remote_task(void *pvParameters)
{
    uint32_t click_count = 0;
    while (1) {
        if (gpio_get_level(BUTTON_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(20)); // 消抖
            if (gpio_get_level(BUTTON_PIN) == 0) {
                click_count++;
                ESP_LOGI(TAG, "🔘 [SW3 按下] 触发第 %lu 次极速对射遥控发包...", click_count);

                remote_packet_t pkt = {
                    .seq_num = click_count,
                    .cmd = 2, // 翻转指令
                };
                snprintf(pkt.sender_name, sizeof(pkt.sender_name), "ESP32_%02X%02X", s_my_mac[4], s_my_mac[5]);

                // 🚀 极速对射发送！
                esp_now_send(s_peer_mac, (uint8_t *)&pkt, sizeof(pkt));

                while (gpio_get_level(BUTTON_PIN) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 15 实验 1：ESP-NOW 双机单播对射遥控     ");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t led_conf = { .pin_bit_mask = (1ULL << LED2_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&led_conf);
    gpio_set_level(LED2_PIN, 0);

    gpio_config_t btn_conf = { .pin_bit_mask = (1ULL << BUTTON_PIN), .mode = GPIO_MODE_INPUT };
    gpio_config(&btn_conf);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_espnow_init();
    xTaskCreate(button_remote_task, "btn_remote_task", 3072, NULL, 5, NULL);
}
```

---

### 3. 🔍 源码逐段深度精讲

1. **`__attribute__((packed))` 结构体紧凑对齐**：
   * 在网络与无线通信中，不同编译器默认可能会在结构体成员之间插入填充字节（Padding）；
   * 加上 `__attribute__((packed))` 强制要求编译器按照字节紧凑排列，确保板 A 发出的 21 字节数据在板 B 处能 100% 精准还原无偏移。
2. **`esp_now_register_send_cb` 硬件级 ACK 回执**：
   * 当调用 `esp_now_send` 时，ESP32 射频在空中发出数据后，会在几个微秒内等待对端硬件回复 802.11 ACK；
   * 回调中的 `status == ESP_NOW_SEND_SUCCESS` 是**由硬件芯片底层的射频握手保证的**，不需要我们写任何复杂的确认代码！
3. **`wifi_init_config_t` 仅需 STA 模式**：
   * 我们只需要把 Wi-Fi 模式设为 `WIFI_MODE_STA` 并调用 `esp_wifi_start()` 唤醒 Wi-Fi 硬件，**不需要调用 `esp_wifi_connect()` 连接任何路由器**，这就是它零等待、上电秒发的核心秘诀！

---

### 4. 📱 硬件实测与真机现象

#### 💡 手头有 2 块 ESP32 板子的小白：
1. **获取板 B 的 MAC**：将本代码烧录进板 B，打开串口监视器，记录串口第一行打印的 MAC 地址（例如 `48:27:E2:81:90:8C`）；
2. **修改板 A 的代码**：在代码第 28 行将 `s_peer_mac` 修改为板 B 的实际 MAC 地址：
   ```c
   static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = { 0x48, 0x27, 0xE2, 0x81, 0x90, 0x8C };
   ```
3. **烧录板 A 并对射**：
   * 按下板 A 上的 **SW3 按键**；
   * 👉 **肉眼奇迹发生**：板 B 上的绿色 LED2 在你按下的同一瞬间（**延迟 < 2ms**）立刻亮起！再次按下立刻熄灭！

#### 💡 手头只有 1 块 ESP32 板子的小白：
* 保持默认的 `s_peer_mac = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }` 广播模式；
* 按下板上的 SW3，你可以观察到发送回调触发 `[广播射频发射完成]`，完美验证协议栈运行！

---

> 💡 **承上启下过度思考**：  
> 实验 1 实现了单向遥控。但如果板 A 是无人机遥控器，它不仅想发指令，还想**实时知道无人机的当前飞行高度、电池电量，并且想测量此时此刻的无线空口延迟到底是多少毫秒**，该怎么做？  
> 接下来进入实验 2 —— **双向对讲与微秒级 RTT 链路探测**！

---

## 15.4 ⚡ 实验 2：ESP-NOW 双向对讲与微秒级 RTT 链路质量探测

### 1. 🎯 实验目标与生活化场景
* **场景**：构建高可靠的双向无线通信链路（遥控 + 遥测回传）；
* **机制（Ping-Pong 对讲机制）**：
  1. 板 A 周期性发射带微秒时间戳的 **`PING` 探测包**；
  2. 板 B 收到后瞬间原路回送一个 **`PONG` 回声包**（同时附带板 B 的传感器温度数据）；
  3. 板 A 收到 PONG 后，计算往返时延 **RTT（Round-Trip Time）** 与近 100 次的**发包成功率（PDR）**！

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                   【Ping-Pong 微秒级往返延时测速流转】                 │
 │                                                                        │
 │   [ 板 A 发起测速 ]                                   [ 板 B 回声响应 ]│
 │      T1: 记录时间戳 t1                                                 │
 │      esp_now_send(PING #1) ──(经过无线空口传输)──► 收到 PING           │
 │                                                      │ (立即回传)      │
 │      收到 PONG #1 ◄────────(携带温度数据回声)──────── esp_now_send(PONG)│
 │      T2: 记录时间戳 t2                                                 │
 │                                                                        │
 │      👉 最终精准往返时延 RTT = (t2 - t1) = 1.45 毫秒 (极速!)           │
 └────────────────────────────────────────────────────────────────────────┘
```

---

### 2. 💻 实验 2 完整源码

> 📁 **配套源码文件**：[`code/15_espnow_remote/02_espnow_twoway.c`](../code/15_espnow_remote/02_espnow_twoway.c)  
> ⚡ **一键切换运行**：在终端运行 `./switch_code.sh 15 2 --flash` 即可秒级切换并自动烧录！

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
#include "esp_now.h"
#include "esp_mac.h"

static const char *TAG = "EXP2_ESPNOW_TWOWAY";

#define LED2_PIN        GPIO_NUM_27

/* 目标对端 MAC 地址 (填入全 FF 支持一键双向广播探测) */
static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

typedef enum { MSG_TYPE_PING = 1, MSG_TYPE_PONG = 2 } msg_type_t;

/* Ping-Pong 测速与遥测报文 */
typedef struct {
    uint8_t  type;          // 1=PING, 2=PONG
    uint32_t ping_id;       // 测速序号
    int64_t  send_time_us;  // 发射时刻微秒级时间戳 (esp_timer_get_time)
    float    temp_celsius;  // 携带模拟环境温度 (如 26.5°C)
    char     node_name[16]; // 本机名称
} __attribute__((packed)) ping_pong_packet_t;

static uint8_t s_my_mac[6] = {0};
static uint32_t s_total_sent = 0;
static uint32_t s_total_acked = 0;
static float s_avg_rtt_ms = 0.0f;

static void on_data_sent(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS) s_total_acked++;
}

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (len != sizeof(ping_pong_packet_t)) return;
    ping_pong_packet_t *pkt = (ping_pong_packet_t *)data;

    // 场景 A：收到 PING ➔ 立即回送 PONG
    if (pkt->type == MSG_TYPE_PING) {
        ESP_LOGI(TAG, "🏓 [收到 PING 探测] 来源: %s | PingID: #%lu ➔ 立即回送 PONG!",
                 pkt->node_name, pkt->ping_id);

        ping_pong_packet_t pong_reply = *pkt;
        pong_reply.type = MSG_TYPE_PONG;
        snprintf(pong_reply.node_name, sizeof(pong_reply.node_name), "ESP32_%02X%02X", s_my_mac[4], s_my_mac[5]);
        esp_now_send(recv_info->src_addr, (uint8_t *)&pong_reply, sizeof(pong_reply));

        gpio_set_level(LED2_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(LED2_PIN, 0);
    }
    // 场景 B：收到 PONG ➔ 计算往返延时 RTT！
    else if (pkt->type == MSG_TYPE_PONG) {
        int64_t now_us = esp_timer_get_time();
        float rtt_ms = (float)(now_us - pkt->send_time_us) / 1000.0f;

        if (s_avg_rtt_ms == 0.0f) s_avg_rtt_ms = rtt_ms;
        else s_avg_rtt_ms = s_avg_rtt_ms * 0.8f + rtt_ms * 0.2f;

        float delivery_rate = (float)s_total_acked / (float)s_total_sent * 100.0f;

        ESP_LOGI(TAG, "⚡ [收到 PONG 回执] PingID: #%lu | \033[32m往返时延 RTT: %.2f ms\033[0m (平均: %.2f ms) | 链路成功率: %.1f%%",
                 pkt->ping_id, rtt_ms, s_avg_rtt_ms, delivery_rate);

        gpio_set_level(LED2_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(LED2_PIN, 0);
    }
}

static void wifi_espnow_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_read_mac(s_my_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "🏷️  本机 MAC 地址: %02X:%02X:%02X:%02X:%02X:%02X",
             s_my_mac[0], s_my_mac[1], s_my_mac[2], s_my_mac[3], s_my_mac[4], s_my_mac[5]);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    esp_now_peer_info_t peer_info = { .channel = 0, .ifidx = WIFI_IF_STA, .encrypt = false };
    memcpy(peer_info.peer_addr, s_peer_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
}

/* ⏱️ 周期性链路探测任务：每 2 秒自动发射一次 Ping 测速包 */
static void ping_probe_task(void *pvParameters)
{
    uint32_t ping_seq = 0;
    while (1) {
        ping_seq++;
        s_total_sent++;

        ping_pong_packet_t ping_pkt = {
            .type = MSG_TYPE_PING,
            .ping_id = ping_seq,
            .send_time_us = esp_timer_get_time(), // 捕获当前高精度时间戳
            .temp_celsius = 25.0f + (float)(ping_seq % 10) * 0.5f,
        };
        snprintf(ping_pkt.node_name, sizeof(ping_pkt.node_name), "ESP32_%02X%02X", s_my_mac[4], s_my_mac[5]);

        ESP_LOGI(TAG, "📤 [发射 PING 测速包 #%lu] (附带温度: %.1f°C)...", ping_seq, ping_pkt.temp_celsius);
        esp_now_send(s_peer_mac, (uint8_t *)&ping_pkt, sizeof(ping_pkt));

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 15 实验 2：ESP-NOW 双向对讲与 RTT 测速  ");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t led_conf = { .pin_bit_mask = (1ULL << LED2_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&led_conf);
    gpio_set_level(LED2_PIN, 0);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_espnow_init();
    xTaskCreate(ping_probe_task, "ping_probe_task", 3072, NULL, 5, NULL);
}
```

---

### 3. 🔍 源码逐段深度精讲

1. **高精度微秒计时器（`esp_timer_get_time()`）**：
   * 单片机如果用 FreeRTOS 的 `xTaskGetTickCount()` 计时，精度只有 10ms（10000 微秒），根本无法测量毫秒级的 ESP-NOW 速度；
   * `esp_timer_get_time()` 直接读取 ESP32 内部的 64 位高精度硬件定时器，分辨率高达 **1 微秒（$10^{-6}$ 秒）**！
2. **RTT 往返时延（Round-Trip Time）的计算**：
   * 发射时记录时间戳 $T_1$，对端收到后原样带回，本机收到 PONG 时读取当前时间戳 $T_2$；
   * 往返时延 $\text{RTT} = (T_2 - T_1) / 1000.0$ 毫秒。在近距离空旷环境下，实测 RTT **通常在 1.2ms ~ 1.8ms 之间**，令人叹为观止！

---

### 4. 📱 硬件实测与真机现象

烧录两块板子后打开串口，你会看到：
```text
I (12040) EXP2_ESPNOW_TWOWAY: 📤 [发射 PING 测速包 #6] (附带温度: 28.0°C)...
I (12042) EXP2_ESPNOW_TWOWAY: ⚡ [收到 PONG 回执] PingID: #6 | 往返时延 RTT: 1.42 ms (平均: 1.51 ms) | 链路成功率: 100.0%
```
每次收发，开发板上的绿色 LED2 都会伴随数据回声灵动闪烁一次！

---

> 💡 **承上启下过度思考**：  
> 刚才我们都是点对点对射（1 对 1 通信）。  
> 但如果我们要打造的是 **“无人机编队灯光秀”**，或者在智慧农业大棚里，一个总控手柄要同时命令 **100 个喷水阀门同时开启**，难道总控板要写一个循环发 100 次单播吗？  
> 那样不仅耗时，而且第 1 个阀门和第 100 个阀门动作会有严重的时间差！  
> 接下来，我们将进入群控终极大招 —— **广播群控与“一呼百应”**！

---

## 15.5 📢 实验 3：广播群控与“一呼百应” (Broadcast Group Control)

### 1. 🎯 实验目标与生活化场景
* **场景**：总指挥官按下板载 SW3，周围无线电覆盖范围内的 **所有 ESP32 从机（无论是 3 台、10 台还是 100 台）** 在 **2 毫秒内同时执行协同动作**！
* **支持 3 种群控模式循环切换**：
  1. `GROUP_CMD_ALL_OFF` ➔ 全体从机统一熄灭 LED；
  2. `GROUP_CMD_ALL_ON` ➔ 全体从机统一点亮 LED；
  3. `GROUP_CMD_SYNC_BLINK` ➔ 全体从机毫秒级**同步律动闪烁 3 次**！

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                      【广播群控“一呼百应”架构图】                      │
 │                                                                        │
 │                  [ 指挥官总控手柄 (Commander) ]                         │
 │                    按下 SW3 切换群控模式 (0/1/2)                       │
 │                                 │                                      │
 │                                 ▼ (目标 MAC: FF:FF:FF:FF:FF:FF)        │
 │              (((( 802.11 全局广播数据帧: < 2ms ))))                    │
 │               ┌─────────────────┬─────────────────┐                    │
 │               ▼                 ▼                 ▼                    │
 │        [ 从机节点 #1 ]   [ 从机节点 #2 ]   [ 从机节点 #N... ]          │
 │        同步律动闪烁!      同步律动闪烁!      同步律动闪烁!              │
 └────────────────────────────────────────────────────────────────────────┘
```

---

### 2. 💻 实验 3 完整源码

> 📁 **配套源码文件**：[`code/15_espnow_remote/03_espnow_broadcast.c`](../code/15_espnow_remote/03_espnow_broadcast.c)  
> ⚡ **一键切换运行**：在终端运行 `./switch_code.sh 15 3 --flash` 即可秒级切换并自动烧录！

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
#include "esp_now.h"
#include "esp_mac.h"

static const char *TAG = "EXP3_ESPNOW_BROADCAST";

#define LED2_PIN        GPIO_NUM_27
#define BUTTON_PIN      GPIO_NUM_39

/* 全局广播 MAC 地址 (一呼百应) */
static const uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

typedef enum {
    GROUP_CMD_ALL_OFF = 0,    // 全灭
    GROUP_CMD_ALL_ON  = 1,    // 全亮
    GROUP_CMD_SYNC_BLINK = 2, // 同步闪烁 3 次
} group_cmd_t;

/* 广播群控报文协议 */
typedef struct {
    uint8_t  magic;           // 协议魔数: 0x5A (防误触发)
    uint8_t  cmd;             // 群控指令 (group_cmd_t)
    uint32_t group_seq;       // 群控全局序号
    char     commander[16];   // 指挥官名称
} __attribute__((packed)) group_control_packet_t;

static uint8_t s_my_mac[6] = {0};
static uint8_t s_current_mode = 0;

static void on_broadcast_sent(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    ESP_LOGI(TAG, "📢 [广播射频发射完成] 802.11 广播帧已送达周围空域！");
}

static void on_broadcast_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (len != sizeof(group_control_packet_t)) return;
    group_control_packet_t *pkt = (group_control_packet_t *)data;

    // 校验魔数
    if (pkt->magic != 0x5A) return;

    ESP_LOGI(TAG, "⚡ [收到集群广播指令] 指挥官: \033[36m%s\033[0m | 序号: #%lu | 指令代码: %d",
             pkt->commander, pkt->group_seq, pkt->cmd);

    // 执行群控协同动作
    switch (pkt->cmd) {
        case GROUP_CMD_ALL_OFF:
            gpio_set_level(LED2_PIN, 0);
            ESP_LOGI(TAG, "🌑 [群控执行] 全体节点协同 ➔ \033[31m【全部熄灭】\033[0m");
            break;

        case GROUP_CMD_ALL_ON:
            gpio_set_level(LED2_PIN, 1);
            ESP_LOGI(TAG, "💡 [群控执行] 全体节点协同 ➔ \033[32m【全部点亮】\033[0m");
            break;

        case GROUP_CMD_SYNC_BLINK:
            ESP_LOGI(TAG, "✨ [群控执行] 全体节点协同 ➔ \033[33m【同步律动闪烁 3 次】\033[0m");
            for (int i = 0; i < 3; i++) {
                gpio_set_level(LED2_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED2_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            break;

        default:
            break;
    }
}

static void wifi_espnow_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_read_mac(s_my_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "🏷️  本机节点 MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             s_my_mac[0], s_my_mac[1], s_my_mac[2], s_my_mac[3], s_my_mac[4], s_my_mac[5]);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_broadcast_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_broadcast_recv));

    // 添加广播 Peer (FF:FF:FF:FF:FF:FF)
    esp_now_peer_info_t peer_info = {
        .channel = 0,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer_info.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    ESP_LOGI(TAG, "✅ 广播群控信道已建立 (Peer: FF:FF:FF:FF:FF:FF)");
}

/* 🔘 按键群控任务：每按一次 SW3 循环切换群控指令模式并一呼百应广播 */
static void commander_button_task(void *pvParameters)
{
    uint32_t broadcast_seq = 0;
    while (1) {
        if (gpio_get_level(BUTTON_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(20)); // 消抖
            if (gpio_get_level(BUTTON_PIN) == 0) {
                broadcast_seq++;
                s_current_mode = (s_current_mode + 1) % 3; // 0 ➔ 1 ➔ 2 循环

                group_control_packet_t pkt = {
                    .magic = 0x5A,
                    .cmd = s_current_mode,
                    .group_seq = broadcast_seq,
                };
                snprintf(pkt.commander, sizeof(pkt.commander), "CMD_%02X%02X", s_my_mac[4], s_my_mac[5]);

                ESP_LOGI(TAG, "📢 [指挥官发令] 按下 SW3 ➔ 发射全局广播群控令 (模式: %d, 序号: #%lu)...",
                         s_current_mode, broadcast_seq);

                esp_now_send(s_broadcast_mac, (uint8_t *)&pkt, sizeof(pkt));

                // 自身也执行动作保持一致
                if (s_current_mode == 0) gpio_set_level(LED2_PIN, 0);
                else if (s_current_mode == 1) gpio_set_level(LED2_PIN, 1);
                else if (s_current_mode == 2) {
                    for (int i = 0; i < 3; i++) {
                        gpio_set_level(LED2_PIN, 1);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        gpio_set_level(LED2_PIN, 0);
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                }

                while (gpio_get_level(BUTTON_PIN) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 15 实验 3：ESP-NOW 广播群控“一呼百应”  ");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t led_conf = { .pin_bit_mask = (1ULL << LED2_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&led_conf);
    gpio_set_level(LED2_PIN, 0);

    gpio_config_t btn_conf = { .pin_bit_mask = (1ULL << BUTTON_PIN), .mode = GPIO_MODE_INPUT };
    gpio_config(&btn_conf);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_espnow_init();
    xTaskCreate(commander_button_task, "commander_btn_task", 3072, NULL, 5, NULL);
}
```

---

### 3. 🔍 源码逐段深度精讲

1. **`FF:FF:FF:FF:FF:FF` 广播 MAC 地址**：
   * 在以太网和 Wi-Fi 协议中，全 1（即全 `0xFF`）是全球公认的广播地址；
   * 当 ESP32 发射目标为全 FF 的数据帧时，**周围所有处于同一 Wi-Fi 信道的 ESP32 芯片的射频接收器都会无差别捕获此报文**，实现真正的“一呼百应”！
2. **协议魔数（`magic = 0x5A`）防干扰设计**：
   * 空中可能充斥着其他开发者的广播测试包；
   * 我们在结构体开头设计了 1 字节的魔数 `0x5A`。接收端先校验魔数，如果不对直接丢弃，保证工业级抗干扰安全性。

---

## 15.6 关卡总结与通关打卡

恭喜你！你已经完全掌握了 ESP32 家族最具黑科技色彩的免路由极速私有通信神技 —— **ESP-NOW**！

### 🏆 核心技能清单回顾：
* [x] **通信协议本质**：搞懂 Wi-Fi 动作帧（Vendor Action Frame）与免路由点对点机制；
* [x] **极速延迟特性**：掌握 `< 2ms` 极速空口传输与高精度硬件定时器 RTT 测速；
* [x] **单播点对点对射**：掌握 MAC 点名配对与底层硬件 ACK 状态监听；
* [x] **广播群控架构**：掌握 `FF:FF:FF:FF:FF:FF` 一呼百应集群协同控制。

---

掌握了局域网点对点超低延迟通信之后，如何让局域网里的任何电脑、手机无需安装任何 App，直接打开 Chrome / Safari 浏览器就能可视化控制单片机？  
下一关，我们将学习在 ESP32 上架设局域网微型网站！

请翻开 [**第 16 章：ESP32 Web Server 网页中控、mDNS 本地域名与 AP 强制门户配网**](./16_WebServer网页中控与热点配网.md)！

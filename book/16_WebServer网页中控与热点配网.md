# 第 16 关：ESP32 Web Server 网页中控、mDNS 本地域名与 AP 强制门户配网

![第16关封面插画](../docs/images/esp32_level16_cover.jpg)

---

## 🎯 本关学习目标

在前面的智能控制中，我们用到了手机 App 和 MQTT 客户端。但在消费级智能家居产品中，最理想的用户体验是：
1. **免装任何第三方 App**：任何电脑、平板或手机，只要打开自带的浏览器（Safari / Chrome），就能直接进入控制面板开关灯光、查看实时温湿度曲线；
2. **免查 IP 直连**：不用去路由器后台翻找分配给 ESP32 的 IP 地址，直接在浏览器输入 `http://esp32.local` 即可直达！
3. **开箱即用的强制门户配网（Captive Portal）**：设备买回家首次通电，ESP32 自动开启一个 Wi-Fi 热点，手机连上后**自动弹窗**让用户输入家里的 Wi-Fi 密码！

完成本关卡后，你将达成以下核心成就：
1. **掌握 ESP-IDF `esp_http_server` 原生驱动**：HTTP GET/POST 路由注册与 URI 处理器；
2. **掌握 mDNS（Multicast DNS）局域网服务发现**：绑定 `esp32.local` 本地域名；
3. **掌握 DNS 劫持与强制门户配网原理**：捕获所有 DNS 请求并重定向至配网网页，将 Wi-Fi 凭证安全持久化到 NVS。

---

## 🧭 关卡实验与源码速查

| 实验编号 | 实验名称 | 核心知识与实验目标 | 配套源码文件 | 一键切换命令 |
| :--- | :--- | :--- | :--- | :--- |
| **实验 1** | **局域网微型 Web 服务器** | 开启 HTTP Server，浏览器访问板载 IP，网页渲染控制面板实时开关 LED2 | [`01_http_web_server.c`](../code/16_web_server_portal/01_http_web_server.c) | `./switch_code.sh 16 1 --flash` |
| **实验 2** | **mDNS 本地域名解析** | 配置设备域名为 `esp32.local`，免查 IP 浏览器直接通过域名秒级访问 | [`02_mdns_hostname.c`](../code/16_web_server_portal/02_mdns_hostname.c) | `./switch_code.sh 16 2 --flash` |
| **实验 3** | **AP 强制门户配网 (Captive Portal)** | 未配网时自动开热点，手机连上自动弹窗输入 Wi-Fi 密码并保存至 NVS | [`03_captive_portal.c`](../code/16_web_server_portal/03_captive_portal.c) | `./switch_code.sh 16 3 --flash` |

---

## 16.1 关卡总结与通关打卡

恭喜你！你已经掌握了商业级物联网产品最核心的配网与轻量化人机交互方案 —— **Web Server + mDNS + Captive Portal**！

### 🏆 核心技能清单回顾：
* [x] **嵌入式 Web 服务器**：掌握 `esp_http_server` 的路由注册与 HTML/JSON 接口响应；
* [x] **mDNS 本地域名**：实现 `esp32.local` 免 IP 访问；
* [x] **AP 强制门户配网**：掌握 DNS 劫持与开机自动弹窗配网商业级全流程。

---

当设备已经部署到千里之外的用户家中或野外，如果固件发现了 Bug 需要修复，或者想要增加新功能，总不可能把设备拆开用串口线烧录吧？  
下一关，我们将学习商业级物联网设备的终极运维武器 —— **OTA 空中固件升级与 A/B 双分区防变砖回退**！

请翻开 [**第 17 章：ESP32 OTA 空中无线固件升级与 A/B 双分区防变砖回滚实战**](./17_OTA空中固件升级与防变砖回滚.md)！

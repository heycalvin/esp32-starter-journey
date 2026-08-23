# 第 17 关：ESP32 OTA 空中无线固件升级与 A/B 双分区防变砖回滚实战

![第17关封面插画](../docs/images/esp32_level17_cover.jpg)

---

## 🎯 本关学习目标

在嵌入式与物联网产品生命周期中，**OTA（Over-The-Air，空中固件升级）** 是决定产品生死存亡的最核心功能：
* **现实挑战**：如果设备已经售出部署在户外、工厂车间或客户家中，遇到紧急 Bug 或功能升级时，不可能派人上门插 USB 线刷机；
* **致命风险**：如果在无线升级过程中突然断电，或者新固件存在严重 Bug 导致开机死锁崩溃，设备会不会彻底“变砖”？

本关将带你彻底攻克商业级 **OTA A/B 双分区无缝乒乓升级** 与 **防变砖自动回退（Rollback）机制**！

完成本关卡后，你将达成以下核心成就：
1. **搞懂 Flash 分区表（Partition Table）底层原理**：factory、ota_0 与 ota_1 双备份乒乓架构；
2. **掌握 HTTP/HTTPS 固件无线拉取与写入流水线**：`esp_https_ota` 与断点续传机制；
3. **掌握固件完整性校验与防变砖自检**：固件 SHA-256 签名校验与 `esp_ota_mark_app_valid_cancel_rollback` 状态确认；
4. **体验一键无线空中刷机**：开发板通过 Wi-Fi 自动检测新版本，5 秒内完成空中刷写并自动重启！

---

## 🧭 关卡实验与源码速查

| 实验编号 | 实验名称 | 核心知识与实验目标 | 配套源码文件 | 一键切换命令 |
| :--- | :--- | :--- | :--- | :--- |
| **实验 1** | **自定义 A/B 双分区表配置** | 划分 `ota_0` 与 `ota_1` 双镜像分区，理解 Bootloader 引导与状态转移 | [`01_ota_partition_check.c`](../code/17_ota_firmware/01_ota_partition_check.c) | `./switch_code.sh 17 1 --flash` |
| **实验 2** | **HTTP 固件空中静默拉取与烧写** | 通过局域网 HTTP 服务器拉取最新 `app.bin` 并烧写至备用分区后自动重启 | [`02_http_ota_upgrade.c`](../code/17_ota_firmware/02_http_ota_upgrade.c) | `./switch_code.sh 17 2 --flash` |
| **实验 3** | **防变砖自检与自动回退机制** | 模拟新固件异常崩溃，验证 Bootloader 毫秒级自动回滚至上一稳定固件 | [`03_ota_rollback_guard.c`](../code/17_ota_firmware/03_ota_rollback_guard.c) | `./switch_code.sh 17 3 --flash` |

---

*(本章完整源码与深度实战在后续章节无缝开启！)*

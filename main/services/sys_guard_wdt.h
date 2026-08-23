#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化任务看门狗 (TWDT)
 * @param timeout_ms 超时时间（毫秒），超时未喂狗将自动触发硬件复位
 */
esp_err_t sys_guard_wdt_init(uint32_t timeout_ms);

/**
 * @brief 将当前任务加入看门狗监控列表
 */
esp_err_t sys_guard_wdt_subscribe_current_task(void);

/**
 * @brief 当前任务喂狗
 */
esp_err_t sys_guard_wdt_feed(void);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化硬件任务看门狗守护服务 (TWDT)
 * @param timeout_ms 超时触发看门狗复位的时间（毫秒）
 */
esp_err_t sys_guard_wdt_init(uint32_t timeout_ms);

/**
 * @brief 将调用本函数的当前任务加入看门狗看护名单
 */
esp_err_t sys_guard_wdt_subscribe_current_task(void);

/**
 * @brief 向看门狗及时喂狗 (重置倒计时)
 */
esp_err_t sys_guard_wdt_feed(void);

#ifdef __cplusplus
}
#endif

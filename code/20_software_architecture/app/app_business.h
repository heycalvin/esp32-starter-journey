#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动业务逻辑任务调度
 */
esp_err_t app_business_start(void);

#ifdef __cplusplus
}
#endif

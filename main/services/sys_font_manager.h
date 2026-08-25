#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 FreeType 矢量字体管理器
 * 
 * @return esp_err_t 成功返回 ESP_OK
 */
esp_err_t sys_font_manager_init(void);

/**
 * @brief 检查是否成功从 TF 卡挂载并加载了中文字体
 */
bool sys_font_manager_has_chinese_font(void);

/**
 * @brief 获取指定字号的字体（优先返回 FreeType 矢量字体，未加载时降级为内置西文字体）
 * 
 * @param size 字号 (如 14, 16, 20, 28, 32)
 * @return const lv_font_t* 字体指针
 */
const lv_font_t *sys_font_manager_get_font(int size);

#ifdef __cplusplus
}
#endif

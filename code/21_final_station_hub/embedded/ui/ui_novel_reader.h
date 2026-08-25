#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化全功能小说阅读器 UI 组件
 * 
 * @param parent_tab 父容器 Tab 对象
 */
void ui_novel_reader_init(lv_obj_t *parent_tab);

/**
 * @brief 打开全屏沉浸阅读器
 */
void ui_novel_reader_open_fullscreen(void);

/**
 * @brief 刷新小说阅读器内容
 */
void ui_novel_reader_refresh_content(void);

#ifdef __cplusplus
}
#endif

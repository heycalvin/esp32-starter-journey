#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化电子相册 / 艺术画廊组件
 * 
 * @param parent_tab 父容器 Tab 对象
 */
void ui_photo_album_init(lv_obj_t *parent_tab);

/**
 * @brief 打开 240x280 全屏沉浸式艺术画廊
 */
void ui_photo_album_open_fullscreen(void);

/**
 * @brief 切换下一张画作 / 相片
 */
void ui_photo_album_next(void);

/**
 * @brief 切换上一张画作 / 相片
 */
void ui_photo_album_prev(void);

#ifdef __cplusplus
}
#endif

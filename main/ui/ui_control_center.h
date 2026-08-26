#pragma once
#include "lvgl.h"

/**
 * @brief 初始化全局下拉控制中心（在 ui_hub_init 调用后调用）
 * @param parent 挂载到 lv_screen_active() 即可
 */
void ui_control_center_init(lv_obj_t *parent);

/**
 * @brief 展开控制中心（带弹性下滑动画）
 */
void ui_control_center_show(void);

/**
 * @brief 收起控制中心（带弹性上滑动画）
 */
void ui_control_center_hide(void);

/**
 * @brief 控制中心是否当前可见
 */
bool ui_control_center_is_visible(void);

#pragma once
#include "lvgl.h"

/**
 * @brief 在指定 Tab 容器内初始化番茄钟/大号时钟页面
 */
void ui_pomodoro_init(lv_obj_t *parent_tab);

/**
 * @brief 每秒刷新时钟/倒计时（在主任务定时器中调用）
 * @param hour  当前小时
 * @param min   当前分钟
 * @param sec   当前秒
 */
void ui_pomodoro_tick(int hour, int min, int sec);

/**
 * @brief 更新大时钟模式下显示的真实日期
 */
void ui_pomodoro_update_date(const char *date_str);

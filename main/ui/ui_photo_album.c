#include "ui_photo_album.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "file_reader.h"
#include "sys_font_manager.h"
#include "bsp_lvgl_port.h"
#include "bsp_sdcard.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char *TAG = "UI_ALBUM";

#define CANVAS_W 240
#define CANVAS_H 280
#define PREV_W   210
#define PREV_H   80

// 画作元数据定义
typedef struct {
    const char *title;       // 艺术标题
    const char *artist;      // 创作者 / 题材
    const char *desc;        // 分辨率与介绍
    uint32_t accent_color;   // 荧光强调色
} art_gallery_item_t;

static const art_gallery_item_t s_preset_arts[] = {
    {
        .title = "赛博极夜 · 霓虹网格",
        .artist = "Cyberpunk 2077 Horizon",
        .desc = "240×280 · 透视网格与落日流光",
        .accent_color = 0x38BDF8
    },
    {
        .title = "猎户座 · 深空星云",
        .artist = "Orion Nebula NGC1976",
        .desc = "240×280 · 远古恒星重聚变星尘",
        .accent_color = 0xF472B6
    },
    {
        .title = "极光之夜 · 翡翠极光",
        .artist = "Aurora Borealis Emerald",
        .desc = "240×280 · 地磁风暴与极地冷杉",
        .accent_color = 0x34D399
    },
    {
        .title = "黄金纪元 · 引力空间",
        .artist = "Gravity Field Mobius",
        .desc = "240×280 · 莫比乌斯曲率环带",
        .accent_color = 0xFBBF24
    },
    {
        .title = "星海远征 · 空间死线",
        .artist = "Starfleet Deep Space",
        .desc = "240×280 · 曲率航迹与三角星舰",
        .accent_color = 0x38BDF8
    }
};

static int s_art_count = sizeof(s_preset_arts) / sizeof(s_preset_arts[0]);
static int s_current_index = 0;
static bool s_slideshow_running = false;
static esp_timer_handle_t s_slideshow_timer = NULL;

// PSRAM 中的真彩像素画布 Buffer (240x280x2 = 134.4 KB)
static uint16_t *s_canvas_buf_fs = NULL;
static lv_image_dsc_t s_img_dsc_fs;

// Tab 3 预览画布 Buffer (210x80x2 = 33.6 KB)
static uint16_t *s_canvas_buf_tab = NULL;
static lv_image_dsc_t s_img_dsc_tab;

// Tab 3 预览组件
static lv_obj_t *s_tab_art_img = NULL;
static lv_obj_t *s_tab_art_title = NULL;
static lv_obj_t *s_tab_art_desc = NULL;
static lv_obj_t *s_tab_art_idx_label = NULL;

// 全屏沉浸式画廊组件 (240x280 满屏)
static lv_obj_t *s_fs_art_win = NULL;
static lv_obj_t *s_fs_img_obj = NULL;
static lv_obj_t *s_fs_floating_header = NULL;
static lv_obj_t *s_fs_floating_footer = NULL;
static lv_obj_t *s_fs_title_label = NULL;
static lv_obj_t *s_fs_desc_label = NULL;
static lv_obj_t *s_fs_counter_label = NULL;
static lv_obj_t *s_fs_lbl_play = NULL;
static bool s_fs_bars_visible = true;

// 触摸位移记录
static lv_point_t s_touch_down_pt;

/* RGB888 转 RGB565 (高低字节对齐) */
static inline uint16_t rgb_to_565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* 🎨 像素级真实画作渲染器 (把真实的画面画到 RGB565 缓冲区中) */
static void render_procedural_artwork(uint16_t *buf, int width, int height, int art_id)
{
    if (!buf) return;

    for (int y = 0; y < height; y++) {
        float ny = (float)y / height; // 0.0 ~ 1.0 (从上到下)
        for (int x = 0; x < width; x++) {
            float nx = (float)x / width; // 0.0 ~ 1.0 (从左到右)
            uint8_t r = 0, g = 0, b = 0;

            switch (art_id % 5) {
                case 0: { // 1. 【赛博极夜 · 霓虹网格】
                    if (ny < 0.55f) {
                        // 上半区：深紫到夜空的渐变 + 巨型发光落日
                        float dy = ny - 0.35f;
                        float dx = (nx - 0.5f) * 1.2f;
                        float dist_sun = sqrtf(dx * dx + dy * dy);
                        if (dist_sun < 0.22f) {
                            // 落日高光 (金橙到荧光粉)
                            float sun_grad = (dist_sun / 0.22f);
                            r = (uint8_t)(255 - sun_grad * 50);
                            g = (uint8_t)(120 - sun_grad * 90);
                            b = (uint8_t)(40 + sun_grad * 120);
                            // 落日横向扫描线
                            if (((y / 2) % 3) == 0 && ny > 0.30f) {
                                r = (uint8_t)(r * 0.4f);
                                g = (uint8_t)(g * 0.4f);
                                b = (uint8_t)(b * 0.4f);
                            }
                        } else {
                            // 深空紫蓝渐变
                            r = (uint8_t)(15 + ny * 60);
                            g = (uint8_t)(5 + ny * 20);
                            b = (uint8_t)(40 + ny * 80);
                        }
                    } else {
                        // 下半区：经典 3D 透视网格
                        float horizon_y = (ny - 0.55f) / 0.45f; // 0.0 ~ 1.0
                        float depth = 1.0f / (horizon_y + 0.08f);
                        float grid_x = (nx - 0.5f) * depth * 8.0f;
                        float grid_y = depth * 4.0f;

                        bool is_line = (fabsf(grid_x - roundf(grid_x)) < 0.12f) ||
                                       (fabsf(grid_y - roundf(grid_y)) < 0.18f);
                        if (is_line) {
                            // 发光赛博青色网格线
                            r = (uint8_t)(40 + horizon_y * 120);
                            g = (uint8_t)(180 + horizon_y * 70);
                            b = 255;
                        } else {
                            // 网格地面基底
                            r = (uint8_t)(10 + horizon_y * 20);
                            g = 5;
                            b = (uint8_t)(25 + horizon_y * 35);
                        }
                    }
                    break;
                }
                case 1: { // 2. 【猎户座 · 深空星云】
                    // 径向星云光晕
                    float cx = (nx - 0.5f) * 1.5f;
                    float cy = (ny - 0.45f) * 1.5f;
                    float d = sqrtf(cx * cx + cy * cy);
                    float nebula = expf(-d * 2.8f) * 1.4f;

                    // 玫瑰红与紫罗兰混合
                    r = (uint8_t)(fminf(255.0f, nebula * 210.0f + 12.0f));
                    g = (uint8_t)(fminf(255.0f, nebula * 60.0f + 5.0f));
                    b = (uint8_t)(fminf(255.0f, nebula * 240.0f + 25.0f));

                    // 恒星与星尘粒子 (确定性伪随机)
                    uint32_t seed = (x * 7919) ^ (y * 6271);
                    if ((seed % 140) == 0) {
                        r = 255; g = 240; b = 255; // 亮星
                    } else if ((seed % 70) == 0) {
                        r = (uint8_t)(r + 60); g = (uint8_t)(g + 60); b = 255;
                    }
                    break;
                }
                case 2: { // 3. 【极光之夜 · 翡翠极光】
                    // 极光正弦波折带
                    float wave1 = sinf(nx * 8.0f + 1.2f) * 0.12f + 0.35f;
                    float wave2 = cosf(nx * 5.0f + 2.0f) * 0.08f + 0.45f;
                    float dist_aurora = fabsf(ny - wave1);
                    float aurora_glow = expf(-dist_aurora * 12.0f);

                    if (ny > 0.78f) {
                        // 底部黑松剪影
                        float tree = fabsf(sinf(nx * 45.0f)) * 0.12f;
                        if (ny > (0.90f - tree)) {
                            r = 2; g = 8; b = 6;
                        } else {
                            r = 4; g = 18; b = 16;
                        }
                    } else {
                        // 翡翠极光与星空
                        r = (uint8_t)(fminf(255.0f, aurora_glow * 70.0f + 5.0f));
                        g = (uint8_t)(fminf(255.0f, aurora_glow * 255.0f + 15.0f));
                        b = (uint8_t)(fminf(255.0f, aurora_glow * 170.0f + 35.0f));

                        // 背景微弱星光
                        if (((x * 37 + y * 71) % 160) == 0) {
                            r = 200; g = 255; b = 230;
                        }
                    }
                    break;
                }
                case 3: { // 4. 【黄金纪元 · 引力空间】
                    // 莫比乌斯曲率同心环
                    float dx = (nx - 0.5f);
                    float dy = (ny - 0.5f);
                    float angle = atan2f(dy, dx);
                    float dist = sqrtf(dx * dx + dy * dy);
                    float ring = sinf(dist * 35.0f - angle * 2.0f);

                    if (ring > 0.2f) {
                        float intensity = (ring - 0.2f) / 0.8f;
                        r = (uint8_t)(fminf(255.0f, 220 + intensity * 35));
                        g = (uint8_t)(fminf(255.0f, 140 + intensity * 60));
                        b = (uint8_t)(20 + intensity * 40);
                    } else {
                        r = (uint8_t)(25 + dist * 40);
                        g = (uint8_t)(15 + dist * 20);
                        b = 5;
                    }
                    break;
                }
                default: { // 5. 【星海远征 · 空间死线】
                    // 深蓝曲率空间 + 死线激光束
                    float beam_dist = fabsf((ny - 0.5f) - (nx - 0.5f) * 0.6f);
                    float beam = expf(-beam_dist * 25.0f);

                    r = (uint8_t)(fminf(255.0f, beam * 100.0f + ny * 20));
                    g = (uint8_t)(fminf(255.0f, beam * 220.0f + ny * 40));
                    b = (uint8_t)(fminf(255.0f, beam * 255.0f + 80.0f + ny * 100));

                    // 空间死线光斑
                    if (fabsf(nx - ny) < 0.02f) {
                        r = 255; g = 255; b = 255;
                    }
                    break;
                }
            }

            buf[y * width + x] = rgb_to_565(r, g, b);
        }
    }
}

static void update_gallery_display(void);

static void slideshow_timer_cb(void *arg)
{
    bsp_lvgl_port_lock(0);
    ui_photo_album_next();
    bsp_lvgl_port_unlock();
}

static void set_slideshow_state(bool run)
{
    s_slideshow_running = run;
    if (s_slideshow_running) {
        if (!s_slideshow_timer) {
            esp_timer_create_args_t timer_args = {
                .callback = &slideshow_timer_cb,
                .name = "album_slide"
            };
            esp_timer_create(&timer_args, &s_slideshow_timer);
        }
        esp_timer_start_periodic(s_slideshow_timer, 4000000); // 4 秒自动轮播
        if (s_fs_lbl_play) lv_label_set_text(s_fs_lbl_play, LV_SYMBOL_PAUSE " 暂停");
    } else {
        if (s_slideshow_timer) {
            esp_timer_stop(s_slideshow_timer);
        }
        if (s_fs_lbl_play) lv_label_set_text(s_fs_lbl_play, LV_SYMBOL_PLAY " 轮播");
    }
}

static void update_gallery_display(void)
{
    int total = s_art_count;
    char tf_path[256] = {0};
    bool is_tf_photo = false;

    int tf_count = file_reader_get_photo_count();
    if (bsp_sdcard_is_mounted() && tf_count > 0 && s_current_index >= s_art_count) {
        is_tf_photo = true;
        file_reader_get_photo_path(s_current_index - s_art_count, tf_path, sizeof(tf_path));
    }

    const art_gallery_item_t *art = &s_preset_arts[s_current_index % s_art_count];
    const lv_font_t *font_cn = sys_font_manager_get_font(14);

    char idx_str[64];
    snprintf(idx_str, sizeof(idx_str), "%d / %d", s_current_index + 1, total + (bsp_sdcard_is_mounted() ? tf_count : 0));

    // 1. 实时计算并渲染 Tab 3 预览画布像素
    if (s_canvas_buf_tab) {
        if (is_tf_photo) {
            file_reader_decode_jpeg_to_buffer(tf_path, s_canvas_buf_tab, PREV_W, PREV_H);
        } else {
            render_procedural_artwork(s_canvas_buf_tab, PREV_W, PREV_H, s_current_index);
        }
        if (s_tab_art_img) {
            lv_image_set_src(s_tab_art_img, &s_img_dsc_tab);
        }
    }

    if (s_tab_art_title) {
        lv_label_set_text(s_tab_art_title, is_tf_photo ? tf_path : art->title);
        lv_obj_set_style_text_color(s_tab_art_title, lv_color_hex(art->accent_color), 0);
        lv_obj_set_style_text_font(s_tab_art_title, font_cn, 0);
    }
    if (s_tab_art_desc) {
        lv_label_set_text(s_tab_art_desc, is_tf_photo ? "TF 卡高保真存储照片" : art->desc);
        lv_obj_set_style_text_font(s_tab_art_desc, font_cn, 0);
    }
    if (s_tab_art_idx_label) {
        lv_label_set_text(s_tab_art_idx_label, idx_str);
    }

    // 2. 实时计算并渲染全屏画廊 240x280 真实像素
    if (s_canvas_buf_fs) {
        if (is_tf_photo) {
            file_reader_decode_jpeg_to_buffer(tf_path, s_canvas_buf_fs, CANVAS_W, CANVAS_H);
        } else {
            render_procedural_artwork(s_canvas_buf_fs, CANVAS_W, CANVAS_H, s_current_index);
        }
        if (s_fs_img_obj) {
            lv_image_set_src(s_fs_img_obj, &s_img_dsc_fs);
        }
    }

    if (s_fs_title_label) {
        lv_label_set_text(s_fs_title_label, is_tf_photo ? tf_path : art->title);
        lv_obj_set_style_text_color(s_fs_title_label, lv_color_hex(art->accent_color), 0);
        lv_obj_set_style_text_font(s_fs_title_label, font_cn, 0);
    }
    if (s_fs_desc_label) {
        lv_label_set_text(s_fs_desc_label, is_tf_photo ? "TF 卡本地相册" : art->desc);
        lv_obj_set_style_text_font(s_fs_desc_label, font_cn, 0);
    }
    if (s_fs_counter_label) {
        lv_label_set_text(s_fs_counter_label, idx_str);
        lv_obj_set_style_text_font(s_fs_counter_label, font_cn, 0);
    }
}

void ui_photo_album_next(void)
{
    int total = s_art_count + (bsp_sdcard_is_mounted() ? file_reader_get_photo_count() : 0);
    if (total <= 0) total = 1;
    s_current_index = (s_current_index + 1) % total;
    update_gallery_display();
}

void ui_photo_album_prev(void)
{
    int total = s_art_count + (bsp_sdcard_is_mounted() ? file_reader_get_photo_count() : 0);
    if (total <= 0) total = 1;
    s_current_index = (s_current_index - 1 + total) % total;
    update_gallery_display();
}

static void on_album_next_btn(lv_event_t *e)
{
    ui_photo_album_next();
}

static void on_album_prev_btn(lv_event_t *e)
{
    ui_photo_album_prev();
}

static void on_slideshow_toggle_btn(lv_event_t *e)
{
    set_slideshow_state(!s_slideshow_running);
}

static void on_close_fullscreen(lv_event_t *e)
{
    set_slideshow_state(false);
    if (s_fs_art_win) {
        lv_obj_add_flag(s_fs_art_win, LV_OBJ_FLAG_HIDDEN);
    }
}

static void on_fs_touch_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s_touch_down_pt);
    } else if (code == LV_EVENT_RELEASED) {
        lv_point_t release_pt;
        lv_indev_get_point(indev, &release_pt);

        int dx = release_pt.x - s_touch_down_pt.x;
        int dy = release_pt.y - s_touch_down_pt.y;

        if (abs(dx) > 35 && abs(dx) > abs(dy)) {
            // 左右轻扫切图
            if (dx < 0) ui_photo_album_next();
            else ui_photo_album_prev();
        } else if (abs(dx) <= 15 && abs(dy) <= 15) {
            // 点击交互
            if (release_pt.x < 70) {
                ui_photo_album_prev(); // 点左侧上一张
            } else if (release_pt.x > 170) {
                ui_photo_album_next(); // 点右侧下一张
            } else {
                // 点击中心 ➔ 切换顶底菜单显隐
                s_fs_bars_visible = !s_fs_bars_visible;
                if (s_fs_floating_header) {
                    if (s_fs_bars_visible) lv_obj_clear_flag(s_fs_floating_header, LV_OBJ_FLAG_HIDDEN);
                    else lv_obj_add_flag(s_fs_floating_header, LV_OBJ_FLAG_HIDDEN);
                }
                if (s_fs_floating_footer) {
                    if (s_fs_bars_visible) lv_obj_clear_flag(s_fs_floating_footer, LV_OBJ_FLAG_HIDDEN);
                    else lv_obj_add_flag(s_fs_floating_footer, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }
}

void ui_photo_album_open_fullscreen(void)
{
    const lv_font_t *font_cn = sys_font_manager_get_font(14);

    if (s_fs_art_win) {
        lv_obj_clear_flag(s_fs_art_win, LV_OBJ_FLAG_HIDDEN);
        update_gallery_display();
        return;
    }

    // 1. 分配全屏 240x280 图像 Buffer 到 PSRAM
    if (!s_canvas_buf_fs) {
        s_canvas_buf_fs = heap_caps_malloc(CANVAS_W * CANVAS_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        if (!s_canvas_buf_fs) {
            s_canvas_buf_fs = malloc(CANVAS_W * CANVAS_H * sizeof(uint16_t));
        }
        s_img_dsc_fs.header.w = CANVAS_W;
        s_img_dsc_fs.header.h = CANVAS_H;
        s_img_dsc_fs.header.cf = LV_COLOR_FORMAT_RGB565;
        s_img_dsc_fs.header.magic = LV_IMAGE_HEADER_MAGIC;
        s_img_dsc_fs.data_size = CANVAS_W * CANVAS_H * sizeof(uint16_t);
        s_img_dsc_fs.data = (const uint8_t *)s_canvas_buf_fs;
    }

    // 2. 240x280 满屏无黑边画廊窗口
    s_fs_art_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_fs_art_win, 240, 280);
    lv_obj_align(s_fs_art_win, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_fs_art_win, lv_color_hex(0x000000), 0);
    lv_obj_set_style_radius(s_fs_art_win, 0, 0);
    lv_obj_set_style_pad_all(s_fs_art_win, 0, 0);
    lv_obj_add_event_cb(s_fs_art_win, on_fs_touch_event, LV_EVENT_ALL, NULL);

    // 满屏真实图像对象 (lv_image)
    s_fs_img_obj = lv_image_create(s_fs_art_win);
    lv_obj_set_size(s_fs_img_obj, 240, 280);
    lv_obj_align(s_fs_img_obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_fs_img_obj, LV_OBJ_FLAG_EVENT_BUBBLE);

    // 浮动顶栏 (半透明毛玻璃)
    s_fs_floating_header = lv_obj_create(s_fs_art_win);
    lv_obj_set_size(s_fs_floating_header, 236, 32);
    lv_obj_align(s_fs_floating_header, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_bg_color(s_fs_floating_header, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_bg_opa(s_fs_floating_header, LV_OPA_80, 0);
    lv_obj_set_style_border_color(s_fs_floating_header, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(s_fs_floating_header, 1, 0);
    lv_obj_set_style_radius(s_fs_floating_header, 8, 0);
    lv_obj_set_style_pad_all(s_fs_floating_header, 2, 0);

    lv_obj_t *btn_back = lv_button_create(s_fs_floating_header);
    lv_obj_set_size(btn_back, 54, 24);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x2563EB), 0);
    lv_obj_add_event_cb(btn_back, on_close_fullscreen, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_bk = lv_label_create(btn_back);
    lv_label_set_text(lbl_bk, LV_SYMBOL_LEFT " 返回");
    lv_obj_set_style_text_font(lbl_bk, font_cn, 0);
    lv_obj_center(lbl_bk);

    s_fs_title_label = lv_label_create(s_fs_floating_header);
    lv_label_set_text(s_fs_title_label, "赛博极夜 · 霓虹网格");
    lv_obj_set_style_text_color(s_fs_title_label, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_font(s_fs_title_label, font_cn, 0);
    lv_obj_align(s_fs_title_label, LV_ALIGN_CENTER, 10, 0);

    // 浮动底栏 (控制面板)
    s_fs_floating_footer = lv_obj_create(s_fs_art_win);
    lv_obj_set_size(s_fs_floating_footer, 236, 36);
    lv_obj_align(s_fs_floating_footer, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(s_fs_floating_footer, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_bg_opa(s_fs_floating_footer, LV_OPA_80, 0);
    lv_obj_set_style_border_color(s_fs_floating_footer, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(s_fs_floating_footer, 1, 0);
    lv_obj_set_style_radius(s_fs_floating_footer, 8, 0);
    lv_obj_set_style_pad_all(s_fs_floating_footer, 2, 0);

    // 上一张
    lv_obj_t *btn_p = lv_button_create(s_fs_floating_footer);
    lv_obj_set_size(btn_p, 34, 26);
    lv_obj_align(btn_p, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_bg_color(btn_p, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(btn_p, on_album_prev_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lp = lv_label_create(btn_p);
    lv_label_set_text(lp, LV_SYMBOL_PREV);
    lv_obj_center(lp);

    // 自动轮播开关
    lv_obj_t *btn_play = lv_button_create(s_fs_floating_footer);
    lv_obj_set_size(btn_play, 64, 26);
    lv_obj_align(btn_play, LV_ALIGN_LEFT_MID, 40, 0);
    lv_obj_set_style_bg_color(btn_play, lv_color_hex(0x0284C7), 0);
    lv_obj_add_event_cb(btn_play, on_slideshow_toggle_btn, LV_EVENT_CLICKED, NULL);
    s_fs_lbl_play = lv_label_create(btn_play);
    lv_label_set_text(s_fs_lbl_play, LV_SYMBOL_PLAY " 轮播");
    lv_obj_set_style_text_font(s_fs_lbl_play, font_cn, 0);
    lv_obj_center(s_fs_lbl_play);

    // 计数指示
    s_fs_counter_label = lv_label_create(s_fs_floating_footer);
    lv_label_set_text(s_fs_counter_label, "1 / 5");
    lv_obj_set_style_text_color(s_fs_counter_label, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(s_fs_counter_label, font_cn, 0);
    lv_obj_align(s_fs_counter_label, LV_ALIGN_CENTER, 36, 0);

    // 下一张
    lv_obj_t *btn_n = lv_button_create(s_fs_floating_footer);
    lv_obj_set_size(btn_n, 34, 26);
    lv_obj_align(btn_n, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_n, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(btn_n, on_album_next_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ln = lv_label_create(btn_n);
    lv_label_set_text(ln, LV_SYMBOL_NEXT);
    lv_obj_center(ln);

    update_gallery_display();
}

static void on_tab_open_fs_clicked(lv_event_t *e)
{
    ui_photo_album_open_fullscreen();
}

void ui_photo_album_init(lv_obj_t *parent_tab)
{
    if (!parent_tab) return;

    const lv_font_t *font_cn = sys_font_manager_get_font(14);

    // 1. 分配 Tab 3 预览画布 Buffer (210x80)
    if (!s_canvas_buf_tab) {
        s_canvas_buf_tab = heap_caps_malloc(PREV_W * PREV_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        if (!s_canvas_buf_tab) {
            s_canvas_buf_tab = malloc(PREV_W * PREV_H * sizeof(uint16_t));
        }
        s_img_dsc_tab.header.w = PREV_W;
        s_img_dsc_tab.header.h = PREV_H;
        s_img_dsc_tab.header.cf = LV_COLOR_FORMAT_RGB565;
        s_img_dsc_tab.header.magic = LV_IMAGE_HEADER_MAGIC;
        s_img_dsc_tab.data_size = PREV_W * PREV_H * sizeof(uint16_t);
        s_img_dsc_tab.data = (const uint8_t *)s_canvas_buf_tab;
    }

    // 2. 顶部标题
    lv_obj_t *title3 = lv_label_create(parent_tab);
    lv_label_set_text(title3, "DIGITAL ART GALLERY");
    lv_obj_set_style_text_color(title3, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(title3, &lv_font_montserrat_14, 0);
    lv_obj_align(title3, LV_ALIGN_TOP_MID, 0, 2);

    // 3. 主画廊预览大卡片 (宽 228, 高 146)
    lv_obj_t *tab_card = lv_obj_create(parent_tab);
    lv_obj_set_size(tab_card, 228, 146);
    lv_obj_align(tab_card, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(tab_card, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_color(tab_card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(tab_card, 1, 0);
    lv_obj_set_style_radius(tab_card, 12, 0);
    lv_obj_set_style_pad_all(tab_card, 8, 0);
    lv_obj_clear_flag(tab_card, LV_OBJ_FLAG_SCROLLABLE);

    // 真正的图片对象 (lv_image)
    s_tab_art_img = lv_image_create(tab_card);
    lv_obj_set_size(s_tab_art_img, PREV_W, PREV_H);
    lv_obj_align(s_tab_art_img, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_color(s_tab_art_img, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_width(s_tab_art_img, 1, 0);
    lv_obj_set_style_radius(s_tab_art_img, 8, 0);

    s_tab_art_title = lv_label_create(tab_card);
    lv_obj_set_width(s_tab_art_title, 140);
    lv_label_set_long_mode(s_tab_art_title, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_tab_art_title, "赛博极夜 · 霓虹网格");
    lv_obj_set_style_text_color(s_tab_art_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_font(s_tab_art_title, font_cn, 0);
    lv_obj_align(s_tab_art_title, LV_ALIGN_TOP_LEFT, 2, 86);

    s_tab_art_desc = lv_label_create(tab_card);
    lv_obj_set_width(s_tab_art_desc, 210);
    lv_label_set_long_mode(s_tab_art_desc, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_tab_art_desc, "240×280 · 透视网格与落日流光");
    lv_obj_set_style_text_color(s_tab_art_desc, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(s_tab_art_desc, font_cn, 0);
    lv_obj_align(s_tab_art_desc, LV_ALIGN_TOP_LEFT, 2, 108);

    s_tab_art_idx_label = lv_label_create(tab_card);
    lv_label_set_text(s_tab_art_idx_label, "1 / 5");
    lv_obj_set_style_text_color(s_tab_art_idx_label, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_text_font(s_tab_art_idx_label, font_cn, 0);
    lv_obj_align(s_tab_art_idx_label, LV_ALIGN_TOP_RIGHT, -2, 86);

    // 4. 底部操作栏：【全屏沉浸画廊】与【下一张】
    lv_obj_t *btn_open_fs = lv_button_create(parent_tab);
    lv_obj_set_size(btn_open_fs, 140, 34);
    lv_obj_align(btn_open_fs, LV_ALIGN_BOTTOM_LEFT, 6, -2);
    lv_obj_set_style_bg_color(btn_open_fs, lv_color_hex(0x0284C7), 0);
    lv_obj_set_style_radius(btn_open_fs, 8, 0);
    lv_obj_clear_flag(btn_open_fs, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_open_fs, on_tab_open_fs_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_ofs = lv_label_create(btn_open_fs);
    lv_label_set_text(lbl_ofs, LV_SYMBOL_IMAGE " 全屏沉浸画廊");
    lv_obj_set_style_text_font(lbl_ofs, font_cn, 0);
    lv_obj_center(lbl_ofs);

    lv_obj_t *btn_tab_next = lv_button_create(parent_tab);
    lv_obj_set_size(btn_tab_next, 80, 34);
    lv_obj_align(btn_tab_next, LV_ALIGN_BOTTOM_RIGHT, -6, -2);
    lv_obj_set_style_bg_color(btn_tab_next, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(btn_tab_next, 8, 0);
    lv_obj_clear_flag(btn_tab_next, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_tab_next, on_album_next_btn, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_tn = lv_label_create(btn_tab_next);
    lv_label_set_text(lbl_tn, "下一张 " LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(lbl_tn, font_cn, 0);
    lv_obj_center(lbl_tn);

    update_gallery_display();
}

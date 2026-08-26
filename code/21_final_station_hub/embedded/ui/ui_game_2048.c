/**
 * @file ui_game_2048.c
 * @brief 经典 2048 触控数字游戏
 *
 * 4×4 方格，手势四向滑动合并数字，目标达到 2048！
 * 使用 LVGL 事件手势 LV_EVENT_GESTURE 驱动。
 * 配色按数字大小自动渐变，视觉效果极佳。
 */
#include "ui_game_2048.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "lvgl.h"
#include "bsp_lvgl_port.h"
#include "sys_font_manager.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "GAME_2048";

// ── 游戏配置 ─────────────────────────────────────────────────────────────────
#define GRID_SIZE    4
#define CELL_SIZE    50     // 每格像素大小
#define CELL_GAP     3      // 格间距
#define BOARD_W      (GRID_SIZE * CELL_SIZE + (GRID_SIZE + 1) * CELL_GAP)  // = 212
#define BOARD_OFFSET_X  14  // 棋盘左边距（居中：(228-212)/2 = 8，加上 parent pad）
#define BOARD_OFFSET_Y  44  // 棋盘顶部（顶栏 44px 高）

// ── 数字对应的颜色 ────────────────────────────────────────────────────────────
static const uint32_t TILE_COLORS[] = {
    0x1E293B, // 0 (空格)
    0xFEF9C3, // 2
    0xFEF08A, // 4
    0xFDE047, // 8
    0xFB923C, // 16
    0xF97316, // 32
    0xEF4444, // 64
    0xDC2626, // 128
    0xC026D3, // 256
    0x9333EA, // 512
    0x6366F1, // 1024
    0x3B82F6, // 2048 (目标!)
};

static const uint32_t TILE_TEXT_COLORS[] = {
    0x64748B, // 0
    0x1E293B, // 2
    0x1E293B, // 4
    0x1E293B, // 8
    0xFFFFFF, // 16
    0xFFFFFF, // 32
    0xFFFFFF, // 64
    0xFFFFFF, // 128
    0xFFFFFF, // 256
    0xFFFFFF, // 512
    0xFFFFFF, // 1024
    0xFFFFFF, // 2048
};

static int color_index(int val)
{
    if (val == 0) return 0;
    int idx = 0;
    while (val > 1) { val >>= 1; idx++; }
    if (idx >= (int)(sizeof(TILE_COLORS) / sizeof(TILE_COLORS[0])))
        idx = (int)(sizeof(TILE_COLORS) / sizeof(TILE_COLORS[0])) - 1;
    return idx;
}

// ── 游戏状态 ─────────────────────────────────────────────────────────────────
static int  s_board[GRID_SIZE][GRID_SIZE];
static int  s_score     = 0;
static int  s_best      = 0;
static bool s_won       = false;
static bool s_lost      = false;

// UI 对象
static lv_obj_t *s_cells[GRID_SIZE][GRID_SIZE];   // 每格容器
static lv_obj_t *s_cell_labels[GRID_SIZE][GRID_SIZE];
static lv_obj_t *s_lbl_score   = NULL;
static lv_obj_t *s_lbl_best    = NULL;
static lv_obj_t *s_lbl_status  = NULL;
static lv_obj_t *s_game_board  = NULL;  // 用于接收手势的容器

/* ── 随机在空格中添加一个 2 或 4 ───────────────────────────────────────────── */
static void add_random_tile(void)
{
    int empties[GRID_SIZE * GRID_SIZE][2];
    int cnt = 0;
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            if (s_board[r][c] == 0) { empties[cnt][0] = r; empties[cnt][1] = c; cnt++; }
    if (cnt == 0) return;
    int idx = esp_random() % cnt;
    s_board[empties[idx][0]][empties[idx][1]] = ((esp_random() % 4) == 0) ? 4 : 2;
}

/* ── 更新所有格子的 UI ─────────────────────────────────────────────────────── */
static void refresh_board_ui(void)
{
    const lv_font_t *font_cn = sys_font_manager_get_font(14);
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            int val = s_board[r][c];
            int ci  = color_index(val);
            lv_obj_set_style_bg_color(s_cells[r][c], lv_color_hex(TILE_COLORS[ci]), 0);
            lv_obj_set_style_bg_opa(s_cells[r][c], LV_OPA_COVER, 0);
            if (val == 0) {
                lv_label_set_text(s_cell_labels[r][c], "");
            } else {
                char buf[12];
                snprintf(buf, sizeof(buf), "%d", val);
                lv_label_set_text(s_cell_labels[r][c], buf);
                lv_obj_set_style_text_color(s_cell_labels[r][c],
                    lv_color_hex(TILE_TEXT_COLORS[ci]), 0);
                // 大数字用更小字体
                if (val >= 1000) {
                    lv_obj_set_style_text_font(s_cell_labels[r][c], font_cn, 0);
                } else if (val >= 100) {
                    lv_obj_set_style_text_font(s_cell_labels[r][c], &lv_font_montserrat_16, 0);
                } else {
                    lv_obj_set_style_text_font(s_cell_labels[r][c], &lv_font_montserrat_20, 0);
                }
            }
        }
    }
    // 刷新分数
    char sbuf[32];
    snprintf(sbuf, sizeof(sbuf), "得分: %d", s_score);
    lv_label_set_text(s_lbl_score, sbuf);
    if (s_score > s_best) s_best = s_score;
    snprintf(sbuf, sizeof(sbuf), "最高: %d", s_best);
    lv_label_set_text(s_lbl_best, sbuf);
}

/* ── 游戏逻辑：向左移动并合并（其他方向通过旋转矩阵实现） ─────────────────── */
static bool move_left(void)
{
    bool changed = false;
    for (int r = 0; r < GRID_SIZE; r++) {
        int row[GRID_SIZE] = {0};
        int pos = 0;
        // 压缩（去零）
        for (int c = 0; c < GRID_SIZE; c++)
            if (s_board[r][c]) row[pos++] = s_board[r][c];
        // 合并
        for (int i = 0; i < pos - 1; i++) {
            if (row[i] == row[i + 1]) {
                row[i] *= 2;
                s_score += row[i];
                if (row[i] == 2048) s_won = true;
                for (int j = i + 1; j < pos - 1; j++) row[j] = row[j + 1];
                row[pos - 1] = 0;
                pos--;
            }
        }
        // 写回
        for (int c = 0; c < GRID_SIZE; c++) {
            if (s_board[r][c] != row[c]) changed = true;
            s_board[r][c] = row[c];
        }
    }
    return changed;
}

static void rotate_cw(void)   // 顺时针旋转 90°
{
    int tmp[GRID_SIZE][GRID_SIZE];
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++)
            tmp[c][GRID_SIZE - 1 - r] = s_board[r][c];
    memcpy(s_board, tmp, sizeof(s_board));
}

static void rotate_ccw(void)  // 逆时针旋转 90°
{
    rotate_cw(); rotate_cw(); rotate_cw();
}

static bool check_lost(void)
{
    for (int r = 0; r < GRID_SIZE; r++)
        for (int c = 0; c < GRID_SIZE; c++) {
            if (s_board[r][c] == 0) return false;
            if (r < GRID_SIZE - 1 && s_board[r][c] == s_board[r + 1][c]) return false;
            if (c < GRID_SIZE - 1 && s_board[r][c] == s_board[r][c + 1]) return false;
        }
    return true;
}

/* ── 执行一步移动 ─────────────────────────────────────────────────────────── */
static void do_move(lv_dir_t dir)
{
    if (s_lost) return;
    bool moved = false;
    switch (dir) {
        case LV_DIR_LEFT:  moved = move_left(); break;
        case LV_DIR_RIGHT: rotate_cw(); rotate_cw(); moved = move_left();
                           rotate_cw(); rotate_cw(); break;
        case LV_DIR_TOP:   rotate_ccw(); moved = move_left(); rotate_cw(); break;
        case LV_DIR_BOTTOM: rotate_cw(); moved = move_left(); rotate_ccw(); break;
        default: break;
    }
    if (moved) {
        add_random_tile();
        if (check_lost()) {
            s_lost = true;
            const lv_font_t *font_cn = sys_font_manager_get_font(14);
            lv_label_set_text(s_lbl_status, "游戏结束！点右上角重开");
            lv_obj_set_style_text_font(s_lbl_status, font_cn, 0);
            lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0xEF4444), 0);
        } else if (s_won) {
            const lv_font_t *font_cn = sys_font_manager_get_font(14);
            lv_label_set_text(s_lbl_status, "达到 2048！你赢了！");
            lv_obj_set_style_text_font(s_lbl_status, font_cn, 0);
            lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0xFBBF24), 0);
        }
        refresh_board_ui();
    }
}

/* ── 初始化新游戏 ─────────────────────────────────────────────────────────── */
static void new_game(void)
{
    memset(s_board, 0, sizeof(s_board));
    s_score = 0;
    s_won   = false;
    s_lost  = false;
    add_random_tile();
    add_random_tile();
    refresh_board_ui();
    const lv_font_t *font_cn = sys_font_manager_get_font(14);
    lv_label_set_text(s_lbl_status, "滑动合并数字，目标 2048！");
    lv_obj_set_style_text_font(s_lbl_status, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0x94A3B8), 0);
}

/* ── 手势事件回调 ─────────────────────────────────────────────────────────── */
static void on_gesture(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    do_move(dir);
}

/* ── 重新开始按钮 ─────────────────────────────────────────────────────────── */
static void on_restart(lv_event_t *e)
{
    new_game();
}

/* ── 公共接口：初始化 UI ─────────────────────────────────────────────────── */
void ui_game_2048_init(lv_obj_t *parent_tab)
{
    if (!parent_tab) return;
    const lv_font_t *font_cn = sys_font_manager_get_font(14);

    /* ── 顶栏（标题 + 分数 + 最高分 + 重开按钮） ────────────────────────── */
    lv_obj_t *top_bar = lv_obj_create(parent_tab);
    lv_obj_set_size(top_bar, 228, 40);
    lv_obj_set_pos(top_bar, 6, 2);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x0A1220), 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 6, 0);
    lv_obj_set_style_pad_all(top_bar, 3, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_title = lv_label_create(top_bar);
    lv_label_set_text(lbl_title, "2048");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFBBF24), 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 2, 0);

    s_lbl_score = lv_label_create(top_bar);
    lv_label_set_text(s_lbl_score, "得分: 0");
    lv_obj_set_style_text_font(s_lbl_score, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_score, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_lbl_score, LV_ALIGN_CENTER, 0, -6);

    s_lbl_best = lv_label_create(top_bar);
    lv_label_set_text(s_lbl_best, "最高: 0");
    lv_obj_set_style_text_font(s_lbl_best, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_best, lv_color_hex(0x64748B), 0);
    lv_obj_align(s_lbl_best, LV_ALIGN_CENTER, 0, 6);

    lv_obj_t *btn_new = lv_button_create(top_bar);
    lv_obj_set_size(btn_new, 42, 30);
    lv_obj_align(btn_new, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_new, lv_color_hex(0xD97706), 0);
    lv_obj_set_style_radius(btn_new, 6, 0);
    lv_obj_add_event_cb(btn_new, on_restart, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_new = lv_label_create(btn_new);
    lv_label_set_text(lbl_new, "重开");
    lv_obj_set_style_text_font(lbl_new, font_cn, 0);
    lv_obj_center(lbl_new);

    /* ── 棋盘容器（接收手势） ────────────────────────────────────────────── */
    s_game_board = lv_obj_create(parent_tab);
    lv_obj_set_size(s_game_board, BOARD_W + CELL_GAP * 2, BOARD_W + CELL_GAP * 2);
    lv_obj_set_pos(s_game_board, (240 - BOARD_W) / 2 - 8, BOARD_OFFSET_Y);
    lv_obj_set_style_bg_color(s_game_board, lv_color_hex(0x0B1829), 0);
    lv_obj_set_style_border_color(s_game_board, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(s_game_board, 1, 0);
    lv_obj_set_style_radius(s_game_board, 8, 0);
    lv_obj_set_style_pad_all(s_game_board, CELL_GAP, 0);
    lv_obj_clear_flag(s_game_board, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_game_board, LV_OBJ_FLAG_GESTURE_BUBBLE);  // 手势冒泡到 tab
    lv_obj_add_event_cb(s_game_board, on_gesture, LV_EVENT_GESTURE, NULL);

    /* ── 创建 4×4 格子 ───────────────────────────────────────────────────── */
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            lv_obj_t *cell = lv_obj_create(s_game_board);
            lv_obj_set_size(cell, CELL_SIZE, CELL_SIZE);
            lv_obj_set_pos(cell, CELL_GAP + c * (CELL_SIZE + CELL_GAP),
                                 CELL_GAP + r * (CELL_SIZE + CELL_GAP));
            lv_obj_set_style_bg_color(cell, lv_color_hex(0x1E293B), 0);
            lv_obj_set_style_border_width(cell, 0, 0);
            lv_obj_set_style_radius(cell, 6, 0);
            lv_obj_set_style_pad_all(cell, 0, 0);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
            s_cells[r][c] = cell;

            lv_obj_t *lbl = lv_label_create(cell);
            lv_label_set_text(lbl, "");
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
            lv_obj_center(lbl);
            s_cell_labels[r][c] = lbl;
        }
    }

    /* ── 底部状态提示文字 ─────────────────────────────────────────────────── */
    s_lbl_status = lv_label_create(parent_tab);
    lv_obj_set_width(s_lbl_status, 228);
    lv_label_set_long_mode(s_lbl_status, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_lbl_status, "滑动合并数字，目标 2048！");
    lv_obj_set_style_text_font(s_lbl_status, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_lbl_status, LV_ALIGN_BOTTOM_MID, 0, -2);

    /* ── 启动新游戏 ──────────────────────────────────────────────────────── */
    new_game();
    ESP_LOGI(TAG, "2048 游戏 UI 初始化完毕");
}

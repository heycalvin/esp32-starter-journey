/**
 * @file ui_game_2048.c
 * @brief 经典 2048 触控数字游戏：动画、结算与本地 Top 5 排行榜
 */
#include "ui_game_2048.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lvgl.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "sys_font_manager.h"

static const char *TAG = "GAME_2048";

#define GRID_SIZE           4
#define CELL_SIZE           50
#define CELL_GAP            3
#define BOARD_W             (GRID_SIZE * CELL_SIZE + (GRID_SIZE + 1) * CELL_GAP)
#define BOARD_OFFSET_Y      44
#define TILE_MOVE_DURATION  140
#define TILE_POP_DURATION   110
#define SWIPE_MIN_DISTANCE  18
#define TOP_SCORE_COUNT     5
#define GAME_NVS_NAMESPACE  "game2048"

static const uint32_t TILE_COLORS[] = {
    0x1E293B, 0xFEF9C3, 0xFEF08A, 0xFDE047, 0xFB923C, 0xF97316,
    0xEF4444, 0xDC2626, 0xC026D3, 0x9333EA, 0x6366F1, 0x3B82F6,
};
static const uint32_t TILE_TEXT_COLORS[] = {
    0x64748B, 0x1E293B, 0x1E293B, 0x1E293B, 0xFFFFFF, 0xFFFFFF,
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
};

typedef struct {
    uint8_t src_row;
    uint8_t src_col;
    uint8_t dst_row;
    uint8_t dst_col;
    int value;
} tile_motion_t;

static int s_board[GRID_SIZE][GRID_SIZE];
static int s_pending_board[GRID_SIZE][GRID_SIZE];
static int s_score;
static int s_pending_score;
static int s_best;
static int32_t s_top_scores[TOP_SCORE_COUNT];
static bool s_won;
static bool s_pending_won;
static bool s_lost;
static bool s_pending_lost;
static bool s_animating;
static int s_spawn_row = -1;
static int s_spawn_col = -1;
static tile_motion_t s_turn_motions[GRID_SIZE * GRID_SIZE];
static int s_turn_motion_count;
static lv_point_t s_swipe_start;
static bool s_swipe_tracking;

static lv_obj_t *s_game_root;
static lv_obj_t *s_game_board;
static lv_obj_t *s_anim_layer;
static lv_obj_t *s_modal_overlay;
static lv_obj_t *s_cells[GRID_SIZE][GRID_SIZE];
static lv_obj_t *s_cell_labels[GRID_SIZE][GRID_SIZE];
static lv_obj_t *s_lbl_score;
static lv_obj_t *s_lbl_best;
static lv_obj_t *s_lbl_status;

static void show_win_modal(void);
static void show_loss_modal(void);
static void show_leaderboard_modal(void);

static int color_index(int val)
{
    int idx = 0;
    while (val > 1) {
        val >>= 1;
        idx++;
    }
    int max = (int)(sizeof(TILE_COLORS) / sizeof(TILE_COLORS[0])) - 1;
    return idx > max ? max : idx;
}

static void cell_position(int row, int col, int *x, int *y)
{
    *x = CELL_GAP + col * (CELL_SIZE + CELL_GAP);
    *y = CELL_GAP + row * (CELL_SIZE + CELL_GAP);
}

static void set_status(const char *text, uint32_t color)
{
    if (!s_lbl_status) return;
    lv_label_set_text(s_lbl_status, text);
    lv_obj_set_style_text_font(s_lbl_status, sys_font_manager_get_font(14), 0);
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(color), 0);
}

static void update_score_labels(void)
{
    if (s_score > s_best) s_best = s_score;
    if (s_top_scores[0] > s_best) s_best = s_top_scores[0];

    char buf[32];
    snprintf(buf, sizeof(buf), "得分: %d", s_score);
    lv_label_set_text(s_lbl_score, buf);
    snprintf(buf, sizeof(buf), "最高: %d", s_best);
    lv_label_set_text(s_lbl_best, buf);
}

static void render_board(const int board[GRID_SIZE][GRID_SIZE])
{
    const lv_font_t *font_cn = sys_font_manager_get_font(14);
    for (int row = 0; row < GRID_SIZE; row++) {
        for (int col = 0; col < GRID_SIZE; col++) {
            int value = board[row][col];
            int ci = color_index(value);
            lv_obj_set_style_bg_color(s_cells[row][col], lv_color_hex(TILE_COLORS[ci]), 0);
            if (value == 0) {
                lv_label_set_text(s_cell_labels[row][col], "");
                continue;
            }

            char text[12];
            snprintf(text, sizeof(text), "%d", value);
            lv_label_set_text(s_cell_labels[row][col], text);
            lv_obj_set_style_text_color(s_cell_labels[row][col],
                                        lv_color_hex(TILE_TEXT_COLORS[ci]), 0);
            lv_obj_set_style_text_font(s_cell_labels[row][col],
                value >= 1000 ? font_cn : value >= 100 ? &lv_font_montserrat_16 : &lv_font_montserrat_20, 0);
        }
    }
}

static void refresh_board_ui(void)
{
    render_board(s_board);
    update_score_labels();
}

static bool add_random_tile(int board[GRID_SIZE][GRID_SIZE], int *out_row, int *out_col)
{
    int empty[GRID_SIZE * GRID_SIZE][2];
    int count = 0;
    for (int row = 0; row < GRID_SIZE; row++) {
        for (int col = 0; col < GRID_SIZE; col++) {
            if (board[row][col] == 0) {
                empty[count][0] = row;
                empty[count][1] = col;
                count++;
            }
        }
    }
    if (count == 0) return false;

    int index = esp_random() % count;
    int row = empty[index][0];
    int col = empty[index][1];
    board[row][col] = (esp_random() % 4 == 0) ? 4 : 2;
    if (out_row) *out_row = row;
    if (out_col) *out_col = col;
    return true;
}

static bool board_is_lost(const int board[GRID_SIZE][GRID_SIZE])
{
    for (int row = 0; row < GRID_SIZE; row++) {
        for (int col = 0; col < GRID_SIZE; col++) {
            if (board[row][col] == 0) return false;
            if (row + 1 < GRID_SIZE && board[row][col] == board[row + 1][col]) return false;
            if (col + 1 < GRID_SIZE && board[row][col] == board[row][col + 1]) return false;
        }
    }
    return true;
}

/* 以目标方向为 index=0，统一生成四条待移动的线。 */
static void line_coord(lv_dir_t dir, int line, int index, int *row, int *col)
{
    switch (dir) {
        case LV_DIR_LEFT:   *row = line; *col = index; break;
        case LV_DIR_RIGHT:  *row = line; *col = GRID_SIZE - 1 - index; break;
        case LV_DIR_TOP:    *row = index; *col = line; break;
        case LV_DIR_BOTTOM: *row = GRID_SIZE - 1 - index; *col = line; break;
        default:             *row = 0; *col = 0; break;
    }
}

static void add_motion(int src_row, int src_col, int dst_row, int dst_col, int value)
{
    if (s_turn_motion_count >= GRID_SIZE * GRID_SIZE) return;
    s_turn_motions[s_turn_motion_count++] = (tile_motion_t) {
        .src_row = (uint8_t)src_row, .src_col = (uint8_t)src_col,
        .dst_row = (uint8_t)dst_row, .dst_col = (uint8_t)dst_col, .value = value,
    };
}

/* 计算目标棋盘及每块数字的源/终点，逻辑与动画使用同一份结果。 */
static bool prepare_turn(lv_dir_t dir)
{
    int next[GRID_SIZE][GRID_SIZE] = {{0}};
    int score_gain = 0;
    bool reached_2048 = false;
    s_turn_motion_count = 0;

    for (int line = 0; line < GRID_SIZE; line++) {
        int values[GRID_SIZE];
        int src_rows[GRID_SIZE];
        int src_cols[GRID_SIZE];
        int count = 0;

        for (int index = 0; index < GRID_SIZE; index++) {
            int row, col;
            line_coord(dir, line, index, &row, &col);
            if (s_board[row][col] != 0) {
                values[count] = s_board[row][col];
                src_rows[count] = row;
                src_cols[count] = col;
                count++;
            }
        }

        int dst_index = 0;
        for (int index = 0; index < count;) {
            int dst_row, dst_col;
            line_coord(dir, line, dst_index, &dst_row, &dst_col);
            if (index + 1 < count && values[index] == values[index + 1]) {
                int merged = values[index] * 2;
                next[dst_row][dst_col] = merged;
                add_motion(src_rows[index], src_cols[index], dst_row, dst_col, values[index]);
                add_motion(src_rows[index + 1], src_cols[index + 1], dst_row, dst_col, values[index + 1]);
                score_gain += merged;
                if (merged >= 2048) reached_2048 = true;
                index += 2;
            } else {
                next[dst_row][dst_col] = values[index];
                add_motion(src_rows[index], src_cols[index], dst_row, dst_col, values[index]);
                index++;
            }
            dst_index++;
        }
    }

    if (memcmp(s_board, next, sizeof(s_board)) == 0) return false;

    memcpy(s_pending_board, next, sizeof(s_pending_board));
    s_pending_score = s_score + score_gain;
    s_pending_won = s_won || reached_2048;
    s_spawn_row = -1;
    s_spawn_col = -1;
    add_random_tile(s_pending_board, &s_spawn_row, &s_spawn_col);
    s_pending_lost = board_is_lost(s_pending_board);
    return true;
}

static void leaderboard_load(void)
{
    memset(s_top_scores, 0, sizeof(s_top_scores));
    nvs_handle_t handle;
    if (nvs_open(GAME_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;

    for (int i = 0; i < TOP_SCORE_COUNT; i++) {
        char key[8];
        snprintf(key, sizeof(key), "top%d", i);
        nvs_get_i32(handle, key, &s_top_scores[i]);
    }
    nvs_close(handle);
    s_best = s_top_scores[0];
}

static void leaderboard_save(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(GAME_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "排行榜保存失败: %s", esp_err_to_name(err));
        return;
    }
    for (int i = 0; i < TOP_SCORE_COUNT; i++) {
        char key[8];
        snprintf(key, sizeof(key), "top%d", i);
        nvs_set_i32(handle, key, s_top_scores[i]);
    }
    nvs_commit(handle);
    nvs_close(handle);
}

static void leaderboard_insert(int score)
{
    for (int i = 0; i < TOP_SCORE_COUNT; i++) {
        if (score <= s_top_scores[i]) continue;
        for (int j = TOP_SCORE_COUNT - 1; j > i; j--) s_top_scores[j] = s_top_scores[j - 1];
        s_top_scores[i] = score;
        leaderboard_save();
        break;
    }
    if (score > s_best) s_best = score;
}

static void anim_x_cb(void *obj, int32_t value)
{
    lv_obj_set_x((lv_obj_t *)obj, value);
}

static void anim_y_cb(void *obj, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)obj, value);
}

static void anim_scale_cb(void *obj, int32_t value)
{
    lv_obj_set_style_transform_scale((lv_obj_t *)obj, value, 0);
}

static void anim_dummy_cb(void *obj, int32_t value)
{
    LV_UNUSED(obj);
    LV_UNUSED(value);
}

static void animate_tile_pop(lv_obj_t *tile)
{
    lv_obj_set_style_transform_pivot_x(tile, CELL_SIZE / 2, 0);
    lv_obj_set_style_transform_pivot_y(tile, CELL_SIZE / 2, 0);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, tile);
    lv_anim_set_exec_cb(&anim, anim_scale_cb);
    lv_anim_set_values(&anim, 190, 256);
    lv_anim_set_duration(&anim, TILE_POP_DURATION);
    lv_anim_set_path_cb(&anim, lv_anim_path_overshoot);
    lv_anim_start(&anim);
}

static lv_obj_t *create_animated_tile(int value, int row, int col)
{
    int x, y;
    cell_position(row, col, &x, &y);
    int ci = color_index(value);
    lv_obj_t *tile = lv_obj_create(s_anim_layer);
    lv_obj_set_size(tile, CELL_SIZE, CELL_SIZE);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_style_bg_color(tile, lv_color_hex(TILE_COLORS[ci]), 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_radius(tile, 6, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    char text[12];
    snprintf(text, sizeof(text), "%d", value);
    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(TILE_TEXT_COLORS[ci]), 0);
    lv_obj_set_style_text_font(label,
        value >= 1000 ? sys_font_manager_get_font(14) : value >= 100 ? &lv_font_montserrat_16 : &lv_font_montserrat_20, 0);
    lv_obj_center(label);
    return tile;
}

static void animate_tile_to(lv_obj_t *tile, int dst_row, int dst_col)
{
    int dst_x, dst_y;
    cell_position(dst_row, dst_col, &dst_x, &dst_y);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, tile);
    lv_anim_set_exec_cb(&anim, anim_x_cb);
    lv_anim_set_values(&anim, lv_obj_get_x(tile), dst_x);
    lv_anim_set_duration(&anim, TILE_MOVE_DURATION);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);

    lv_anim_set_exec_cb(&anim, anim_y_cb);
    lv_anim_set_values(&anim, lv_obj_get_y(tile), dst_y);
    lv_anim_start(&anim);
}

static void close_modal(void)
{
    if (s_modal_overlay) lv_obj_delete(s_modal_overlay);
    s_modal_overlay = NULL;
}

static void on_turn_animation_done(lv_anim_t *anim)
{
    LV_UNUSED(anim);
    if (s_anim_layer) lv_obj_delete(s_anim_layer);
    s_anim_layer = NULL;

    bool just_won = !s_won && s_pending_won;
    memcpy(s_board, s_pending_board, sizeof(s_board));
    s_score = s_pending_score;
    s_won = s_pending_won;
    s_lost = s_pending_lost;
    refresh_board_ui();

    if (s_spawn_row >= 0) animate_tile_pop(s_cells[s_spawn_row][s_spawn_col]);
    s_animating = false;

    if (s_lost) {
        leaderboard_insert(s_score);
        update_score_labels();
        set_status("本局结束，成绩已保存", 0xEF4444);
        show_loss_modal();
    } else if (just_won) {
        set_status("达到 2048，闯关成功！", 0xFBBF24);
        show_win_modal();
    }
}

static void start_turn_animation(void)
{
    static const int empty_board[GRID_SIZE][GRID_SIZE] = {{0}};
    s_animating = true;
    render_board(empty_board);

    s_anim_layer = lv_obj_create(s_game_board);
    lv_obj_set_size(s_anim_layer, BOARD_W, BOARD_W);
    lv_obj_set_pos(s_anim_layer, 0, 0);
    lv_obj_set_style_bg_opa(s_anim_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_anim_layer, 0, 0);
    lv_obj_set_style_pad_all(s_anim_layer, 0, 0);
    lv_obj_clear_flag(s_anim_layer, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < s_turn_motion_count; i++) {
        tile_motion_t *motion = &s_turn_motions[i];
        lv_obj_t *tile = create_animated_tile(motion->value, motion->src_row, motion->src_col);
        animate_tile_to(tile, motion->dst_row, motion->dst_col);
    }

    lv_anim_t finish;
    lv_anim_init(&finish);
    lv_anim_set_var(&finish, s_game_board);
    lv_anim_set_exec_cb(&finish, anim_dummy_cb);
    lv_anim_set_values(&finish, 0, 1);
    lv_anim_set_duration(&finish, TILE_MOVE_DURATION);
    lv_anim_set_completed_cb(&finish, on_turn_animation_done);
    lv_anim_start(&finish);
}

static void modal_button(lv_obj_t *panel, const char *text, int x, int y, int width,
                         uint32_t color, lv_event_cb_t callback)
{
    lv_obj_t *button = lv_button_create(panel);
    lv_obj_set_size(button, width, 30);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
    lv_obj_set_style_radius(button, 7, 0);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, sys_font_manager_get_font(14), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(label);
}

static lv_obj_t *create_modal_panel(int height)
{
    close_modal();
    s_modal_overlay = lv_obj_create(s_game_root);
    lv_obj_set_size(s_modal_overlay, 240, 280);
    lv_obj_set_pos(s_modal_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_modal_overlay, lv_color_hex(0x020617), 0);
    lv_obj_set_style_bg_opa(s_modal_overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(s_modal_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_modal_overlay, 0, 0);
    lv_obj_clear_flag(s_modal_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_modal_overlay, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *panel = lv_obj_create(s_modal_overlay);
    lv_obj_set_size(panel, 216, height);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x0F1B2D), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

static void modal_title(lv_obj_t *panel, const char *title, uint32_t color)
{
    lv_obj_t *label = lv_label_create(panel);
    lv_label_set_text(label, title);
    /* Montserrat 仅有拉丁字符；结算标题必须走项目中文字体，否则会显示方块。 */
    lv_obj_set_style_text_font(label, sys_font_manager_get_font(14), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 4);
}

static void on_continue_clicked(lv_event_t *event)
{
    LV_UNUSED(event);
    close_modal();
    set_status("继续挑战，冲击更高分！", 0x94A3B8);
}

static void new_game(void)
{
    close_modal();
    memset(s_board, 0, sizeof(s_board));
    s_score = 0;
    s_won = false;
    s_lost = false;
    s_spawn_row = -1;
    s_spawn_col = -1;
    add_random_tile(s_board, NULL, NULL);
    add_random_tile(s_board, NULL, NULL);
    refresh_board_ui();
    set_status("滑动合并数字，目标 2048！", 0x94A3B8);
}

static void on_restart_clicked(lv_event_t *event)
{
    LV_UNUSED(event);
    if (!s_animating) new_game();
}

static void on_rank_close_clicked(lv_event_t *event)
{
    LV_UNUSED(event);
    close_modal();
}

static void on_rank_clicked(lv_event_t *event)
{
    LV_UNUSED(event);
    show_leaderboard_modal();
}

static void show_win_modal(void)
{
    lv_obj_t *panel = create_modal_panel(142);
    modal_title(panel, "闯关成功!", 0xFBBF24);
    lv_obj_t *message = lv_label_create(panel);
    lv_label_set_text(message, "你合成了 2048");
    lv_obj_set_style_text_font(message, sys_font_manager_get_font(14), 0);
    lv_obj_set_style_text_color(message, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(message, LV_ALIGN_TOP_MID, 0, 38);
    modal_button(panel, "继续挑战", 10, 92, 92, 0x0E7490, on_continue_clicked);
    modal_button(panel, "重新开始", 114, 92, 92, 0xD97706, on_restart_clicked);
}

static void show_loss_modal(void)
{
    lv_obj_t *panel = create_modal_panel(154);
    modal_title(panel, "本局结束", 0xF87171);
    char score[32];
    snprintf(score, sizeof(score), "本局得分: %d", s_score);
    lv_obj_t *message = lv_label_create(panel);
    lv_label_set_text(message, score);
    lv_obj_set_style_text_font(message, sys_font_manager_get_font(14), 0);
    lv_obj_set_style_text_color(message, lv_color_hex(0xCBD5E1), 0);
    lv_obj_align(message, LV_ALIGN_TOP_MID, 0, 38);
    lv_obj_t *saved = lv_label_create(panel);
    lv_label_set_text(saved, "成绩已写入本地 Top 5");
    lv_obj_set_style_text_font(saved, sys_font_manager_get_font(14), 0);
    lv_obj_set_style_text_color(saved, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(saved, LV_ALIGN_TOP_MID, 0, 60);
    modal_button(panel, "排行榜", 10, 104, 92, 0x0E7490, on_rank_clicked);
    modal_button(panel, "再来一局", 114, 104, 92, 0xD97706, on_restart_clicked);
}

static void show_leaderboard_modal(void)
{
    lv_obj_t *panel = create_modal_panel(222);
    modal_title(panel, "Top 5", 0x38BDF8);
    for (int i = 0; i < TOP_SCORE_COUNT; i++) {
        char line[32];
        snprintf(line, sizeof(line), "%d.  %ld", i + 1, (long)s_top_scores[i]);
        lv_obj_t *label = lv_label_create(panel);
        lv_label_set_text(label, line);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(i == 0 ? 0xFBBF24 : 0xCBD5E1), 0);
        lv_obj_set_pos(label, 34, 40 + i * 25);
    }
    modal_button(panel, "返回", 62, 178, 92, 0x334155, on_rank_close_clicked);
}

static void do_move(lv_dir_t dir)
{
    if (s_animating || s_lost || s_modal_overlay) return;
    if (prepare_turn(dir)) {
        start_turn_animation();
    } else if (board_is_lost(s_board)) {
        s_lost = true;
        leaderboard_insert(s_score);
        update_score_labels();
        set_status("本局结束，成绩已保存", 0xEF4444);
        show_loss_modal();
    }
}

static void on_board_touch(lv_event_t *event)
{
    if (s_animating || s_modal_overlay) return;
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    switch (lv_event_get_code(event)) {
        case LV_EVENT_PRESSED:
            s_swipe_start = point;
            s_swipe_tracking = true;
            break;
        case LV_EVENT_RELEASED: {
            if (!s_swipe_tracking) break;
            s_swipe_tracking = false;
            int dx = point.x - s_swipe_start.x;
            int dy = point.y - s_swipe_start.y;
            if (abs(dx) < SWIPE_MIN_DISTANCE && abs(dy) < SWIPE_MIN_DISTANCE) break;
            if (abs(dx) >= abs(dy)) do_move(dx > 0 ? LV_DIR_RIGHT : LV_DIR_LEFT);
            else do_move(dy > 0 ? LV_DIR_BOTTOM : LV_DIR_TOP);
            break;
        }
        case LV_EVENT_PRESS_LOST:
            s_swipe_tracking = false;
            break;
        default:
            break;
    }
}

void ui_game_2048_init(lv_obj_t *parent_tab)
{
    if (!parent_tab) return;
    const lv_font_t *font_cn = sys_font_manager_get_font(14);
    s_game_root = parent_tab;
    leaderboard_load();

    lv_obj_t *top_bar = lv_obj_create(parent_tab);
    lv_obj_set_size(top_bar, 228, 40);
    lv_obj_set_pos(top_bar, 6, 2);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x0A1220), 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 6, 0);
    lv_obj_set_style_pad_all(top_bar, 3, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(top_bar);
    lv_label_set_text(title, "2048");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFBBF24), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 2, 0);

    s_lbl_score = lv_label_create(top_bar);
    lv_obj_set_style_text_font(s_lbl_score, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_score, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_lbl_score, LV_ALIGN_CENTER, 0, -6);
    s_lbl_best = lv_label_create(top_bar);
    lv_obj_set_style_text_font(s_lbl_best, font_cn, 0);
    lv_obj_set_style_text_color(s_lbl_best, lv_color_hex(0x64748B), 0);
    lv_obj_align(s_lbl_best, LV_ALIGN_CENTER, 0, 6);

    lv_obj_t *restart = lv_button_create(top_bar);
    lv_obj_set_size(restart, 42, 30);
    lv_obj_align(restart, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(restart, lv_color_hex(0xD97706), 0);
    lv_obj_set_style_radius(restart, 6, 0);
    lv_obj_add_event_cb(restart, on_restart_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *restart_label = lv_label_create(restart);
    lv_label_set_text(restart_label, "重开");
    lv_obj_set_style_text_font(restart_label, font_cn, 0);
    lv_obj_center(restart_label);

    s_game_board = lv_obj_create(parent_tab);
    lv_obj_set_size(s_game_board, BOARD_W + CELL_GAP * 2, BOARD_W + CELL_GAP * 2);
    lv_obj_set_pos(s_game_board, (240 - BOARD_W) / 2 - 8, BOARD_OFFSET_Y);
    lv_obj_set_style_bg_color(s_game_board, lv_color_hex(0x0B1829), 0);
    lv_obj_set_style_border_color(s_game_board, lv_color_hex(0x1E3A5F), 0);
    lv_obj_set_style_border_width(s_game_board, 1, 0);
    lv_obj_set_style_radius(s_game_board, 8, 0);
    lv_obj_set_style_pad_all(s_game_board, CELL_GAP, 0);
    lv_obj_clear_flag(s_game_board, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_game_board, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(s_game_board, on_board_touch, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_game_board, on_board_touch, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_game_board, on_board_touch, LV_EVENT_PRESS_LOST, NULL);

    for (int row = 0; row < GRID_SIZE; row++) {
        for (int col = 0; col < GRID_SIZE; col++) {
            int x, y;
            cell_position(row, col, &x, &y);
            lv_obj_t *cell = lv_obj_create(s_game_board);
            lv_obj_set_size(cell, CELL_SIZE, CELL_SIZE);
            lv_obj_set_pos(cell, x, y);
            lv_obj_set_style_bg_color(cell, lv_color_hex(TILE_COLORS[0]), 0);
            lv_obj_set_style_border_width(cell, 0, 0);
            lv_obj_set_style_radius(cell, 6, 0);
            lv_obj_set_style_pad_all(cell, 0, 0);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
            s_cells[row][col] = cell;
            s_cell_labels[row][col] = lv_label_create(cell);
            lv_obj_center(s_cell_labels[row][col]);
        }
    }

    s_lbl_status = lv_label_create(parent_tab);
    lv_obj_set_width(s_lbl_status, 228);
    lv_label_set_long_mode(s_lbl_status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(s_lbl_status, font_cn, 0);
    lv_obj_align(s_lbl_status, LV_ALIGN_BOTTOM_MID, 0, -2);

    new_game();
    ESP_LOGI(TAG, "2048 游戏 UI 初始化完成，Top 5 已加载");
}

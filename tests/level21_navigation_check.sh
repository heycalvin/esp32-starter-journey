#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HUB_FILE="$ROOT_DIR/main/ui/ui_hub.c"
HUB_HEADER="$ROOT_DIR/main/ui/ui_hub.h"
GAME_FILE="$ROOT_DIR/main/ui/ui_game_2048.c"
BUTTON_FILE="$ROOT_DIR/main/app/app_business.c"

failures=0

require_text() {
    local file="$1"
    local pattern="$2"
    local description="$3"
    if ! rg -q --fixed-strings "$pattern" "$file"; then
        echo "FAIL: $description"
        failures=$((failures + 1))
    fi
}

forbid_text() {
    local file="$1"
    local pattern="$2"
    local description="$3"
    if rg -q --fixed-strings "$pattern" "$file"; then
        echo "FAIL: $description"
        failures=$((failures + 1))
    fi
}

tile_count="$(rg -c "lv_tileview_add_tile" "$HUB_FILE" || true)"
if [[ "$tile_count" != "5" ]]; then
    echo "FAIL: expected exactly 5 TileView pages, found $tile_count"
    failures=$((failures + 1))
fi

require_text "$HUB_FILE" "s_app_layer" "UI app layer is missing"
require_text "$HUB_FILE" "ui_hub_handle_sw3_short_press" "SW3 short-press UI navigation is missing"
require_text "$HUB_HEADER" "void ui_hub_handle_sw3_short_press(void);" "SW3 navigation API is not public"
require_text "$BUTTON_FILE" "button_navigation_task" "SW3 is not handled by a dedicated task"
require_text "$BUTTON_FILE" "raw_pressed != stable_pressed" "SW3 debounce does not compare raw and stable states"
require_text "$HUB_FILE" "s_label_wifi_icon" "home title Wi-Fi icon was removed"
require_text "$HUB_FILE" "s_label_top_net" "home title network status was removed"
require_text "$HUB_FILE" "s_label_top_sd" "home title SD status was removed"
require_text "$HUB_FILE" "s_label_top_heap" "home title heap status was removed"
require_text "$HUB_FILE" "lv_obj_set_size(bento_card1, 111, 106);" "home environment card changed size unexpectedly"
require_text "$HUB_FILE" "lv_obj_set_size(bento_card2, 111, 106);" "home lighting card changed size unexpectedly"
require_text "$HUB_FILE" "lv_obj_t *bento_card2 = lv_button_create(tile_home);" "home lighting card is not a full-card button"
require_text "$HUB_FILE" "s_btn_bento_led = bento_card2;" "full-card lighting button is not wired to the LED callback"

forbid_text "$HUB_FILE" "ui_photo_album_init(tile_photo)" "photo album is still initialized inside TileView"
forbid_text "$HUB_FILE" "ui_novel_reader_init(tile_novel)" "novel reader is still initialized inside TileView"
forbid_text "$HUB_FILE" "ui_game_2048_init(tile_game)" "2048 is still initialized inside TileView"
forbid_text "$GAME_FILE" "LV_OBJ_FLAG_GESTURE_BUBBLE" "2048 gesture still bubbles to the outer TileView"
forbid_text "$HUB_FILE" "lbl_nav_hint" "home page still contains a directional hint label"
forbid_text "$HUB_FILE" "▼下拉设置  ▲上滑传感  ◀番茄钟  ▶应用" "home page still displays directional instructions"
forbid_text "$BUTTON_FILE" "短按开关灯" "SW3 short press still toggles the LED"
forbid_text "$HUB_FILE" "lv_button_create(bento_card2)" "lighting card still contains a nested button"
forbid_text "$HUB_FILE" "s_label_bento_ip" "lighting card still contains an IP label"
forbid_text "$HUB_FILE" "s_label_location" "home location field was renamed unnecessarily"
forbid_text "$HUB_FILE" "s_label_weather_desc" "home weather field was renamed unnecessarily"
forbid_text "$HUB_FILE" "lv_obj_set_size(bento_card1, 111, 118);" "home environment card was expanded unexpectedly"
forbid_text "$HUB_FILE" "lv_obj_set_size(bento_card2, 111, 118);" "home lighting card was expanded unexpectedly"

if (( failures > 0 )); then
    echo "Level 21 navigation check: FAILED ($failures issue(s))"
    exit 1
fi

echo "Level 21 navigation check: PASS"

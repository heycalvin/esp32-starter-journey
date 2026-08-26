#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
POMO_FILE="$ROOT_DIR/main/ui/ui_pomodoro.c"
POMO_HEADER="$ROOT_DIR/main/ui/ui_pomodoro.h"
BUSINESS_FILE="$ROOT_DIR/main/app/app_business.c"

failures=0

require_text() {
    local file="$1"
    local pattern="$2"
    local description="$3"
    if ! rg -q --fixed-strings -- "$pattern" "$file"; then
        echo "FAIL: $description"
        failures=$((failures + 1))
    fi
}

forbid_text() {
    local file="$1"
    local pattern="$2"
    local description="$3"
    if rg -q --fixed-strings -- "$pattern" "$file"; then
        echo "FAIL: $description"
        failures=$((failures + 1))
    fi
}

require_text "$POMO_FILE" "POMO_PHASE_FOCUS" "focus phase is missing"
require_text "$POMO_FILE" "POMO_PHASE_REST" "rest phase is missing"
require_text "$POMO_FILE" "POMO_FOCUS_SECONDS" "focus duration is not explicit"
require_text "$POMO_FILE" "POMO_REST_SECONDS" "rest duration is not explicit"
require_text "$POMO_FILE" "s_lbl_pomo_phase" "current phase label is missing"
require_text "$POMO_FILE" "s_bar_pomo_progress" "pomodoro progress bar is missing"
require_text "$POMO_FILE" "\"跳过\"" "skip control is missing"
require_text "$POMO_FILE" "\"开始\"" "start control is missing"
require_text "$POMO_FILE" "\"暂停\"" "pause control is missing"
require_text "$POMO_FILE" "\"切换到大时钟\"" "clock mode control is not self-explanatory"
require_text "$POMO_FILE" "\"切换到番茄钟\"" "pomodoro mode control is not self-explanatory"
require_text "$POMO_HEADER" "void ui_pomodoro_update_date(const char *date_str);" "pomodoro date update API is missing"
require_text "$BUSINESS_FILE" "ui_pomodoro_update_date(date_str);" "real date is not passed to pomodoro UI"

forbid_text "$POMO_FILE" "s_lbl_clock_sec" "large clock still uses a split seconds label"
forbid_text "$POMO_FILE" "s_lbl_clock_big" "large clock still uses the old split time label"
forbid_text "$POMO_FILE" "-- 年 -- 月 -- 日" "clock mode still uses a placeholder date"
forbid_text "$POMO_FILE" "lv_obj_set_style_text_color(s_lbl_clock_date, lv_color_hex(0x334155), 0);" "clock date still uses low contrast"

if (( failures > 0 )); then
    echo "Level 21 Pomodoro check: FAILED ($failures issue(s))"
    exit 1
fi

echo "Level 21 Pomodoro check: PASS"

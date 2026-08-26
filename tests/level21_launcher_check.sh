#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HUB_FILE="$ROOT_DIR/main/ui/ui_hub.c"

failures=0

require_text() {
    local pattern="$1"
    local description="$2"
    if ! rg -q --fixed-strings -- "$pattern" "$HUB_FILE"; then
        echo "FAIL: $description"
        failures=$((failures + 1))
    fi
}

forbid_text() {
    local pattern="$1"
    local description="$2"
    if rg -q --fixed-strings -- "$pattern" "$HUB_FILE"; then
        echo "FAIL: $description"
        failures=$((failures + 1))
    fi
}

require_text "lv_obj_set_style_text_font(launcher_title, font_cn, 0);" "launcher title does not use the Chinese font"
require_text "LV_SYMBOL_IMAGE" "photo app icon is missing"
require_text "LV_SYMBOL_FILE" "novel app icon is missing"
require_text "lv_obj_set_size(btn, 228, 48);" "launcher apps are not full-width rows"
require_text "const char *app_descs[]" "launcher app purposes are not explained"
require_text "返回首页" "launcher has no visible home button"
require_text "lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, 0);" "launcher icons do not use the symbol font"
require_text "lv_obj_set_style_text_font(name, font_cn, 0);" "launcher names do not use the Chinese font"
require_text "lv_obj_set_style_text_font(desc, font_cn, 0);" "launcher descriptions do not use the Chinese font"

forbid_text "lv_obj_set_style_text_font(launcher_title, &lv_font_montserrat_20, 0);" "launcher title still uses a font without Chinese glyphs"
forbid_text "🎮" "launcher still uses an unsupported game emoji"
forbid_text "🖼" "launcher still uses an unsupported photo emoji"
forbid_text "📖" "launcher still uses an unsupported novel emoji"

if (( failures > 0 )); then
    echo "Level 21 launcher check: FAILED ($failures issue(s))"
    exit 1
fi

echo "Level 21 launcher check: PASS"

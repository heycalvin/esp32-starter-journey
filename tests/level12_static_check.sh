#!/usr/bin/env bash

set -euo pipefail

require_text() {
    local file="$1"
    local expected="$2"

    if ! rg -Fq -- "$expected" "$file"; then
        printf '缺少要求内容：%s（文件：%s）\n' "$expected" "$file" >&2
        exit 1
    fi
}

require_text "code/12_wifi_weather/02_sntp_time_sync.c" "esp_netif_sntp_init"
require_text "code/12_wifi_weather/02_sntp_time_sync.c" "esp_netif_sntp_sync_wait"
require_text "code/12_wifi_weather/03_http_weather_clock.c" "current=temperature_2m%2Cwind_speed_10m%2Cweather_code"
require_text "code/12_wifi_weather/03_http_weather_clock.c" "cJSON_IsNumber"
require_text "book/12_WiFi连接管理与HTTP天气时钟.md" "12.10 常见问题"
require_text "book/12_WiFi连接管理与HTTP天气时钟.md" "不等于已经在真机上联网成功"

printf '第 12 关静态一致性检查通过。\n'

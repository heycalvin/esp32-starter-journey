#!/usr/bin/env bash

# ==============================================================================
# 🚀 ESP32 物联网实战闯关 —— 示例代码一键切换工具 (switch_code.sh)
# ==============================================================================

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CODE_DIR="${PROJECT_ROOT}/code"
TARGET_MAIN="${PROJECT_ROOT}/main/app_main.c"

function print_banner() {
    echo "================================================================="
    echo "   🛠️  ESP32 实战教程 —— 示例代码一键切换工具 (switch_code)"
    echo "================================================================="
}

function print_usage() {
    print_banner
    echo "📖 用法:"
    echo "  ./switch_code.sh list                     # 列出所有可用的关卡与实验"
    echo "  ./switch_code.sh <关卡号> [实验号]         # 切换指定实验代码到 main/app_main.c"
    echo "  ./switch_code.sh <关卡号> [实验号] --build # 切换并立即编译"
    echo "  ./switch_code.sh <关卡号> [实验号] --flash # 切换并立即烧录监控"
    echo ""
    echo "💡 示例:"
    echo "  ./switch_code.sh 1                        # 切换到 Level 01: 串口通信"
    echo "  ./switch_code.sh 2 2                      # 切换到 Level 02 实验 2: PWM呼吸灯"
    echo "  ./switch_code.sh 7 4 --flash              # 切换到 Level 07 实验 4: 温声雷达并烧录"
    echo "================================================================="
}

function list_experiments() {
    print_banner
    echo "📂 当前可供切换运行的示例代码列表："
    echo ""

    local chapters=(
        "01_hello_world:第 01 关: 串口通信与 Hello World"
        "02_gpio_pwm:第 02 关: GPIO 输出与 PWM 呼吸灯"
        "03_gpio_input:第 03 关: 按键输入检测与人体红外感应"
        "04_gpio_interrupt:第 04 关: GPIO 外部中断与事件驱动"
        "05_freertos_queue:第 05 关: FreeRTOS 多任务与双核队列"
        "06_ws2812_rmt:第 06 关: RMT 硬件脉冲与 WS2812 幻彩 RGB"
        "07_adc_ultrasonic:第 07 关: ADC 模拟量采集与超声波测距"
        "08_i2c_dht11:第 08 关: I2C 通信总线与 DHT11 温湿度"
        "09_nvs_storage:第 09 关: NVS 存储与 Flash 偏好设置"
        "10_st7789_display:第 10 关: ST7789 彩屏与几何图形渲染"
        "11_lvgl_touch:第 11 关: LVGL 图形框架、电容触摸与字库适配"
        "12_wifi_weather:第 12 关: WiFi 联网与 HTTP 天气时钟"
        "13_mqtt_iot:第 13 关: MQTT 物联网通信与手机远程控制"
        "14_ble_gatt:第 14 关: BLE 蓝牙广播与手机透传遥控"
        "15_espnow_remote:第 15 关: ESP-NOW 超低延迟私有通信与双机遥控"
        "16_web_server_portal:第 16 关: Web Server 网页中控与 AP 门户配网"
        "17_ota_firmware:第 17 关: OTA 空中无线升级与 A/B 分区防变砖"
        "18_sdcard_fatfs:第 18 关: TF 卡 SDIO 驱动与 FatFS 文件系统"
        "19_low_power_deepsleep:第 19 关: 低功耗电源管理与 Deep-sleep 休眠"
        "20_software_architecture:第 20 关: 嵌入式分层架构、事件总线与看门狗"
        "21_final_station_hub:第 21 关: 桌面智能气象站与物联网超级中控台"
    )

    for entry in "${chapters[@]}"; do
        local ch_dir="${entry%%:*}"
        local title="${entry##*:}"
        local ch_path="${CODE_DIR}/${ch_dir}"

        echo -e "\033[1;36m▶ ${title}\033[0m  (目录: code/${ch_dir})"
        if [ -d "${ch_path}" ]; then
            # 1. 检查是否是全栈单一大工程 (如 21_final_station_hub 包含 embedded/)
            if [ -d "${ch_path}/embedded" ]; then
                local short_prefix=$(echo "$ch_dir" | cut -c 1-2)
                echo -e "   [\033[33m${short_prefix}\033[0m] 🏆 embedded/ (桌面智能气象站与中控台嵌入式端)  \033[90m🌟 全栈一体化大工程 [embedded/ + server/ + app/]\033[0m"
            # 2. 检查是否包含子工程目录 (如 01_bsp_decoupling, 02_fsm_state_machine)
            elif sub_dirs=($(find "${ch_path}" -maxdepth 1 -type d -name "[0-9][0-9]_*" | sort)); [ ${#sub_dirs[@]} -gt 0 ]; then
                local idx=1
                for sd in "${sub_dirs[@]}"; do
                    local first_line=""
                    local entry_file="${sd}/app_main.c"
                    if [ -f "${entry_file}" ]; then
                        first_line=$(grep -E "🌟|🚀|📁" "${entry_file}" | head -n 1 | sed 's/^[ \t]*//')
                    fi
                    local sd_name=$(basename "$sd")
                    local short_prefix=$(echo "$ch_dir" | cut -c 1-2)
                    echo -e "   [\033[33m${short_prefix} ${idx}\033[0m] 📁 ${sd_name}/  \033[90m${first_line}\033[0m"
                    idx=$((idx + 1))
                done
            else
                # 3. 标准单文件列表
                local files=($(find "${ch_path}" -maxdepth 1 -name "*.c" | sort))
                local idx=1
                for f in "${files[@]}"; do
                    local first_line=""
                    if [ -f "$f" ]; then
                        first_line=$(grep -E "🌟|🚀|📁" "$f" | head -n 1 | sed 's/^[ \t]*//')
                    fi
                    local f_name=$(basename "$f")
                    local short_prefix=$(echo "$ch_dir" | cut -c 1-2)
                    echo -e "   [\033[33m${short_prefix} ${idx}\033[0m] ${f_name}  \033[90m${first_line}\033[0m"
                    idx=$((idx + 1))
                done
            fi
        fi
        echo ""
    done
    echo "================================================================="
}

# 参数解析
if [ "$1" == "list" ] || [ "$1" == "-l" ] || [ "$1" == "--list" ] || [ "$1" == "-list" ]; then
    list_experiments
    exit 0
fi

if [ -z "$1" ] || [ "$1" == "-h" ] || [ "$1" == "--help" ] || [ "$1" == "help" ]; then
    print_usage
    exit 0
fi

CH_NUM=$(printf "%02d" "$1" 2>/dev/null || echo "$1")
EXP_NUM="${2:-1}"

# 寻找对应的章节目录
MATCH_DIR=$(find "${CODE_DIR}" -maxdepth 1 -type d -name "${CH_NUM}_*" | head -n 1)

if [ -z "${MATCH_DIR}" ] || [ ! -d "${MATCH_DIR}" ]; then
    echo -e "\033[31m❌ 错误：未找到第 ${CH_NUM} 关对应的代码目录！\033[0m"
    echo "使用 './switch_code.sh list' 查看全部可用关卡。"
    exit 1
fi

# 清理 main/ 目录下的历史残留分层子目录 (bsp, services, ui, app, components, events)
rm -rf "${PROJECT_ROOT}/main/bsp" "${PROJECT_ROOT}/main/services" "${PROJECT_ROOT}/main/ui" "${PROJECT_ROOT}/main/app" "${PROJECT_ROOT}/main/components" "${PROJECT_ROOT}/main/events"

PADDED_EXP=$(printf "%02d" "$EXP_NUM" 2>/dev/null || echo "$EXP_NUM")

# 判断目标实验类型
EMBEDDED_PATH="${MATCH_DIR}/embedded"
MATCH_SUB_DIR=$(find "${MATCH_DIR}" -maxdepth 1 -type d -name "${PADDED_EXP}_*" | head -n 1)

if [ -d "${EMBEDDED_PATH}" ]; then
    # ── 模式 A1: 关卡 21 从 embedded/ 子目录拉取固件代码到 main/ ──
    for item in "${EMBEDDED_PATH}"/*; do
        local item_name=$(basename "$item")
        if [ "$item_name" == "partitions.csv" ]; then
            cp -f "$item" "${PROJECT_ROOT}/partitions.csv"
        elif [ -d "$item" ]; then
            cp -rf "$item" "${PROJECT_ROOT}/main/"
        else
            cp -f "$item" "${PROJECT_ROOT}/main/"
        fi
    done

    REL_SRC="${EMBEDDED_PATH#$PROJECT_ROOT/}"
    echo "================================================================="
    echo -e "\033[32m🏆 成功切换第 21 关毕业设计全栈大工程！\033[0m"
    echo -e "   嵌入式源目录: \033[33m${REL_SRC}\033[0m"
    echo -e "   目标工作区:   \033[36mmain/ [已拉取完整的 BSP + Services + UI + App 模块]\033[0m"
    echo "================================================================="
elif [ -n "${MATCH_SUB_DIR}" ] && [ -d "${MATCH_SUB_DIR}" ]; then
    # ── 模式 A2: 复制整个子工程目录到 main/ ──
    for item in "${MATCH_SUB_DIR}"/*; do
        local item_name=$(basename "$item")
        if [ "$item_name" == "partitions.csv" ]; then
            cp -f "$item" "${PROJECT_ROOT}/partitions.csv"
        elif [ -d "$item" ]; then
            cp -rf "$item" "${PROJECT_ROOT}/main/"
        else
            cp -f "$item" "${PROJECT_ROOT}/main/"
        fi
    done

    REL_SRC="${MATCH_SUB_DIR#$PROJECT_ROOT/}"
    echo "================================================================="
    echo -e "\033[32m✅ 成功切换多文件模块化工程！\033[0m"
    echo -e "   源工程: \033[33m${REL_SRC}\033[0m"
    echo -e "   目标位: \033[36mmain/ [包含独立分层组件]\033[0m"
    echo "================================================================="
else
    # ── 模式 B: 标准单文件切换 ──
    EXP_FILES=($(find "${MATCH_DIR}" -maxdepth 1 -name "*.c" | sort))
    TOTAL_EXPS=${#EXP_FILES[@]}

    if [ "${TOTAL_EXPS}" -eq 0 ]; then
        echo -e "\033[31m❌ 错误：第 ${CH_NUM} 关未找到编号为 ${EXP_NUM} 的实验或子工程！\033[0m"
        exit 1
    fi

    MATCHED_BY_PREFIX=$(find "${MATCH_DIR}" -maxdepth 1 -name "${PADDED_EXP}_*.c" | head -n 1)
    if [ -n "${MATCHED_BY_PREFIX}" ] && [ -f "${MATCHED_BY_PREFIX}" ]; then
        TARGET_SRC="${MATCHED_BY_PREFIX}"
    elif [ "$EXP_NUM" -le "${TOTAL_EXPS}" ] && [ "$EXP_NUM" -ge 1 ]; then
        TARGET_SRC="${EXP_FILES[$(( EXP_NUM - 1 ))]}"
    else
        echo -e "\033[31m❌ 错误：第 ${CH_NUM} 关未找到编号为 ${EXP_NUM} 的实验！\033[0m"
        exit 1
    fi

    cp "${TARGET_SRC}" "${TARGET_MAIN}"
    REL_SRC="${TARGET_SRC#$PROJECT_ROOT/}"
    echo "================================================================="
    echo -e "\033[32m✅ 成功切换单文件示例代码！\033[0m"
    echo -e "   源文件: \033[33m${REL_SRC}\033[0m"
    echo -e "   目标位: \033[36mmain/app_main.c\033[0m"
    echo "================================================================="
fi

# 检查后续指令 (--build / --flash)
for arg in "$@"; do
    if [ "${arg}" == "--build" ] || [ "${arg}" == "-b" ] || [ "${arg}" == "--flash" ] || [ "${arg}" == "-f" ]; then
        if ! command -v idf.py &> /dev/null; then
            if [ -f "/Users/calvin/.espressif/v6.0.2/esp-idf/export.sh" ]; then
                echo "⚙️ 正在自动加载 ESP-IDF 环境变量..."
                . "/Users/calvin/.espressif/v6.0.2/esp-idf/export.sh" >/dev/null 2>&1
            elif [ -f "$HOME/esp/esp-idf/export.sh" ]; then
                . "$HOME/esp/esp-idf/export.sh" >/dev/null 2>&1
            fi
        fi
    fi

    if [ "${arg}" == "--build" ] || [ "${arg}" == "-b" ]; then
        echo -e "\n🔨 正在执行编译 (idf.py reconfigure && idf.py build)..."
        idf.py reconfigure >/dev/null 2>&1 || true
        idf.py build
        break
    elif [ "${arg}" == "--flash" ] || [ "${arg}" == "-f" ]; then
        echo -e "\n⚡ 正在执行烧录与串口监视 (idf.py flash monitor)..."
        idf.py reconfigure >/dev/null 2>&1 || true
        idf.py flash monitor
        break
    fi
done

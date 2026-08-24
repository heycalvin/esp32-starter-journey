/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 17 关：OTA 空中固件升级与防变砖回滚
 * 📁 实验 3: 工业级固件自检守护与 Bootloader 自动回滚 (Rollback) 防变砖机制
 * ==============================================================================
 * 
 * 📌 【实验目标】
 * 1. 深入掌握 ESP-IDF 防变砖回滚核心机制（`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`）；
 * 2. 检测当前固件是否为首次启动的“待验证新固件”（`ESP_OTA_IMG_PENDING_VERIFY`）；
 * 3. 模拟健康自检流程：
 *    - 🟢 【自检通过】在 10 秒内按下板载按键 SW3 (GPIO39)，执行 `esp_ota_mark_app_valid_cancel_rollback()` 永久确认固件健康；
 *    - 🔴 【自检超时 / 崩溃模拟】10 秒未按按键，执行 `esp_ota_mark_app_invalid_rollback_and_reboot()`，
 *       Bootloader 将毫秒级自动回滚至上一个稳定运行的旧固件！
 * 
 * 🔌 【硬件连接】
 * - 用户按键 SW3: GPIO39 (VN) (输入专用，按下为低电平 0，用于人工确认自检通过)
 * - 板载绿色 LED2: GPIO27 (待验证时以 5Hz 警示闪烁，自检通过后常亮)
 * ==============================================================================
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"

#define LED_PIN         GPIO_NUM_27
#define BUTTON_SW3_PIN  GPIO_NUM_39

static const char *TAG = "EXP3_OTA_ROLLBACK";

/**
 * @brief 将 OTA 镜像状态转换为可读中文
 */
static const char* ota_state_to_str(esp_ota_img_states_t state) {
    switch (state) {
        case ESP_OTA_IMG_NEW:             return "NEW (全新写入，未启动过)";
        case ESP_OTA_IMG_PENDING_VERIFY:  return "PENDING_VERIFY (待自检验证，若死机将自动回滚)";
        case ESP_OTA_IMG_VALID:           return "VALID (已确认健康稳定)";
        case ESP_OTA_IMG_INVALID:         return "INVALID (已损坏或自检失败)";
        case ESP_OTA_IMG_ABORTED:         return "ABORTED (烧写中断废弃)";
        case ESP_OTA_IMG_UNDEFINED:       return "UNDEFINED (未定义/非OTA模式)";
        default:                          return "UNKNOWN";
    }
}

/**
 * @brief 初始化硬件 GPIO
 */
static void init_hardware(void) {
    // 1. 初始化 LED2 输出
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED_PIN, 0);

    // 2. 初始化 SW3 按键输入 (GPIO39 内部无上下拉电阻，依赖硬件板载上拉)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_SW3_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_conf);
}

void app_main(void) {
    // 1. 初始化 NVS 闪存
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 初始化硬件
    init_hardware();

    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   🛡️ ESP32 工业级 OTA 防变砖回滚 (Rollback) 守护启动    ");
    ESP_LOGI(TAG, "==========================================================");

    // 3. 查询当前运行分区与其 OTA 状态
    const esp_partition_t *running_part = esp_ota_get_running_partition();
    const esp_app_desc_t *app_desc = esp_app_get_description();

    ESP_LOGI(TAG, "📦 固件名称: [%s], 版本号: [%s]", app_desc->project_name, app_desc->version);
    ESP_LOGI(TAG, "📍 当前运行分区: [%s] (地址: 0x%08lX)",
             running_part ? running_part->label : "NULL",
             running_part ? running_part->address : 0);

    esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(running_part, &ota_state) == ESP_OK) {
        ESP_LOGI(TAG, "🔍 当前分区状态: [%s]", ota_state_to_str(ota_state));
    } else {
        ESP_LOGW(TAG, "⚠️ 无法获取分区状态（可能当前处于非 Rollback 编译配置或出厂分区）");
    }

    // 4. 判断是否处于待自检验证阶段 (PENDING_VERIFY)
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGW(TAG, "----------------------------------------------------------");
        ESP_LOGW(TAG, "⚠️ 【重要提示】当前固件处于【待验证 (PENDING_VERIFY)】状态！");
        ESP_LOGW(TAG, "⚠️ 系统进入 10 秒健康自检倒计时！");
        ESP_LOGW(TAG, "👉 请在 10 秒内按下板载按键 【SW3 (GPIO39)】 确认固件正常；");
        ESP_LOGW(TAG, "👉 若 10 秒内未按或发生死机崩溃，Bootloader 将自动回退至上一稳定版本！");
        ESP_LOGW(TAG, "----------------------------------------------------------");

        bool verified = false;
        for (int i = 10; i > 0; i--) {
            ESP_LOGW(TAG, "⏳ 自检倒计时剩余: %d 秒... (快闪警示中)", i);

            // 倒计时期间快速闪烁 LED2 (100ms)
            for (int k = 0; k < 5; k++) {
                gpio_set_level(LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));

                // 实时检测按键 SW3 (低电平按下)
                if (gpio_get_level(BUTTON_SW3_PIN) == 0) {
                    // 消抖 20ms
                    vTaskDelay(pdMS_TO_TICKS(20));
                    if (gpio_get_level(BUTTON_SW3_PIN) == 0) {
                        verified = true;
                        break;
                    }
                }
            }

            if (verified) {
                break;
            }
        }

        if (verified) {
            // ✅ 用户人工确认 / 自检通过：标记固件有效并取消回滚！
            ESP_LOGI(TAG, "==========================================================");
            ESP_LOGI(TAG, "🎉 检测到按键 SW3 按下！系统自检顺利通过！");
            esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "✅ 【成功】固件已被标记为 VALID (稳定健康)！回退机制已永久取消！");
                gpio_set_level(LED_PIN, 1); // 自检成功，LED2 常亮
            } else {
                ESP_LOGE(TAG, "❌ 标记固件有效失败: 0x%x", err);
            }
            ESP_LOGI(TAG, "==========================================================");
        } else {
            // ❌ 倒计时超时：模拟自检失败，主动触发回滚并重启！
            ESP_LOGE(TAG, "==========================================================");
            ESP_LOGE(TAG, "💥 自检超时或异常！模拟判定新固件存在致命 Bug！");
            ESP_LOGE(TAG, "🔄 正在调用 esp_ota_mark_app_invalid_rollback_and_reboot()...");
            ESP_LOGE(TAG, "🔄 正在自动回滚至上一版本并重启...");
            ESP_LOGE(TAG, "==========================================================");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
    } else if (ota_state == ESP_OTA_IMG_VALID) {
        ESP_LOGI(TAG, "🟢 当前固件已是验证通过的【稳定健康版本 (VALID)】，无需再次自检。");
        gpio_set_level(LED_PIN, 1);
    } else {
        ESP_LOGI(TAG, "ℹ️ 正常运行中... 当前分区未启用回滚待验证模式。");
    }

    // 5. 稳定运行主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "💓 [系统稳定运行] 当前分区: [%s], 固件版本: [%s]",
                 running_part ? running_part->label : "NULL",
                 app_desc->version);
    }
}

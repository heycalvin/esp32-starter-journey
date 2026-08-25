/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 20 关：嵌入式软件工程与模块化分层架构
 * 📁 实验 1: BSP 板级支持包与驱动抽象分层实战 (Driver Decoupling)
 * ==============================================================================
 * 
 * 📌 【实验目标】
 * 1. 彻底告别在业务逻辑代码中到处硬编码 `GPIO_NUM_27`、`gpio_set_level()` 的坏习惯；
 * 2. 掌握 BSP（Board Support Package，板级支持包）分层架构设计；
 * 3. 业务层只依赖清晰抽象的语义化接口（如 `bsp_led_set(true)`、`bsp_button_is_pressed()`）；
 * 4. 体验“底层硬件更换引脚或驱动芯片，上层业务代码 0 修改”的模块化工程美感！
 * 
 * 📌 【硬件引脚说明】
 * - 板载 LED2: GPIO27 (由 BSP 层封装控制)
 * - 用户按键 SW3: GPIO39 (由 BSP 层封装读取与消抖)
 * ==============================================================================
 */

#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "EXP1_BSP_LAYER";

/* ==============================================================================
 * 🔧 一、 BSP 驱动抽象层 (底层：负责直接和硬件寄存器/引脚打交道)
 * ============================================================================== */
#define BSP_LED2_PIN      GPIO_NUM_27
#define BSP_BUTTON_PIN    GPIO_NUM_39

typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_ON  = 1,
    LED_STATE_TOGGLE
} bsp_led_cmd_t;

static bool s_current_led_state = false;

/**
 * @brief 初始化板载基础硬件外设 (GPIO、按键等)
 */
static void bsp_init(void)
{
    // 1. 初始化 LED2 输出引脚
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << BSP_LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_cfg);
    gpio_set_level(BSP_LED2_PIN, 0);

    // 2. 初始化按键输入引脚
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BSP_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);
    ESP_LOGI(TAG, "🔧 [BSP 硬件层] 底层 GPIO 与外设初始化完毕！");
}

/**
 * @brief 语义化控制板载 LED 状态
 */
static void bsp_led_control(bsp_led_cmd_t cmd)
{
    if (cmd == LED_STATE_TOGGLE) {
        s_current_led_state = !s_current_led_state;
    } else {
        s_current_led_state = (cmd == LED_STATE_ON);
    }
    gpio_set_level(BSP_LED2_PIN, s_current_led_state ? 1 : 0);
}

/**
 * @brief 语义化获取用户按键物理状态
 */
static bool bsp_button_is_pressed(void)
{
    return (gpio_get_level(BSP_BUTTON_PIN) == 0);
}

/* ==============================================================================
 * 💼 二、 业务应用层 (上层：纯业务逻辑，完全不知道具体是哪根引脚/芯片型号！)
 * ============================================================================== */
static void business_logic_task(void *pvParameters)
{
    ESP_LOGI(TAG, "💼 [业务逻辑层] 业务中枢任务启动运行...");

    while (1) {
        if (bsp_button_is_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(20)); // 消抖
            if (bsp_button_is_pressed()) {
                ESP_LOGI(TAG, "🔘 [业务层感知] 用户触发了按键动作，切换灯光状态！");
                bsp_led_control(LED_STATE_TOGGLE);

                // 等待按键释放
                while (bsp_button_is_pressed()) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "   🚀 Level 20 实验 1：BSP 驱动层与业务逻辑解耦架构       ");
    ESP_LOGI(TAG, "==========================================================");

    // 1. 初始化底层硬件抽象
    bsp_init();

    // 2. 启动上层业务任务
    xTaskCreate(business_logic_task, "biz_task", 3072, NULL, 5, NULL);
}

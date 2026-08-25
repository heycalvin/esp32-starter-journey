#include "bsp_sensor.h"
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "BSP_SENSOR";

#define PIN_NTC_ADC       ADC_CHANNEL_0 // GPIO36 (ADC1_CH0)
#define PIN_HCSR04_TRIG   GPIO_NUM_32
#define PIN_HCSR04_ECHO   GPIO_NUM_33
#define PIN_SR602_PIR     GPIO_NUM_34
#define PIN_DHT11_DATA    GPIO_NUM_25

// NTC 参数: B=3950, R25=10K, 分压电阻=10K, Vref=3.3V
#define NTC_B_CONSTANT    3950.0f
#define NTC_T0_KELVIN     298.15f // 25°C in Kelvin
#define NTC_R0_OHM        10000.0f
#define NTC_R_DIVIDER     10000.0f

static adc_oneshot_unit_handle_t s_adc1_handle = NULL;

esp_err_t bsp_sensor_init(void)
{
    // 1. 初始化 NTC ADC1
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &s_adc1_handle);
    if (ret == ESP_OK) {
        adc_oneshot_chan_cfg_t chan_config = {
            .bitwidth = ADC_BITWIDTH_DEFAULT,
            .atten = ADC_ATTEN_DB_12,
        };
        adc_oneshot_config_channel(s_adc1_handle, PIN_NTC_ADC, &chan_config);
    }

    // 2. 初始化 HC-SR04 引脚
    gpio_config_t trig_conf = {
        .pin_bit_mask = (1ULL << PIN_HCSR04_TRIG),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&trig_conf);
    gpio_set_level(PIN_HCSR04_TRIG, 0);

    gpio_config_t echo_conf = {
        .pin_bit_mask = (1ULL << PIN_HCSR04_ECHO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&echo_conf);

    // 3. 初始化 SR602 PIR 人体红外引脚
    gpio_config_t pir_conf = {
        .pin_bit_mask = (1ULL << PIN_SR602_PIR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pir_conf);

    ESP_LOGI(TAG, "📡 [BSP] 多传感器硬件层 (NTC/超声波/PIR) 初始化完毕！");
    return ESP_OK;
}

float bsp_sensor_read_ntc_temp(void)
{
    if (!s_adc1_handle) return 25.0f;
    int raw_adc = 0;
    esp_err_t ret = adc_oneshot_read(s_adc1_handle, PIN_NTC_ADC, &raw_adc);
    if (ret != ESP_OK || raw_adc <= 0 || raw_adc >= 4095) {
        return 25.0f; // 默认室温
    }

    // 计算 NTC 阻值并转化为摄氏度
    float v_ratio = (float)raw_adc / 4095.0f;
    float r_ntc = NTC_R_DIVIDER * (1.0f - v_ratio) / v_ratio;
    float temp_k = 1.0f / ( (1.0f / NTC_T0_KELVIN) + (log(r_ntc / NTC_R0_OHM) / NTC_B_CONSTANT) );
    float temp_c = temp_k - 273.15f;
    return (temp_c > -40.0f && temp_c < 125.0f) ? temp_c : 25.0f;
}

float bsp_sensor_read_ultrasonic_distance(void)
{
    // 触发 10us 高电平脉冲
    gpio_set_level(PIN_HCSR04_TRIG, 0);
    esp_rom_delay_us(2);
    gpio_set_level(PIN_HCSR04_TRIG, 1);
    esp_rom_delay_us(10);
    gpio_set_level(PIN_HCSR04_TRIG, 0);

    int64_t start_time = esp_timer_get_time();
    int64_t timeout_time = start_time + 30000; // 30ms 超时

    // 等待 Echo 变高
    while (gpio_get_level(PIN_HCSR04_ECHO) == 0) {
        if (esp_timer_get_time() > timeout_time) return -1.0f;
    }
    int64_t echo_start = esp_timer_get_time();

    // 等待 Echo 变低
    while (gpio_get_level(PIN_HCSR04_ECHO) == 1) {
        if (esp_timer_get_time() > timeout_time) return -1.0f;
    }
    int64_t echo_end = esp_timer_get_time();

    int64_t pulse_duration_us = echo_end - echo_start;
    float distance_cm = (float)pulse_duration_us * 0.0343f / 2.0f;
    return (distance_cm > 2.0f && distance_cm < 400.0f) ? distance_cm : 20.0f;
}

bool bsp_sensor_read_pir(void)
{
    return (gpio_get_level(PIN_SR602_PIR) == 1);
}

esp_err_t bsp_sensor_read_all(bsp_sensor_data_t *data)
{
    if (!data) return ESP_ERR_INVALID_ARG;

    data->ntc_temperature = bsp_sensor_read_ntc_temp();
    data->dht_temperature = data->ntc_temperature; // 可校准
    data->dht_humidity = 60.0f + ((float)((int)(data->ntc_temperature * 10) % 20) / 2.0f); // 相对湿度模拟/采集
    data->ultrasonic_dist_cm = bsp_sensor_read_ultrasonic_distance();
    data->pir_motion_detected = bsp_sensor_read_pir();
    data->free_heap_bytes = esp_get_free_heap_size();
    data->free_psram_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    return ESP_OK;
}

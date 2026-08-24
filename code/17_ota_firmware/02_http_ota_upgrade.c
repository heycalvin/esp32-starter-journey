/**
 * ==============================================================================
 * 🚀 ESP32 物联网实战闯关 —— 第 17 关：OTA 空中固件升级与防变砖回滚
 * 📁 实验 2: HTTP 局域网空中无线拉取固件与流式烧写
 * ==============================================================================
 * 
 * 📌 【实验目标】
 * 1. 连接 Wi-Fi 路由器，接入局域网；
 * 2. 通过 HTTP 协议拉取电脑端搭建的本地 HTTP 服务器上的 `firmware.bin` 固件；
 * 3. 使用 `esp_https_ota` 高阶组件流式下载并自动写入目标 OTA 分区；
 * 4. 实时计算并打印下载百分比进度，下载完成校验 SHA-256 并自动重启！
 * 
 * 🛠️ 【本地固件服务器搭建指引（电脑端）】
 * 1. 编译生成 bin 固件：idf.py build
 * 2. 进入固件输出目录：cd build
 * 3. 复制生成的 bin 为固件名：cp esp32-start.bin firmware.bin
 * 4. 一键启动极简 HTTP 服务器：python3 -m http.server 8070
 * 5. 在下方代码中将 CONFIG_OTA_FIRMWARE_URL 改为电脑局域网 IP（如 http://192.168.1.100:8070/firmware.bin）
 * 
 * 🔌 【硬件连接】
 * - 板载绿色 LED2: GPIO27 (升级过程中高频快闪指示下载中)
 * ==============================================================================
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_app_format.h"

// ⚙️ Wi-Fi 与 OTA 服务器配置（请根据实际环境修改）
#define WIFI_SSID           "Your_WiFi_SSID"
#define WIFI_PASS           "Your_WiFi_Password"
#define OTA_FIRMWARE_URL    "http://192.168.1.100:8070/firmware.bin"

#define LED_PIN             GPIO_NUM_27
static const char *TAG = "EXP2_HTTP_OTA";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static bool s_is_upgrading = false;

/**
 * @brief Wi-Fi 事件回调处理
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    static int retry_num = 0;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "📡 正在连接 Wi-Fi: %s ...", WIFI_SSID);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < 5) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGW(TAG, "⚠️ Wi-Fi 连接断开，正在重试第 %d/5 次...", retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "❌ Wi-Fi 连接失败，请检查 SSID/密码！");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "✅ Wi-Fi 已连接！获取到 IP 地址: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief 初始化 Wi-Fi STA 模式
 */
static void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/**
 * @brief 执行 OTA 固件拉取与流式烧写任务
 */
static void ota_upgrade_task(void *pvParameter) {
    ESP_LOGI(TAG, "🚀 开始执行 OTA 固件升级流程...");
    ESP_LOGI(TAG, "🌐 目标固件 URL: %s", OTA_FIRMWARE_URL);

    // 1. 获取当前运行分区与升级目标分区信息
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "📍 当前运行分区: [%s] (0x%08lX)", running_partition->label, running_partition->address);
    ESP_LOGI(TAG, "🎯 写入目标分区: [%s] (0x%08lX)", update_partition->label, update_partition->address);

    s_is_upgrading = true;

    // 2. 配置 HTTP 客户端与 OTA 句柄
    esp_http_client_config_t http_config = {
        .url = OTA_FIRMWARE_URL,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ OTA 初始化失败 (0x%x)，请检查服务器是否开启或 URL 是否可达！", err);
        s_is_upgrading = false;
        vTaskDelete(NULL);
        return;
    }

    // 3. 读取新固件的 App Description 信息并校验
    esp_app_desc_t new_app_info;
    err = esp_https_ota_get_img_desc(ota_handle, &new_app_info);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "==========================================================");
        ESP_LOGI(TAG, "📦 发现新固件: [%s] 版本: [%s]", new_app_info.project_name, new_app_info.version);
        ESP_LOGI(TAG, "⏰ 固件编译时间: %s %s", new_app_info.date, new_app_info.time);
        ESP_LOGI(TAG, "==========================================================");
    } else {
        ESP_LOGW(TAG, "⚠️ 无法提前读取新固件元数据，继续下载...");
    }

    // 4. 流式下载与烧写循环
    int last_progress = -1;
    while (1) {
        err = esp_https_ota_perform(ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }

        // 计算下载进度
        int total_len = esp_https_ota_get_image_size(ota_handle);
        int read_len = esp_https_ota_get_image_len_read(ota_handle);
        if (total_len > 0) {
            int progress = (read_len * 100) / total_len;
            if (progress != last_progress && progress % 10 == 0) {
                last_progress = progress;
                ESP_LOGI(TAG, "⏬ 固件下载烧写进度: [%d%%] (已下载: %d / 总计: %d 字节)",
                         progress, read_len, total_len);
            }
        }
    }

    // 5. 校验结果与完成流程
    if (err == ESP_OK) {
        err = esp_https_ota_finish(ota_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "🎉 ==========================================================");
            ESP_LOGI(TAG, "🎉 OTA 升级写入成功！新固件已就绪！");
            ESP_LOGI(TAG, "🔄 系统将在 3 秒后自动重启并切换至新分区 [%s]...", update_partition->label);
            ESP_LOGI(TAG, "🎉 ==========================================================");
            vTaskDelay(pdMS_TO_TICKS(3000));
            esp_restart();
        } else {
            ESP_LOGE(TAG, "❌ OTA 校验与收尾失败: 0x%x", err);
        }
    } else {
        ESP_LOGE(TAG, "❌ OTA 升级传输中断: 0x%x", err);
        esp_https_ota_abort(ota_handle);
    }

    s_is_upgrading = false;
    vTaskDelete(NULL);
}

void app_main(void) {
    // 1. 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 初始化 LED2 指示灯
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_PIN, 0);

    const esp_app_desc_t *app_desc = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "🌟 当前运行固件版本: [%s], 分区: [%s]", app_desc->version, running ? running->label : "Unknown");

    // 3. 防变砖安全确认：若当前固件处于待自检状态 (PENDING_VERIFY)，必须先确认健康才能允许下次 OTA
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "🟢 检测到当前固件处于待验证状态 (PENDING_VERIFY)，主动标记为有效 (Valid) 并取消回滚！");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }

    // 4. 启动 Wi-Fi
    wifi_init_sta();


    // 等待 Wi-Fi 连接成功
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "🎉 网络畅通，5 秒后启动 OTA 空中升级任务...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        xTaskCreate(&ota_upgrade_task, "ota_task", 8192, NULL, 5, NULL);
    } else {
        ESP_LOGE(TAG, "❌ 网络连接失败，无法启动 OTA 升级任务。");
    }

    // 4. 指示灯心跳主循环
    while (1) {
        if (s_is_upgrading) {
            // 升级中：快闪指示（100ms）
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            // 平常状态：慢闪呼吸（1s）
            gpio_set_level(LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(800));
        }
    }
}

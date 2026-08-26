file(GLOB_RECURSE SOURCES "*.c")

set(EXTRA_INCLUDES "")
foreach(dir "bsp" "services" "ui" "app")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}")
        list(APPEND EXTRA_INCLUDES "${dir}")
    endif()
endforeach()

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "." ${EXTRA_INCLUDES}
    REQUIRES driver esp_timer esp_psram spi_flash esp_driver_ledc esp_driver_gpio esp_driver_rmt led_strip esp_adc esp_driver_i2c nvs_flash esp_lcd esp_wifi esp_event esp_netif esp_http_client esp_https_ota esp_http_server mdns lwip bt fatfs esp_driver_sdmmc esp_driver_sdspi esp_lvgl_port lvgl esp_lcd_touch_cst816s esp_lcd_touch app_update
)



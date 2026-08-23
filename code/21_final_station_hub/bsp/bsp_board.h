#pragma once

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// 板载指示灯与按键
#define BOARD_LED2_PIN          GPIO_NUM_27
#define BOARD_SW3_BTN_PIN       GPIO_NUM_39

// 传感器引脚
#define BOARD_DHT11_PIN         GPIO_NUM_25
#define BOARD_NTC_ADC_PIN       GPIO_NUM_36
#define BOARD_US_TRIG_PIN       GPIO_NUM_32
#define BOARD_US_ECHO_PIN       GPIO_NUM_33
#define BOARD_PIR_SR602_PIN     GPIO_NUM_34
#define BOARD_WS2812_PIN        GPIO_NUM_26

#ifdef __cplusplus
}
#endif

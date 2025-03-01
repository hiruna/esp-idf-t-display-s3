// SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_lvgl_port.h"

// Refer to https://github.com/Xinyuan-LilyGO/T-Display-S3/tree/main for more information
// T-Display S3 LCD Pins

#define LCD_PIN_NUM_PWR            15  // LCD_Power_On
#define LCD_PIN_NUM_BK_LIGHT       38  // LCD_BL
#define LCD_PIN_NUM_DATA0          39  // LCD_D0
#define LCD_PIN_NUM_DATA1          40  // LCD_D1
#define LCD_PIN_NUM_DATA2          41  // LCD_D2
#define LCD_PIN_NUM_DATA3          42  // LCD_D3
#define LCD_PIN_NUM_DATA4          45  // LCD_D4
#define LCD_PIN_NUM_DATA5          46  // LCD_D5
#define LCD_PIN_NUM_DATA6          47  // LCD_D6
#define LCD_PIN_NUM_DATA7          48  // LCD_D7
#define LCD_PIN_NUM_PCLK           8   // LCD_WR
#define LCD_PIN_NUM_RD             9   // LCD_RD
#define LCD_PIN_NUM_DC             7   // LCD_DC
#define LCD_PIN_NUM_CS             6   // LCD_CS
#define LCD_PIN_NUM_RST            5   // LCD_RES

#define LCD_BK_LIGHT_LEDC_CH       0

// T-Display Battery Voltage
#define BAT_PIN_NUM_VOLT           4     // (ADC_UNIT_1, ADC_CHANNEL_3) -  LCD_BAT_VOLT
#define NO_BAT_MILLIVOLTS          4500  // greater than 4600 no battery connected
#define BAT_CHARGE_MILLIVOLTS      4350  // greater than 4350 means charging
// T-Display Buttons
#define BTN_PIN_NUM_1              GPIO_NUM_0   // BOOT
#define BTN_PIN_NUM_2              GPIO_NUM_14  // IO14

// ----------------------------------------------------------------------------


// Allocate color data from PSRAM
// refer to https://github.com/espressif/esp-idf/blob/master/examples/peripherals/lcd/i80_controller/README.md#lvgl-porting-example-based-on-i80-interfaced-lcd-controller
#define CONFIG_LCD_I80_COLOR_IN_PSRAM 1

// The pixel number in horizontal and vertical
#define LCD_H_RES              320
#define LCD_V_RES              170

#define LCD_PWR_ON_LEVEL  1
#define LCD_PWR_OFF_LEVEL !LCD_PWR_ON_LEVEL


// Bit number used to represent command and parameter for ST7789 display
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

// Number of LCD i80 data lines (0-7)
#define LCD_I80_BUS_WIDTH      8

// PCLK frequency can't go too high as the limitation of PSRAM bandwidth
// try 2-17
#define LCD_PIXEL_CLOCK_HZ     (17 * 1000 * 1000)

#define LCD_I80_TRANS_QUEUE_SIZE 20
#define LCD_I80_DC_CMD_LEVEL     0
#define LCD_I80_DC_CMD_LEVEL     0
#define LCD_I80_DC_DUMMY_LEVEL   0
#define LCD_I80_DC_DATA_LEVEL    1


// Supported alignment: 16, 32, 64.
// A higher alignment can enable higher burst transfer size, thus a higher i80 bus throughput.
#define LCD_PSRAM_TRANS_ALIGN    64
#define LCD_SRAM_TRANS_ALIGN     4

// best to keep this as is (1/10th of the display pixels)
#define LVGL_BUFFER_SIZE        (((LCD_H_RES * LCD_V_RES) / 10) + LCD_H_RES)

// LVGL Timer options
#define LVGL_TICK_PERIOD_MS    5
#define LVGL_MAX_SLEEP_MS      (LVGL_TICK_PERIOD_MS * 2) // this affects how fast the screen is refreshed
#define LVGL_TASK_STACK_SIZE   (4 * 1024)
#define LVGL_TASK_PRIORITY     2


void lcd_init(lv_disp_t **disp_handle, bool backlight_on);

void lcd_set_brightness_step(uint8_t brightness_step);

void lcd_set_brightness_step_fade(uint8_t brightness_step, uint32_t fade_time_ms);

void lcd_set_brightness_pct(uint8_t brightness_percent);

void lcd_set_brightness_pct_fade(uint8_t brightness_percent, uint32_t fade_time_ms);

void lcd_increment_brightness_step();

void lcd_decrement_brightness_step();

uint8_t lcd_get_brightness_step();

uint8_t lcd_get_brightness_pct();

int get_battery_voltage();

int get_battery_percentage();

double volts_to_percentage(double volts);

bool usb_power_voltage(int milliVolts);

bool usb_power_connected();

#ifdef __cplusplus
} /*extern "C"*/
#endif
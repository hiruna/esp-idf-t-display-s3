/*
 * SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "aw9364.h"

const char *TAG = "lcd_brightness";

#define LCD_PIN_NUM_BK_LIGHT    38

// AW9364 handle (brightness controller)
static aw9364_dev_handle_t aw9364_dev_hdl;

void app_main() {

    ESP_LOGI(TAG, "Configuring LCD Brightness...");
    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t lcd_backlight_channel = {
            .gpio_num = LCD_PIN_NUM_BK_LIGHT,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_1,
            .duty = 0,
            .hpoint = 0
    };
    const ledc_timer_config_t lcd_backlight_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_10_BIT,
            .timer_num = LEDC_TIMER_1,
            .freq_hz = 5000,
            .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_LOGI(TAG, "initialize aw9364");
    esp_err_t err = aw9364_init(&lcd_backlight_channel, &lcd_backlight_timer, &aw9364_dev_hdl);
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "aw9364_set_brightness_step, no fade");
    aw9364_set_brightness_step(aw9364_dev_hdl, 5,0);

    ESP_LOGI(TAG, "wait 500ms");
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "aw9364_set_brightness_pct, no fade");
    aw9364_set_brightness_pct(aw9364_dev_hdl, 100,0);

    ESP_LOGI(TAG, "wait 500ms");
    vTaskDelay(500 / portTICK_PERIOD_MS);

    for (int i = 0; i < 16; ++i) {
        ESP_LOGI(TAG, "aw9364_decrement_brightness_step [%d], no fade", i);
        aw9364_decrement_brightness_step(aw9364_dev_hdl, 0);
        ESP_LOGI(TAG, "wait 100ms");
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "wait 500ms");
    vTaskDelay(500 / portTICK_PERIOD_MS);

    for (int i = 0; i < 16; ++i) {
        ESP_LOGI(TAG, "aw9364_increment_brightness_step [%d], no fade", i);
        aw9364_increment_brightness_step(aw9364_dev_hdl, 0);
        ESP_LOGI(TAG, "wait 100ms");
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    for (;;) {
        ESP_LOGI(TAG, "Brightness step value = %d, percentage = %d, fade will start in 10secs", aw9364_get_brightness_pct(aw9364_dev_hdl), aw9364_get_brightness_step(aw9364_dev_hdl));
        vTaskDelay(10000 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "aw9364_set_brightness_pct [0%], 3000ms fade");
        aw9364_set_brightness_pct(aw9364_dev_hdl, 0,3000);

        // fade operations are non blocking so delay before calling another fade
        ESP_LOGI(TAG, "waiting for ~3000ms");
        vTaskDelay(3100 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "aw9364_set_brightness_pct [100%], 3000ms fade");
        aw9364_set_brightness_pct(aw9364_dev_hdl, 100,3000);
    }
}
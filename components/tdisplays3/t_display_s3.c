// SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
// SPDX-License-Identifier: MIT

#include "t_display_s3.h"
#include <stdio.h>
#include <esp_log.h>
#include "esp_adc/adc_oneshot.h"
#include <soc/adc_channel.h>
#include <esp_lcd_panel_st7789.h>
#include <driver/ledc.h>
#include "math.h"
#include "aw9364.h"
//


static const char *TAG = "esp_idf_t_display_s3";

// ADC handle for battery voltage monitoring
static adc_oneshot_unit_handle_t adc_handle;
// ADC calibration handle for battery voltage monitoring
static adc_cali_handle_t adc_cali_handle;
// AW9364 handle (brightness controller)
static aw9364_dev_handle_t aw9364_dev_hdl;

// initialize the LCD I80 bus
static void init_lcd_i80_bus(esp_lcd_panel_io_handle_t *io_handle) {
    ESP_LOGI(TAG, "Initializing Intel 8080 bus...");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .dc_gpio_num = LCD_PIN_NUM_DC,
            .wr_gpio_num = LCD_PIN_NUM_PCLK,
            .data_gpio_nums = {
                    LCD_PIN_NUM_DATA0,
                    LCD_PIN_NUM_DATA1,
                    LCD_PIN_NUM_DATA2,
                    LCD_PIN_NUM_DATA3,
                    LCD_PIN_NUM_DATA4,
                    LCD_PIN_NUM_DATA5,
                    LCD_PIN_NUM_DATA6,
                    LCD_PIN_NUM_DATA7
            },
            .bus_width = LCD_I80_BUS_WIDTH,
            // transfer 100 lines of pixels (assume pixel is RGB565) at most in one transaction
            .max_transfer_bytes = LCD_H_RES * 100 * sizeof(uint16_t),
            .psram_trans_align = LCD_PSRAM_TRANS_ALIGN,
            .sram_trans_align = LCD_SRAM_TRANS_ALIGN,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    esp_lcd_panel_io_i80_config_t io_config = {
            .cs_gpio_num = LCD_PIN_NUM_CS,
            .pclk_hz = LCD_PIXEL_CLOCK_HZ,
            .trans_queue_depth = LCD_I80_TRANS_QUEUE_SIZE,
            .dc_levels = {
                    .dc_idle_level = LCD_I80_DC_CMD_LEVEL,
                    .dc_cmd_level = LCD_I80_DC_CMD_LEVEL,
                    .dc_dummy_level = LCD_I80_DC_DUMMY_LEVEL,
                    .dc_data_level = LCD_I80_DC_DATA_LEVEL,
            },
//            .flags = {
//                    .swap_color_bytes = !LV_COLOR_16_SWAP, // Swap can be done in LvGL (default) or DMA
//            },
//            .user_ctx = user_ctx,
            .lcd_cmd_bits = LCD_CMD_BITS,
            .lcd_param_bits = LCD_PARAM_BITS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, io_handle));
}

static void init_lcd_panel(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t *panel) {
    esp_lcd_panel_handle_t panel_handle = NULL;

    ESP_LOGI(TAG, "Initializing ST7789 LCD Driver...");
    esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = LCD_PIN_NUM_RST,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);

    esp_lcd_panel_invert_color(panel_handle, true);

    // landscape, buttons on left, screen on right
    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, false, true);
    esp_lcd_panel_set_gap(panel_handle, 0, 35);

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    *panel = panel_handle;
}

static void init_battery_adc() {
    if (adc_handle == NULL) {
        /* Initialize ADC */
        const adc_oneshot_unit_init_cfg_t adc_cfg = {
                .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &adc_handle));
    }
}

static void init_battery_monitor() {
    ESP_LOGI(TAG, "Configuring battery monitor...");
    /* Initialize ADC and get ADC handle */
    init_battery_adc();

    /* Init ADC channels */
    const adc_oneshot_chan_cfg_t adc_chan_cfg = {
            .bitwidth = ADC_BITWIDTH_DEFAULT,
            .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_3, &adc_chan_cfg));

    /* ESP32-S3 supports Curve Fitting calibration scheme */
    const adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle));
}


int get_battery_voltage() {
    int voltage, adc_raw;

    assert(adc_handle);
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_3, &adc_raw));
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage));
    return voltage * 2;
}

double volts_to_percentage(double volts) {
    // equation based on https://electronics.stackexchange.com/a/551667
    return 123 - ((double) 123 / pow((1 + pow(((double) volts / 3.7), 80)), 0.165));
}

int get_battery_percentage() {
    return (int) ceil(volts_to_percentage((double)get_battery_voltage()/1000));
}

bool usb_power_voltage(int milliVolts) {
    return ceilf((float) (milliVolts - 100) / 1000) == 5.0;
}

bool usb_power_connected() {
    return usb_power_voltage((double)get_battery_voltage());
}

static void lcd_power_init(void) {
    ESP_LOGI(TAG, "Configuring LCD PWR GPIO...");
    gpio_config_t lcd_pwr_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << LCD_PIN_NUM_PWR
    };
    ESP_ERROR_CHECK(gpio_config(&lcd_pwr_gpio_config));
    gpio_set_level(LCD_PIN_NUM_PWR, LCD_PWR_ON_LEVEL);

    ESP_LOGI(TAG, "Configuring LCD RD GPIO...");
    gpio_config_t lcd_rd_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pin_bit_mask = 1ULL << LCD_PIN_NUM_RD
    };
    ESP_ERROR_CHECK(gpio_config(&lcd_rd_gpio_config));

}

static void lcd_brightness_init(void) {
    ESP_LOGI(TAG, "Configuring LCD Brightness...");
    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t lcd_backlight_channel = {
            .gpio_num = LCD_PIN_NUM_BK_LIGHT,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LCD_BK_LIGHT_LEDC_CH,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = 1,
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

    ESP_LOGI(TAG, "aw9364_init");
    esp_err_t err = aw9364_init(&lcd_backlight_channel, &lcd_backlight_timer, &aw9364_dev_hdl);
    ESP_ERROR_CHECK(err);
}

void lcd_set_brightness_step(uint8_t brightness_step) {
    ESP_ERROR_CHECK(aw9364_set_brightness_step(aw9364_dev_hdl, brightness_step, 0));
}

void lcd_set_brightness_step_fade(uint8_t brightness_step, uint32_t fade_time_ms) {
    ESP_ERROR_CHECK(aw9364_set_brightness_step(aw9364_dev_hdl, brightness_step, fade_time_ms));
}

void lcd_set_brightness_pct(uint8_t brightness_percent) {
    ESP_ERROR_CHECK(aw9364_set_brightness_pct(aw9364_dev_hdl, brightness_percent, 0));
}

void lcd_set_brightness_pct_fade(uint8_t brightness_percent, uint32_t fade_time_ms) {
    ESP_ERROR_CHECK(aw9364_set_brightness_pct(aw9364_dev_hdl, brightness_percent, fade_time_ms));
}

void lcd_increment_brightness_step() {
    ESP_ERROR_CHECK(aw9364_increment_brightness_step(aw9364_dev_hdl, 0));
}

void lcd_decrement_brightness_step() {
    ESP_ERROR_CHECK(aw9364_decrement_brightness_step(aw9364_dev_hdl, 0));
}

uint8_t lcd_get_brightness_step() {
    return aw9364_get_brightness_step(aw9364_dev_hdl);
}

uint8_t lcd_get_brightness_pct() {
    return aw9364_get_brightness_pct(aw9364_dev_hdl);
}

static lv_disp_t *lcd_lvgl_add_disp(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t panel_handle) {
    ESP_LOGI(TAG, "Adding display driver to lvgl port...");
    /* Add LCD screen */
    const lvgl_port_display_cfg_t disp_cfg = {
            .io_handle = io_handle,
            .panel_handle = panel_handle,
            .buffer_size = LVGL_BUFFER_SIZE,
            .double_buffer = true,
            .hres = LCD_H_RES,
            .vres = LCD_V_RES,
            .monochrome = false,
            .color_format = LV_COLOR_FORMAT_RGB565,
            /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
            .rotation = {
                    .swap_xy = true,
                    .mirror_x = false,
                    .mirror_y = true,
            },
            .flags = {
                    .buff_spiram = true,
                    .swap_bytes = true,
            }
    };
    return lvgl_port_add_disp(&disp_cfg);
}

void lcd_init(lv_disp_t **disp_handle, bool backlight_on) {
    /* lvgl_port initialization */
    const lvgl_port_cfg_t lvgl_cfg = {
            .task_priority = LVGL_TASK_PRIORITY,
            .task_stack = LVGL_TASK_STACK_SIZE,
            .task_affinity = 1,
            .task_max_sleep_ms = LVGL_MAX_SLEEP_MS,
            .timer_period_ms = LVGL_TICK_PERIOD_MS

    };
    esp_err_t err = lvgl_port_init(&lvgl_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error initializing lvgl port!");
    }

    lcd_power_init();
    lcd_brightness_init();

    init_battery_monitor();

    /* LCD IO */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    init_lcd_i80_bus(&io_handle);

    /* LCD driver initialization */
    esp_lcd_panel_handle_t panel_handle = NULL;
    init_lcd_panel(io_handle, &panel_handle);

    lv_disp_t *disp_hdl = lcd_lvgl_add_disp(io_handle, panel_handle);

    *disp_handle = disp_hdl;

    if (backlight_on) {
        lcd_set_brightness_step(100);
    }
}
// SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <math.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "t_display_s3.h"
#include "iot_button.h"
#include "button_gpio.h"


#if defined CONFIG_LV_USE_DEMO_BENCHMARK
#include "lvgl__lvgl/demos/benchmark/lv_demo_benchmark.h"
#elif defined CONFIG_LV_USE_DEMO_STRESS
#include "lvgl__lvgl/demos/stress/lv_demo_stress.h"
#endif

#define TAG "ESP-IDF-T-Display-S3-Example"

#define NUM_BUTTONS 2

// gpio nums of the buttons
static gpio_num_t btn_gpio_nums[NUM_BUTTONS] = {
        BTN_PIN_NUM_1,
        BTN_PIN_NUM_2,
};

// store button handles
button_handle_t btn_handles[NUM_BUTTONS];

// used for setup_test_ui() function
char *power_icon = LV_SYMBOL_POWER;
char *lbl_btn_1_value = "";
char *lbl_btn_2_value = "";
int battery_voltage;
int battery_percentage;
bool on_usb_power = false;
int current_battery_symbol_idx = 0;
char *battery_symbols[5] = {
        LV_SYMBOL_BATTERY_EMPTY,
        LV_SYMBOL_BATTERY_1,
        LV_SYMBOL_BATTERY_2,
        LV_SYMBOL_BATTERY_3,
        LV_SYMBOL_BATTERY_FULL
};

TaskHandle_t lcd_brightness_task_hdl;
esp_timer_handle_t lcd_brightness_timer_hdl;

// lvgl ui elements
lv_obj_t *side_bar;
lv_obj_t *top_bar;
lv_obj_t *bottom_bar;
lv_obj_t *lbl_power_mode;
lv_obj_t *lbl_battery_pct;
lv_obj_t *lbl_voltage;
lv_obj_t *lbl_power_icon;
lv_obj_t *lbl_btn_1;
lv_obj_t *lbl_btn_2;
lv_obj_t *screen_brightness_slider;
lv_obj_t *screen_brightness;

static int get_button_idx(button_handle_t btn_hdl) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if(btn_handles[i]==btn_hdl) {
            return i;
        }
    }
    return -1;
}

// Callback function when buttons are released
static void button_event_handler_cb(void *arg, void *usr_data) {
    button_handle_t button_hdl = (button_handle_t) arg;
    button_event_t btn_event = iot_button_get_event(button_hdl);
    int btn_idx = get_button_idx(button_hdl);

    ESP_LOGI(TAG, "button %d, event %s", btn_idx, iot_button_get_event_str(btn_event));
    switch (btn_event) {
        case BUTTON_PRESS_DOWN:
        case BUTTON_LONG_PRESS_START:
        case BUTTON_LONG_PRESS_HOLD:
            if(btn_idx == 0) {
                lbl_btn_1_value = LV_SYMBOL_LEFT;
                lcd_increment_brightness_step();
            }
            if(btn_idx == 1) {
                lbl_btn_2_value = LV_SYMBOL_LEFT;
                lcd_decrement_brightness_step();
            }
            break;
        default:
            if(btn_idx == 0) {
                lbl_btn_1_value = "";;
            }
            if(btn_idx == 1) {
                lbl_btn_2_value = "";;
            }
    }
}

// Function to configure the boo & GPIO14 buttons using espressif/button component
static void setup_buttons() {
    for (size_t i = 0; i < NUM_BUTTONS; i++) {
        ESP_LOGI(TAG, "Configuring button %ld", ((int32_t) i) + 1);
        button_config_t btn_cfg = {0};
        button_gpio_config_t btn_gpio_cfg = {
                .gpio_num = (int32_t) btn_gpio_nums[i],
                .active_level = 0,
        };
        button_handle_t btn_handle;
        esp_err_t err = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "error iot_button_new_gpio_device [button %d]: %s", i + 1, esp_err_to_name(err));
        }
        if (NULL == btn_handle) {
            ESP_LOGE(TAG, "Button %d create failed", i + 1);
        }
        err = iot_button_register_cb(btn_handle, BUTTON_PRESS_DOWN, NULL, button_event_handler_cb, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "error iot_button_register_cb [button %d]: %s", i + 1, esp_err_to_name(err));
        }
        err = iot_button_register_cb(btn_handle, BUTTON_PRESS_UP, NULL, button_event_handler_cb, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "error iot_button_register_cb [button %d]: %s", i + 1, esp_err_to_name(err));
        }
        button_event_args_t long_press_start_args = {
                .long_press = {
                        .press_time = 300,
                }
        };
        err = iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_START, &long_press_start_args, button_event_handler_cb, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "error iot_button_register_cb [button %d]: %s", i + 1, esp_err_to_name(err));
        }
        err = iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_HOLD, NULL, button_event_handler_cb, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "error iot_button_register_cb [button %d]: %s", i + 1, esp_err_to_name(err));
        }
        btn_handles[i] = btn_handle;
    }
}


void ui_init() {
    side_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_width(side_bar, 50);
    lv_obj_set_height(side_bar, LCD_V_RES);
    lv_obj_remove_flag(side_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(side_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(side_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(side_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl_btn_1 = lv_label_create(side_bar);
    lv_obj_align(lbl_btn_1, LV_ALIGN_TOP_MID, 0, 0);

    lbl_btn_2 = lv_label_create(side_bar);
    lv_obj_align(lbl_btn_2, LV_ALIGN_BOTTOM_MID, 0, 0);

    top_bar = lv_obj_create(lv_screen_active());
    lv_obj_align(top_bar, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_width(top_bar, LCD_H_RES - 50);
    lv_obj_set_height(top_bar, 50);
    lv_obj_remove_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(top_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(top_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(top_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl_power_mode = lv_label_create(top_bar);
    lv_obj_align(lbl_power_mode, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_voltage = lv_label_create(top_bar);
    lv_obj_align(lbl_voltage, LV_ALIGN_TOP_RIGHT, 0, 0);

    lbl_power_icon = lv_label_create(top_bar);
    lv_obj_align(lbl_power_icon, LV_ALIGN_BOTTOM_RIGHT, 0, 5);

    lbl_battery_pct = lv_label_create(top_bar);
    lv_obj_align(lbl_battery_pct, LV_ALIGN_BOTTOM_LEFT, 0, 5);

    bottom_bar = lv_obj_create(lv_screen_active());
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_width(bottom_bar, LCD_H_RES - 50);
    lv_obj_set_height(bottom_bar, 50);
    lv_obj_remove_flag(bottom_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(bottom_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bottom_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bottom_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);


    screen_brightness_slider = lv_slider_create(lv_screen_active());
    lv_obj_set_size(screen_brightness_slider, LCD_H_RES - 100, 25);
    lv_obj_align(screen_brightness_slider, LV_ALIGN_CENTER, 30, 0);
    lv_slider_set_range(screen_brightness_slider, 0, 16);
    screen_brightness = lv_label_create(screen_brightness_slider);
    lv_obj_align(screen_brightness, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(screen_brightness, "Brightness");
    lv_slider_set_value(screen_brightness_slider, lcd_get_brightness_step(), LV_ANIM_OFF);
}

static void update_hw_info_timer_cb(void *arg) {
    battery_voltage = get_battery_voltage();
    on_usb_power = usb_power_voltage(battery_voltage);
    battery_percentage = (int) volts_to_percentage((double) battery_voltage / 1000);
}

static void update_ui() {
    lv_label_set_text(lbl_btn_1, lbl_btn_1_value);
    lv_label_set_text(lbl_btn_2, lbl_btn_2_value);
    lv_slider_set_value(screen_brightness_slider, lcd_get_brightness_step(), LV_ANIM_OFF);
    if (on_usb_power) {
        power_icon = LV_SYMBOL_USB;
        lv_label_set_text(lbl_power_mode, "USB Power");
        lv_label_set_text(lbl_battery_pct, "----------");
    } else {
        power_icon = battery_symbols[current_battery_symbol_idx];
        if (battery_percentage > 100) {
            battery_percentage = 100;
        }
        lv_label_set_text(lbl_power_mode, "Battery Power");
        lv_label_set_text_fmt(lbl_battery_pct, "Charge Level: %d %%", battery_percentage);
    }
    lv_label_set_text_fmt(lbl_voltage, "%d mV", battery_voltage);


    if (battery_percentage > 75 && battery_percentage <= 100) {
        current_battery_symbol_idx = 4;
    } else if (battery_percentage > 50 && battery_percentage <= 75) {
        current_battery_symbol_idx = 3;
    } else if (battery_percentage > 25 && battery_percentage <= 50) {
        current_battery_symbol_idx = 2;
    } else if (battery_percentage > 10 && battery_percentage <= 25) {
        current_battery_symbol_idx = 1;
    } else {
        current_battery_symbol_idx = 0;
    }

    lv_label_set_text(lbl_power_icon, power_icon);
}


static void ui_update_task(void *pvParam) {
    // setup the test ui
    lvgl_port_lock(0);
    ui_init();
    lvgl_port_unlock();

    while (1) {
        // update the ui every 50 milliseconds
        //vTaskDelay(pdMS_TO_TICKS(50));
        if (lvgl_port_lock(0)) {
            // update ui under lvgl semaphore lock
            update_ui();
            lvgl_port_unlock();
        }
    }

    // a freeRTOS task should never return ^^^
}

// increment lvgl timer
static void lvgl_ticker_timer_cb(void *arg)
{
    /* Tell LVGL how many milliseconds have elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void ui_lvgl_demos_task(void *pvParam) {
    // this is a workaround to get the lvgl demos working with esp-idf lvgl port
    // lvgl demos run in lv timers and so lvgl port locks are not called resulting in errors
    ESP_LOGI(TAG, "starting ui_lvgl_demos_task");

    // first acquire port lock
    lvgl_port_lock(0);

    // stop lvgl port (lvgl tick timer)
    if(lvgl_port_stop() == ESP_OK) {
        ESP_LOGI(TAG, "lvgl_port_stop ok");
    } else {
        ESP_LOGE(TAG, "lvgl_port_stop error!");
    }

    // since lv ticker is stopped by lvgl_port_stop, we need to create a timer for tick
    lv_timer_enable(true);
    const esp_timer_create_args_t lvgl_tick_timer_args = {
            .callback = &lvgl_ticker_timer_cb,
            .name = "LVGL tick",
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&lvgl_tick_timer_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000);


    // start the lvgl demos
#if defined CONFIG_LV_USE_DEMO_STRESS
    // if you specified CONFIG_LV_USE_DEMO_STRESS in sdkconfig, it will run lv_demo_stress
    lv_demo_stress();
#elif defined CONFIG_LV_USE_DEMO_BENCHMARK
    // if you specified CONFIG_LV_USE_DEMO_BENCHMARK in sdkconfig, it will run lv_demo_benchmark
    lv_demo_benchmark();
#endif

    // infinite loop that handles the lv_timer_handler api calls
    // similar logic to lvgl port
    uint32_t task_delay_ms = 0;
    for(;;) {
        lvgl_port_lock(0);
        if (lv_display_get_default()) {
            task_delay_ms = lv_timer_handler();
        } else {
            task_delay_ms = 1;
        }
        if(task_delay_ms == LV_NO_TIMER_READY) {
            task_delay_ms = LVGL_MAX_SLEEP_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
        lvgl_port_unlock();
    }
}

void app_main(void) {
    // LVGL display handle
    static lv_display_t *disp_handle;

    // initialize the LCD
    // don't turn on backlight yet - demo of gradual brightness increase is shown below
    // otherwise you can set it to true to turn on the backlight at lcd init
    lcd_init(&disp_handle, false);

#if defined CONFIG_LV_USE_DEMO_BENCHMARK || defined CONFIG_LV_USE_DEMO_STRESS
    lcd_set_brightness_step(100);
    // configure a FreeRTOS task, pinned to the second core (core 0 should be used for hw such as wifi, bt etc)
    TaskHandle_t lvgl_demo_task_hdl = NULL;
    xTaskCreatePinnedToCore(ui_lvgl_demos_task, "ui_lvgl_demos_task", 4096 * 2, NULL, 0, &lvgl_demo_task_hdl, 1);
    vTaskDelay(pdMS_TO_TICKS(5000));
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
#else
    // otherwise it will show my example

    // configure the buttons
    setup_buttons();

    // Configure a periodic timer to update the battery voltage, brightness level etc
    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &update_hw_info_timer_cb,
            .name = "update_hw_info_timer"
    };

    esp_timer_handle_t update_hw_info_timer_handle;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &update_hw_info_timer_handle));
    // update the hw info 250 milliseconds
    ESP_ERROR_CHECK(esp_timer_start_periodic(update_hw_info_timer_handle, 250 * 1000));

    // configure a FreeRTOS task, pinned to the second core (core 0 should be used for hw such as wifi, bt etc)
    xTaskCreatePinnedToCore(ui_update_task, "update_ui", 4096 * 2, NULL, 0, NULL, 1);

    // demonstrate the lcd brightness fade using aw9364 driver
    lcd_set_brightness_pct_fade(100,3000);
//    vTaskDelay(pdMS_TO_TICKS(100));

    // de-initialize lcd and other components
    // lvgl_port_remove_disp(disp_handle);

#endif
}
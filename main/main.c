#include <stdio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "t_display_s3.h"
#include "iot_button.h"

#if defined CONFIG_LV_USE_DEMO_BENCHMARK
#include "lvgl/demos/benchmark/lv_demo_benchmark.h"
#elif defined CONFIG_LV_USE_DEMO_STRESS
#include "lvgl/demos/stress/lv_demo_stress.h"
#endif

#define TAG "ESP-IDF-T-Display-S3-Example"

// gpio nums of the buttons
static gpio_num_t btn_gpio_nums[2] = {
        BTN_PIN_NUM_1,
        BTN_PIN_NUM_2,
};

// store button handles
button_handle_t btn_handles[2];

// keep a track of button press states
bool btn_1_pressed = false;
bool btn_2_pressed = false;

// used for setup_test_ui() function
char *power_icon = LV_SYMBOL_POWER;
char *lbl_btn_1_value = "";
char *lbl_btn_2_value = "";
int battery_voltage;
int battery_percentage;
int last_screen_brightness_step = 16;
int screen_brightness_step = 16;
int current_battery_symbol_idx = 0;
char *battery_symbols[5] = {
        LV_SYMBOL_BATTERY_EMPTY,
        LV_SYMBOL_BATTERY_1,
        LV_SYMBOL_BATTERY_2,
        LV_SYMBOL_BATTERY_3,
        LV_SYMBOL_BATTERY_FULL
};

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

// Callback function when buttons are pressed down
static void button_press_down_cb(void *arg, void *usr_data) {
    button_handle_t button = (button_handle_t) arg;
    if (button == btn_handles[0]) {
//        ESP_LOGI(TAG, "0 BUTTON_PRESS_DOWN");
        btn_1_pressed = true;
    }
    if (button == btn_handles[1]) {
//        ESP_LOGI(TAG, "1 BUTTON_PRESS_DOWN");
        btn_2_pressed = true;
    }
}

// Callback function when buttons are released
static void button_press_up_cb(void *arg, void *usr_data) {
    button_handle_t button = (button_handle_t) arg;
    if (button == btn_handles[0]) {
//        ESP_LOGI(TAG, "0 BUTTON_PRESS_UP");
        btn_1_pressed = false;
    }
    if (button == btn_handles[1]) {
//        ESP_LOGI(TAG, "1 BUTTON_PRESS_UP");
        btn_2_pressed = false;
    }
}

// Function to configure the boo & GPIO14 buttons using espressif/button component
static void setup_buttons() {
    for (size_t i = 0; i < 2; i++) {
        ESP_LOGI(TAG, "Configuring button %ld", ((int32_t) i) + 1);
        button_config_t gpio_btn_cfg = {
                .type = BUTTON_TYPE_GPIO,
                .short_press_time = 500,
                .gpio_button_config = {
                        .gpio_num = (int32_t) btn_gpio_nums[i],
                        .active_level = 0,
                },
        };
        button_handle_t btn_handle = iot_button_create(&gpio_btn_cfg);
        if (NULL == btn_handle) {
            ESP_LOGE(TAG, "Button %d create failed", i + 1);
        }
        esp_err_t err = iot_button_register_cb(btn_handle, BUTTON_PRESS_DOWN, button_press_down_cb, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "error iot_button_register_cb [button %d]: %s", i + 1, esp_err_to_name(err));
        }
        err = iot_button_register_cb(btn_handle, BUTTON_PRESS_UP, button_press_up_cb, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "error iot_button_register_cb [button %d]: %s", i + 1, esp_err_to_name(err));
        }
        btn_handles[i] = btn_handle;
    }
}


void setup_test_ui() {
    lvgl_port_lock(0);

    side_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_width(side_bar, 50);
    lv_obj_set_height(side_bar, LCD_V_RES);
    lv_obj_clear_flag(side_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(side_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(side_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(side_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl_btn_1 = lv_label_create(side_bar);
    lv_obj_align(lbl_btn_1, LV_ALIGN_TOP_MID, 0, 0);

    lbl_btn_2 = lv_label_create(side_bar);
    lv_obj_align(lbl_btn_2, LV_ALIGN_BOTTOM_MID, 0, 0);

    top_bar = lv_obj_create(lv_scr_act());
    lv_obj_align(top_bar, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_width(top_bar, LCD_H_RES - 50);
    lv_obj_set_height(top_bar, 50);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);
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

    bottom_bar = lv_obj_create(lv_scr_act());
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_width(bottom_bar, LCD_H_RES - 50);
    lv_obj_set_height(bottom_bar, 50);
    lv_obj_clear_flag(bottom_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(bottom_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bottom_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bottom_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);


    screen_brightness_slider = lv_slider_create(lv_scr_act());
    lv_obj_set_size(screen_brightness_slider, LCD_H_RES - 100, 25);
    lv_obj_align(screen_brightness_slider, LV_ALIGN_CENTER, 30, 0);
    lv_slider_set_range(screen_brightness_slider, 0, 16);
    screen_brightness = lv_label_create(screen_brightness_slider);
    lv_obj_align(screen_brightness, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(screen_brightness, "Brightness");
    lv_slider_set_value(screen_brightness_slider, screen_brightness_step, LV_ANIM_OFF);

    lvgl_port_unlock();
}

static void update_hw_info_timer_cb(void *arg) {
    if (btn_1_pressed) {
        lbl_btn_1_value = LV_SYMBOL_LEFT;
        if (screen_brightness_step < 16) {
            screen_brightness_step++;
        }
        if (screen_brightness_step > 16) {
            screen_brightness_step = 16;
        }
    } else {
        lbl_btn_1_value = "";
    }
    if (btn_2_pressed) {
        lbl_btn_2_value = LV_SYMBOL_LEFT;
        if (screen_brightness_step > 0) {
            screen_brightness_step--;
        }
        if (screen_brightness_step < 0) {
            screen_brightness_step = 0;
        }
    } else {
        lbl_btn_2_value = "";
    }

    if (last_screen_brightness_step != screen_brightness_step) {
        lcd_brightness_set(ceil(screen_brightness_step * (int) (100 / (float) 16)));
    }
    last_screen_brightness_step = screen_brightness_step;

    battery_voltage = get_battery_voltage();
    battery_percentage = (int) volts_to_percentage((double) battery_voltage / 1000);
}

static void update_ui() {
    lv_label_set_text(lbl_btn_1, lbl_btn_1_value);
    lv_label_set_text(lbl_btn_2, lbl_btn_2_value);
    lv_slider_set_value(screen_brightness_slider, screen_brightness_step, LV_ANIM_OFF);
    if (battery_voltage - 30 > NO_BAT_MILLIVOLTS) {
        power_icon = LV_SYMBOL_USB;
        lv_label_set_text(lbl_power_mode, "No battery");
        lv_label_set_text(lbl_battery_pct, "----------");
    } else {
        power_icon = battery_symbols[current_battery_symbol_idx];
        if (battery_voltage > BAT_CHARGE_MILLIVOLTS) {
            lv_label_set_text(lbl_power_mode, "Battery Charging");
            if (battery_percentage > 100) {
                battery_percentage = 100;
            }
            lv_label_set_text_fmt(lbl_battery_pct, "Charge Level: %d %%", battery_percentage);
        } else {
            lv_label_set_text(lbl_power_mode, "Battery Discharging");
            lv_label_set_text_fmt(lbl_battery_pct, "Charge Level: %d %%", battery_percentage);
        }
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
    setup_test_ui();

    // Configure a periodic timer to update the battery voltage, brightness level etc
    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &update_hw_info_timer_cb,
            .name = "update_hw_info_timer"
    };

    esp_timer_handle_t ui_update_timer_handle;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &ui_update_timer_handle));
    // update the hw info 250 milliseconds
    ESP_ERROR_CHECK(esp_timer_start_periodic(ui_update_timer_handle, 250 * 1000));

    while (1) {
        // update the ui every 50 milliseconds
        vTaskDelay(pdMS_TO_TICKS(50));
        if (lvgl_port_lock(0)) {
            // update ui under lvgl semaphore lock
            update_ui();
            lvgl_port_unlock();
        }
    }

    // a freeRTOS task should never return ^^^
}

void app_main(void) {
    // LVGL display driver
    static lv_disp_drv_t disp_drv;
    // LVGL display handle
    static lv_disp_t *disp_handle;

    // initialize the LCD
    // don't turn on backlight yet - demo of gradual brightness increase is shown below
    // otherwise you can set it to true to turn on the backlight at lcd init
    lcd_init(disp_drv, &disp_handle, false);

#if defined CONFIG_LV_USE_DEMO_BENCHMARK
    // if you specified CONFIG_LV_USE_DEMO_BENCHMARK in sdkconfig, it will run lv_demo_benchmark
    lv_demo_benchmark();

#elif defined CONFIG_LV_USE_DEMO_STRESS
    // if you specified CONFIG_LV_USE_DEMO_STRESS in sdkconfig, it will run lv_demo_stress
    lv_demo_stress();
#else
    // otherwise it will show my example

    // configure the buttons
    setup_buttons();

    // configure a FreeRTOS task, pinned to the second core (core 0 should be used for hw such as wifi, bt etc)
    xTaskCreatePinnedToCore(ui_update_task, "update_ui", 4096 * 2, NULL, 0, NULL, 1);

    // this is a blocking action to demonstrate the lcd brightness fade in using a simple loop
    vTaskDelay(pdMS_TO_TICKS(100));
    screen_brightness_step = 0;
    last_screen_brightness_step = 0;
    lcd_brightness_set(screen_brightness_step);
    vTaskDelay(pdMS_TO_TICKS(50));

    for (int i = 0; i <= 16; i++) {
        screen_brightness_step++;
        last_screen_brightness_step++;
        lcd_brightness_set(ceil(screen_brightness_step * (int) (100 / (float) 16)));
        vTaskDelay(pdMS_TO_TICKS(70));
    }

    // de-initialize lcd and other components
    // lvgl_port_remove_disp(disp_handle);

#endif
}
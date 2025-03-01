#include <stdio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "t_display_s3.h"
#include "iot_button.h"
#include "ui/ui.h"

#define TAG "__UI_PROJECT_NAME__"

// gpio nums of the buttons
static gpio_num_t btn_gpio_nums[2] = {
        BTN_PIN_NUM_1,
        BTN_PIN_NUM_2,
};

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

static void update_hw_info_timer_cb(void *arg) {
    battery_voltage = get_battery_voltage();
    on_usb_power = usb_power_voltage(battery_voltage);
    battery_percentage = (int) volts_to_percentage((double) battery_voltage / 1000);
}

static void update_ui() {
    // Perform your ui updates here

    ESP_LOGI(TAG, "update_ui function call");
}


static void ui_update_task(void *pvParam) {
    // setup the test ui
    lvgl_port_lock(0);
    ui_init();
    lvgl_port_unlock();

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
    // LVGL display handle
    static lv_display_t *disp_handle;

    // initialize the LCD
    // don't turn on backlight yet - demo of gradual brightness increase is shown below
    // otherwise you can set it to true to turn on the backlight at lcd init
    lcd_init(&disp_handle, false);


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

    // de-initialize lcd and other components
    // lvgl_port_remove_disp(disp_handle);
}
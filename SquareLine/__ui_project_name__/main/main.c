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

// store button handles
button_handle_t btn_handles[2];

// keep a track of button press states
bool btn_1_pressed = false;
bool btn_2_pressed = false;

// used for setup_test_ui() function
int battery_voltage;
int battery_percentage;
bool on_usb_power = false;
int last_screen_brightness_step = 16;
int screen_brightness_step = 16;
int current_battery_symbol_idx = 0;

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


static void update_hw_info_timer_cb(void *arg) {
    if (btn_1_pressed) {
        if (screen_brightness_step < 16) {
            screen_brightness_step++;
        }
        if (screen_brightness_step > 16) {
            screen_brightness_step = 16;
        }
    }
    if (btn_2_pressed) {
        if (screen_brightness_step > 0) {
            screen_brightness_step--;
        }
        if (screen_brightness_step < 0) {
            screen_brightness_step = 0;
        }
    }

    if (last_screen_brightness_step != screen_brightness_step) {
        lcd_brightness_set(ceil(screen_brightness_step * (int) (100 / (float) 16)));
    }
    last_screen_brightness_step = screen_brightness_step;

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
    // LVGL display driver
    static lv_disp_drv_t disp_drv;
    // LVGL display handle
    static lv_disp_t *disp_handle;

    // initialize the LCD
    // don't turn on backlight yet - demo of gradual brightness increase is shown below
    // otherwise you can set it to true to turn on the backlight at lcd init
    lcd_init(disp_drv, &disp_handle, false);


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
}
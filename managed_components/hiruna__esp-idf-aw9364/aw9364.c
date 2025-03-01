/*
 * SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "aw9364.h"
#include "esp_log.h"

static const char *TAG = "AW9364";

struct aw9364_dev_t {
    ledc_channel_t channel;
    ledc_timer_t timer_num;
    ledc_timer_bit_t duty_resolution;
    uint32_t max_duty_resolution;
    uint8_t brightness_step;
};

typedef struct aw9364_dev_t aw9364_dev_t;

static uint32_t brightness_step_to_duty_cycle(aw9364_dev_t *dev, uint8_t brightness_step) {
    return brightness_step * dev->max_duty_resolution / AW9364_MAX_BRIGHTNESS_STEPS;
}

static uint8_t brightness_step_to_pct(uint8_t brightness_step) {
    return brightness_step * 100 / AW9364_MAX_BRIGHTNESS_STEPS;
}

static uint8_t brightness_pct_to_step(uint8_t brightness_pct) {
    return brightness_pct * AW9364_MAX_BRIGHTNESS_STEPS / 100;
}

static esp_err_t set_brightness(aw9364_dev_t *dev, uint32_t duty_cycle, uint32_t fade_time_ms) {
    esp_err_t err = ESP_FAIL;
    uint32_t fade_time = fade_time_ms;
    if (fade_time_ms > AW9364_MAX_FADE_TIME_MS) {
        fade_time = AW9364_MAX_FADE_TIME_MS;
    }
    if(fade_time > AW9364_MIN_FADE_TIME_MS) {
        err = ledc_set_fade_time_and_start(LEDC_LOW_SPEED_MODE, dev->channel, duty_cycle, fade_time, LEDC_FADE_NO_WAIT);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_set_fade_time_and_start error");
            return err;
        }
        return ESP_OK;
    }
    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, dev->channel, duty_cycle);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_set_duty error");
        return err;
    }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE,  dev->channel);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_update_duty error");
        return err;
    }
    return ESP_OK;
}

esp_err_t aw9364_init(const ledc_channel_config_t *ledc_channel_cfg, const ledc_timer_config_t *ledc_timer_cfg, aw9364_dev_t **out_hdl) {
    esp_err_t err = ESP_FAIL;
    if (ledc_channel_cfg == NULL || ledc_timer_cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    aw9364_dev_t *dev = (aw9364_dev_t *) malloc(sizeof(aw9364_dev_t));
    if (!dev) {
        return ESP_ERR_NO_MEM;
    }

    err = ledc_timer_config(ledc_timer_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config error");
        goto cleanup;
    }
    err = ledc_channel_config(ledc_channel_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config error");
        goto cleanup;
    }

    // Enable fade functionality
    err = ledc_fade_func_install(0);
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) { // ESP_ERR_INVALID_STATE = Fade function already installed
        ESP_LOGE(TAG, "ledc_fade_func_install error");
        goto cleanup;
    }
    dev->channel = ledc_channel_cfg->channel;
    dev->timer_num = ledc_timer_cfg->timer_num;
    dev->duty_resolution = ledc_timer_cfg->duty_resolution;
    dev->max_duty_resolution = ((1 << dev->duty_resolution) - 1);
    dev->brightness_step = 0;
    *out_hdl = dev;
    return ESP_OK;

    cleanup:
    free(dev);
    return err;
}

esp_err_t aw9364_set_brightness_step(aw9364_dev_t *dev, uint8_t step, uint32_t fade_time_ms) {
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t brightness_step = step;
    if (step > AW9364_MAX_BRIGHTNESS_STEPS) {
        brightness_step = AW9364_MAX_BRIGHTNESS_STEPS;
    }
    dev->brightness_step = brightness_step;
    uint32_t duty_cycle = brightness_step_to_duty_cycle(dev, brightness_step);
    return set_brightness(dev, duty_cycle, fade_time_ms);
}

esp_err_t aw9364_set_brightness_pct(aw9364_dev_t *dev, uint8_t pct, uint32_t fade_time_ms) {
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t brightness_pct = pct;
    if(brightness_pct > 100) {
        brightness_pct = 100;
    }
    uint8_t brightness_step = brightness_pct_to_step(brightness_pct);
    return aw9364_set_brightness_step(dev, brightness_step, fade_time_ms);
}

esp_err_t aw9364_increment_brightness_step(aw9364_dev_t *dev, uint32_t fade_time_ms) {
    uint8_t step = dev->brightness_step + 1;
    return aw9364_set_brightness_step(dev, step, fade_time_ms);
}

esp_err_t aw9364_decrement_brightness_step(aw9364_dev_t *dev, uint32_t fade_time_ms) {
    uint8_t step = dev->brightness_step > 0 ? dev->brightness_step - 1: 0;
    return aw9364_set_brightness_step(dev, step, fade_time_ms);
}

uint8_t aw9364_get_brightness_pct(aw9364_dev_t *dev) {
    if (!dev) {
        return 0;
    }
    return brightness_step_to_pct(dev->brightness_step);
}

uint8_t aw9364_get_brightness_step(aw9364_dev_t *dev) {
    if (!dev) {
        return 0;
    }
    return dev->brightness_step;
}

esp_err_t aw9364_deinit(aw9364_dev_t *dev) {
    if(dev) {
        ledc_fade_stop(LEDC_LOW_SPEED_MODE, dev->channel);
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, dev->timer_num);
        ledc_timer_config_t timer_cfg = {
                .speed_mode = LEDC_LOW_SPEED_MODE,
                .timer_num = dev->timer_num,
                .deconfigure = true,
        };
        ledc_timer_config(&timer_cfg);
        free(dev);
    }
    return ESP_OK;
}
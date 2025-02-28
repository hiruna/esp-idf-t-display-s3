/*
 * SPDX-FileCopyrightText: © 2025 Hiruna Wijesinghe <hiruna.kawinda@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <driver/ledc.h>

#define AW9364_MAX_BRIGHTNESS_STEPS 16
#define AW9364_MIN_FADE_TIME_MS 50
#define AW9364_MAX_FADE_TIME_MS 10000

/**
 * @brief AW9364 handle
 */
typedef struct aw9364_dev_t *aw9364_dev_handle_t;

/**
 * @brief Initialize AW9364 driver
 *
 * @param ledc_channel_cfg Pointer of LEDC timer configure struct
 * @param ledc_timer_cfg Pointer of LEDC channel configure struct
 * @param out_hdl Pointer of AW9364 handle
 *
 * @return
 *      - ESP_OK Success
 *      - ESP_ERR_INVALID_ARG Parameter error
 *      - ESP_ERR_NO_MEM Out of memory
 *      - ESP_FAIL Other failures
 */
esp_err_t aw9364_init(const ledc_channel_config_t *ledc_channel_cfg, const ledc_timer_config_t *ledc_timer_cfg, aw9364_dev_handle_t *out_hdl);

/**
 * @brief Set brightness using step value (0-16)
 *
 * @param dev AW9364 handle
 * @param step Brightness step value, range is [0..16]
 * @param fade_time_ms Duration of the brightness change/fade. If 0, brightness changes immediately
 *
 * @return
 *      - ESP_OK Success
 *      - ESP_ERR_INVALID_ARG Parameter error
 *      - ESP_FAIL Other failures
 */
esp_err_t aw9364_set_brightness_step(aw9364_dev_handle_t dev, uint8_t step, uint32_t fade_time_ms);

/**
 * @brief Set brightness using percentage value (0-100)
 *
 * @param dev AW9364 handle
 * @param pct Brightness percentage value, range is [0..100]
 * @param fade_time_ms Duration of the brightness change/fade. If 0, brightness changes immediately
 *
 * @return
 *      - ESP_OK Success
 *      - ESP_ERR_INVALID_ARG Parameter error
 *      - ESP_FAIL Other failures
 */
esp_err_t aw9364_set_brightness_pct(aw9364_dev_handle_t dev, uint8_t pct, uint32_t fade_time_ms);

/**
 * @brief Increase the brightness by 1 step
 *
 * @param dev AW9364 handle
 * @param fade_time_ms Duration of the brightness change/fade. If 0, brightness changes immediately
 *
 * @note fade_time_ms usually set to 0 for this function
 *
 * @return
 *      - ESP_OK Success
 *      - ESP_ERR_INVALID_ARG Parameter error
 *      - ESP_FAIL Other failures
 */
esp_err_t aw9364_increment_brightness_step(aw9364_dev_handle_t dev, uint32_t fade_time_ms);

/**
 * @brief Decrease the brightness by 1 step
 *
 * @param dev AW9364 handle
 * @param fade_time_ms Duration of the brightness change/fade. If 0, brightness changes immediately
 *
 * @note fade_time_ms usually set to 0 for this function
 *
 * @return
 *      - ESP_OK Success
 *      - ESP_ERR_INVALID_ARG Parameter error
 *      - ESP_FAIL Other failures
 */
esp_err_t aw9364_decrement_brightness_step(aw9364_dev_handle_t dev, uint32_t fade_time_ms);

/**
 * @brief Get current brightness level as a percentage
 *
 * @param dev AW9364 handle
 *
 * @return
 *      - Current percentage value of brightness
 *      - 0 is also returned if dev handle is NULL
 */
uint8_t aw9364_get_brightness_pct(aw9364_dev_handle_t dev);

/**
 * @brief Get current brightness level as a step value
 *
 * @param dev AW9364 handle
 *
 * @return
 *      - Current step value of brightness
 *      - 0 is also returned if dev handle is NULL
 */
uint8_t aw9364_get_brightness_step(aw9364_dev_handle_t dev);

/**
 * @brief De-initialize the driver by de-configuring ledc driver & freeing dev handle
 *
 * @param dev AW9364 handle
 *
 * @return
 *      - ESP_OK Success
 */
esp_err_t aw9364_deinit(aw9364_dev_handle_t dev);

#ifdef __cplusplus
} /*extern "C"*/
#endif
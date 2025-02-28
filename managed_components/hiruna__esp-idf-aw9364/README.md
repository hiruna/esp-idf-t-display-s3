# esp-idf-aw9364

[AW9364](https://www.awinic.com/en/productDetail/AW9364DNR) C Driver for ESP-IDF

Datasheet: https://doc.awinic.com/doc/202402/d1a46d65-a45d-494f-bfc9-3d6a6a93a860.pdf

## Tested Hardware & Software Framework(s)
* ST7789 display
* ESP32 S3
* ESP-IDF Version 5.5.x ([master branch](https://github.com/espressif/esp-idf))

## Capabilities

* 16-step brightness control using LEDC driver
* Allow user to set fade duration to gradually increase/decrease brightness
  * This is a non-blocking ledc fade api function call
* User can set brightness in step value (0-16) or as a percentage (0-100)
* User can retrieve currently set brightness

## Examples
* [lcd_brightness](./examples/lcd_brightness)
  * Demonstrates the brightness control with and without fade
* [esp-idf-t-display-s3](https://github.com/hiruna/esp-idf-t-display-s3)
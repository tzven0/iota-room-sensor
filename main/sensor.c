// Copyright 2021 IOTA Stiftung
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <stdlib.h>

#include "dht.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "sensor.h"

#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32C3
#include "driver/temp_sensor.h"
#define ENABLE_TEMP 1
#else
#define ENABLE_TEMP 0
#define TEMP_SENSOR_TYPE DHT_TYPE_DHT11
#define TEMP_SENSOR_GPIO_PIN 18
#endif

static const char *TAG = "TempSensor";

void init_tempsensor() {
#if ENABLE_TEMP
  // Initialize touch pad peripheral, it will start a timer to run a filter
  ESP_LOGI(TAG, "Initializing Temperature sensor");
  temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
  temp_sensor_get_config(&temp_sensor);
  ESP_LOGI(TAG, "default dac %d, clk_div %d", temp_sensor.dac_offset, temp_sensor.clk_div);
  temp_sensor.dac_offset = TSENS_DAC_DEFAULT;  // DEFAULT: range:-10℃ ~  80℃, error < 1℃.
  temp_sensor_set_config(temp_sensor);
#else
  ESP_LOGI(TAG, "Temperature sensor is DHT");
#endif
}

float get_temp() {
  struct sensor_readings_t sensor_reading;
  sensor_reading.temperature = -99.0;
  sensor_reading.humidity = -99.0;
#if ENABLE_TEMP
  float temp = 0.0;
  temp_sensor_start();
  vTaskDelay(1000 / portTICK_RATE_MS);
  temp_sensor_read_celsius(&temp);
  temp_sensor_stop();
  sensor_reading.temperature = temp;
#else
  static const dht_sensor_type_t sensor_type = TEMP_SENSOR_TYPE;
  static const gpio_num_t dht_gpio = TEMP_SENSOR_GPIO_PIN;
  int16_t temperature = 0;
  int16_t humidity = 0;
  esp_err_t temp_ret = dht_read_data(sensor_type, dht_gpio, &humidity, &temperature);
  if (temp_ret == ESP_OK) {
    ESP_LOGI(TAG, "Humidity: %d%% Temp: %dC", humidity / 10, temperature / 10);
    sensor_reading.humidity = humidity / 10;
    sensor_reading.temperature = temperature / 10;
  } else {
    ESP_LOGE(TAG, "Could not fetch sensor readings.");
  }
#endif
  return sensor_reading;
}

// Copyright 2021 IOTA Stiftung
// SPDX-License-Identifier: Apache-2.0

#pragma once

struct sensor_readings_t {
  float temperature, humidity;
};

void init_tempsensor();
float get_temp();
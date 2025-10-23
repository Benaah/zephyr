/*
 * Copyright (c) 2025 Kargo Chain
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENSOR_MANAGER_H_
#define SENSOR_MANAGER_H_

#include <zephyr/kernel.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sensor_data {
	int64_t timestamp;
	float temperature;
	float humidity;
	float accel_x;
	float accel_y;
	float accel_z;
	float latitude;
	float longitude;
	uint8_t gnss_valid;
};

/**
 * @brief Initialize sensor manager
 * @return 0 on success, negative errno on failure
 */
int sensor_manager_init(void);

/**
 * @brief Read all sensors
 * @param data Pointer to store sensor data
 * @return 0 on success, negative errno on failure
 */
int sensor_manager_read(struct sensor_data *data);

/**
 * @brief Get total number of readings taken
 * @return Reading count
 */
uint32_t sensor_manager_get_reading_count(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_MANAGER_H_ */

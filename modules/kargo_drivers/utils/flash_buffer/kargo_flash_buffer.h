/*
 * Copyright (c) 2025 Kargo Chain
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KARGO_FLASH_BUFFER_H_
#define KARGO_FLASH_BUFFER_H_

#include <zephyr/kernel.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sensor reading structure for buffering
 */
struct kargo_sensor_reading {
	int64_t timestamp;
	float temperature;
	float humidity;
	float accel_x;
	float accel_y;
	float accel_z;
	float latitude;
	float longitude;
	uint8_t gnss_valid;
	uint8_t reserved[3];
} __packed;

/**
 * @brief Initialize flash buffer subsystem
 * 
 * @return 0 on success, negative errno on failure
 */
int kargo_flash_buffer_init(void);

/**
 * @brief Store a sensor reading to flash buffer
 * 
 * @param reading Pointer to sensor reading structure
 * @return 0 on success, negative errno on failure
 */
int kargo_flash_buffer_store(const struct kargo_sensor_reading *reading);

/**
 * @brief Retrieve oldest buffered reading
 * 
 * @param reading Pointer to store retrieved reading
 * @return 0 on success, -ENODATA if buffer empty, negative errno on failure
 */
int kargo_flash_buffer_retrieve(struct kargo_sensor_reading *reading);

/**
 * @brief Get number of buffered readings
 * 
 * @return Number of readings in buffer
 */
uint16_t kargo_flash_buffer_count(void);

/**
 * @brief Clear all buffered readings
 * 
 * @return 0 on success, negative errno on failure
 */
int kargo_flash_buffer_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* KARGO_FLASH_BUFFER_H_ */

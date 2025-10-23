/*
 * Copyright (c) 2025 Kargo Chain
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MQTT_MANAGER_H_
#define MQTT_MANAGER_H_

#include <zephyr/kernel.h>
#include "sensor_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize MQTT manager
 * @return 0 on success, negative errno on failure
 */
int mqtt_manager_init(void);

/**
 * @brief Publish sensor data to MQTT broker
 * @param data Pointer to sensor data
 * @return 0 on success, negative errno on failure
 */
int mqtt_manager_publish(const struct sensor_data *data);

/**
 * @brief Check if MQTT is connected
 * @return true if connected, false otherwise
 */
bool mqtt_manager_is_connected(void);

/**
 * @brief Get number of buffered messages
 * @return Number of buffered messages
 */
uint16_t mqtt_manager_get_buffered_count(void);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_MANAGER_H_ */

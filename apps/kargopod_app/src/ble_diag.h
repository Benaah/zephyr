/*
 * Copyright (c) 2025 Kargo Chain
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BLE_DIAG_H_
#define BLE_DIAG_H_

#include <zephyr/kernel.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize BLE diagnostic mode
 * @return 0 on success, negative errno on failure
 */
int ble_diag_init(void);

/**
 * @brief Update diagnostic status
 * @param mqtt_connected MQTT connection status
 * @param reading_count Total reading count
 */
void ble_diag_update_status(bool mqtt_connected, uint32_t reading_count);

/**
 * @brief Check if BLE client is connected
 * @return true if connected, false otherwise
 */
bool ble_diag_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_DIAG_H_ */

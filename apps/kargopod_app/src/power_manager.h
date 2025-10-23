/*
 * Copyright (c) 2025 Kargo Chain
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef POWER_MANAGER_H_
#define POWER_MANAGER_H_

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize power manager
 * @return 0 on success, negative errno on failure
 */
int power_manager_init(void);

/**
 * @brief Enter deep sleep mode
 * @param duration_ms Sleep duration in milliseconds
 * @return 0 on success, negative errno on failure
 */
int power_manager_deep_sleep(uint32_t duration_ms);

/**
 * @brief Get battery voltage
 * @return Battery voltage in millivolts, negative on error
 */
int power_manager_get_battery_mv(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_MANAGER_H_ */

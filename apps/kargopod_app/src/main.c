/*
 * Copyright (c) 2025 Kargo Chain
 * SPDX-License-Identifier: Apache-2.0
 * 
 * KargoPod Main Application
 * Reads sensors every 15 minutes, buffers offline, publishes via MQTT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/watchdog.h>

#include "sensor_manager.h"
#include "mqtt_manager.h"
#include "power_manager.h"
#include "ble_diag.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Application configuration */
#define SENSOR_READ_INTERVAL_MS    (CONFIG_APP_SENSOR_READ_INTERVAL_MIN * 60 * 1000)
#define WATCHDOG_TIMEOUT_MS        300000  /* 5 minutes */

static const struct device *wdt_dev;
static int wdt_channel_id;

/* Function declarations */
static int init_watchdog(void);
static void main_loop(void);

int main(void)
{
	int ret;

	LOG_INF("========================================");
	LOG_INF("  KargoPod IoT Application");
	LOG_INF("  Version: 1.0.0");
	LOG_INF("  Build: " __DATE__ " " __TIME__);
	LOG_INF("========================================");

	/* Initialize watchdog */
	ret = init_watchdog();
	if (ret < 0) {
		LOG_ERR("Watchdog init failed: %d", ret);
		/* Continue without watchdog */
	}

	/* Initialize sensor manager */
	ret = sensor_manager_init();
	if (ret < 0) {
		LOG_ERR("Sensor manager init failed: %d", ret);
		return ret;
	}

	/* Initialize power manager */
	ret = power_manager_init();
	if (ret < 0) {
		LOG_ERR("Power manager init failed: %d", ret);
		return ret;
	}

	/* Initialize MQTT manager */
	ret = mqtt_manager_init();
	if (ret < 0) {
		LOG_ERR("MQTT manager init failed: %d", ret);
		/* Continue - will buffer readings */
	}

#ifdef CONFIG_APP_BLE_DIAG_ENABLED
	/* Initialize BLE diagnostic mode */
	ret = ble_diag_init();
	if (ret < 0) {
		LOG_WRN("BLE diagnostic init failed: %d", ret);
		/* Continue without BLE */
	}
#endif

	LOG_INF("All subsystems initialized");
	LOG_INF("Sensor read interval: %d minutes", CONFIG_APP_SENSOR_READ_INTERVAL_MIN);

	/* Enter main application loop */
	main_loop();

	/* Should never reach here */
	return 0;
}

static int init_watchdog(void)
{
	struct wdt_timeout_cfg wdt_config;

	wdt_dev = DEVICE_DT_GET(DT_ALIAS(watchdog0));
	if (!device_is_ready(wdt_dev)) {
		LOG_ERR("Watchdog device not ready");
		return -ENODEV;
	}

	wdt_config.flags = WDT_FLAG_RESET_SOC;
	wdt_config.window.min = 0U;
	wdt_config.window.max = WATCHDOG_TIMEOUT_MS;
	wdt_config.callback = NULL;

	wdt_channel_id = wdt_install_timeout(wdt_dev, &wdt_config);
	if (wdt_channel_id < 0) {
		LOG_ERR("Watchdog install failed: %d", wdt_channel_id);
		return wdt_channel_id;
	}

	wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);

	LOG_INF("Watchdog initialized (timeout: %d ms)", WATCHDOG_TIMEOUT_MS);
	return 0;
}

static void main_loop(void)
{
	struct sensor_data data;
	uint32_t cycle_count = 0;
	int ret;

	while (1) {
		cycle_count++;
		LOG_INF("=== Cycle %u ===", cycle_count);

		/* Feed watchdog */
		if (wdt_dev) {
			wdt_feed(wdt_dev, wdt_channel_id);
		}

		/* Read sensors */
		ret = sensor_manager_read(&data);
		if (ret < 0) {
			LOG_ERR("Sensor read failed: %d", ret);
		} else {
			LOG_INF("Sensors read successfully");
			LOG_INF("  Temp: %.2f°C, Humidity: %.2f%%",
				data.temperature, data.humidity);
			LOG_INF("  Accel: X=%.2f Y=%.2f Z=%.2f m/s²",
				data.accel_x, data.accel_y, data.accel_z);
			
			if (data.gnss_valid) {
				LOG_INF("  GPS: %.6f, %.6f", data.latitude, data.longitude);
			}

			/* Publish reading via MQTT (will buffer if offline) */
			ret = mqtt_manager_publish(&data);
			if (ret < 0) {
				LOG_WRN("MQTT publish failed: %d (buffered)", ret);
			}
		}

		/* Update diagnostics */
#ifdef CONFIG_APP_BLE_DIAG_ENABLED
		ble_diag_update_status(mqtt_manager_is_connected(), 
				       sensor_manager_get_reading_count());
#endif

		/* Check if we should enter deep sleep */
#ifdef CONFIG_APP_DEEP_SLEEP_ENABLED
		if (!mqtt_manager_is_connected() && 
		    !ble_diag_is_connected() &&
		    sensor_manager_get_reading_count() > 0) {
			LOG_INF("Entering power save mode");
			power_manager_deep_sleep(SENSOR_READ_INTERVAL_MS);
		} else {
			/* Normal sleep */
			k_sleep(K_MSEC(SENSOR_READ_INTERVAL_MS));
		}
#else
		/* Normal sleep without deep sleep */
		k_sleep(K_MSEC(SENSOR_READ_INTERVAL_MS));
#endif
	}
}

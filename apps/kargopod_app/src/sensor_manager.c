/*
 * Copyright (c) 2025 Kargo Chain
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sensor_manager.h"
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_mgr, LOG_LEVEL_INF);

static uint32_t reading_count = 0;
static bool initialized = false;

int sensor_manager_init(void)
{
	if (initialized) {
		return 0;
	}

	LOG_INF("Initializing sensors...");

	/* Sensors are auto-initialized by device tree */
	/* We just verify they're available */

	const struct device *sht4x = DEVICE_DT_GET_ANY(sensirion_sht4x);
	if (device_is_ready(sht4x)) {
		LOG_INF("SHT4x sensor ready");
	} else {
		LOG_WRN("SHT4x sensor not available");
	}

	const struct device *lis3dh = DEVICE_DT_GET_ANY(st_lis3dh);
	if (device_is_ready(lis3dh)) {
		LOG_INF("LIS3DH accelerometer ready");
	} else {
		LOG_WRN("LIS3DH accelerometer not available");
	}

	initialized = true;
	LOG_INF("Sensor manager initialized");

	return 0;
}

int sensor_manager_read(struct sensor_data *data)
{
	const struct device *sht4x = DEVICE_DT_GET_ANY(sensirion_sht4x);
	const struct device *lis3dh = DEVICE_DT_GET_ANY(st_lis3dh);
	struct sensor_value temp, humidity, accel[3];
	int ret;

	if (!initialized) {
		return -EINVAL;
	}

	/* Get timestamp */
	data->timestamp = k_uptime_get();
	data->gnss_valid = 0;
	data->latitude = 0.0f;
	data->longitude = 0.0f;

	/* Read SHT4x (temperature & humidity) */
	if (device_is_ready(sht4x)) {
		ret = sensor_sample_fetch(sht4x);
		if (ret == 0) {
			sensor_channel_get(sht4x, SENSOR_CHAN_AMBIENT_TEMP, &temp);
			sensor_channel_get(sht4x, SENSOR_CHAN_HUMIDITY, &humidity);
			
			data->temperature = sensor_value_to_float(&temp);
			data->humidity = sensor_value_to_float(&humidity);
			
			LOG_DBG("SHT4x: T=%.2f°C, RH=%.2f%%", 
				data->temperature, data->humidity);
		} else {
			LOG_ERR("SHT4x fetch failed: %d", ret);
			data->temperature = 0.0f;
			data->humidity = 0.0f;
		}
	} else {
		/* Use default values if sensor not available */
		data->temperature = 25.0f;
		data->humidity = 50.0f;
	}

	/* Read LIS3DH (accelerometer) */
	if (device_is_ready(lis3dh)) {
		ret = sensor_sample_fetch(lis3dh);
		if (ret == 0) {
			sensor_channel_get(lis3dh, SENSOR_CHAN_ACCEL_XYZ, accel);
			
			data->accel_x = sensor_value_to_float(&accel[0]);
			data->accel_y = sensor_value_to_float(&accel[1]);
			data->accel_z = sensor_value_to_float(&accel[2]);
			
			LOG_DBG("LIS3DH: X=%.2f Y=%.2f Z=%.2f m/s²",
				data->accel_x, data->accel_y, data->accel_z);
		} else {
			LOG_ERR("LIS3DH fetch failed: %d", ret);
			data->accel_x = 0.0f;
			data->accel_y = 0.0f;
			data->accel_z = 0.0f;
		}
	} else {
		/* Default gravity */
		data->accel_x = 0.0f;
		data->accel_y = 0.0f;
		data->accel_z = 9.81f;
	}

	/* TODO: Read GNSS (nRF9160 or u-blox M10) */
	/* For now, GNSS is marked as invalid */

	reading_count++;
	return 0;
}

uint32_t sensor_manager_get_reading_count(void)
{
	return reading_count;
}

/*
 * Copyright (c) 2025 Kargo Chain
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sensirion_sht4x

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(sht4x, CONFIG_SENSOR_LOG_LEVEL);

#define SHT4X_I2C_ADDR_A    0x44
#define SHT4X_I2C_ADDR_B    0x45

/* Commands */
#define SHT4X_CMD_MEASURE_HPM    0xFD  /* High precision measurement */
#define SHT4X_CMD_MEASURE_MPM    0xF6  /* Medium precision measurement */
#define SHT4X_CMD_MEASURE_LPM    0xE0  /* Low precision measurement */
#define SHT4X_CMD_READ_SERIAL    0x89  /* Read serial number */
#define SHT4X_CMD_SOFT_RESET     0x94  /* Soft reset */

/* Timing (ms) */
#define SHT4X_MEAS_TIME_HPM      10
#define SHT4X_MEAS_TIME_MPM      5
#define SHT4X_MEAS_TIME_LPM      2

struct sht4x_config {
	struct i2c_dt_spec i2c;
};

struct sht4x_data {
	int16_t temperature;
	uint16_t humidity;
};

static int sht4x_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct sht4x_config *cfg = dev->config;
	struct sht4x_data *data = dev->data;
	uint8_t cmd = SHT4X_CMD_MEASURE_HPM;
	uint8_t rx_buf[6];
	int ret;

	/* Send measurement command */
	ret = i2c_write_dt(&cfg->i2c, &cmd, 1);
	if (ret < 0) {
		LOG_ERR("Failed to send measurement command");
		return ret;
	}

	/* Wait for measurement */
	k_sleep(K_MSEC(SHT4X_MEAS_TIME_HPM));

	/* Read measurement data */
	ret = i2c_read_dt(&cfg->i2c, rx_buf, sizeof(rx_buf));
	if (ret < 0) {
		LOG_ERR("Failed to read measurement data");
		return ret;
	}

	/* Convert temperature: T = -45 + 175 * (raw / 65535) */
	uint16_t temp_raw = (rx_buf[0] << 8) | rx_buf[1];
	data->temperature = -45 + ((175 * (int32_t)temp_raw) / 65535);

	/* Convert humidity: RH = -6 + 125 * (raw / 65535) */
	uint16_t hum_raw = (rx_buf[3] << 8) | rx_buf[4];
	data->humidity = ((125 * (int32_t)hum_raw) / 65535) - 6;

	return 0;
}

static int sht4x_channel_get(const struct device *dev,
			      enum sensor_channel chan,
			      struct sensor_value *val)
{
	struct sht4x_data *data = dev->data;

	switch (chan) {
	case SENSOR_CHAN_AMBIENT_TEMP:
		val->val1 = data->temperature;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_HUMIDITY:
		val->val1 = data->humidity;
		val->val2 = 0;
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static int sht4x_init(const struct device *dev)
{
	const struct sht4x_config *cfg = dev->config;
	uint8_t cmd = SHT4X_CMD_SOFT_RESET;

	if (!device_is_ready(cfg->i2c.bus)) {
		LOG_ERR("I2C bus device not ready");
		return -ENODEV;
	}

	/* Soft reset */
	if (i2c_write_dt(&cfg->i2c, &cmd, 1) < 0) {
		LOG_ERR("Failed to reset sensor");
		return -EIO;
	}

	k_sleep(K_MSEC(1));

	LOG_INF("SHT4x initialized");
	return 0;
}

static const struct sensor_driver_api sht4x_api = {
	.sample_fetch = sht4x_sample_fetch,
	.channel_get = sht4x_channel_get,
};

#define SHT4X_INIT(n)							\
	static struct sht4x_data sht4x_data_##n;			\
									\
	static const struct sht4x_config sht4x_config_##n = {		\
		.i2c = I2C_DT_SPEC_INST_GET(n),				\
	};								\
									\
	DEVICE_DT_INST_DEFINE(n, sht4x_init, NULL,			\
			      &sht4x_data_##n, &sht4x_config_##n,	\
			      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,	\
			      &sht4x_api);

DT_INST_FOREACH_STATUS_OKAY(SHT4X_INIT)

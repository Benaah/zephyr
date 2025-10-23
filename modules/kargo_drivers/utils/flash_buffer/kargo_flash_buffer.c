/*
 * Copyright (c) 2025 Kargo Chain
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kargo_flash_buffer.h"
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(kargo_flash_buffer, CONFIG_SENSOR_LOG_LEVEL);

#define NVS_PARTITION_ID      FIXED_PARTITION_ID(storage_partition)
#define NVS_SECTOR_COUNT      4
#define NVS_COUNT_ID          0
#define NVS_READ_ID_BASE      1
#define NVS_WRITE_ID_BASE     1

static struct nvs_fs nvs;
static uint16_t buffered_count = 0;
static uint16_t read_id = 1;
static uint16_t write_id = 1;
static bool initialized = false;

int kargo_flash_buffer_init(void)
{
	int err;
	struct flash_pages_info info;

	if (initialized) {
		return 0;
	}

	nvs.flash_device = FIXED_PARTITION_DEVICE(storage_partition);
	if (!device_is_ready(nvs.flash_device)) {
		LOG_ERR("Flash device not ready");
		return -ENODEV;
	}

	nvs.offset = FIXED_PARTITION_OFFSET(storage_partition);
	err = flash_get_page_info_by_offs(nvs.flash_device, nvs.offset, &info);
	if (err) {
		LOG_ERR("Unable to get page info: %d", err);
		return err;
	}

	nvs.sector_size = info.size;
	nvs.sector_count = NVS_SECTOR_COUNT;

	err = nvs_mount(&nvs);
	if (err) {
		LOG_ERR("NVS mount failed: %d", err);
		return err;
	}

	/* Read buffered count */
	err = nvs_read(&nvs, NVS_COUNT_ID, &buffered_count, sizeof(buffered_count));
	if (err < 0) {
		buffered_count = 0;
		read_id = NVS_READ_ID_BASE;
		write_id = NVS_WRITE_ID_BASE;
	} else {
		read_id = NVS_READ_ID_BASE;
		write_id = buffered_count + NVS_WRITE_ID_BASE;
		if (write_id > CONFIG_KARGO_FLASH_BUFFER_SIZE) {
			write_id = NVS_WRITE_ID_BASE;
		}
	}

	initialized = true;
	LOG_INF("Flash buffer initialized, buffered: %d", buffered_count);

	return 0;
}

int kargo_flash_buffer_store(const struct kargo_sensor_reading *reading)
{
	int err;

	if (!initialized) {
		return -EINVAL;
	}

	if (buffered_count >= CONFIG_KARGO_FLASH_BUFFER_SIZE) {
		LOG_WRN("Buffer full, overwriting oldest");
		read_id++;
		if (read_id > CONFIG_KARGO_FLASH_BUFFER_SIZE) {
			read_id = NVS_READ_ID_BASE;
		}
	} else {
		buffered_count++;
	}

	err = nvs_write(&nvs, write_id, reading, sizeof(*reading));
	if (err < 0) {
		LOG_ERR("NVS write failed: %d", err);
		return err;
	}

	write_id++;
	if (write_id > CONFIG_KARGO_FLASH_BUFFER_SIZE) {
		write_id = NVS_WRITE_ID_BASE;
	}

	/* Update count */
	nvs_write(&nvs, NVS_COUNT_ID, &buffered_count, sizeof(buffered_count));

	LOG_DBG("Reading stored, total: %d", buffered_count);

	return 0;
}

int kargo_flash_buffer_retrieve(struct kargo_sensor_reading *reading)
{
	int err;

	if (!initialized) {
		return -EINVAL;
	}

	if (buffered_count == 0) {
		return -ENODATA;
	}

	err = nvs_read(&nvs, read_id, reading, sizeof(*reading));
	if (err < 0) {
		LOG_ERR("NVS read failed: %d", err);
		return err;
	}

	read_id++;
	if (read_id > CONFIG_KARGO_FLASH_BUFFER_SIZE) {
		read_id = NVS_READ_ID_BASE;
	}

	buffered_count--;
	nvs_write(&nvs, NVS_COUNT_ID, &buffered_count, sizeof(buffered_count));

	LOG_DBG("Reading retrieved, remaining: %d", buffered_count);

	return 0;
}

uint16_t kargo_flash_buffer_count(void)
{
	return buffered_count;
}

int kargo_flash_buffer_clear(void)
{
	if (!initialized) {
		return -EINVAL;
	}

	buffered_count = 0;
	read_id = NVS_READ_ID_BASE;
	write_id = NVS_WRITE_ID_BASE;

	nvs_write(&nvs, NVS_COUNT_ID, &buffered_count, sizeof(buffered_count));

	LOG_INF("Flash buffer cleared");

	return 0;
}

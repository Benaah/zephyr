/*
 * Copyright (c) 2025 Kargo Chain
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_diag.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

LOG_MODULE_REGISTER(ble_diag, LOG_LEVEL_INF);

/* Custom service UUID: 12345678-1234-5678-1234-56789abcdef0 */
#define BT_UUID_KARGO_SERVICE  BT_UUID_DECLARE_128( \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0))

/* Diagnostic characteristic UUID: 12345678-1234-5678-1234-56789abcdef1 */
#define BT_UUID_KARGO_DIAG  BT_UUID_DECLARE_128( \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1))

static char diag_data[256] = "KargoPod ready";
static bool client_connected = false;
static bool initialized = false;

/* BLE callbacks */
static void connected_cb(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("BLE connection failed: %u", err);
	} else {
		LOG_INF("BLE client connected");
		client_connected = true;
	}
}

static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("BLE client disconnected: %u", reason);
	client_connected = false;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected_cb,
	.disconnected = disconnected_cb,
};

/* GATT characteristics */
static ssize_t read_diag(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 void *buf, uint16_t len, uint16_t offset)
{
	const char *value = diag_data;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, strlen(value));
}

static ssize_t write_diag(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	if (offset + len > sizeof(diag_data) - 1) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(diag_data + offset, buf, len);
	diag_data[offset + len] = '\0';

	LOG_INF("Diagnostic command received: %s", diag_data);

	/* Parse and handle commands */
	if (strcmp(diag_data, "reboot") == 0) {
		LOG_WRN("Reboot requested via BLE");
		/* sys_reboot(SYS_REBOOT_WARM); */
	}

	return len;
}

/* GATT service definition */
BT_GATT_SERVICE_DEFINE(kargo_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_KARGO_SERVICE),
	BT_GATT_CHARACTERISTIC(BT_UUID_KARGO_DIAG,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       read_diag, write_diag, diag_data),
);

/* Advertising data */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		      BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)),
};

int ble_diag_init(void)
{
	int err;

	if (initialized) {
		return 0;
	}

	LOG_INF("Initializing BLE diagnostic mode...");

	/* Enable Bluetooth */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed: %d", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start: %d", err);
		return err;
	}

	LOG_INF("BLE advertising started");
	initialized = true;

	return 0;
}

void ble_diag_update_status(bool mqtt_connected, uint32_t reading_count)
{
	if (!initialized) {
		return;
	}

	snprintf(diag_data, sizeof(diag_data),
		 "MQTT:%s Readings:%u Uptime:%lld",
		 mqtt_connected ? "ON" : "OFF",
		 reading_count,
		 k_uptime_get() / 1000);
}

bool ble_diag_is_connected(void)
{
	return client_connected;
}

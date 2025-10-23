/*
 * Copyright (c) 2025 Kargo Chain
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/random/random.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(kargo_iot, LOG_LEVEL_INF);

/* Configuration */
#define SENSOR_READ_INTERVAL_MS     (15 * 60 * 1000)  /* 15 minutes */
#define MQTT_KEEPALIVE_SEC          60
#define MQTT_RECONNECT_DELAY_MS     5000
#define MQTT_PUBLISH_TIMEOUT_MS     5000
#define MAX_BUFFERED_READINGS       100
#define NVS_PARTITION_ID            FIXED_PARTITION_ID(storage_partition)
#define NVS_SECTOR_COUNT            4

/* WiFi Configuration - Update these with your credentials */
#define WIFI_SSID                   CONFIG_WIFI_SSID
#define WIFI_PSK                    CONFIG_WIFI_PASSWORD

/* AWS IoT Configuration - Update these with your settings */
#define AWS_IOT_ENDPOINT            CONFIG_AWS_IOT_ENDPOINT
#define AWS_IOT_PORT                8883
#define AWS_IOT_CLIENT_ID           CONFIG_AWS_IOT_CLIENT_ID
#define AWS_IOT_TOPIC               "kargo/sensors/" CONFIG_AWS_IOT_CLIENT_ID "/data"

/* BLE Service UUID for diagnostic mode */
#define BT_UUID_KARGO_SERVICE       BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0))
#define BT_UUID_KARGO_DIAGNOSTICS   BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1))

/* Sensor data structure */
struct sensor_reading {
	int64_t timestamp;
	double temperature;
	double humidity;
	double accel_x;
	double accel_y;
	double accel_z;
	uint8_t valid;
};

/* Global variables */
static struct nvs_fs nvs;
static struct mqtt_client mqtt_client;
static struct sockaddr_storage broker;
static struct zsock_pollfd fds[1];
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];
static uint8_t payload_buf[512];
static bool mqtt_connected = false;
static bool wifi_connected = false;
static struct sensor_reading current_reading;
static uint16_t nvs_read_id = 1;
static uint16_t nvs_write_id = 1;
static uint16_t buffered_count = 0;

/* BLE diagnostic data */
static char diagnostic_data[256] = "System OK";
static bool ble_diagnostic_mode = false;

/* Thread stacks */
K_THREAD_STACK_DEFINE(sensor_stack, 2048);
K_THREAD_STACK_DEFINE(mqtt_stack, 4096);
K_THREAD_STACK_DEFINE(publish_stack, 2048);

static struct k_thread sensor_thread;
static struct k_thread mqtt_thread;
static struct k_thread publish_thread;

/* Semaphores and events */
K_SEM_DEFINE(mqtt_connected_sem, 0, 1);
K_SEM_DEFINE(reading_ready_sem, 0, 1);

/* Forward declarations */
static void sensor_task(void *p1, void *p2, void *p3);
static void mqtt_task(void *p1, void *p2, void *p3);
static void publish_task(void *p1, void *p2, void *p3);
static int init_sensors(void);
static int read_sensors(struct sensor_reading *reading);
static int init_storage(void);
static int buffer_reading(struct sensor_reading *reading);
static int get_buffered_reading(struct sensor_reading *reading);
static int init_wifi(void);
static int init_mqtt(void);
static int publish_reading(struct sensor_reading *reading);
static void enter_deep_sleep(void);
static int init_ble_diagnostic(void);

/* BLE GATT Characteristics */
static ssize_t read_diagnostic(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                void *buf, uint16_t len, uint16_t offset)
{
	const char *value = diagnostic_data;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, strlen(value));
}

static ssize_t write_diagnostic(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	if (offset + len > sizeof(diagnostic_data)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(diagnostic_data + offset, buf, len);
	diagnostic_data[offset + len] = '\0';

	LOG_INF("Diagnostic command received: %s", diagnostic_data);

	return len;
}

BT_GATT_SERVICE_DEFINE(kargo_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_KARGO_SERVICE),
	BT_GATT_CHARACTERISTIC(BT_UUID_KARGO_DIAGNOSTICS,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       read_diagnostic, write_diagnostic, diagnostic_data),
);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)),
};

/* WiFi event handler */
static struct net_mgmt_event_callback wifi_cb;

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				     uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		LOG_INF("WiFi connected");
		wifi_connected = true;
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		LOG_INF("WiFi disconnected");
		wifi_connected = false;
		mqtt_connected = false;
		break;
	default:
		break;
	}
}

/* MQTT event handler */
static void mqtt_evt_handler(struct mqtt_client *const client,
			      const struct mqtt_evt *evt)
{
	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result == 0) {
			LOG_INF("MQTT connected");
			mqtt_connected = true;
			k_sem_give(&mqtt_connected_sem);
		} else {
			LOG_ERR("MQTT connection failed: %d", evt->result);
			mqtt_connected = false;
		}
		break;

	case MQTT_EVT_DISCONNECT:
		LOG_INF("MQTT disconnected: %d", evt->result);
		mqtt_connected = false;
		break;

	case MQTT_EVT_PUBACK:
		LOG_DBG("MQTT PUBACK packet id: %u", evt->param.puback.message_id);
		break;

	default:
		break;
	}
}

/* Initialize NVS storage */
static int init_storage(void)
{
	int err;
	struct flash_pages_info info;

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

	LOG_INF("NVS storage initialized");

	/* Read buffered count */
	err = nvs_read(&nvs, 0, &buffered_count, sizeof(buffered_count));
	if (err < 0) {
		buffered_count = 0;
		nvs_read_id = 1;
		nvs_write_id = 1;
	} else {
		nvs_read_id = 1;
		nvs_write_id = buffered_count + 1;
	}

	LOG_INF("Buffered readings: %d", buffered_count);

	return 0;
}

/* Buffer a sensor reading to flash */
static int buffer_reading(struct sensor_reading *reading)
{
	int err;

	if (buffered_count >= MAX_BUFFERED_READINGS) {
		LOG_WRN("Buffer full, overwriting oldest reading");
		nvs_read_id++;
		if (nvs_read_id > MAX_BUFFERED_READINGS) {
			nvs_read_id = 1;
		}
	} else {
		buffered_count++;
	}

	err = nvs_write(&nvs, nvs_write_id, reading, sizeof(*reading));
	if (err < 0) {
		LOG_ERR("NVS write failed: %d", err);
		return err;
	}

	nvs_write_id++;
	if (nvs_write_id > MAX_BUFFERED_READINGS) {
		nvs_write_id = 1;
	}

	/* Update buffered count */
	nvs_write(&nvs, 0, &buffered_count, sizeof(buffered_count));

	LOG_DBG("Reading buffered, total: %d", buffered_count);

	return 0;
}

/* Get a buffered reading from flash */
static int get_buffered_reading(struct sensor_reading *reading)
{
	int err;

	if (buffered_count == 0) {
		return -ENODATA;
	}

	err = nvs_read(&nvs, nvs_read_id, reading, sizeof(*reading));
	if (err < 0) {
		LOG_ERR("NVS read failed: %d", err);
		return err;
	}

	nvs_read_id++;
	if (nvs_read_id > MAX_BUFFERED_READINGS) {
		nvs_read_id = 1;
	}

	buffered_count--;
	nvs_write(&nvs, 0, &buffered_count, sizeof(buffered_count));

	LOG_DBG("Reading retrieved, remaining: %d", buffered_count);

	return 0;
}

/* Initialize WiFi */
static int init_wifi(void)
{
	struct net_if *iface = net_if_get_default();
	struct wifi_connect_req_params wifi_params = { 0 };

	if (!iface) {
		LOG_ERR("WiFi interface not available");
		return -ENODEV;
	}

	net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT |
				     NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	wifi_params.ssid = WIFI_SSID;
	wifi_params.ssid_length = strlen(WIFI_SSID);
	wifi_params.psk = WIFI_PSK;
	wifi_params.psk_length = strlen(WIFI_PSK);
	wifi_params.channel = WIFI_CHANNEL_ANY;
	wifi_params.security = WIFI_SECURITY_TYPE_PSK;
	wifi_params.band = WIFI_FREQ_BAND_2_4_GHZ;
	wifi_params.mfp = WIFI_MFP_OPTIONAL;

	LOG_INF("Connecting to WiFi SSID: %s", WIFI_SSID);

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params,
		     sizeof(struct wifi_connect_req_params))) {
		LOG_ERR("WiFi connection request failed");
		return -EAGAIN;
	}

	/* Wait for connection */
	for (int i = 0; i < 20; i++) {
		if (wifi_connected) {
			break;
		}
		k_sleep(K_MSEC(500));
	}

	if (!wifi_connected) {
		LOG_ERR("WiFi connection timeout");
		return -ETIMEDOUT;
	}

	LOG_INF("WiFi connected successfully");
	return 0;
}

/* Initialize MQTT client */
static int init_mqtt(void)
{
	int err;
	struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;

	mqtt_client_init(&mqtt_client);

	/* Broker address */
	broker4->sin_family = AF_INET;
	broker4->sin_port = htons(AWS_IOT_PORT);
	zsock_inet_pton(AF_INET, AWS_IOT_ENDPOINT, &broker4->sin_addr);

	/* MQTT client configuration */
	mqtt_client.broker = &broker;
	mqtt_client.evt_cb = mqtt_evt_handler;
	mqtt_client.client_id.utf8 = (uint8_t *)AWS_IOT_CLIENT_ID;
	mqtt_client.client_id.size = strlen(AWS_IOT_CLIENT_ID);
	mqtt_client.password = NULL;
	mqtt_client.user_name = NULL;
	mqtt_client.protocol_version = MQTT_VERSION_3_1_1;
	mqtt_client.keepalive = MQTT_KEEPALIVE_SEC;
	mqtt_client.clean_session = 1;

	/* MQTT buffers */
	mqtt_client.rx_buf = rx_buffer;
	mqtt_client.rx_buf_size = sizeof(rx_buffer);
	mqtt_client.tx_buf = tx_buffer;
	mqtt_client.tx_buf_size = sizeof(tx_buffer);

	/* TLS configuration */
	struct mqtt_sec_config *tls_config = &mqtt_client.transport.tls.config;
	tls_config->peer_verify = TLS_PEER_VERIFY_REQUIRED;
	tls_config->cipher_list = NULL;
	tls_config->sec_tag_list = (sec_tag_t[]){CONFIG_AWS_IOT_SEC_TAG};
	tls_config->sec_tag_count = 1;
	tls_config->hostname = AWS_IOT_ENDPOINT;

	mqtt_client.transport.type = MQTT_TRANSPORT_SECURE;

	LOG_INF("MQTT client initialized");

	return 0;
}

/* Publish sensor reading via MQTT */
static int publish_reading(struct sensor_reading *reading)
{
	struct mqtt_publish_param param;
	char timestamp_str[32];

	if (!mqtt_connected) {
		LOG_WRN("MQTT not connected, buffering reading");
		return buffer_reading(reading);
	}

	/* Format JSON payload */
	snprintf(timestamp_str, sizeof(timestamp_str), "%lld", reading->timestamp);
	snprintf(payload_buf, sizeof(payload_buf),
		 "{\"timestamp\":%s,\"temperature\":%.2f,\"humidity\":%.2f,"
		 "\"accel\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}}",
		 timestamp_str, reading->temperature, reading->humidity,
		 reading->accel_x, reading->accel_y, reading->accel_z);

	param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
	param.message.topic.topic.utf8 = (uint8_t *)AWS_IOT_TOPIC;
	param.message.topic.topic.size = strlen(AWS_IOT_TOPIC);
	param.message.payload.data = payload_buf;
	param.message.payload.len = strlen(payload_buf);
	param.message_id = sys_rand32_get();
	param.dup_flag = 0;
	param.retain_flag = 0;

	LOG_INF("Publishing: %s", payload_buf);

	int err = mqtt_publish(&mqtt_client, &param);
	if (err) {
		LOG_ERR("MQTT publish failed: %d, buffering", err);
		return buffer_reading(reading);
	}

	LOG_INF("Reading published successfully");
	return 0;
}

/* Initialize sensors */
static int init_sensors(void)
{
	/* SHT4x and LIS3DH will be initialized by Zephyr device tree */
	LOG_INF("Sensors initialized");
	return 0;
}

/* Read sensor data */
static int read_sensors(struct sensor_reading *reading)
{
	const struct device *sht4x = DEVICE_DT_GET_ANY(sensirion_sht4x);
	const struct device *lis3dh = DEVICE_DT_GET_ANY(st_lis3dh);
	struct sensor_value temp, humidity, accel[3];
	int err;

	reading->timestamp = k_uptime_get();
	reading->valid = 1;

	/* Read temperature and humidity from SHT4x */
	if (device_is_ready(sht4x)) {
		err = sensor_sample_fetch(sht4x);
		if (err == 0) {
			sensor_channel_get(sht4x, SENSOR_CHAN_AMBIENT_TEMP, &temp);
			sensor_channel_get(sht4x, SENSOR_CHAN_HUMIDITY, &humidity);
			reading->temperature = sensor_value_to_double(&temp);
			reading->humidity = sensor_value_to_double(&humidity);
			LOG_INF("SHT4x - Temp: %.2f°C, Humidity: %.2f%%",
				reading->temperature, reading->humidity);
		} else {
			LOG_ERR("SHT4x fetch failed: %d", err);
			reading->temperature = 0.0;
			reading->humidity = 0.0;
		}
	} else {
		LOG_WRN("SHT4x not ready");
		reading->temperature = 25.0;  /* Default values */
		reading->humidity = 50.0;
	}

	/* Read accelerometer from LIS3DH */
	if (device_is_ready(lis3dh)) {
		err = sensor_sample_fetch(lis3dh);
		if (err == 0) {
			sensor_channel_get(lis3dh, SENSOR_CHAN_ACCEL_XYZ, accel);
			reading->accel_x = sensor_value_to_double(&accel[0]);
			reading->accel_y = sensor_value_to_double(&accel[1]);
			reading->accel_z = sensor_value_to_double(&accel[2]);
			LOG_INF("LIS3DH - X: %.2f, Y: %.2f, Z: %.2f m/s²",
				reading->accel_x, reading->accel_y, reading->accel_z);
		} else {
			LOG_ERR("LIS3DH fetch failed: %d", err);
			reading->accel_x = 0.0;
			reading->accel_y = 0.0;
			reading->accel_z = 0.0;
		}
	} else {
		LOG_WRN("LIS3DH not ready");
		reading->accel_x = 0.0;
		reading->accel_y = 0.0;
		reading->accel_z = 9.8;  /* Default gravity */
	}

	return 0;
}

/* Sensor reading task */
static void sensor_task(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("Sensor task started");

	while (1) {
		/* Read sensors */
		read_sensors(&current_reading);

		/* Signal that reading is ready */
		k_sem_give(&reading_ready_sem);

		/* Wait for next reading interval */
		k_sleep(K_MSEC(SENSOR_READ_INTERVAL_MS));
	}
}

/* MQTT connection task */
static void mqtt_task(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int err;

	LOG_INF("MQTT task started");

	while (1) {
		if (!wifi_connected) {
			LOG_INF("Waiting for WiFi...");
			k_sleep(K_MSEC(1000));
			continue;
		}

		if (!mqtt_connected) {
			LOG_INF("Connecting to MQTT broker...");
			err = mqtt_connect(&mqtt_client);
			if (err) {
				LOG_ERR("MQTT connect failed: %d", err);
				k_sleep(K_MSEC(MQTT_RECONNECT_DELAY_MS));
				continue;
			}

			fds[0].fd = mqtt_client.transport.tls.sock;
			fds[0].events = ZSOCK_POLLIN;

			/* Wait for CONNACK */
			k_sem_take(&mqtt_connected_sem, K_SECONDS(10));
		}

		/* Process MQTT events */
		if (mqtt_connected) {
			err = zsock_poll(fds, 1, mqtt_keepalive_time_left(&mqtt_client));
			if (err < 0) {
				LOG_ERR("Poll error: %d", errno);
				mqtt_disconnect(&mqtt_client);
				mqtt_connected = false;
				continue;
			}

			err = mqtt_live(&mqtt_client);
			if (err != 0 && err != -EAGAIN) {
				LOG_ERR("MQTT live failed: %d", err);
				mqtt_disconnect(&mqtt_client);
				mqtt_connected = false;
			}

			if (fds[0].revents & ZSOCK_POLLIN) {
				err = mqtt_input(&mqtt_client);
				if (err != 0) {
					LOG_ERR("MQTT input error: %d", err);
					mqtt_disconnect(&mqtt_client);
					mqtt_connected = false;
				}
			}
		}

		k_sleep(K_MSEC(100));
	}
}

/* Publish task - handles publishing sensor readings and buffered data */
static void publish_task(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct sensor_reading buffered;

	LOG_INF("Publish task started");

	while (1) {
		/* Wait for new reading */
		k_sem_take(&reading_ready_sem, K_FOREVER);

		/* Try to publish buffered readings first */
		while (buffered_count > 0 && mqtt_connected) {
			if (get_buffered_reading(&buffered) == 0) {
				publish_reading(&buffered);
				k_sleep(K_MSEC(100));
			} else {
				break;
			}
		}

		/* Publish current reading */
		publish_reading(&current_reading);
	}
}

/* Initialize BLE diagnostic mode */
static int init_ble_diagnostic(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return err;
	}

	LOG_INF("BLE diagnostic mode enabled");
	ble_diagnostic_mode = true;

	return 0;
}

/* Enter deep sleep mode */
static void enter_deep_sleep(void)
{
	LOG_INF("Entering deep sleep mode");

	/* Configure wake-up timer */
	/* Note: Actual implementation depends on ESP32-S3 RTC timer configuration */

	/* Put system into deep sleep */
	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});
}

/* Main function */
int main(void)
{
	int err;

	LOG_INF("Kargo IoT Sensor Application Starting...");
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	/* Initialize storage */
	err = init_storage();
	if (err) {
		LOG_ERR("Storage initialization failed: %d", err);
	}

	/* Initialize sensors */
	err = init_sensors();
	if (err) {
		LOG_ERR("Sensor initialization failed: %d", err);
	}

	/* Initialize WiFi */
	err = init_wifi();
	if (err) {
		LOG_ERR("WiFi initialization failed: %d", err);
	}

	/* Initialize MQTT */
	err = init_mqtt();
	if (err) {
		LOG_ERR("MQTT initialization failed: %d", err);
	}

	/* Initialize BLE diagnostic mode (optional) */
	if (IS_ENABLED(CONFIG_BT)) {
		init_ble_diagnostic();
	}

	/* Create threads */
	k_thread_create(&sensor_thread, sensor_stack,
			K_THREAD_STACK_SIZEOF(sensor_stack),
			sensor_task, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);
	k_thread_name_set(&sensor_thread, "sensor");

	k_thread_create(&mqtt_thread, mqtt_stack,
			K_THREAD_STACK_SIZEOF(mqtt_stack),
			mqtt_task, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);
	k_thread_name_set(&mqtt_thread, "mqtt");

	k_thread_create(&publish_thread, publish_stack,
			K_THREAD_STACK_SIZEOF(publish_stack),
			publish_task, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);
	k_thread_name_set(&publish_thread, "publish");

	LOG_INF("All tasks started, system running");

	/* Main loop - can be used for system monitoring */
	while (1) {
		/* Update diagnostic info */
		snprintf(diagnostic_data, sizeof(diagnostic_data),
			 "WiFi:%s MQTT:%s Buffered:%d Uptime:%lld",
			 wifi_connected ? "ON" : "OFF",
			 mqtt_connected ? "ON" : "OFF",
			 buffered_count,
			 k_uptime_get() / 1000);

		k_sleep(K_SECONDS(30));
	}

	return 0;
}

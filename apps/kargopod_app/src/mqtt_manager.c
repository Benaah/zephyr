/*
 * Copyright (c) 2025 Kargo Chain
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mqtt_manager.h"
#include "kargo_flash_buffer.h"
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/tls_credentials.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(mqtt_mgr, LOG_LEVEL_INF);

/* MQTT configuration */
#define MQTT_BROKER         CONFIG_AWS_IOT_ENDPOINT
#define MQTT_PORT           8883
#define MQTT_CLIENT_ID      CONFIG_AWS_IOT_CLIENT_ID
#define MQTT_TOPIC          "kargo/sensors/" CONFIG_AWS_IOT_CLIENT_ID "/data"
#define MQTT_KEEPALIVE      CONFIG_KARGO_MQTT_KEEPALIVE_SEC
#define MQTT_BUFFER_SIZE    CONFIG_KARGO_MQTT_BUFFER_SIZE

static struct mqtt_client client;
static struct sockaddr_storage broker;
static uint8_t rx_buffer[MQTT_BUFFER_SIZE];
static uint8_t tx_buffer[MQTT_BUFFER_SIZE];
static uint8_t payload_buffer[MQTT_BUFFER_SIZE];
static bool connected = false;
static bool initialized = false;

K_THREAD_STACK_DEFINE(mqtt_thread_stack, 4096);
static struct k_thread mqtt_thread_data;

/* Forward declarations */
static void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt);
static void mqtt_thread_fn(void *p1, void *p2, void *p3);
static int mqtt_connect(void);

int mqtt_manager_init(void)
{
	int ret;

	if (initialized) {
		return 0;
	}

	LOG_INF("Initializing MQTT manager...");

	/* Initialize flash buffer for offline storage */
	ret = kargo_flash_buffer_init();
	if (ret < 0) {
		LOG_ERR("Flash buffer init failed: %d", ret);
		return ret;
	}

	/* Initialize MQTT client */
	mqtt_client_init(&client);

	struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
	broker4->sin_family = AF_INET;
	broker4->sin_port = htons(MQTT_PORT);
	
	/* Note: DNS resolution should happen at runtime */
	/* For now, IP address needs to be configured */

	client.broker = &broker;
	client.evt_cb = mqtt_evt_handler;
	client.client_id.utf8 = (uint8_t *)MQTT_CLIENT_ID;
	client.client_id.size = strlen(MQTT_CLIENT_ID);
	client.protocol_version = MQTT_VERSION_3_1_1;
	client.keepalive = MQTT_KEEPALIVE;
	client.clean_session = 1;

	client.rx_buf = rx_buffer;
	client.rx_buf_size = sizeof(rx_buffer);
	client.tx_buf = tx_buffer;
	client.tx_buf_size = sizeof(tx_buffer);

	/* TLS configuration */
	struct mqtt_sec_config *tls_config = &client.transport.tls.config;
	tls_config->peer_verify = TLS_PEER_VERIFY_REQUIRED;
	tls_config->cipher_list = NULL;
	tls_config->sec_tag_list = (sec_tag_t[]){CONFIG_AWS_IOT_SEC_TAG};
	tls_config->sec_tag_count = 1;
	tls_config->hostname = MQTT_BROKER;

	client.transport.type = MQTT_TRANSPORT_SECURE;

	/* Start MQTT thread */
	k_thread_create(&mqtt_thread_data, mqtt_thread_stack,
			K_THREAD_STACK_SIZEOF(mqtt_thread_stack),
			mqtt_thread_fn, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);
	k_thread_name_set(&mqtt_thread_data, "mqtt");

	initialized = true;
	LOG_INF("MQTT manager initialized");

	return 0;
}

int mqtt_manager_publish(const struct sensor_data *data)
{
	struct mqtt_publish_param param;
	struct kargo_sensor_reading reading;
	int ret;

	if (!initialized) {
		return -EINVAL;
	}

	/* Convert to buffer format */
	reading.timestamp = data->timestamp;
	reading.temperature = data->temperature;
	reading.humidity = data->humidity;
	reading.accel_x = data->accel_x;
	reading.accel_y = data->accel_y;
	reading.accel_z = data->accel_z;
	reading.latitude = data->latitude;
	reading.longitude = data->longitude;
	reading.gnss_valid = data->gnss_valid;

	if (!connected) {
		LOG_WRN("MQTT not connected, buffering reading");
		return kargo_flash_buffer_store(&reading);
	}

	/* Format JSON payload */
	snprintf(payload_buffer, sizeof(payload_buffer),
		"{\"ts\":%lld,\"temp\":%.2f,\"hum\":%.2f,"
		"\"acc\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},"
		"\"gps\":{\"lat\":%.6f,\"lon\":%.6f,\"valid\":%d}}",
		reading.timestamp,
		reading.temperature,
		reading.humidity,
		reading.accel_x,
		reading.accel_y,
		reading.accel_z,
		reading.latitude,
		reading.longitude,
		reading.gnss_valid);

	param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
	param.message.topic.topic.utf8 = (uint8_t *)MQTT_TOPIC;
	param.message.topic.topic.size = strlen(MQTT_TOPIC);
	param.message.payload.data = payload_buffer;
	param.message.payload.len = strlen(payload_buffer);
	param.message_id = sys_rand32_get();
	param.dup_flag = 0;
	param.retain_flag = 0;

	ret = mqtt_publish(&client, &param);
	if (ret) {
		LOG_ERR("MQTT publish failed: %d, buffering", ret);
		return kargo_flash_buffer_store(&reading);
	}

	LOG_INF("Published: %s", payload_buffer);
	return 0;
}

bool mqtt_manager_is_connected(void)
{
	return connected;
}

uint16_t mqtt_manager_get_buffered_count(void)
{
	return kargo_flash_buffer_count();
}

static void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt)
{
	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result == 0) {
			LOG_INF("MQTT connected!");
			connected = true;
		} else {
			LOG_ERR("MQTT connection failed: %d", evt->result);
			connected = false;
		}
		break;

	case MQTT_EVT_DISCONNECT:
		LOG_WRN("MQTT disconnected: %d", evt->result);
		connected = false;
		break;

	case MQTT_EVT_PUBACK:
		LOG_DBG("MQTT PUBACK: %u", evt->param.puback.message_id);
		break;

	default:
		break;
	}
}

static void mqtt_thread_fn(void *p1, void *p2, void *p3)
{
	struct zsock_pollfd fds[1];
	int ret;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("MQTT thread started");

	while (1) {
		if (!connected) {
			LOG_INF("Attempting MQTT connection...");
			ret = mqtt_connect();
			if (ret < 0) {
				LOG_ERR("MQTT connect failed: %d", ret);
				k_sleep(K_MSEC(CONFIG_KARGO_MQTT_RECONNECT_DELAY_MS));
				continue;
			}

			fds[0].fd = client.transport.tls.sock;
			fds[0].events = ZSOCK_POLLIN;
		}

		/* Process MQTT */
		if (connected) {
			ret = zsock_poll(fds, 1, mqtt_keepalive_time_left(&client));
			if (ret < 0) {
				LOG_ERR("Poll error: %d", errno);
				mqtt_disconnect(&client);
				connected = false;
				continue;
			}

			ret = mqtt_live(&client);
			if (ret != 0 && ret != -EAGAIN) {
				LOG_ERR("MQTT live error: %d", ret);
				mqtt_disconnect(&client);
				connected = false;
			}

			if (fds[0].revents & ZSOCK_POLLIN) {
				ret = mqtt_input(&client);
				if (ret != 0) {
					LOG_ERR("MQTT input error: %d", ret);
					mqtt_disconnect(&client);
					connected = false;
				}
			}

			/* Publish buffered readings */
			if (kargo_flash_buffer_count() > 0) {
				struct kargo_sensor_reading buffered;
				if (kargo_flash_buffer_retrieve(&buffered) == 0) {
					struct sensor_data data = {
						.timestamp = buffered.timestamp,
						.temperature = buffered.temperature,
						.humidity = buffered.humidity,
						.accel_x = buffered.accel_x,
						.accel_y = buffered.accel_y,
						.accel_z = buffered.accel_z,
						.latitude = buffered.latitude,
						.longitude = buffered.longitude,
						.gnss_valid = buffered.gnss_valid
					};
					mqtt_manager_publish(&data);
				}
			}
		}

		k_sleep(K_MSEC(100));
	}
}

static int mqtt_connect(void)
{
	/* In real implementation, perform DNS resolution and network setup */
	return mqtt_connect(&client);
}

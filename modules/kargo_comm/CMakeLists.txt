# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2025 Kargo Chain

zephyr_library()

add_subdirectory_ifdef(CONFIG_KARGO_MQTT subsys/net/kargo_mqtt)
add_subdirectory_ifdef(CONFIG_KARGO_MODEM_NRF9160 subsys/modem/nrf9160)
add_subdirectory_ifdef(CONFIG_KARGO_GNSS subsys/gnss)

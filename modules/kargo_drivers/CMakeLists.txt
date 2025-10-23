# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2025 Kargo Chain

zephyr_library()

# Add sensor drivers
add_subdirectory_ifdef(CONFIG_KARGO_SENSOR_SHT4X drivers/sensor/sht4x)
add_subdirectory_ifdef(CONFIG_KARGO_SENSOR_BMA4XX drivers/sensor/bma4xx)
add_subdirectory_ifdef(CONFIG_KARGO_SENSOR_LIS3DH drivers/sensor/lis3dh_wrapper)
add_subdirectory_ifdef(CONFIG_KARGO_SENSOR_ALS drivers/sensor/als)

# Add utility modules
add_subdirectory_ifdef(CONFIG_KARGO_POWER_MGMT utils/power_mgmt)
add_subdirectory_ifdef(CONFIG_KARGO_FLASH_BUFFER utils/flash_buffer)

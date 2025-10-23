# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2025 Kargo Chain

board_runner_args(esp32 "--esp-boot-address=0x0")
board_runner_args(esp32 "--esp-app-address=0x10000")

include(${ZEPHYR_BASE}/boards/common/esp32.board.cmake)

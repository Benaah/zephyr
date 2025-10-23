# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2025 Kargo Chain

board_runner_args(nrfjprog "--nrf-family=NRF91")
board_runner_args(jlink "--device=nrf9160" "--speed=4000")

include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)

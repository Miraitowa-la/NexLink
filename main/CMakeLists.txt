# 无线 RX：按保存模式选择有线 DAP+CDC 或 ESP-NOW + 目标板 SWD/JTAG/UART。
# idf_component_register(SRCS "main_rx.c" INCLUDE_DIRS "." REQUIRES BSP Middlewares)

# 无线 TX：P0 按保存模式选择关闭或 ESP-NOW；USB DAP/CDC 前端将在 P3 接入。
# idf_component_register(SRCS "main_tx.c" INCLUDE_DIRS "." REQUIRES BSP Middlewares)

# 有线 DIRECT：USB DAP/CDC 直接连接本机 SWD/JTAG/UART，不初始化 ESP-NOW。
idf_component_register(SRCS "main_direct.c" INCLUDE_DIRS "." REQUIRES BSP Middlewares)

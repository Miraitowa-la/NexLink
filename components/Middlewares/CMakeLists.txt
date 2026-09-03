set(srcs
    bridge_target/bridge_target.c
    debug_probe/cmsis_dap_core.c
    debug_probe/DAP/Source/DAP_vendor.c
    debug_probe/DAP/Source/DAP.c
    debug_probe/DAP/Source/JTAG_DP.c
    debug_probe/DAP/Source/SW_DP.c
    espnow_transport/espnow_transport.c
    nexlink_link/nexlink_link.c
    nexlink_dap_bridge/nexlink_dap_bridge.c
    nexlink_mode/nexlink_mode.c
    nexlink_pairing/nexlink_pairing.c
    nexlink_protocol/nexlink_protocol.c
    nexlink_tx_frontend/nexlink_tx_frontend.c
    transport_uart/transport_uart.c
    usb_device/cmsis_dap_v1_hid.c
    usb_device/cmsis_dap_v2_bulk.c
    usb_device/usb_device.c
    usb_device/usb_phy.c
)

set(include_dirs
    bridge_target
    debug_probe/include
    debug_probe/DAP/Include
    debug_probe/DAP/Config
    espnow_transport
    nexlink_link
    nexlink_dap_bridge
    nexlink_mode
    nexlink_pairing
    nexlink_protocol
    nexlink_tx_frontend
    nexlink_task_config
    transport_uart
    usb_device
)

set(requires
    esp_driver_gpio
    esp_driver_uart
    esp_event
    esp_netif
    esp_wifi
    nvs_flash
    freertos
    tinyusb
    usb
    esp_ringbuf
    esp_timer
    BSP
)

idf_component_register(
    SRCS
    ${srcs}
    INCLUDE_DIRS
    ${include_dirs}
    REQUIRES
    ${requires}
    LDFRAGMENTS
    "debug_probe/noflash.lf"
)

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/usb_device)
target_compile_definitions(${COMPONENT_LIB} PRIVATE BCDDEVICE=0x0002)

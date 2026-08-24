idf_component_register(
    SRCS
    "main.c"
    "board/src/crc32.c"
    "board/src/led_ws2812.c"
    "board/src/ota_proto.c"
    "board/src/ota_session.c"
    "board/src/uart_driver.c"
    "iot/onenet_dm.c"
    "iot/onenet_mqtt.c"
    "iot/onenet_ota.c"
    "iot/onenet_token.c"
    "iot/ota_manager.c"
    "iot/wifi_manager.c"

    INCLUDE_DIRS
    "."
    "board/inc"
    "iot"
)


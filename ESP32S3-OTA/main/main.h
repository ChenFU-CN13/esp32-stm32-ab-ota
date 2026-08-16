#include <stdio.h>
#include "onenet_mqtt.h"
#include "wifi_manager.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "onenet_dm.h"
#include "myuart.h"

#define TAG     "main"
#define EV_WIFI_CONNECTED_BIT           (1<<0)
#define DEFAULT_SSID        "a1"
#define DEFAULT_PASSWORD    "KYWH8888"
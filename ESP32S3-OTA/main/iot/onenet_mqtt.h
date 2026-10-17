#ifndef _ONENET_MQTT_H_
#define _ONENET_MQTT_H_
#include "esp_err.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "cJSON.h"
#include "mqtt_client.h"
#include "onenet_token.h"
#include "onenet_dm.h"
#include "onenet_ota.h"
#include "credentials.h"

#define TM_EXPIRE_TIME 2074855187



/*写代码的逻辑应该是先写初始化
如果在初始化遇到注册函数，
要注册回调函数，可以先创建空的回调函数，
等写完通信协议的初始化启动，发送，接收，停止等操作函数
再回头写回调函数

一般回调函数的内容都是响应事件，注意观察回调函数的不同类型
一般回调函数都是函数指针预定义了
对于不同的事件标志位进行不同的操作
比如事件1：连接成功，则置相关成功标志位，然后开始订阅主题   发送数据    打印日志
    事件2：连接失败，则置相关失败标志位标志位   然后打印日志等等
    事件3：收到信息，则回应标志位，打印相关日志
    等等



QoS 等级详解：
    QoS 0 - 最多一次传递 (At most once)
        消息可能会丢失
        发送方只发送一次，不等待确认
        性能最高，开销最小
    QoS 1 - 至少一次传递 (At least once)
        消息不会丢失，但可能重复
        发送方会确保消息至少被接收方接收一次
        会有确认和重传机制  (多数用这个)
    QoS 2 - 恰好一次传递 (Exactly once)
        消息只被传递一次，不会丢失也不会重复
        最高的可靠性，但也最消耗资源
        有复杂的确认和握手过程    

    */




/**
 * 上报数据
 * @param data 数据
 * @return 错误
 */
esp_err_t onenet_post_property_data(const char* data);

/**
 * 启动onenet连接
 * @return 错误码
 */
esp_err_t onenet_start(void);

/**
 * 停止onenet连接
 * @param 无
 * @return 错误码
 */
esp_err_t onenet_stop(void);

/**
 * @brief 查询当前OneNET MQTT连接状态。
 * @param 无。
 * @return true表示MQTT已连接，false表示尚未连接或已断开。
 */
bool onenet_is_connected(void);

#endif

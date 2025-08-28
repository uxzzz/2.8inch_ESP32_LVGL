

#ifndef _USART_H
#define _USART_H
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/uart_select.h"
#include "driver/gpio.h"

// UART配置参数
#define UART_PORT_NUM       UART_NUM_1
#define USART_TX_GPIO_PIN   GPIO_NUM_1
#define USART_RX_GPIO_PIN   GPIO_NUM_3
#define UART_BAUD_RATE     115200           // 波特率
#define UART_BUF_SIZE      1024             // 缓冲区大小

/**
 * @brief       初始化UART通信
 * @param       无
 * @retval      无
 */
void uart_comm_init(void);

/**
 * @brief       发送字符串到STM32
 * @param       str: 要发送的字符串
 * @retval      无
 */
void uart_send_string(const char* str);

/**
 * @brief       创建UART接收任务
 * @param       无
 * @retval      无
 */
void uart_create_receive_task(void);

/**
 * @brief       创建UART发送任务
 * @param       无
 * @retval      无
 */
void uart_create_send_task(void);

/**
 * @brief       获取最新接收的数据
 * @param       buf: 存储数据的缓冲区
 * @param       max_len: 缓冲区最大长度
 * @retval      实际数据长度
 */
uint16_t uart_get_received_data(char* buf, uint16_t max_len);

#endif /* _USART_H */
    
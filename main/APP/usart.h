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

#endif /* _USART_H */
    
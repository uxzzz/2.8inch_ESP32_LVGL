#include "usart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "UART_COMM";
void uart_comm_init(void) {
    // 配置UART参数
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_PORT_NUM, &uart_config);
    
    // 设置GPIO引脚
    uart_set_pin(UART_PORT_NUM, USART_TX_GPIO_PIN, USART_RX_GPIO_PIN, 
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
    // 安装UART驱动
    uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 
                       0, 0, NULL, 0);
                       
    ESP_LOGI(TAG, "UART初始化完成");
    printf("UART1 initialized successfully\n");
}
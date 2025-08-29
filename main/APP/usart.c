#include "usart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "UART_COMM";
static char g_rx_buf[UART_BUF_SIZE] = {0};  // 接收缓冲区
static int16_t g_send_count = 0;           // 发送计数器

/**
 * @brief       UART初始化
 * @param       无
 * @retval      无
 */
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

/**
 * @brief       发送字符串到STM32
 * @param       str: 要发送的字符串
 * @retval      无
 */
void uart_send_string(const char* str) {
    if (str == NULL) return;
    uart_write_bytes(UART_PORT_NUM, str, strlen(str));
}

/**
 * @brief       UART接收任务
 * @param       pvParameters: 传入参数
 * @retval      无
 */
static void uart_receive_task(void *pvParameters) {
    int rx_len;
    
    while (1) {
        // 接收数据（超时100ms）
        rx_len = uart_read_bytes(UART_PORT_NUM, g_rx_buf, UART_BUF_SIZE - 1, 1000 / portTICK_PERIOD_MS);
        
        if (rx_len > 0) {
            g_rx_buf[rx_len] = '\0';  // 添加字符串结束符
            ESP_LOGI(TAG, "接收到数据: %s", g_rx_buf);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // 短暂延时
    }
}

/**
 * @brief       UART发送任务(每秒发送一次测试数据)
 * @param       pvParameters: 传入参数
 * @retval      无
 */
static void uart_send_task(void *pvParameters) {
    char send_buf[UART_BUF_SIZE];
    
    while (1) {
        // 构建测试数据，包含计数器
        g_send_count++;
        sprintf(send_buf, "ESP32S3 Test Data: %d\r\n", g_send_count);
        
        // 发送数据到STM32
        uart_send_string(send_buf);
        ESP_LOGI(TAG, "发送数据: %s", send_buf);
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1秒发送一次
    }
}

/**
 * @brief       创建UART接收任务
 * @param       无
 * @retval      无
 */
void uart_create_receive_task(void) {
    xTaskCreate(uart_receive_task, "uart_receive_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "UART接收任务创建成功");
}

/**
 * @brief       创建UART发送任务
 * @param       无
 * @retval      无
 */
void uart_create_send_task(void) {
    xTaskCreate(uart_send_task, "uart_send_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "UART发送任务创建成功");
}

/**
 * @brief       获取最新接收的数据
 * @param       buf: 存储数据的缓冲区
 * @param       max_len: 缓冲区最大长度
 * @retval      实际数据长度
 */
uint16_t uart_get_received_data(char* buf, uint16_t max_len) {
    if (buf == NULL || max_len == 0) return 0;
    
    uint16_t copy_len = strlen(g_rx_buf) < max_len - 1 ? strlen(g_rx_buf) : max_len - 1;
    strncpy(buf, g_rx_buf, copy_len);
    buf[copy_len] = '\0';
    
    return copy_len;
}
    
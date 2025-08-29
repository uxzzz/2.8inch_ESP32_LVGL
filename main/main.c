#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "esp_timer.h"
#include "lv_demos.h"
#include "lv_test_ui.h"
#include "driver/uart.h"
#include "usart.h"

static void inc_lvgl_tick(void *arg)
{
    lv_tick_inc(10);
}
// UART1测试任务
void uart1_test_task(void *pvParameters)
{
    uint8_t data[UART_BUF_SIZE];
    int test_count = 0;
    
    while(1) {
        // 发送测试消息
        char message[64];
        int len = snprintf(message, sizeof(message), "UART1 Test Message %d\n", test_count++);
        uart_write_bytes(UART_NUM_1, message, len);
        printf("Sent: %s", message);
        
        // 尝试接收数据
        int received_len = uart_read_bytes(UART_NUM_1, data, UART_BUF_SIZE - 1, pdMS_TO_TICKS(10));
        if (received_len > 0) {
            data[received_len] = '\0'; // 添加字符串结束符
            // 发送相同的回复
            uart_write_bytes(UART_NUM_1, (const char*)data, received_len);
            printf("Received %d bytes: %s\n", received_len, data);
        } else {
            printf("No data received\n");
        }
        
        // 等待一段时间
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
// 添加UART回显任务
void uart_echo_task(void *pvParameters)
{
    uint8_t data[UART_BUF_SIZE];
    
    while(1) {
        // 尝试接收数据
        int received_len = uart_read_bytes(UART_NUM_1, data, UART_BUF_SIZE - 1, 100 / portTICK_PERIOD_MS);
        if (received_len > 0) {
            data[received_len] = '\0'; // 添加字符串结束符
            printf("Received %d bytes: %s\n", received_len, data);
            
            // 发送相同的回复
            uart_write_bytes(UART_NUM_1, (const char*)data, received_len);
            printf("Echoed back: %s\n", data);
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    // 初始化背光控制
    #define LCD_BL_PIN 21  // 根据实际硬件连接修改
    esp_rom_gpio_pad_select_gpio(LCD_BL_PIN);
    gpio_set_direction(LCD_BL_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_BL_PIN, 1);  // 开启背光
    lv_init();            //init lvgl
    lv_port_disp_init();  //init display
    lv_port_indev_init(); //init touch screen
    /* 为LVGL提供时基单元 */
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &inc_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 10 * 1000));
    uart_comm_init();
    
    // 创建UART回显任务
    xTaskCreate(uart1_test_task, "uart1_test_task", 4096, NULL, 10, NULL);
    
    lv_test_ui();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_task_handler();
    }
}
// void app_main()
// {
//     // 初始化背光控制
//     #define LCD_BL_PIN 21  // 根据实际硬件连接修改
//     esp_rom_gpio_pad_select_gpio(LCD_BL_PIN);
//     gpio_set_direction(LCD_BL_PIN, GPIO_MODE_OUTPUT);
//     gpio_set_level(LCD_BL_PIN, 1);  // 开启背光
    
//     // 添加调试输出
//     printf("Backlight initialized\n");
    
//     // 原有的初始化代码
//     lv_init();            //init lvgl
//     lv_port_disp_init();  //init display
//     lv_port_indev_init(); //init touch screen
//     /* 为LVGL提供时基单元 */
//     const esp_timer_create_args_t lvgl_tick_timer_args = {
//         .callback = &inc_lvgl_tick,
//         .name = "lvgl_tick"
//     };
//     esp_timer_handle_t lvgl_tick_timer = NULL;
//     ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
//     ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 10 * 1000));
//     uart_comm_init();
    
//     // 创建简单的测试界面
//     lv_obj_t * label = lv_label_create(lv_scr_act());
//     lv_label_set_text(label, "Hello World!");
//     lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    
//     printf("LVGL test interface created\n");
    
//     while (1) {
//         lv_task_handler();
//         vTaskDelay(pdMS_TO_TICKS(10));
//     }
// }







// #include <stdio.h>
// #include "sdkconfig.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_system.h"
// #include "esp_spi_flash.h"
// #include "driver/gpio.h"
// #include "driver/uart.h"

// #include "lvgl.h"
// #include "lv_port_disp.h"
// #include "lv_port_indev.h"
// #include "esp_timer.h"
// #include "lv_demos.h"
// #include "lv_test_ui.h"
// #include "usart.h"
// // UART1配置
// // #define UART1_TX_PIN 1
// // #define UART1_RX_PIN 3
// // #define UART1_BAUD_RATE 115200
// // #define UART1_BUFFER_SIZE 1024

// static void inc_lvgl_tick(void *arg)
// {
//     lv_tick_inc(10);
// }

// // UART1初始化函数
// void uart1_init(void)
// {
//     uart_config_t uart_config = {
//         .baud_rate = UART_BAUD_RATE,
//         .data_bits = UART_DATA_8_BITS,
//         .parity = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//         .source_clk = UART_SCLK_DEFAULT,
//     };
    
//     // 安装UART驱动程序
//     uart_driver_install(UART_NUM_1, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    
//     // 配置UART参数
//     uart_param_config(UART_NUM_1, &uart_config);
    
//     // 设置UART引脚
//     uart_set_pin(UART_NUM_1, USART_TX_GPIO_PIN, USART_RX_GPIO_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
//     printf("UART1 initialized successfully\n");
// }



// void app_main(void)
// {
//     // 初始化背光控制
//     #define LCD_BL_PIN 21
//     esp_rom_gpio_pad_select_gpio(LCD_BL_PIN);
//     gpio_set_direction(LCD_BL_PIN, GPIO_MODE_OUTPUT);
//     gpio_set_level(LCD_BL_PIN, 1);  // 开启背光
    
//     // 初始化LVGL
//     lv_init();
//     lv_port_disp_init();
//     lv_port_indev_init();
    
//     // 为LVGL提供时基单元
//     const esp_timer_create_args_t lvgl_tick_timer_args = {
//         .callback = &inc_lvgl_tick,
//         .name = "lvgl_tick"
//     };
//     esp_timer_handle_t lvgl_tick_timer = NULL;
//     ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
//     ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 10 * 1000));
//     uart_comm_init();
//     // 初始化UART1
//     //uart1_init();
    
//     // 创建UART1测试任务
//     xTaskCreate(uart1_test_task, "uart1_test_task", 4096, NULL, 10, NULL);
    
//     // 初始化其他UART通信（如果有）
    
    
//     // 初始化LVGL测试UI
//     lv_test_ui();
    
//     printf("UART test application started\n");
    
//     // 主循环
//     while (1) {
//         vTaskDelay(pdMS_TO_TICKS(10));
//         lv_task_handler();
//     }
// }
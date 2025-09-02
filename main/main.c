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
#include "lv_voltage_ui.h"
#include "driver/uart.h"
#include "usart.h"
#include "esp_log.h"
#include "lv_photogate_ui.h"

static const char *TAG = "UART_COMM";
static void inc_lvgl_tick(void *arg)
{
    lv_tick_inc(10);
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
    ESP_LOGI(TAG, "UART初始化完成");
    
    //光电传感器测试
    lv_photogate_ui();

    //电压测试
    // lv_voltage_ui();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_task_handler();
    }
}
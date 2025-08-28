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
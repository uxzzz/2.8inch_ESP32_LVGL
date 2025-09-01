#ifndef __LV_PHOTOGATE_UI_H
#define __LV_PHOTOGATE_UI_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "string.h"
#include "usart.h"
#include "lvgl.h"
#include <stdlib.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "soc/uart_reg.h"
#include "soc/interrupts.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include <math.h>

// 声明外部字体
LV_FONT_DECLARE(lv_font_gb2312_wryh_26);

// 函数声明
void lv_photogate_ui(void);
void create_photogate_ui(void);
void uart_receive_task(void *pvParameters);

// 全局变量声明
extern lv_obj_t *ch_a_container;
extern lv_obj_t *ch_b_container;
extern lv_obj_t *ch_a_title;
extern lv_obj_t *ch_a_feed_in_label;
extern lv_obj_t *ch_a_feed_in_value;
extern lv_obj_t *ch_a_feed_out_label;
extern lv_obj_t *ch_a_feed_out_value;
extern lv_obj_t *ch_a_sensor_a1_label;
extern lv_obj_t *ch_a_sensor_a1_value;
extern lv_obj_t *ch_a_sensor_a2_label;
extern lv_obj_t *ch_a_sensor_a2_value;
extern lv_obj_t *ch_b_title;
extern lv_obj_t *ch_b_feed_in_label;
extern lv_obj_t *ch_b_feed_in_value;
extern lv_obj_t *ch_b_feed_out_label;
extern lv_obj_t *ch_b_feed_out_value;
extern lv_obj_t *ch_b_sensor_b1_label;
extern lv_obj_t *ch_b_sensor_b1_value;
extern lv_obj_t *ch_b_sensor_b2_label;
extern lv_obj_t *ch_b_sensor_b2_value;

// // 串口相关配置
// #define UART_PORT_NUM UART_NUM_1
// #define UART_BUF_SIZE 1024

#endif /* __LV_PHOTOGATE_UI_H */
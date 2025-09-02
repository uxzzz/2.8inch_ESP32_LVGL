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


// 函数声明
void lv_photogate_ui(void);
void create_photogate_ui(void);
void uart_receive_task(void *pvParameters);


#endif /* __LV_PHOTOGATE_UI_H */
#ifndef __LV_TEST_UI_H
#define __LV_TEST_UI_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
//#include "lcd.h"
#include "string.h"
#include "usart.h"
//#include "lcd_display.h"
//#include "myiic.h"
//#include "xl9555.h"
#include "lvgl.h"
//#include "../../components/BSP/USART/usart.h"
#include <stdlib.h>
#include <string.h>
//#include "lv_main_ui.h"
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
void lv_test_ui(void);
static void send_btn_event_handler(lv_event_t *e);
static void close_btn_event_handler(lv_event_t *e);
static void kb_event_handler(lv_event_t *e);
static void ta_event_handler(lv_event_t *e);
void add_serial_data(void);
//void clear_serial_buffer(void);
static void clear_serial_buffer(lv_event_t *e) ;
void create_serial_monitor_window(void);
static void global_click_event_handler(lv_event_t *e);
#endif /* __LV_TEST_UI_H */
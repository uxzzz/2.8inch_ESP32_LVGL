#include "lv_photogate_ui.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"

// 如果你的工程使用其他路径或 SDK（如 esp-idf），把上面的 FreeRTOS/stream_buffer include 调整为你工程所需的路径
// 串口相关函数（uart_read_bytes、UART_PORT_NUM、UART_BUF_SIZE）假定在别处定义

// 定义字体
LV_FONT_DECLARE(Chinese_1);

// 定义UI组件变量
static lv_obj_t *ch_a_container;  // 通道A容器
static lv_obj_t *ch_b_container;  // 通道B容器

// 通道A的UI组件
static lv_obj_t *ch_a_title;      // 通道A标题
static lv_obj_t *ch_a_feed_in_label;  // 进料次数标签
static lv_obj_t *ch_a_feed_in_value;  // 进料次数值
static lv_obj_t *ch_a_feed_out_label; // 退料次数标签
static lv_obj_t *ch_a_feed_out_value; // 退料次数值
static lv_obj_t *ch_a_feed_in_fail_label;  // 进料失败标签
static lv_obj_t *ch_a_feed_in_fail_value;  // 进料失败值
static lv_obj_t *ch_a_feed_out_fail_label; // 退料失败标签
static lv_obj_t *ch_a_feed_out_fail_value; // 退料失败值
static lv_obj_t *ch_a_voltage_label;      // 电压标签
static lv_obj_t *ch_a_voltage_value;      // 电压值
static lv_obj_t *ch_a_sensor_a1_label;    // 光电门A1文本标签（需改颜色）
static lv_obj_t *ch_a_sensor_a1_value;    // 光电门A1数值标签（抖动次数，颜色不变）
static lv_obj_t *ch_a_sensor_a2_label;    // 光电门A2文本标签（需改颜色）
static lv_obj_t *ch_a_sensor_a2_value;    // 光电门A2数值标签（抖动次数，颜色不变）

// 通道B的UI组件
static lv_obj_t *ch_b_title;      // 通道B标题
static lv_obj_t *ch_b_feed_in_label;  // 进料次数标签
static lv_obj_t *ch_b_feed_in_value;  // 进料次数值
static lv_obj_t *ch_b_feed_out_label; // 退料次数标签
static lv_obj_t *ch_b_feed_out_value; // 退料次数值
static lv_obj_t *ch_b_feed_in_fail_label;  // 进料失败标签
static lv_obj_t *ch_b_feed_in_fail_value;  // 进料失败值
static lv_obj_t *ch_b_feed_out_fail_label; // 退料失败标签
static lv_obj_t *ch_b_feed_out_fail_value; // 退料失败值
static lv_obj_t *ch_b_voltage_label;      // 电压标签
static lv_obj_t *ch_b_voltage_value;      // 电压值
static lv_obj_t *ch_b_sensor_b1_label;    // 光电门B1文本标签（需改颜色）
static lv_obj_t *ch_b_sensor_b1_value;    // 光电门B1数值标签（抖动次数，颜色不变）
static lv_obj_t *ch_b_sensor_b2_label;    // 光电门B2文本标签（需改颜色）
static lv_obj_t *ch_b_sensor_b2_value;    // 光电门B2数值标签（抖动次数，颜色不变)

// 任务和流缓冲区
static TaskHandle_t serial_task_handle = NULL;
static StreamBufferHandle_t uart_stream_buf = NULL;
static lv_timer_t *update_timer = NULL;  // UI更新定时器

// 流缓冲区配置
#define STREAM_BUF_SIZE 4096     // 流缓冲区大小
#define TRIGGER_LEVEL 128        // 触发级别

// 屏幕尺寸
#define SCREEN_WIDTH 320         // 宽度320px
#define SCREEN_HEIGHT 240        // 高度240px
#define CHANNEL_WIDTH (SCREEN_WIDTH / 2)  // 每个通道宽度

// 协议帧结构体（同步上位机mixed_frame_t：length=0x24，含4个光电门状态）
typedef struct {
    uint8_t header;     // 帧头 0xF7
    uint8_t address;    // 地址 0x10
    uint8_t length;     // 数据长度 0x24（上位机定义）
    uint8_t status;     // 状态码
    uint8_t function;   // 功能码 0x01（混合模式）
    uint8_t data[36];   // 36字节数据（原32字节+新增4字节光电门状态）
    uint8_t checksum;   // CRC8校验码
} __attribute__((packed)) protocol_frame_t;

// 解析后的数据（含4个光电门状态）
typedef struct {
    uint16_t ch0_feed_in_count;   // 通道0（A）进料计数
    uint16_t ch0_feed_out_count;  // 通道0（A）退料计数
    uint16_t ch0_feed_in_fail_count;  // 通道0（A）进料失败计数
    uint16_t ch0_feed_out_fail_count; // 通道0（A）退料失败计数
    uint16_t ch0_insert_jitter_count; // 通道0（A）insert光电门（A1）抖动次数
    uint16_t ch0_feedin_jitter_count; // 通道0（A）feedin光电门（A2）抖动次数
    uint16_t ch1_feed_in_count;   // 通道1（B）进料计数
    uint16_t ch1_feed_out_count;  // 通道1（B）退料计数
    uint16_t ch1_feed_in_fail_count;  // 通道1（B）进料失败计数
    uint16_t ch1_feed_out_fail_count; // 通道1（B）退料失败计数
    uint16_t ch1_insert_jitter_count; // 通道1（B）insert光电门（B1）抖动次数
    uint16_t ch1_feedin_jitter_count; // 通道1（B）feedin光电门（B2）抖动次数
    float vol_motor1;             // 电机1电压（通道A）
    float vol_motor2;             // 电机2电压（通道B）
    uint8_t status_code;          // 状态码
    uint8_t ch0_insert_state;     // 通道0（A）insert光电门状态（A1：1=检测到，0=未检测）
    uint8_t ch0_feedin_state;     // 通道0（A）feedin光电门状态（A2：1=检测到，0=未检测）
    uint8_t ch1_insert_state;     // 通道1（B）insert光电门状态（B1：1=检测到，0=未检测）
    uint8_t ch1_feedin_state;     // 通道1（B）feedin光电门状态（B2：1=检测到，0=未检测）
} parsed_data_t;

// 上一次的数据用于比较
static parsed_data_t last_data = {0};

// 存储当前显示的值，用于比较是否需要更新
static char ch_a_feed_in_value_text[16] = "0";
static char ch_a_feed_out_value_text[16] = "0";
static char ch_a_feed_in_fail_value_text[16] = "0";
static char ch_a_feed_out_fail_value_text[16] = "0";
static char ch_a_voltage_value_text[16] = "0.00 V";
static char ch_a_sensor_a1_value_text[16] = "0";
static char ch_a_sensor_a2_value_text[16] = "0";

static char ch_b_feed_in_value_text[16] = "0";
static char ch_b_feed_out_value_text[16] = "0";
static char ch_b_feed_in_fail_value_text[16] = "0";
static char ch_b_feed_out_fail_value_text[16] = "0";
static char ch_b_voltage_value_text[16] = "0.00 V";
static char ch_b_sensor_b1_value_text[16] = "0";
static char ch_b_sensor_b2_value_text[16] = "0";

// --- 颜色缓存（用于避免频繁查询/写入 LVGL） ---
static lv_color_t ch_a_feed_in_value_color_cache;
static lv_color_t ch_a_feed_out_value_color_cache;
static lv_color_t ch_a_feed_in_fail_value_color_cache;
static lv_color_t ch_a_feed_out_fail_value_color_cache;
static lv_color_t ch_a_voltage_value_color_cache;
static lv_color_t ch_a_sensor_a1_value_color_cache;
static lv_color_t ch_a_sensor_a2_value_color_cache;
static lv_color_t ch_a_sensor_a1_label_color_cache;
static lv_color_t ch_a_sensor_a2_label_color_cache;
static lv_color_t ch_a_title_color_cache;

static lv_color_t ch_b_feed_in_value_color_cache;
static lv_color_t ch_b_feed_out_value_color_cache;
static lv_color_t ch_b_feed_in_fail_value_color_cache;
static lv_color_t ch_b_feed_out_fail_value_color_cache;
static lv_color_t ch_b_voltage_value_color_cache;
static lv_color_t ch_b_sensor_b1_value_color_cache;
static lv_color_t ch_b_sensor_b2_value_color_cache;
static lv_color_t ch_b_sensor_b1_label_color_cache;
static lv_color_t ch_b_sensor_b2_label_color_cache;
static lv_color_t ch_b_title_color_cache;

// CRC8计算函数（保持不变）
static uint8_t crc8_calculate(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;
    uint8_t i;
    
    while (length--) {
        crc ^= *data++;
        for (i = 0; i < 8; i++) {
            if (crc & 0x01)
                crc = (crc >> 1) ^ 0x8C;
            else
                crc >>= 1;
        }
    }
    return crc;
}

// 互斥锁用于保护UI更新（保持不变）
static SemaphoreHandle_t ui_mutex = NULL;

// 串口数据接收任务（保持不变）
void uart_receive_task(void *pvParameters) {
    uint8_t temp_buffer[UART_BUF_SIZE];
    while(1) {
        int rx_len = uart_read_bytes(UART_PORT_NUM, temp_buffer, UART_BUF_SIZE - 1, pdMS_TO_TICKS(10));
        
        if (rx_len > 0 && uart_stream_buf != NULL) {
            xStreamBufferSend(uart_stream_buf, temp_buffer, rx_len, portMAX_DELAY);
        }
        taskYIELD();
    }
}

// 检查数据是否有变化（保持不变）
static bool data_changed(const parsed_data_t *new_data) {
    return memcmp(new_data, &last_data, sizeof(parsed_data_t)) != 0;
}

// 解析协议帧（保持不变）
static bool parse_protocol_frame(const protocol_frame_t *frame, parsed_data_t *data) {
    if (frame->header != 0xF7 || frame->address != 0x10 || frame->length != 0x24 || frame->function != 0x01) {
        return false;
    }
    
    // CRC校验：数据长度36字节，check_data总长=4+36=40字节
    uint8_t check_data[40];
    check_data[0] = frame->address;
    check_data[1] = frame->length;
    check_data[2] = frame->status;
    check_data[3] = frame->function;
    memcpy(&check_data[4], frame->data, 36);
    
    uint8_t crc_calculated = crc8_calculate(check_data, 40);
    if (crc_calculated != frame->checksum) {
        return false;
    }
    
    // 解析原有数据
    data->ch0_feed_in_count = frame->data[0] | (frame->data[1] << 8);
    data->ch0_feed_out_count = frame->data[2] | (frame->data[3] << 8);
    data->ch0_feed_in_fail_count = frame->data[4] | (frame->data[5] << 8);
    data->ch0_feed_out_fail_count = frame->data[6] | (frame->data[7] << 8);
    data->ch0_insert_jitter_count = frame->data[8] | (frame->data[9] << 8);
    data->ch0_feedin_jitter_count = frame->data[10] | (frame->data[11] << 8);
    data->ch1_feed_in_count = frame->data[12] | (frame->data[13] << 8);
    data->ch1_feed_out_count = frame->data[14] | (frame->data[15] << 8);
    data->ch1_feed_in_fail_count = frame->data[16] | (frame->data[17] << 8);
    data->ch1_feed_out_fail_count = frame->data[18] | (frame->data[19] << 8);
    data->ch1_insert_jitter_count = frame->data[20] | (frame->data[21] << 8);
    data->ch1_feedin_jitter_count = frame->data[22] | (frame->data[23] << 8);
    memcpy(&data->vol_motor1, &frame->data[24], 4);
    memcpy(&data->vol_motor2, &frame->data[28], 4);
    data->status_code = frame->status;
    
    // 解析光电门状态
    data->ch0_insert_state = frame->data[32];  // 通道0（A）insert状态（A1）
    data->ch1_insert_state = frame->data[33];  // 通道1（B）insert状态（B1）
    data->ch0_feedin_state = frame->data[34];  // 通道0（A）feedin状态（A2）
    data->ch1_feedin_state = frame->data[35];  // 通道1（B）feedin状态（B2）
    
    return true;
}

// 带缓存的安全更新函数
// text_cache: 指向用于缓存文本的缓冲区（如果为 NULL 则不使用文本缓存）
// text_cache_size: text_cache 的大小
// color_cache: 指向用于缓存颜色的 lv_color_t（若为 NULL 则每次强制设置颜色）
static void safe_update_label_cached(lv_obj_t *label, const char *new_text,
                                     lv_color_t color, char *text_cache,
                                     size_t text_cache_size, lv_color_t *color_cache)
{
    if (label == NULL) return;

    // 文本更新：如果提供了 text_cache 则用缓存比较以避免调用 lv_label_get_text
    if (new_text != NULL) {
        bool need_set_text = true;
        if (text_cache != NULL) {
            if (strncmp(text_cache, new_text, text_cache_size) == 0) {
                need_set_text = false;
            }
        } else {
            // 没有缓存则回退到读取 label 文本（仅在必须时）
            const char *cur = lv_label_get_text(label);
            if (cur && strcmp(cur, new_text) == 0) need_set_text = false;
        }

        if (need_set_text) {
            lv_label_set_text(label, new_text);
            if (text_cache != NULL) {
                strncpy(text_cache, new_text, text_cache_size);
                text_cache[text_cache_size - 1] = '\0';
            }
        }
    }

    // 颜色更新：如果提供 color_cache 则以缓存为准，减少不必要的 set 调用
    if (color_cache != NULL) {
        if (color_cache->full != color.full) {
            lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
            *color_cache = color;
        }
    } else {
        lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    }
}

// 销毁UI资源（保持不变）
static void destroy_ui_resources() {
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    
    if (serial_task_handle) {
        vTaskDelete(serial_task_handle);
        serial_task_handle = NULL;
    }
    
    if (uart_stream_buf) {
        vStreamBufferDelete(uart_stream_buf);
        uart_stream_buf = NULL;
    }
    
    if (ui_mutex) {
        vSemaphoreDelete(ui_mutex);
        ui_mutex = NULL;
    }
}

// 创建通道UI（文本标签初始颜色设为白色）
static void create_channel_ui(lv_obj_t *parent, const char *title, 
                             lv_obj_t **feed_in_label, lv_obj_t **feed_in_value,
                             lv_obj_t **feed_out_label, lv_obj_t **feed_out_value,
                             lv_obj_t **feed_in_fail_label, lv_obj_t **feed_in_fail_value,
                             lv_obj_t **feed_out_fail_label, lv_obj_t **feed_out_fail_value,
                             lv_obj_t **voltage_label, lv_obj_t **voltage_value,
                             lv_obj_t **sensor_a1_label, lv_obj_t **sensor_a1_value,
                             lv_obj_t **sensor_a2_label, lv_obj_t **sensor_a2_value) {
    // 通道标题
    lv_obj_t *title_label = lv_label_create(parent);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 2);
    
    // 进料次数
    *feed_in_label = lv_label_create(parent);
    lv_label_set_text(*feed_in_label, "进料次数：");
    lv_obj_set_style_text_font(*feed_in_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_in_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(*feed_in_label, LV_ALIGN_TOP_LEFT, 20, 30);
    
    *feed_in_value = lv_label_create(parent);
    lv_label_set_text(*feed_in_value, "0");
    lv_obj_set_style_text_font(*feed_in_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_in_value, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_width(*feed_in_value, 60);
    lv_obj_align_to(*feed_in_value, *feed_in_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    
    // 退料次数
    *feed_out_label = lv_label_create(parent);
    lv_label_set_text(*feed_out_label, "退料次数：");
    lv_obj_set_style_text_font(*feed_out_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_out_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(*feed_out_label, *feed_in_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    
    *feed_out_value = lv_label_create(parent);
    lv_label_set_text(*feed_out_value, "0");
    lv_obj_set_style_text_font(*feed_out_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_out_value, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_width(*feed_out_value, 60);
    lv_obj_align_to(*feed_out_value, *feed_out_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    
    // 进料失败次数
    *feed_in_fail_label = lv_label_create(parent);
    lv_label_set_text(*feed_in_fail_label, "进料失败次数：");
    lv_obj_set_style_text_font(*feed_in_fail_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_in_fail_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(*feed_in_fail_label, *feed_out_label, LV_ALIGN_OUT_BOTTOM_LEFT, -5, 10);
    
    *feed_in_fail_value = lv_label_create(parent);
    lv_label_set_text(*feed_in_fail_value, "0");
    lv_obj_set_style_text_font(*feed_in_fail_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_in_fail_value, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_width(*feed_in_fail_value, 60);
    lv_obj_align_to(*feed_in_fail_value, *feed_in_fail_label, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    
    // 退料失败次数
    *feed_out_fail_label = lv_label_create(parent);
    lv_label_set_text(*feed_out_fail_label, "退料失败次数：");
    lv_obj_set_style_text_font(*feed_out_fail_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_out_fail_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(*feed_out_fail_label, *feed_in_fail_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    
    *feed_out_fail_value = lv_label_create(parent);
    lv_label_set_text(*feed_out_fail_value, "0");
    lv_obj_set_style_text_font(*feed_out_fail_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_out_fail_value, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_width(*feed_out_fail_value, 60);
    lv_obj_align_to(*feed_out_fail_value, *feed_out_fail_label, LV_ALIGN_OUT_RIGHT_MID,10, 0);
    
    // 电压
    *voltage_label = lv_label_create(parent);
    lv_label_set_text(*voltage_label, "电压：");
    lv_obj_set_style_text_font(*voltage_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*voltage_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(*voltage_label, *feed_out_fail_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    
    *voltage_value = lv_label_create(parent);
    lv_label_set_text(*voltage_value, "0.00 V");
    lv_obj_set_style_text_font(*voltage_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*voltage_value, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_width(*voltage_value, 80);
    lv_obj_align_to(*voltage_value, *voltage_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    
    // 光电门1文本标签（初始白色，后续动态改色）
    *sensor_a1_label = lv_label_create(parent);
    lv_label_set_text(*sensor_a1_label, "插入光电门：");
    lv_obj_set_style_text_font(*sensor_a1_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*sensor_a1_label, lv_color_white(), LV_PART_MAIN); // 初始白色
    lv_obj_align_to(*sensor_a1_label, *voltage_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    
    // 光电门1数值标签（初始绿色）
    *sensor_a1_value = lv_label_create(parent);
    lv_label_set_text(*sensor_a1_value, "0");
    lv_obj_set_style_text_font(*sensor_a1_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*sensor_a1_value, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_width(*sensor_a1_value, 60);
    lv_obj_align_to(*sensor_a1_value, *sensor_a1_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    
    // 光电门2文本标签（初始白色，后续动态改色）
    *sensor_a2_label = lv_label_create(parent);
    lv_label_set_text(*sensor_a2_label, "进料光电门：");
    lv_obj_set_style_text_font(*sensor_a2_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*sensor_a2_label, lv_color_white(), LV_PART_MAIN); // 初始白色
    lv_obj_align_to(*sensor_a2_label, *sensor_a1_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    
    // 光电门2数值标签（初始绿色）
    *sensor_a2_value = lv_label_create(parent);
    lv_label_set_text(*sensor_a2_value, "0");
    lv_obj_set_style_text_font(*sensor_a2_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*sensor_a2_value, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_width(*sensor_a2_value, 60);
    lv_obj_align_to(*sensor_a2_value, *sensor_a2_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
}

// 创建光电门监控界面（保持不变，修正通道B文本标签）
void create_photogate_ui(void) {
    // 清除屏幕滚动标志
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    
    // 设置屏幕背景
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_100, LV_PART_MAIN);
    
    // 通道A容器
    ch_a_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ch_a_container, CHANNEL_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(ch_a_container, 0, 0);
    lv_obj_set_style_border_width(ch_a_container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ch_a_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ch_a_container, LV_OPA_100, LV_PART_MAIN);
    lv_obj_clear_flag(ch_a_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // 通道B容器
    ch_b_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ch_b_container, CHANNEL_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(ch_b_container, CHANNEL_WIDTH, 0);
    lv_obj_set_style_border_width(ch_b_container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ch_b_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ch_b_container, LV_OPA_100, LV_PART_MAIN);
    lv_obj_clear_flag(ch_b_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建通道A UI（文本标签为A1/A2）
    create_channel_ui(ch_a_container, "通道A", 
                     &ch_a_feed_in_label, &ch_a_feed_in_value,
                     &ch_a_feed_out_label, &ch_a_feed_out_value,
                     &ch_a_feed_in_fail_label, &ch_a_feed_in_fail_value,
                     &ch_a_feed_out_fail_label, &ch_a_feed_out_fail_value,
                     &ch_a_voltage_label, &ch_a_voltage_value,
                     &ch_a_sensor_a1_label, &ch_a_sensor_a1_value,
                     &ch_a_sensor_a2_label, &ch_a_sensor_a2_value);
    ch_a_title = lv_obj_get_child(ch_a_container, 0);
    
    // 创建通道B UI（文本标签为B1/B2）
    create_channel_ui(ch_b_container, "通道B", 
                     &ch_b_feed_in_label, &ch_b_feed_in_value,
                     &ch_b_feed_out_label, &ch_b_feed_out_value,
                     &ch_b_feed_in_fail_label, &ch_b_feed_in_fail_value,
                     &ch_b_feed_out_fail_label, &ch_b_feed_out_fail_value,
                     &ch_b_voltage_label, &ch_b_voltage_value,
                     &ch_b_sensor_b1_label, &ch_b_sensor_b1_value,
                     &ch_b_sensor_b2_label, &ch_b_sensor_b2_value);
    ch_b_title = lv_obj_get_child(ch_b_container, 0);
    // 修正通道B光电门文本标签初始文本
    lv_label_set_text(ch_b_sensor_b1_label, "插入光电门：");
    lv_label_set_text(ch_b_sensor_b2_label, "进料光电门：");

    // 初始化颜色缓存（与 create_channel_ui 中设置的初始颜色一致）
    ch_a_feed_in_value_color_cache = lv_color_hex(0x00FF00);
    ch_a_feed_out_value_color_cache = lv_color_hex(0x00FF00);
    ch_a_feed_in_fail_value_color_cache = lv_color_hex(0x00FF00);
    ch_a_feed_out_fail_value_color_cache = lv_color_hex(0x00FF00);
    ch_a_voltage_value_color_cache = lv_color_hex(0x00FF00);
    ch_a_sensor_a1_value_color_cache = lv_color_hex(0x00FF00);
    ch_a_sensor_a2_value_color_cache = lv_color_hex(0x00FF00);
    ch_a_sensor_a1_label_color_cache = lv_color_white();
    ch_a_sensor_a2_label_color_cache = lv_color_white();
    ch_a_title_color_cache = lv_color_white();

    ch_b_feed_in_value_color_cache = lv_color_hex(0x00FF00);
    ch_b_feed_out_value_color_cache = lv_color_hex(0x00FF00);
    ch_b_feed_in_fail_value_color_cache = lv_color_hex(0x00FF00);
    ch_b_feed_out_fail_value_color_cache = lv_color_hex(0x00FF00);
    ch_b_voltage_value_color_cache = lv_color_hex(0x00FF00);
    ch_b_sensor_b1_value_color_cache = lv_color_hex(0x00FF00);
    ch_b_sensor_b2_value_color_cache = lv_color_hex(0x00FF00);
    ch_b_sensor_b1_label_color_cache = lv_color_white();
    ch_b_sensor_b2_label_color_cache = lv_color_white();
    ch_b_title_color_cache = lv_color_white();
}

// ########################### 改进后的 update_ui_timer（完整实现） ###########################
static void update_ui_timer(lv_timer_t *timer) {
    (void) timer;
    if (uart_stream_buf == NULL) return;
    
    const uint8_t max_frames = 5;
    uint8_t frame_count = 0;
    
    // 仅当足够数据时尝试读取完整帧
    while (frame_count < max_frames &&
           xStreamBufferBytesAvailable(uart_stream_buf) >= sizeof(protocol_frame_t)) {
        frame_count++;
        
        protocol_frame_t frame;
        size_t bytes_read = xStreamBufferReceive(uart_stream_buf, &frame, sizeof(protocol_frame_t), 0);
        if (bytes_read != sizeof(protocol_frame_t)) {
            // 如果未读到完整帧则退出循环
            break;
        }
        
        parsed_data_t data;
        if (!parse_protocol_frame(&frame, &data)) {
            // CRC 或帧头不对，跳过
            continue;
        }
        
        if (!data_changed(&data)) {
            // 数据未变则跳过显示更新，但仍保存 last_data（可选，不保存以避免误判）
            // continue;
        }

        // 保护 LVGL 更新
        if (xSemaphoreTake(ui_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            char buffer[32];
            lv_color_t color;

            // -------------------------- 通道A（ch0）更新（使用缓存函数） --------------------------
            snprintf(buffer, sizeof(buffer), "%d", data.ch0_feed_in_count);
            safe_update_label_cached(ch_a_feed_in_value, buffer, lv_color_hex(0x00FF00),
                                     ch_a_feed_in_value_text, sizeof(ch_a_feed_in_value_text),
                                     &ch_a_feed_in_value_color_cache);

            snprintf(buffer, sizeof(buffer), "%d", data.ch0_feed_out_count);
            safe_update_label_cached(ch_a_feed_out_value, buffer, lv_color_hex(0x00FF00),
                                     ch_a_feed_out_value_text, sizeof(ch_a_feed_out_value_text),
                                     &ch_a_feed_out_value_color_cache);

            snprintf(buffer, sizeof(buffer), "%d", data.ch0_feed_in_fail_count);
            color = data.ch0_feed_in_fail_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_a_feed_in_fail_value, buffer, color,
                                     ch_a_feed_in_fail_value_text, sizeof(ch_a_feed_in_fail_value_text),
                                     &ch_a_feed_in_fail_value_color_cache);

            snprintf(buffer, sizeof(buffer), "%d", data.ch0_feed_out_fail_count);
            color = data.ch0_feed_out_fail_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_a_feed_out_fail_value, buffer, color,
                                     ch_a_feed_out_fail_value_text, sizeof(ch_a_feed_out_fail_value_text),
                                     &ch_a_feed_out_fail_value_color_cache);

            snprintf(buffer, sizeof(buffer), "%.2f V", data.vol_motor1);
            color = data.vol_motor1 < 0.4f ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_a_voltage_value, buffer, color,
                                     ch_a_voltage_value_text, sizeof(ch_a_voltage_value_text),
                                     &ch_a_voltage_value_color_cache);

            // 光电门文本标签颜色（A1）
            color = (data.ch0_insert_state == 1) ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_a_sensor_a1_label, NULL, color,
                                     NULL, 0, &ch_a_sensor_a1_label_color_cache);

            // 光电门A1数值标签（抖动次数）
            snprintf(buffer, sizeof(buffer), "%d", data.ch0_insert_jitter_count);
            color = data.ch0_insert_jitter_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_a_sensor_a1_value, buffer, color,
                                     ch_a_sensor_a1_value_text, sizeof(ch_a_sensor_a1_value_text),
                                     &ch_a_sensor_a1_value_color_cache);

            // 光电门A2文本标签颜色（A2）
            color = (data.ch0_feedin_state == 1) ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_a_sensor_a2_label, NULL, color,
                                     NULL, 0, &ch_a_sensor_a2_label_color_cache);

            // 光电门A2数值标签（抖动次数）
            snprintf(buffer, sizeof(buffer), "%d", data.ch0_feedin_jitter_count);
            color = data.ch0_feedin_jitter_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_a_sensor_a2_value, buffer, color,
                                     ch_a_sensor_a2_value_text, sizeof(ch_a_sensor_a2_value_text),
                                     &ch_a_sensor_a2_value_color_cache);

            // 通道A标题颜色
            color = (data.status_code & 0x01) ? lv_color_hex(0xFF0000) : lv_color_white();
            safe_update_label_cached(ch_a_title, "通道A", color, NULL, 0, &ch_a_title_color_cache);

            // -------------------------- 通道B（ch1）更新（使用缓存函数） --------------------------
            snprintf(buffer, sizeof(buffer), "%d", data.ch1_feed_in_count);
            safe_update_label_cached(ch_b_feed_in_value, buffer, lv_color_hex(0x00FF00),
                                     ch_b_feed_in_value_text, sizeof(ch_b_feed_in_value_text),
                                     &ch_b_feed_in_value_color_cache);

            snprintf(buffer, sizeof(buffer), "%d", data.ch1_feed_out_count);
            safe_update_label_cached(ch_b_feed_out_value, buffer, lv_color_hex(0x00FF00),
                                     ch_b_feed_out_value_text, sizeof(ch_b_feed_out_value_text),
                                     &ch_b_feed_out_value_color_cache);

            snprintf(buffer, sizeof(buffer), "%d", data.ch1_feed_in_fail_count);
            color = data.ch1_feed_in_fail_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_b_feed_in_fail_value, buffer, color,
                                     ch_b_feed_in_fail_value_text, sizeof(ch_b_feed_in_fail_value_text),
                                     &ch_b_feed_in_fail_value_color_cache);

            snprintf(buffer, sizeof(buffer), "%d", data.ch1_feed_out_fail_count);
            color = data.ch1_feed_out_fail_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_b_feed_out_fail_value, buffer, color,
                                     ch_b_feed_out_fail_value_text, sizeof(ch_b_feed_out_fail_value_text),
                                     &ch_b_feed_out_fail_value_color_cache);

            snprintf(buffer, sizeof(buffer), "%.2f V", data.vol_motor2);
            color = data.vol_motor2 < 0.4f ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_b_voltage_value, buffer, color,
                                     ch_b_voltage_value_text, sizeof(ch_b_voltage_value_text),
                                     &ch_b_voltage_value_color_cache);

            // 光电门B1文本标签颜色（检测到=绿，未检测=红）
            color = (data.ch1_insert_state == 1) ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_b_sensor_b1_label, NULL, color,
                                     NULL, 0, &ch_b_sensor_b1_label_color_cache);

            // 光电门B1数值（抖动次数）
            snprintf(buffer, sizeof(buffer), "%d", data.ch1_insert_jitter_count);
            color = data.ch1_insert_jitter_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_b_sensor_b1_value, buffer, color,
                                     ch_b_sensor_b1_value_text, sizeof(ch_b_sensor_b1_value_text),
                                     &ch_b_sensor_b1_value_color_cache);

            // 光电门B2文本标签颜色
            color = (data.ch1_feedin_state == 1) ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_b_sensor_b2_label, NULL, color,
                                     NULL, 0, &ch_b_sensor_b2_label_color_cache);

            // 光电门B2数值（抖动次数）
            snprintf(buffer, sizeof(buffer), "%d", data.ch1_feedin_jitter_count);
            color = data.ch1_feedin_jitter_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            safe_update_label_cached(ch_b_sensor_b2_value, buffer, color,
                                     ch_b_sensor_b2_value_text, sizeof(ch_b_sensor_b2_value_text),
                                     &ch_b_sensor_b2_value_color_cache);

            // 通道B标题颜色
            color = (data.status_code & 0x02) ? lv_color_hex(0xFF0000) : lv_color_white();
            safe_update_label_cached(ch_b_title, "通道B", color, NULL, 0, &ch_b_title_color_cache);

            // 保存当前数据（用于下次比较）
            last_data = data;

            xSemaphoreGive(ui_mutex);
        }
    }
}

// 主启动程序（保持不变）
void lv_photogate_ui(void) {
    // 创建互斥锁
    if (ui_mutex == NULL) {
        ui_mutex = xSemaphoreCreateMutex();
    }
    
    // 创建流缓冲区
    if (uart_stream_buf == NULL) {
        uart_stream_buf = xStreamBufferCreate(STREAM_BUF_SIZE, TRIGGER_LEVEL);
    }
    
    // 创建光电门监控界面
    create_photogate_ui();
    
    // 创建LVGL定时器
    if (update_timer == NULL) {
        update_timer = lv_timer_create(update_ui_timer, 100, NULL);
        lv_timer_set_repeat_count(update_timer, -1);
    }
    
    // 创建串口接收任务
    if (serial_task_handle == NULL) {
        xTaskCreatePinnedToCore(uart_receive_task,
                    "uart_receive_task",
                    4096,
                    NULL,
                    6,
                    &serial_task_handle,
                    0);
    }
}

#include "lv_photogate_ui.h"

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
static lv_obj_t *ch_a_sensor_a1_label;    // 光电门A1标签
static lv_obj_t *ch_a_sensor_a1_value;    // 光电门A1值
static lv_obj_t *ch_a_sensor_a2_label;    // 光电门A2标签
static lv_obj_t *ch_a_sensor_a2_value;    // 光电门A2值

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
static lv_obj_t *ch_b_sensor_b1_label;    // 光电门B1标签
static lv_obj_t *ch_b_sensor_b1_value;    // 光电门B1值
static lv_obj_t *ch_b_sensor_b2_label;    // 光电门B2标签
static lv_obj_t *ch_b_sensor_b2_value;    // 光电门B2值

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

// 协议帧结构体
typedef struct {
    uint8_t header;     // 帧头 0xF7
    uint8_t address;    // 地址 0x10
    uint8_t length;     // 数据长度 0x20
    uint8_t status;     // 状态码
    uint8_t function;   // 功能码
    uint8_t data[32];   // 32字节数据（12个计数 + 2个电压）
    uint8_t checksum;   // CRC8校验码
} __attribute__((packed)) protocol_frame_t;

// 解析后的数据
typedef struct {
    uint16_t ch0_feed_in_count;   // 通道0进料计数
    uint16_t ch0_feed_out_count;  // 通道0退料计数
    uint16_t ch0_feed_in_fail_count;  // 通道0进料失败计数
    uint16_t ch0_feed_out_fail_count; // 通道0退料失败计数
    uint16_t ch0_insert_jitter_count; // 通道0 insert光电门抖动次数
    uint16_t ch0_feedin_jitter_count; // 通道0 feedin光电门抖动次数
    uint16_t ch1_feed_in_count;   // 通道1进料计数
    uint16_t ch1_feed_out_count;  // 通道1退料计数
    uint16_t ch1_feed_in_fail_count;  // 通道1进料失败计数
    uint16_t ch1_feed_out_fail_count; // 通道1退料失败计数
    uint16_t ch1_insert_jitter_count; // 通道1 insert光电门抖动次数
    uint16_t ch1_feedin_jitter_count; // 通道1 feedin光电门抖动次数
    float vol_motor1;             // 电机1电压（通道A）
    float vol_motor2;             // 电机2电压（通道B）
    uint8_t status_code;          // 状态码
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

// CRC8计算函数
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

// 互斥锁用于保护UI更新
static SemaphoreHandle_t ui_mutex = NULL;

// 串口数据接收任务
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

// 检查数据是否有变化
static bool data_changed(const parsed_data_t *new_data) {
    return memcmp(new_data, &last_data, sizeof(parsed_data_t)) != 0;
}

// 解析协议帧
static bool parse_protocol_frame(const protocol_frame_t *frame, parsed_data_t *data) {
    // 验证帧头、地址、长度和功能码
    if (frame->header != 0xF7 || frame->address != 0x10 || frame->length != 0x20 || frame->function != 0x01) {
        return false;
    }
    
    // 验证CRC
    uint8_t check_data[36];
    check_data[0] = frame->address;
    check_data[1] = frame->length;
    check_data[2] = frame->status;
    check_data[3] = frame->function;
    memcpy(&check_data[4], frame->data, 32);
    
    uint8_t crc_calculated = crc8_calculate(check_data, 36);
    if (crc_calculated != frame->checksum) {
        return false;
    }
    
    // 解析数据
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
    
    return true;
}

// 安全更新标签文本和颜色
static void safe_update_label(lv_obj_t *label, const char *new_text, lv_color_t color) {
    if (label == NULL) return;
    
    // 检查文本是否变化
    const char *current_text = lv_label_get_text(label);
    if (strcmp(current_text, new_text) != 0) {
        lv_label_set_text(label, new_text);
    }
    
    // 检查颜色是否变化
    lv_color_t current_color;
    lv_obj_get_style_text_color(label, &current_color);
    if (current_color.full != color.full) {
        lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    }
}

// LVGL定时器回调 - 安全更新UI
static void update_ui_timer(lv_timer_t *timer) {
    if (uart_stream_buf == NULL) return;
    
    // 每次最多处理5个协议帧
    uint8_t max_frames = 5;
    uint8_t frame_count = 0;
    
    while (frame_count < max_frames && 
          xStreamBufferBytesAvailable(uart_stream_buf) >= sizeof(protocol_frame_t)) {
        frame_count++;
        
        // 读取完整的协议帧
        protocol_frame_t frame;
        size_t bytes_read = xStreamBufferReceive(uart_stream_buf, &frame, sizeof(protocol_frame_t), 0);
        
        if (bytes_read != sizeof(protocol_frame_t)) {
            break;
        }
        
        // 解析协议帧
        parsed_data_t data;
        if (!parse_protocol_frame(&frame, &data)) {
            continue;
        }
        
        // 检查数据是否有变化
        if (!data_changed(&data)) {
            continue;
        }
        
        // 获取互斥锁，保护UI更新
        if (xSemaphoreTake(ui_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            char buffer[16];
            lv_color_t color;
            
            // 更新通道A
            snprintf(buffer, sizeof(buffer), "%d", data.ch0_feed_in_count);
            if (strcmp(buffer, ch_a_feed_in_value_text) != 0) {
                strncpy(ch_a_feed_in_value_text, buffer, sizeof(ch_a_feed_in_value_text));
                safe_update_label(ch_a_feed_in_value, ch_a_feed_in_value_text, lv_color_hex(0x00FF00));
            }
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch0_feed_out_count);
            if (strcmp(buffer, ch_a_feed_out_value_text) != 0) {
                strncpy(ch_a_feed_out_value_text, buffer, sizeof(ch_a_feed_out_value_text));
                safe_update_label(ch_a_feed_out_value, ch_a_feed_out_value_text, lv_color_hex(0x00FF00));
            }
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch0_feed_in_fail_count);
            color = data.ch0_feed_in_fail_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            if (strcmp(buffer, ch_a_feed_in_fail_value_text) != 0 || 
                lv_obj_get_style_text_color(ch_a_feed_in_fail_value, LV_PART_MAIN).full != color.full) {
                strncpy(ch_a_feed_in_fail_value_text, buffer, sizeof(ch_a_feed_in_fail_value_text));
                safe_update_label(ch_a_feed_in_fail_value, ch_a_feed_in_fail_value_text, color);
            }
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch0_feed_out_fail_count);
            color = data.ch0_feed_out_fail_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            if (strcmp(buffer, ch_a_feed_out_fail_value_text) != 0 || 
                lv_obj_get_style_text_color(ch_a_feed_out_fail_value, LV_PART_MAIN).full != color.full) {
                strncpy(ch_a_feed_out_fail_value_text, buffer, sizeof(ch_a_feed_out_fail_value_text));
                safe_update_label(ch_a_feed_out_fail_value, ch_a_feed_out_fail_value_text, color);
            }
            
            snprintf(buffer, sizeof(buffer), "%.2f V", data.vol_motor1);
            color = data.vol_motor1 < 0.4f ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            if (strcmp(buffer, ch_a_voltage_value_text) != 0 || 
                lv_obj_get_style_text_color(ch_a_voltage_value, LV_PART_MAIN).full != color.full) {
                strncpy(ch_a_voltage_value_text, buffer, sizeof(ch_a_voltage_value_text));
                safe_update_label(ch_a_voltage_value, ch_a_voltage_value_text, color);
            }
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch0_insert_jitter_count);
            color = data.ch0_insert_jitter_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            if (strcmp(buffer, ch_a_sensor_a1_value_text) != 0 || 
                lv_obj_get_style_text_color(ch_a_sensor_a1_value, LV_PART_MAIN).full != color.full) {
                strncpy(ch_a_sensor_a1_value_text, buffer, sizeof(ch_a_sensor_a1_value_text));
                safe_update_label(ch_a_sensor_a1_value, ch_a_sensor_a1_value_text, color);
            }
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch0_feedin_jitter_count);
            color = data.ch0_feedin_jitter_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            if (strcmp(buffer, ch_a_sensor_a2_value_text) != 0 || 
                lv_obj_get_style_text_color(ch_a_sensor_a2_value, LV_PART_MAIN).full != color.full) {
                strncpy(ch_a_sensor_a2_value_text, buffer, sizeof(ch_a_sensor_a2_value_text));
                safe_update_label(ch_a_sensor_a2_value, ch_a_sensor_a2_value_text, color);
            }
            
            // 更新通道A标题颜色基于状态
            color = (data.status_code & 0x01) ? lv_color_hex(0xFF0000) : lv_color_white();
            if (lv_obj_get_style_text_color(ch_a_title, LV_PART_MAIN).full != color.full) {
                lv_obj_set_style_text_color(ch_a_title, color, LV_PART_MAIN);
            }
            
            // 更新通道B
            snprintf(buffer, sizeof(buffer), "%d", data.ch1_feed_in_count);
            if (strcmp(buffer, ch_b_feed_in_value_text) != 0) {
                strncpy(ch_b_feed_in_value_text, buffer, sizeof(ch_b_feed_in_value_text));
                safe_update_label(ch_b_feed_in_value, ch_b_feed_in_value_text, lv_color_hex(0x00FF00));
            }
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch1_feed_out_count);
            if (strcmp(buffer, ch_b_feed_out_value_text) != 0) {
                strncpy(ch_b_feed_out_value_text, buffer, sizeof(ch_b_feed_out_value_text));
                safe_update_label(ch_b_feed_out_value, ch_b_feed_out_value_text, lv_color_hex(0x00FF00));
            }
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch1_feed_in_fail_count);
            color = data.ch1_feed_in_fail_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            if (strcmp(buffer, ch_b_feed_in_fail_value_text) != 0 || 
                lv_obj_get_style_text_color(ch_b_feed_in_fail_value, LV_PART_MAIN).full != color.full) {
                strncpy(ch_b_feed_in_fail_value_text, buffer, sizeof(ch_b_feed_in_fail_value_text));
                safe_update_label(ch_b_feed_in_fail_value, ch_b_feed_in_fail_value_text, color);
            }
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch1_feed_out_fail_count);
            color = data.ch1_feed_out_fail_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            if (strcmp(buffer, ch_b_feed_out_fail_value_text) != 0 || 
                lv_obj_get_style_text_color(ch_b_feed_out_fail_value, LV_PART_MAIN).full != color.full) {
                strncpy(ch_b_feed_out_fail_value_text, buffer, sizeof(ch_b_feed_out_fail_value_text));
                safe_update_label(ch_b_feed_out_fail_value, ch_b_feed_out_fail_value_text, color);
            }
            
            snprintf(buffer, sizeof(buffer), "%.2f V", data.vol_motor2);
            color = data.vol_motor2 < 0.4f ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            if (strcmp(buffer, ch_b_voltage_value_text) != 0 || 
                lv_obj_get_style_text_color(ch_b_voltage_value, LV_PART_MAIN).full != color.full) {
                strncpy(ch_b_voltage_value_text, buffer, sizeof(ch_b_voltage_value_text));
                safe_update_label(ch_b_voltage_value, ch_b_voltage_value_text, color);
            }
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch1_insert_jitter_count);
            color = data.ch1_insert_jitter_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            if (strcmp(buffer, ch_b_sensor_b1_value_text) != 0 || 
                lv_obj_get_style_text_color(ch_b_sensor_b1_value, LV_PART_MAIN).full != color.full) {
                strncpy(ch_b_sensor_b1_value_text, buffer, sizeof(ch_b_sensor_b1_value_text));
                safe_update_label(ch_b_sensor_b1_value, ch_b_sensor_b1_value_text, color);
            }
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch1_feedin_jitter_count);
            color = data.ch1_feedin_jitter_count == 0 ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000);
            if (strcmp(buffer, ch_b_sensor_b2_value_text) != 0 || 
                lv_obj_get_style_text_color(ch_b_sensor_b2_value, LV_PART_MAIN).full != color.full) {
                strncpy(ch_b_sensor_b2_value_text, buffer, sizeof(ch_b_sensor_b2_value_text));
                safe_update_label(ch_b_sensor_b2_value, ch_b_sensor_b2_value_text, color);
            }
            
            // 更新通道B标题颜色基于状态
            color = (data.status_code & 0x02) ? lv_color_hex(0xFF0000) : lv_color_white();
            if (lv_obj_get_style_text_color(ch_b_title, LV_PART_MAIN).full != color.full) {
                lv_obj_set_style_text_color(ch_b_title, color, LV_PART_MAIN);
            }
            
            // 保存当前数据
            last_data = data;
            
            xSemaphoreGive(ui_mutex);
        }
    }
}

// 销毁UI资源
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

// 创建通道UI
static void create_channel_ui(lv_obj_t *parent, const char *title, 
                             lv_obj_t **feed_in_label, lv_obj_t **feed_in_value,
                             lv_obj_t **feed_out_label, lv_obj_t **feed_out_value,
                             lv_obj_t **feed_in_fail_label, lv_obj_t **feed_in_fail_value,
                             lv_obj_t **feed_out_fail_label, lv_obj_t **feed_out_fail_value,
                             lv_obj_t **voltage_label, lv_obj_t **voltage_value,
                             lv_obj_t **sensor_a1_label, lv_obj_t **sensor_a1_value,
                             lv_obj_t **sensor_a2_label, lv_obj_t **sensor_a2_value) {
    // 创建通道标题（放在最上方居中）
    lv_obj_t *title_label = lv_label_create(parent);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 2); // 上移到2px
    
    // 创建进料次数标签和值
    *feed_in_label = lv_label_create(parent);
    lv_label_set_text(*feed_in_label, "进料次数：");
    lv_obj_set_style_text_font(*feed_in_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_in_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(*feed_in_label, LV_ALIGN_TOP_LEFT, 20, 30); // 上移到30px
    
    *feed_in_value = lv_label_create(parent);
    lv_label_set_text(*feed_in_value, "0");
    lv_obj_set_style_text_font(*feed_in_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_in_value, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_width(*feed_in_value, 60); // 固定宽度避免跳动
    lv_obj_align_to(*feed_in_value, *feed_in_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // 增加水平间距
    
    // 创建退料次数标签和值
    *feed_out_label = lv_label_create(parent);
    lv_label_set_text(*feed_out_label, "退料次数：");
    lv_obj_set_style_text_font(*feed_out_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_out_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(*feed_out_label, *feed_in_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10); // 缩小垂直间隙到10px
    
    *feed_out_value = lv_label_create(parent);
    lv_label_set_text(*feed_out_value, "0");
    lv_obj_set_style_text_font(*feed_out_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_out_value, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_width(*feed_out_value, 60); // 固定宽度避免跳动
    lv_obj_align_to(*feed_out_value, *feed_out_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // 增加水平间距
    
    // 创建进料失败标签和值
    *feed_in_fail_label = lv_label_create(parent);
    lv_label_set_text(*feed_in_fail_label, "进料失败次数：");
    lv_obj_set_style_text_font(*feed_in_fail_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_in_fail_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(*feed_in_fail_label, *feed_out_label, LV_ALIGN_OUT_BOTTOM_LEFT, -5, 10); // 缩小垂直间隙到10px
    
    *feed_in_fail_value = lv_label_create(parent);
    lv_label_set_text(*feed_in_fail_value, "0");
    lv_obj_set_style_text_font(*feed_in_fail_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_width(*feed_in_fail_value, 60); // 固定宽度避免跳动
    lv_obj_align_to(*feed_in_fail_value, *feed_in_fail_label, LV_ALIGN_OUT_RIGHT_MID, 5, 0); // 增加水平间距
    
    // 创建退料失败标签和值
    *feed_out_fail_label = lv_label_create(parent);
    lv_label_set_text(*feed_out_fail_label, "退料失败次数：");
    lv_obj_set_style_text_font(*feed_out_fail_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_out_fail_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(*feed_out_fail_label, *feed_in_fail_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10); // 缩小垂直间隙到10px
    
    *feed_out_fail_value = lv_label_create(parent);
    lv_label_set_text(*feed_out_fail_value, "0");
    lv_obj_set_style_text_font(*feed_out_fail_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_width(*feed_out_fail_value, 60); // 固定宽度避免跳动
    lv_obj_align_to(*feed_out_fail_value, *feed_out_fail_label, LV_ALIGN_OUT_RIGHT_MID,10, 0); // 增加水平间距
    
    // 创建电压标签和值
    *voltage_label = lv_label_create(parent);
    lv_label_set_text(*voltage_label, "电压：");
    lv_obj_set_style_text_font(*voltage_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*voltage_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(*voltage_label, *feed_out_fail_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10); // 缩小垂直间隙到10px
    
    *voltage_value = lv_label_create(parent);
    lv_label_set_text(*voltage_value, "0.00 V");
    lv_obj_set_style_text_font(*voltage_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*voltage_value, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_width(*voltage_value, 80); // 固定宽度避免跳动
    lv_obj_align_to(*voltage_value, *voltage_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // 增加水平间距
    
    // 创建光电门A1标签和值
    *sensor_a1_label = lv_label_create(parent);
    lv_label_set_text(*sensor_a1_label, "光电门A1：");
    lv_obj_set_style_text_font(*sensor_a1_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*sensor_a1_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(*sensor_a1_label, *voltage_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10); // 缩小垂直间隙到10px
    
    *sensor_a1_value = lv_label_create(parent);
    lv_label_set_text(*sensor_a1_value, "0");
    lv_obj_set_style_text_font(*sensor_a1_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_width(*sensor_a1_value, 60); // 固定宽度避免跳动
    lv_obj_align_to(*sensor_a1_value, *sensor_a1_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // 增加水平间距
    
    // 创建光电门A2标签和值
    *sensor_a2_label = lv_label_create(parent);
    lv_label_set_text(*sensor_a2_label, "光电门A2：");
    lv_obj_set_style_text_font(*sensor_a2_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*sensor_a2_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(*sensor_a2_label, *sensor_a1_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10); // 缩小垂直间隙到10px
    
    *sensor_a2_value = lv_label_create(parent);
    lv_label_set_text(*sensor_a2_value, "0");
    lv_obj_set_style_text_font(*sensor_a2_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_width(*sensor_a2_value, 60); // 固定宽度避免跳动
    lv_obj_align_to(*sensor_a2_value, *sensor_a2_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // 增加水平间距
}

// 创建光电门监控界面
void create_photogate_ui(void) {
    // 清除屏幕的滚动标志
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    
    // 设置屏幕背景为黑色
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_100, LV_PART_MAIN);
    
    // 创建通道A容器 - 去除边框
    ch_a_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ch_a_container, CHANNEL_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(ch_a_container, 0, 0);
    lv_obj_set_style_border_width(ch_a_container, 0, LV_PART_MAIN); // 去除边框
    lv_obj_set_style_bg_color(ch_a_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ch_a_container, LV_OPA_100, LV_PART_MAIN);
    lv_obj_clear_flag(ch_a_container, LV_OBJ_FLAG_SCROLLABLE); // 禁用滚动
    
    // 创建通道B容器 - 去除边框
    ch_b_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(ch_b_container, CHANNEL_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(ch_b_container, CHANNEL_WIDTH, 0);
    lv_obj_set_style_border_width(ch_b_container, 0, LV_PART_MAIN); // 去除边框
    lv_obj_set_style_bg_color(ch_b_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ch_b_container, LV_OPA_100, LV_PART_MAIN);
    lv_obj_clear_flag(ch_b_container, LV_OBJ_FLAG_SCROLLABLE); // 禁用滚动
    
    // 创建通道A的UI
    create_channel_ui(ch_a_container, "通道A", 
                     &ch_a_feed_in_label, &ch_a_feed_in_value,
                     &ch_a_feed_out_label, &ch_a_feed_out_value,
                     &ch_a_feed_in_fail_label, &ch_a_feed_in_fail_value,
                     &ch_a_feed_out_fail_label, &ch_a_feed_out_fail_value,
                     &ch_a_voltage_label, &ch_a_voltage_value,
                     &ch_a_sensor_a1_label, &ch_a_sensor_a1_value,
                     &ch_a_sensor_a2_label, &ch_a_sensor_a2_value);
    ch_a_title = lv_obj_get_child(ch_a_container, 0);  // 假设标题是第一个子对象
    
    // 创建通道B的UI
    create_channel_ui(ch_b_container, "通道B", 
                     &ch_b_feed_in_label, &ch_b_feed_in_value,
                     &ch_b_feed_out_label, &ch_b_feed_out_value,
                     &ch_b_feed_in_fail_label, &ch_b_feed_in_fail_value,
                     &ch_b_feed_out_fail_label, &ch_b_feed_out_fail_value,
                     &ch_b_voltage_label, &ch_b_voltage_value,
                     &ch_b_sensor_b1_label, &ch_b_sensor_b1_value,
                     &ch_b_sensor_b2_label, &ch_b_sensor_b2_value);
    ch_b_title = lv_obj_get_child(ch_b_container, 0);  // 假设标题是第一个子对象
}

// 主启动程序
void lv_photogate_ui(void) {
    // 创建互斥锁
    if (ui_mutex == NULL) {
        ui_mutex = xSemaphoreCreateMutex();
    }
    
    // 创建流缓冲区（如果尚未创建）
    if (uart_stream_buf == NULL) {
        uart_stream_buf = xStreamBufferCreate(STREAM_BUF_SIZE, TRIGGER_LEVEL);
    }
    
    // 创建光电门监控界面
    create_photogate_ui();
    
    // 创建LVGL定时器用于安全更新UI
    if (update_timer == NULL) {
        update_timer = lv_timer_create(update_ui_timer, 50, NULL); // 增加到50ms减少刷新频率
        lv_timer_set_repeat_count(update_timer, -1);
    }
    
    // 创建串口接收任务（如果尚未创建）
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
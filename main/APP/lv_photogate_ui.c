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
static lv_obj_t *ch_a_sensor_a1_label; // 传感器A1标签
static lv_obj_t *ch_a_sensor_a1_value; // 传感器A1值
static lv_obj_t *ch_a_sensor_a2_label; // 传感器A2标签
static lv_obj_t *ch_a_sensor_a2_value; // 传感器A2值

// 通道B的UI组件
static lv_obj_t *ch_b_title;      // 通道B标题
static lv_obj_t *ch_b_feed_in_label;  // 进料次数标签
static lv_obj_t *ch_b_feed_in_value;  // 进料次数值
static lv_obj_t *ch_b_feed_out_label; // 退料次数标签
static lv_obj_t *ch_b_feed_out_value; // 退料次数值
static lv_obj_t *ch_b_sensor_b1_label; // 传感器B1标签
static lv_obj_t *ch_b_sensor_b1_value; // 传感器B1值
static lv_obj_t *ch_b_sensor_b2_label; // 传感器B2标签
static lv_obj_t *ch_b_sensor_b2_value; // 传感器B2值

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
    uint8_t length;     // 数据长度 0x0C
    uint8_t status;     // 状态码
    uint8_t function;   // 功能码
    uint8_t data[12];   // 12字节数据（4个计数+4个光电状态）
    uint8_t checksum;   // CRC8校验码
} __attribute__((packed)) protocol_frame_t;

// 解析后的数据
typedef struct {
    uint16_t ch0_feed_in_count;   // 通道0进料计数
    uint16_t ch0_feed_out_count;  // 通道0退料计数
    uint16_t ch1_feed_in_count;   // 通道1进料计数
    uint16_t ch1_feed_out_count;  // 通道1退料计数
    uint8_t insert0;              // 通道0插入光电状态
    uint8_t feedin0;              // 通道0进料光电状态
    uint8_t insert1;              // 通道1插入光电状态
    uint8_t feedin1;              // 通道1进料光电状态
    uint8_t status_code;          // 状态码
} parsed_data_t;

// 上一次的数据用于比较
static parsed_data_t last_data = {0};

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
    // 验证帧头
    if (frame->header != 0xF7 || frame->address != 0x10 || frame->length != 0x0C) {
        return false;
    }
    
    // 验证CRC
    uint8_t check_data[16];
    check_data[0] = frame->address;
    check_data[1] = frame->length;
    check_data[2] = frame->status;
    check_data[3] = frame->function;
    memcpy(&check_data[4], frame->data, 12);
    
    uint8_t crc_calculated = crc8_calculate(check_data, 16);
    if (crc_calculated != frame->checksum) {
        return false;
    }
    
    // 解析数据
    data->ch0_feed_in_count = (frame->data[1] << 8) | frame->data[0];
    data->ch0_feed_out_count = (frame->data[3] << 8) | frame->data[2];
    data->ch1_feed_in_count = (frame->data[5] << 8) | frame->data[4];
    data->ch1_feed_out_count = (frame->data[7] << 8) | frame->data[6];
    data->insert0 = frame->data[8];
    data->feedin0 = frame->data[9];
    data->insert1 = frame->data[10];
    data->feedin1 = frame->data[11];
    data->status_code = frame->status;
    
    return true;
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
            // 更新通道A
            char buffer[16];
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch0_feed_in_count);
            lv_label_set_text(ch_a_feed_in_value, buffer);
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch0_feed_out_count);
            lv_label_set_text(ch_a_feed_out_value, buffer);
            
            snprintf(buffer, sizeof(buffer), "%d", data.insert0);
            lv_label_set_text(ch_a_sensor_a1_value, buffer);
            lv_obj_set_style_text_color(ch_a_sensor_a1_value, 
                                      data.insert0 == 0 ? lv_color_hex(0xFF0000) : lv_color_hex(0x00FF00), 
                                      LV_PART_MAIN);
            
            snprintf(buffer, sizeof(buffer), "%d", data.feedin0);
            lv_label_set_text(ch_a_sensor_a2_value, buffer);
            lv_obj_set_style_text_color(ch_a_sensor_a2_value, 
                                      data.feedin0 == 0 ? lv_color_hex(0xFF0000) : lv_color_hex(0x00FF00), 
                                      LV_PART_MAIN);
            
            // 更新通道B
            snprintf(buffer, sizeof(buffer), "%d", data.ch1_feed_in_count);
            lv_label_set_text(ch_b_feed_in_value, buffer);
            
            snprintf(buffer, sizeof(buffer), "%d", data.ch1_feed_out_count);
            lv_label_set_text(ch_b_feed_out_value, buffer);
            
            snprintf(buffer, sizeof(buffer), "%d", data.insert1);
            lv_label_set_text(ch_b_sensor_b1_value, buffer);
            lv_obj_set_style_text_color(ch_b_sensor_b1_value, 
                                      data.insert1 == 0 ? lv_color_hex(0xFF0000) : lv_color_hex(0x00FF00), 
                                      LV_PART_MAIN);
            
            snprintf(buffer, sizeof(buffer), "%d", data.feedin1);
            lv_label_set_text(ch_b_sensor_b2_value, buffer);
            lv_obj_set_style_text_color(ch_b_sensor_b2_value, 
                                      data.feedin1 == 0 ? lv_color_hex(0xFF0000) : lv_color_hex(0x00FF00), 
                                      LV_PART_MAIN);
            
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
                             lv_obj_t **sensor1_label, lv_obj_t **sensor1_value,
                             lv_obj_t **sensor2_label, lv_obj_t **sensor2_value,
                             const char *sensor1_name, const char *sensor2_name) {
    // 创建通道标题（放在最上方居中）
    lv_obj_t *title_label = lv_label_create(parent);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 15); // 增加顶部间距
    
    // 创建进料次数标签和值
    *feed_in_label = lv_label_create(parent);
    lv_label_set_text(*feed_in_label, "进料次数：");
    lv_obj_set_style_text_font(*feed_in_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_in_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(*feed_in_label, LV_ALIGN_TOP_LEFT, 20, 60); // 向下移动
    
    *feed_in_value = lv_label_create(parent);
    lv_label_set_text(*feed_in_value, "0");
    lv_obj_set_style_text_font(*feed_in_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_in_value, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_align_to(*feed_in_value, *feed_in_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // 增加水平间距
    
    // 创建退料次数标签和值
    *feed_out_label = lv_label_create(parent);
    lv_label_set_text(*feed_out_label, "退料次数：");
    lv_obj_set_style_text_font(*feed_out_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_out_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(*feed_out_label, *feed_in_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 30); // 增加垂直间距
    
    *feed_out_value = lv_label_create(parent);
    lv_label_set_text(*feed_out_value, "0");
    lv_obj_set_style_text_font(*feed_out_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*feed_out_value, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_align_to(*feed_out_value, *feed_out_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // 增加水平间距
    
    // 创建传感器1标签和值
    *sensor1_label = lv_label_create(parent);
    lv_label_set_text(*sensor1_label, sensor1_name);
    lv_obj_set_style_text_font(*sensor1_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*sensor1_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(*sensor1_label, *feed_out_label, LV_ALIGN_OUT_BOTTOM_LEFT, -5, 30); // 增加垂直间距
    
    *sensor1_value = lv_label_create(parent);
    lv_label_set_text(*sensor1_value, "0");
    lv_obj_set_style_text_font(*sensor1_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_align_to(*sensor1_value, *sensor1_label, LV_ALIGN_OUT_RIGHT_MID, 5, 0); // 增加水平间距
    
    // 创建传感器2标签和值
    *sensor2_label = lv_label_create(parent);
    lv_label_set_text(*sensor2_label, sensor2_name);
    lv_obj_set_style_text_font(*sensor2_label, &Chinese_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(*sensor2_label, lv_color_white(), LV_PART_MAIN);
    // 左移传感器文字并稍微上移A2/B2传感器
    lv_obj_align_to(*sensor2_label, *sensor1_label, LV_ALIGN_OUT_BOTTOM_LEFT, -5, 25); // 左移5像素，垂直间距减少5像素
    
    *sensor2_value = lv_label_create(parent);
    lv_label_set_text(*sensor2_value, "0");
    lv_obj_set_style_text_font(*sensor2_value, &Chinese_1, LV_PART_MAIN);
    lv_obj_align_to(*sensor2_value, *sensor2_label, LV_ALIGN_OUT_RIGHT_MID, 5, 0); // 增加水平间距
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
                     &ch_a_sensor_a1_label, &ch_a_sensor_a1_value,
                     &ch_a_sensor_a2_label, &ch_a_sensor_a2_value,
                     "光电传感器A1：", "光电传感器A2：");
    
    // 创建通道B的UI
    create_channel_ui(ch_b_container, "通道B", 
                     &ch_b_feed_in_label, &ch_b_feed_in_value,
                     &ch_b_feed_out_label, &ch_b_feed_out_value,
                     &ch_b_sensor_b1_label, &ch_b_sensor_b1_value,
                     &ch_b_sensor_b2_label, &ch_b_sensor_b2_value,
                     "光电传感器B1：", "光电传感器B2：");
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
        update_timer = lv_timer_create(update_ui_timer, 50, NULL); // 降低刷新频率为50ms
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
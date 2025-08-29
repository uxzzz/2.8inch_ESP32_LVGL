#include "lv_test_ui.h"

LV_FONT_DECLARE(Chinese_1);
LV_FONT_DECLARE(lv_font_gb2312_wryh_26);  

// 定义UI组件变量
static lv_obj_t *ta_prefix;       // 第一行显示区域（CH1）
static lv_obj_t *ta_first_line;   // 第二行显示区域（电机1）
static lv_obj_t *ta_full;          // 第三行显示区域（电机2）

// 任务和流缓冲区
static TaskHandle_t serial_task_handle = NULL;
static StreamBufferHandle_t uart_stream_buf = NULL;
static lv_timer_t *update_timer = NULL;  // UI更新定时器

// 流缓冲区配置
#define STREAM_BUF_SIZE 4096     // 流缓冲区大小
#define TRIGGER_LEVEL 128        // 触发级别
#define MAX_UPDATE_CHUNK 256     // 每次更新最大字节数

// 屏幕尺寸
#define SCREEN_WIDTH 320         // 宽度240px
#define SCREEN_HEIGHT 240        // 高度320px
#define ROW_HEIGHT ((SCREEN_HEIGHT-80) / 3)  // 每行高度约106.67px

// 协议帧结构体（与发送端一致）
typedef struct {
    uint8_t header;     // 帧头 0xF7
    uint8_t address;    // 地址 0x10
    uint8_t length;     // 数据长度 0x0C
    uint8_t status;     // 状态码
    uint8_t function;   // 功能码
    float data[3];      // 3个浮点数电压值
    uint8_t checksum;   // CRC8校验码
} __attribute__((packed)) protocol_frame_t;

// CRC8计算函数（与发送端完全一致）
static uint8_t crc8_calculate(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;
    uint8_t i;
    
    while (length--) {
        crc ^= *data++;
        for (i = 0; i < 8; i++) {
            if (crc & 0x01)
                crc = (crc >> 1) ^ 0x8C;  // CRC8多项式: x^8 + x^2 + x^1 + 1 (0x107)
            else
                crc >>= 1;
        }
    }
    return crc;
}

// 数据结构：记录接收数据状态
typedef struct {
    uint32_t last_update_tick;     // 上次更新时间
    float last_voltage1;           // 上一次的电压1值
    float last_voltage2;           // 上一次的电压2值
    float last_voltage3;           // 上一次的电压3值
    uint8_t last_status;            // 上一次的状态值
} text_metrics_t;

static text_metrics_t rx_metrics = {0};

// 检查串口是否初始化
static bool is_uart_initialized(void) {
    return true; // 实际项目中需要实现
}

// 互斥锁用于保护UI更新
static SemaphoreHandle_t ui_mutex = NULL;

// 串口数据接收任务
void uart_receive_task(void *pvParameters) {
    uint8_t temp_buffer[UART_BUF_SIZE];
    while(1) {
        // 从串口读取数据
        int rx_len = uart_read_bytes(UART_PORT_NUM, temp_buffer, UART_BUF_SIZE - 1, 
                                    pdMS_TO_TICKS(10)); // 减少阻塞时间
        
        if (rx_len > 0) {
            // 写入流缓冲区
            if (uart_stream_buf != NULL) {
                // 使用线程安全的发送方式
                size_t bytes_written = xStreamBufferSend(uart_stream_buf, temp_buffer, rx_len, portMAX_DELAY);
                
                // 如果缓冲区满，触发定时器立即处理
                if (bytes_written < (size_t)rx_len && update_timer) {
                    lv_timer_reset(update_timer);
                }
            }
        }
        taskYIELD(); // 短暂让出CPU
    }
}

// 检查浮点数是否有效
static bool is_valid_float(float f) {
    return !(isnan(f) || isinf(f));
}

// 安全格式化浮点数
static void format_float(char *buf, size_t size, float value) {
    if (is_valid_float(value)) {
        snprintf(buf, size, "%.2f", value);
    } else {
        strncpy(buf, "N/A", size);
    }
}

// LVGL定时器回调 - 安全更新UI
static void update_ui_timer(lv_timer_t *timer) {
    if (uart_stream_buf == NULL) return;
    
    // 每次最多处理5个协议帧
    uint8_t max_frames = 5;
    uint8_t frame_count = 0;
    size_t available;
    
    while (frame_count < max_frames && 
          (available = xStreamBufferBytesAvailable(uart_stream_buf)) >= sizeof(protocol_frame_t)) {
        frame_count++;
        
        // 读取完整的协议帧
        protocol_frame_t frame;
        size_t bytes_read = xStreamBufferReceive(uart_stream_buf, &frame, sizeof(protocol_frame_t), 0);
        
        if (bytes_read != sizeof(protocol_frame_t)) {
            // 读取失败，跳出循环
            break;
        }
        
        // 验证帧头
        if (frame.header != 0xF7 || frame.address != 0x10 || frame.length != 0x0C) {
            // 帧头验证失败，丢弃并继续
            continue;
        }
        
        // 验证CRC（从address开始到data结束共16字节）
        uint8_t check_data[16];
        check_data[0] = frame.address;
        check_data[1] = frame.length;
        check_data[2] = frame.status;
        check_data[3] = frame.function;
        memcpy(&check_data[4], &frame.data, 12);
        
        uint8_t crc_calculated = crc8_calculate(check_data, 16);
        if (crc_calculated != frame.checksum) {
            // CRC校验失败，丢弃
            continue;
        }
        
        // 解析成功，提取数据
        float voltage1 = frame.data[0]; // 缓冲器电压
        float voltage2 = frame.data[1]; // 电机1电压
        float voltage3 = frame.data[2]; // 电机2电压
        uint8_t status = frame.status;
        
        // 获取互斥锁，保护UI更新
        if (xSemaphoreTake(ui_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            // 更新第一行显示区域（CH1：缓冲器）
            if (ta_prefix && lv_obj_is_valid(ta_prefix)) {
                char display_text[64];
                uint8_t status_buf = status & 0x01; // 取状态码的bit0（缓冲器状态）
                
                // 仅当值变化时更新
                if (voltage1 != rx_metrics.last_voltage1 || status_buf != (rx_metrics.last_status & 0x01)) {
                    char voltage_str[16];
                    format_float(voltage_str, sizeof(voltage_str), voltage1);
                    snprintf(display_text, sizeof(display_text), "CH1：%sV  状态：%s", 
                            voltage_str, status_buf ? "异常" : "正常");
                    lv_label_set_text(ta_prefix, display_text);
                    lv_obj_set_style_text_color(ta_prefix, 
                                              status_buf ? lv_color_hex(0xFF0000) : lv_color_hex(0x00FF00), 
                                              LV_PART_MAIN);
                }
            }
            
            // 更新第二行显示区域（电机1） - 特殊优化
            if (ta_first_line && lv_obj_is_valid(ta_first_line)) {
                char display_text[64];
                uint8_t status_m1 = (status >> 1) & 0x01; // 取状态码的bit1（电机1状态）
                
                // 仅当值变化时更新
                if (voltage2 != rx_metrics.last_voltage2 || status_m1 != ((rx_metrics.last_status >> 1) & 0x01)) {
                    char voltage_str[16];
                    format_float(voltage_str, sizeof(voltage_str), voltage2);
                    
                    // 安全格式化字符串
                    int len = snprintf(display_text, sizeof(display_text), "电机1：%sV  状态：%s", 
                                      voltage_str, status_m1 ? "异常" : "正常");
                    
                    // 确保字符串终止
                    if (len >= sizeof(display_text)) {
                        display_text[sizeof(display_text) - 1] = '\0';
                    }
                    
                    // 设置文本前先锁定对象
                    lv_obj_invalidate(ta_first_line);
                    lv_label_set_text(ta_first_line, display_text);
                    
                    // 优化颜色设置
                    const lv_color_t color = status_m1 ? 
                        lv_color_hex(0xFF0000) : lv_color_hex(0x00FF00);
                    lv_obj_set_style_text_color(ta_first_line, color, LV_PART_MAIN);
                    
                    // 强制重绘和样式刷新
                    lv_obj_refresh_style(ta_first_line, LV_PART_MAIN, LV_STYLE_PROP_ANY);
                    lv_obj_invalidate(ta_first_line);
                }
            }
            
            // 更新第三行显示区域（电机2）
            if (ta_full && lv_obj_is_valid(ta_full)) {
                char display_text[64];
                uint8_t status_m2 = (status >> 2) & 0x01; // 取状态码的bit2（电机2状态）
                
                // 仅当值变化时更新
                if (voltage3 != rx_metrics.last_voltage3 || status_m2 != ((rx_metrics.last_status >> 2) & 0x01)) {
                    char voltage_str[16];
                    format_float(voltage_str, sizeof(voltage_str), voltage3);
                    snprintf(display_text, sizeof(display_text), "电机2：%sV  状态：%s", 
                            voltage_str, status_m2 ? "异常" : "正常");
                    lv_label_set_text(ta_full, display_text);
                    lv_obj_set_style_text_color(ta_full, 
                                              status_m2 ? lv_color_hex(0xFF0000) : lv_color_hex(0x00FF00), 
                                              LV_PART_MAIN);
                }
            }
            
            // 记录当前值
            rx_metrics.last_voltage1 = voltage1;
            rx_metrics.last_voltage2 = voltage2;
            rx_metrics.last_voltage3 = voltage3;
            rx_metrics.last_status = status;
            rx_metrics.last_update_tick = lv_tick_get();
            
            xSemaphoreGive(ui_mutex);
        }
    }
}

// 销毁UI资源
static void destroy_ui_resources() {
    // 删除定时器
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    
    // 删除任务
    if (serial_task_handle) {
        vTaskDelete(serial_task_handle);
        serial_task_handle = NULL;
    }
    
    // 删除流缓冲区
    if (uart_stream_buf) {
        vStreamBufferDelete(uart_stream_buf);
        uart_stream_buf = NULL;
    }
    
    // 删除互斥锁
    if (ui_mutex) {
        vSemaphoreDelete(ui_mutex);
        ui_mutex = NULL;
    }
}

// 创建串口监控界面
void create_serial_monitor_ui(void) {
    // 清除屏幕的滚动标志
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    
    // 如果UI已存在，先关闭
    if (ta_prefix || ta_first_line || ta_full) {
        destroy_ui_resources();
        if (ta_prefix && lv_obj_is_valid(ta_prefix)) lv_obj_del(ta_prefix);
        if (ta_first_line && lv_obj_is_valid(ta_first_line)) lv_obj_del(ta_first_line);
        if (ta_full && lv_obj_is_valid(ta_full)) lv_obj_del(ta_full);
        ta_prefix = NULL;
        ta_first_line = NULL;
        ta_full = NULL;
    }
    
    // 设置屏幕背景为黑色
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_100, LV_PART_MAIN);
    
    // 禁用屏幕的滚动功能
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建主容器 - 使用Flex布局
    lv_obj_t *main_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_container, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(main_container, 0, 40); // 从顶部开始
    lv_obj_set_flex_flow(main_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(main_container, 0, 0);
    lv_obj_set_style_border_width(main_container, 0, 0);
    lv_obj_set_style_bg_color(main_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_container, LV_OPA_100, LV_PART_MAIN);
    lv_obj_clear_flag(main_container, LV_OBJ_FLAG_SCROLLABLE); // 禁用滚动

    // 创建三行显示区域（均分屏幕高度）
    // 第一行显示区域（CH1）
    ta_prefix = lv_label_create(main_container);
    lv_obj_set_size(ta_prefix, SCREEN_WIDTH, ROW_HEIGHT);
    lv_label_set_text(ta_prefix, "CH1:---V  状态:正常");
    lv_obj_set_style_bg_color(ta_prefix, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_text_color(ta_prefix, lv_color_hex(0x00FF00), LV_PART_MAIN); 
    lv_obj_set_style_pad_all(ta_prefix, 5, 0);
    lv_obj_set_style_radius(ta_prefix, 0, 0);
    lv_obj_set_style_text_font(ta_prefix, &lv_font_gb2312_wryh_26, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ta_prefix, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_clear_flag(ta_prefix, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_align(ta_prefix, LV_ALIGN_CENTER, 0);

    // 第二行显示区域（电机1） - 增加样式稳定性
    ta_first_line = lv_label_create(main_container);
    lv_obj_set_size(ta_first_line, SCREEN_WIDTH, ROW_HEIGHT);
    lv_label_set_text(ta_first_line, "电机1:---V  状态:正常");
    lv_obj_set_style_bg_color(ta_first_line, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_text_color(ta_first_line, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_pad_all(ta_first_line, 5, 0);
    lv_obj_set_style_radius(ta_first_line, 0, 0);
    lv_obj_set_style_text_font(ta_first_line, &lv_font_gb2312_wryh_26, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ta_first_line, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_clear_flag(ta_first_line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_align(ta_first_line, LV_ALIGN_CENTER, 0);
    // // 设置固定宽度和高度，避免布局变化
    // lv_obj_set_width(ta_first_line, SCREEN_WIDTH - 20);
    // lv_obj_set_height(ta_first_line, ROW_HEIGHT - 5);
    // lv_obj_set_style_text_letter_space(ta_first_line, 1, 0); // 增加字间距
    // lv_obj_set_style_text_line_space(ta_first_line, 2, 0); // 增加行间距

    // 第三行显示区域（电机2）
    ta_full = lv_label_create(main_container);
    lv_obj_set_size(ta_full, SCREEN_WIDTH, ROW_HEIGHT);
    lv_label_set_text(ta_full, "电机2:---V  状态:正常");
    lv_obj_set_style_bg_color(ta_full, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_text_color(ta_full, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_pad_all(ta_full, 5, 0);
    lv_obj_set_style_radius(ta_full, 0, 0);
    lv_obj_set_style_text_font(ta_full, &lv_font_gb2312_wryh_26, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ta_full, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_clear_flag(ta_full, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_align(ta_full, LV_ALIGN_CENTER, 0);

    // 初始化缓冲区
    memset(&rx_metrics, 0, sizeof(rx_metrics));
}

// 主启动程序
void lv_test_ui(void) {
    // 创建互斥锁
    if (ui_mutex == NULL) {
        ui_mutex = xSemaphoreCreateMutex();
    }
    
    // 创建流缓冲区（如果尚未创建）
    if (uart_stream_buf == NULL) {
        uart_stream_buf = xStreamBufferCreate(STREAM_BUF_SIZE, TRIGGER_LEVEL);
    }
    
    // 创建串口监控界面
    create_serial_monitor_ui();
    
    // 创建LVGL定时器用于安全更新UI
    if (update_timer == NULL) {
        update_timer = lv_timer_create(update_ui_timer, 20, NULL);
        lv_timer_set_repeat_count(update_timer, -1); // 无限重复
    }
    
    // 创建串口接收任务（如果尚未创建）
    if (serial_task_handle == NULL) {
        xTaskCreatePinnedToCore(uart_receive_task,
                    "uart_receive_task",
                    4096,
                    NULL,
                    6,  // 提高优先级，确保数据接收不被阻塞
                    &serial_task_handle,
                    0); // 指定在核心0上运行
    }
}
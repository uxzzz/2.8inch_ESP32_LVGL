#include "lv_test_ui.h"

LV_FONT_DECLARE(Chinese_1);       /* 声明字体 */
// extern const lv_font_t Chinese_1;       /* 声明字体 */
// extern const lv_font_t lv_font_gb2312_wryh_22;
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
#define UART_BUF_SIZE 512        // UART缓冲区大小
#define MAX_UPDATE_CHUNK 256     // 每次更新最大字节数

// 屏幕尺寸
#define SCREEN_WIDTH 320         // 宽度240px
#define SCREEN_HEIGHT 240        // 高度320px
#define ROW_HEIGHT ((SCREEN_HEIGHT-80) / 3)  // 每行高度约106.67px

// 数据结构：记录接收数据状态
typedef struct {
    uint32_t last_update_tick;     // 上次更新时间
} text_metrics_t;

static text_metrics_t rx_metrics = {0};

// 检查串口是否初始化
static bool is_uart_initialized(void) {
    return true; // 实际项目中需要实现
}

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

// 提取前三位数字
static void extract_first_three_digits(const char *buffer, size_t len, char *output) {
    size_t i = 0;
    size_t out_idx = 0;
    size_t digit_count = 0;
    
    output[0] = '\0'; // 清空输出
    
    // 查找前三位数字
    for (i = 0; i < len && digit_count < 3; i++) {
        if (buffer[i] >= '0' && buffer[i] <= '9') {
            output[out_idx++] = buffer[i];
            digit_count++;
        }
    }
    output[out_idx] = '\0'; // 确保字符串结束
}

// LVGL定时器回调 - 安全更新UI
static void update_ui_timer(lv_timer_t *timer) {
    if (uart_stream_buf == NULL) return;
    
    // 检查缓冲区中可读数据量
    size_t available = xStreamBufferBytesAvailable(uart_stream_buf);
    if (available == 0) return;
    
    // 限制每次处理的最大数据量
    size_t bytes_to_read = (available > MAX_UPDATE_CHUNK) ? MAX_UPDATE_CHUNK : available;
    
    // 创建缓冲区读取数据
    uint8_t buffer[MAX_UPDATE_CHUNK + 1]; // +1 for null terminator
    size_t bytes_read = xStreamBufferReceive(uart_stream_buf, buffer, bytes_to_read, 0);
    
    if (bytes_read > 0) {
        // 确保字符串以null结尾
        buffer[bytes_read] = '\0';
        
        // 提取前三位数字
        char prefix[4] = "";
        extract_first_three_digits((char*)buffer, bytes_read, prefix);
        
        // 创建显示文本缓冲区
        char display_text[64];

        // 将prefix转换为整数
        int prefix_value = atoi(prefix);

        // // 设置文本颜色
        // if ( (prefix_value % 5 == 0)) {
        //     lv_obj_set_style_text_color(ta_full, lv_color_hex(0xFF0000), LV_PART_MAIN);
        // } else {
        //     lv_obj_set_style_text_color(ta_full, lv_color_hex(0x00FF00), LV_PART_MAIN);
        // }
                
        // 更新第一行显示区域（CH1）
        if (ta_prefix && lv_obj_is_valid(ta_prefix)) {
            // 先确定状态和颜色
            if (prefix_value % 5 == 0) {
                // 可被5整除，显示异常状态并设置为红色
                strcpy(display_text, "CH1：");
                strcat(display_text, prefix);
                strcat(display_text, "V  状态：异常");
                lv_obj_set_style_text_color(ta_prefix, lv_color_hex(0xFF0000), LV_PART_MAIN);
            } else {
                // 不能被5整除，显示中位状态并设置为绿色
                strcpy(display_text, "CH1：");
                strcat(display_text, prefix);
                strcat(display_text, "V  状态：中位");
                lv_obj_set_style_text_color(ta_prefix, lv_color_hex(0x00FF00), LV_PART_MAIN);
            }
            // 设置文本
            lv_label_set_text(ta_prefix, display_text);
        }

        
        // 更新第二行显示区域（电机1）
        if (ta_first_line && lv_obj_is_valid(ta_first_line)) {
            // 先确定状态和颜色
            if (prefix_value % 5 == 0) {
                // 可被5整除，显示异常状态并设置为红色
                strcpy(display_text, "电机1：");
                strcat(display_text, prefix);
                strcat(display_text, "V  状态：异常");
                lv_obj_set_style_text_color(ta_first_line, lv_color_hex(0xFF0000), LV_PART_MAIN);
            } else {
                // 不能被5整除，显示正常状态并设置为绿色
                strcpy(display_text, "电机1：");
                strcat(display_text, prefix);
                strcat(display_text, "V  状态：正常");
                lv_obj_set_style_text_color(ta_first_line, lv_color_hex(0x00FF00), LV_PART_MAIN);
            }
            // 设置文本
            lv_label_set_text(ta_first_line, display_text);
        }
        
        // 更新第三行显示区域（电机2）
        if (ta_full && lv_obj_is_valid(ta_full)) {
            // 先确定状态和颜色
            if (prefix_value % 5 == 0) {
                // 可被5整除，显示异常状态并设置为红色
                strcpy(display_text, "电机2：");
                strcat(display_text, prefix);
                strcat(display_text, "V  状态：异常");
                lv_obj_set_style_text_color(ta_full, lv_color_hex(0xFF0000), LV_PART_MAIN);
            } else {
                // 不能被5整除，显示正常状态并设置为绿色
                strcpy(display_text, "电机2：");
                strcat(display_text, prefix);
                strcat(display_text, "V  状态：正常");
                lv_obj_set_style_text_color(ta_full, lv_color_hex(0x00FF00), LV_PART_MAIN);
            }
            // 设置文本
            lv_label_set_text(ta_full, display_text);
        }
        
        // 记录更新时间
        rx_metrics.last_update_tick = lv_tick_get();
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
    lv_label_set_text(ta_prefix, "CH1:---V  状态:中位");
    lv_obj_set_style_bg_color(ta_prefix, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_text_color(ta_prefix, lv_color_hex(0x00FF00), LV_PART_MAIN); // 红色字体
    lv_obj_set_style_pad_all(ta_prefix, 5, 0);
    lv_obj_set_style_radius(ta_prefix, 0, 0);
    lv_obj_set_style_text_font(ta_prefix, &lv_font_gb2312_wryh_26, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ta_prefix, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_clear_flag(ta_prefix, LV_OBJ_FLAG_SCROLLABLE); // 禁用滚动
    lv_obj_set_style_align(ta_prefix, LV_ALIGN_CENTER, 0); // 垂直居中

    // 第二行显示区域（电机1）
    ta_first_line = lv_label_create(main_container);
    lv_obj_set_size(ta_first_line, SCREEN_WIDTH, ROW_HEIGHT);
    lv_label_set_text(ta_first_line, "电机1:---V  状态:正常");
    lv_obj_set_style_bg_color(ta_first_line, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_text_color(ta_first_line, lv_color_hex(0x00FF00), LV_PART_MAIN); // 红色字体
    lv_obj_set_style_pad_all(ta_first_line, 5, 0);
    lv_obj_set_style_radius(ta_first_line, 0, 0);
    lv_obj_set_style_text_font(ta_first_line, &lv_font_gb2312_wryh_26, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ta_first_line, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_clear_flag(ta_first_line, LV_OBJ_FLAG_SCROLLABLE); // 禁用滚动
    lv_obj_set_style_align(ta_first_line, LV_ALIGN_CENTER, 0); // 垂直居中

    // 第三行显示区域（电机2）
    ta_full = lv_label_create(main_container);
    lv_obj_set_size(ta_full, SCREEN_WIDTH, ROW_HEIGHT);
    lv_label_set_text(ta_full, "电机2:---V  状态:正常");
    lv_obj_set_style_bg_color(ta_full, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_text_color(ta_full, lv_color_hex(0x00FF00), LV_PART_MAIN); // 红色字体
    lv_obj_set_style_pad_all(ta_full, 5, 0);
    lv_obj_set_style_radius(ta_full, 0, 0);
    lv_obj_set_style_text_font(ta_full, &lv_font_gb2312_wryh_26, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ta_full, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_clear_flag(ta_full, LV_OBJ_FLAG_SCROLLABLE); // 禁用滚动
    lv_obj_set_style_align(ta_full, LV_ALIGN_CENTER, 0); // 垂直居中

    // 初始化缓冲区
    memset(&rx_metrics, 0, sizeof(rx_metrics));
}

// 主启动程序
void lv_test_ui(void) {
    
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
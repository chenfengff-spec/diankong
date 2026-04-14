#include "debug_commands.h"
#include "usart.h" // 确保这里包含了 usart.h，以便访问 huart6
extern JobConfig_t g_job_config;

// [新增]：用于将触发模式枚举转换为字符串的辅助函数实现
const char* GetTriggerModeString(TriggerMode_t mode) {
    switch (mode) {
        case TRIGGER_MODE_NONE: return "NONE";
        case TRIGGER_MODE_DELAY: return "DELAY";
        case TRIGGER_MODE_DEPTH: return "DEPTH";
        case TRIGGER_MODE_DEPTH_OR_DELAY: return "DEPTH_OR_DELAY";
        case TRIGGER_MODE_SERIAL_COMMAND: return "SERIAL_COMMAND";
        default: return "UNKNOWN";
    }
}

/**
  * @brief  独立调试模式命令处理函数。
  *         在主循环中，当 debugMode 激活时调用。
  * @param  None
  * @retval None
  */
void HandleDebugMode_Independent(void) {
    // 确保接收缓冲区有完整的命令
    if (rx1S.data_length >= 2 &&
        rx1S.rx_buf[rx1S.data_length - 2] == 0x0D &&
        rx1S.rx_buf[rx1S.data_length - 1] == 0x0A)
    {
        // 将回车换行替换为字符串结束符，方便 strncmp/sscanf
        rx1S.rx_buf[rx1S.data_length - 2] = '\0';
        char* command_str = (char*)rx1S.rx_buf;
        char cmd_buffer[RX_BUF_LEN]; // 用于存储提取的命令部分
        int value_int;
        float value_float;

        // 打印接收到的命令进行调试
        // printf("\r\nReceived debug command: %s\r\n", command_str);

        // 解析并执行调试命令
        if (strncmp(command_str, "exit", 4) == 0) {
            debugMode = 0; // 退出调试模式
            HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"\r\n#offdebugmode#\r\n", strlen("\r\n#offdebugmode#\r\n"), 0xFF);
        }
        // 设置作业模式
        else if (sscanf(command_str, "set_mode %d", &value_int) == 1) {
            if (value_int >= TRIGGER_MODE_NONE && value_int <= TRIGGER_MODE_SERIAL_COMMAND) {
                g_job_config.trigger_mode = (TriggerMode_t)value_int;
                Config_Save();
                printf("\r\n#作业模式设置为: %s (%d)#\r\n", GetTriggerModeString(g_job_config.trigger_mode), g_job_config.trigger_mode);
            } else {
                printf("\r\n#无效模式ID，请使用0-4#\r\n");
            }
        }
        // 设置延时触发时间
        else if (sscanf(command_str, "set_delay %d", &value_int) == 1) {
            g_job_config.params.delay_time_ms = (uint32_t)value_int;
            Config_Save();
            printf("\r\n#延时时间设置为: %lu ms#\r\n", g_job_config.params.delay_time_ms);
        }
        // 设置深度触发值
        else if (sscanf(command_str, "set_depth %f", &value_float) == 1) {
            g_job_config.params.depth_value = value_float;
            Config_Save();
            printf("\r\n#深度触发值设置为: %.2f m#\r\n", g_job_config.params.depth_value);
        }
        // 读取运行日志 (占位符)
        else if (strncmp(command_str, "read_log", 8) == 0) {
            HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"\r\n#读取运行过程参数 (待实现)#\r\n", strlen("\r\n#读取运行过程参数 (待实现)#\r\n"), 0xFF);
            // 这里需要实现从 Flash 读取 g_sampling_log 并打印的逻辑
        }
        // 查询当前配置和状态
        else if (strncmp(command_str, "status", 6) == 0) {
            printf("\r\n--- 当前系统状态 ---\r\n");
            printf("当前模式: %s (%d)\r\n", GetTriggerModeString(g_job_config.trigger_mode), g_job_config.trigger_mode);
            if (g_job_config.trigger_mode == TRIGGER_MODE_DELAY || g_job_config.trigger_mode == TRIGGER_MODE_DEPTH_OR_DELAY) {
                printf("延时设置: %lu ms\r\n", g_job_config.params.delay_time_ms);
            }
            if (g_job_config.trigger_mode == TRIGGER_MODE_DEPTH || g_job_config.trigger_mode == TRIGGER_MODE_DEPTH_OR_DELAY) {
                printf("深度设置: %.2f m\r\n", g_job_config.params.depth_value);
            }
            printf("-------------------\r\n");
            printf("\r\n#状态查询完成#\r\n");
        }

        else {
            HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"\r\n#NAN#\r\n", strlen("\r\n#NAN#\r\n"), 0xFF);
        }
        memset(rx1S.rx_buf, 0x00, RX_BUF_LEN);
        rx1S.data_length = 0;
	}
}
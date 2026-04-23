#include "debug_commands.h"
#include "usart.h" // 确保这里包含了 usart.h，以便访问 huart6
#include "config_manager.h" // 包含配置管理器头文件以访问 g_job_config

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
            Config_Save(); // 保存当前配置到 Flash
            debugMode = 0; // 退出调试模式
            RS485_Transmit(DEBUG_UART, (uint8_t*)"\r\n#offdebugmode#\r\n", strlen("\r\n#offdebugmode#\r\n"), RS485_PORT_5); // 发送数据
        }
        // 设置作业模式
        else if (sscanf(command_str, "set_mode %d", &value_int) == 1) {
            char response[128];
            if (value_int >= TRIGGER_MODE_NONE && value_int <= TRIGGER_MODE_SERIAL_COMMAND) {
                g_job_config.trigger_mode = (TriggerMode_t)value_int;
                snprintf(response, sizeof(response), "\r\n#Job mode set to: %s (%d)#\r\n",
                        GetTriggerModeString(g_job_config.trigger_mode), g_job_config.trigger_mode);
                RS485_Transmit(DEBUG_UART, (uint8_t*)response, strlen(response), RS485_PORT_5);
            } else {
                snprintf(response, sizeof(response), "\r\n#Invalid mode ID, please use 0-4#\r\n");
                RS485_Transmit(DEBUG_UART, (uint8_t*)response, strlen(response), RS485_PORT_5);
            }
        }
        // Set delay trigger time
        else if (sscanf(command_str, "set_delay %d", &value_int) == 1) {
            char response[128];
            g_job_config.params.delay_time_ms = (uint32_t)value_int;
            snprintf(response, sizeof(response), "\r\n#Delay time set to: %lu ms#\r\n", g_job_config.params.delay_time_ms);
            RS485_Transmit(DEBUG_UART, (uint8_t*)response, strlen(response), RS485_PORT_5);
        }
        // Set depth trigger value
        else if (sscanf(command_str, "set_depth %f", &value_float) == 1) {
            char response[128];
            g_job_config.params.depth_value = value_float;
            snprintf(response, sizeof(response), "\r\n#Depth trigger value set to: %.2f m#\r\n", g_job_config.params.depth_value);
            RS485_Transmit(DEBUG_UART, (uint8_t*)response, strlen(response), RS485_PORT_5);
        }
        // Read runtime log (placeholder)
        else if (strncmp(command_str, "read_log", 8) == 0) {
            char response[] = "\r\n#Read runtime parameters (to be implemented)#\r\n";
            RS485_Transmit(DEBUG_UART, (uint8_t*)response, strlen(response), RS485_PORT_5);
        }
        // Query current configuration and status
         else if (strncmp(command_str, "status", 6) == 0) {
            RS485_5_DE_Transmit(); // 切换到发送模式
            printf("yes");
            HAL_Delay(1000);
            printf("\r\n--- Current System Status ---\r\n");
            printf("Current mode: %s (%d)\r\n", GetTriggerModeString(g_job_config.trigger_mode), g_job_config.trigger_mode);
            if (g_job_config.trigger_mode == TRIGGER_MODE_DELAY || g_job_config.trigger_mode == TRIGGER_MODE_DEPTH_OR_DELAY) {
                printf("Delay setting: %lu ms\r\n", g_job_config.params.delay_time_ms);
            }
            if (g_job_config.trigger_mode == TRIGGER_MODE_DEPTH || g_job_config.trigger_mode == TRIGGER_MODE_DEPTH_OR_DELAY) {
                printf("Depth setting: %.2f m\r\n", g_job_config.params.depth_value);
            }
            printf("-------------------\r\n");
            printf("\r\n#Status query completed#\r\n");
            RS485_5_DE_Receive(); // 切换回接收模式
        }
        else {
            char response[] = "\r\n#NAN#\r\n";
            RS485_Transmit(DEBUG_UART, (uint8_t*)response, strlen(response), RS485_PORT_5);
        }
        memset(rx1S.rx_buf, 0x00, RX_BUF_LEN);
        rx1S.data_length = 0;
	}
}

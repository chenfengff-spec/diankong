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
        else if (strncmp(command_str, "read_log", 8) == 0) {
            HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"\r\n#开始读取设备内部日志#\r\n", strlen("\r\n#开始读取设备内部日志#\r\n"), 0xFF);
            
            extern bool Log_Load(void); // 确保在 config_manager.h 中声明，或者在这里 extern 声明

            if (Log_Load()) {
                printf("\r\n================ 历史运行参数报告 ================\r\n");
                
                // 1. 打印自检结果
                printf("1. 系统自检结果: %s\r\n", g_sampling_log.system_self_check_overall_ok ? "PASSED" : "FAILED");
                printf("   - 蠕动泵1通信: %s\r\n", g_sampling_log.pump1_comm_ok_log ? "OK" : "FAIL");
                printf("   - 蠕动泵2通信: %s\r\n", g_sampling_log.pump2_comm_ok_log ? "OK" : "FAIL");
                printf("   - 脉冲采集器通信: %s\r\n", g_sampling_log.pulse_collector_comm_ok_log ? "OK" : "FAIL");
                printf("   - 模拟量采集器通信: %s\r\n", g_sampling_log.analog_collector_comm_ok_log ? "OK" : "FAIL");
                
                // 2. 打印触发信息
                printf("2. 触发参数:\r\n");
                printf("   - 触发模式: %s (%d)\r\n", GetTriggerModeString(g_sampling_log.trigger_mode_at_start), g_sampling_log.trigger_mode_at_start);
                printf("   - 达到触发条件时间(系统启动后): %lu ms\r\n", g_sampling_log.sampling_start_time);
                
                // 3. 打印蠕动泵2 (取样) 运行信息
                printf("3. 海水过滤取样阶段 (蠕动泵2):\r\n");
                printf("   - 启动时刻: %lu ms\r\n", g_sampling_log.pump2_start_time);
                printf("   - 停止时刻: %lu ms\r\n", g_sampling_log.pump2_stop_time);
                printf("   - 最终累计脉冲数: %lu\r\n", g_sampling_log.pump2_final_pulse_count);

                // 4. 打印蠕动泵1 (清洗) 运行信息
                printf("4. 设备清洗阶段 (蠕动泵1):\r\n");
                printf("   - 启动时刻: %lu ms\r\n", g_sampling_log.pump1_start_time);
                printf("   - 停止时刻: %lu ms\r\n", g_sampling_log.pump1_stop_time);
                printf("====================================================\r\n");
            } else {
                printf("\r\n!!! 读取失败：Flash 中未找到有效日志数据或数据已损坏 !!!\r\n");
            }
        }
        else if (strncmp(command_str, "pump2_run", 9) == 0) {
            // 1. 设置默认值
            int speed = 375; // 默认转速 375
            int dir = 1;     // 默认方向 1 (CW)
            
            // 2. 尝试从命令中提取参数
            // 如果输入 "pump2_run"，提取到 0 个，保持默认 (375, 1)
            // 如果输入 "pump2_run 600"，提取到 1 个，变为 (600, 1)
            // 如果输入 "pump2_run 600 0"，提取到 2 个，变为 (600, 0)
            sscanf(command_str, "pump2_run %d %d", &speed, &dir);
            
            // 3. 安全校验 (防止输入超出范围的数值)
            if (speed < 0) speed = 0;
            if (speed > 3000) speed = 3000;
            if (dir != 0 && dir != 1) dir = 1; // 强制方向只能是 0 或 1

            // 4. 执行控制
            ModbusRTU_PumpStart(&huart3, RS485_3_DE_Transmit, RS485_3_DE_Receive, 0x01, dir, 10, speed, 360000);
            
            // 5. 动态反馈信息
            char reply[64];
            snprintf(reply, sizeof(reply), "\r\n#Pump2 START (Speed:%d, Dir:%d)#\r\n", speed, dir);
            RS485_Transmit(DEBUG_UART, reply, strlen(reply), RS485_PORT_5);
        }
        else if (strncmp(command_str, "pump2STOP", 9) == 0) {
            ModbusRTU_PumpStop(&huart3, RS485_3_DE_Transmit, RS485_3_DE_Receive, 0x01, 0);
            RS485_Transmit(DEBUG_UART, "\r\n#Pump2STOP START#\r\n", strlen("\r\n#Pump2STOP START#\r\n"), RS485_PORT_5);
        }
        else if (strncmp(command_str, "pump1_run", 9) == 0) {
            int speed = 375; // 默认转速 375
            int dir = 1;     // 默认方向 1 (CW)
            
            sscanf(command_str, "pump1_run %d %d", &speed, &dir);
            
            if (speed < 0) speed = 0;
            if (speed > 3000) speed = 3000;
            if (dir != 0 && dir != 1) dir = 1;

            ModbusRTU_PumpStart(&huart2, RS485_2_DE_Transmit, RS485_2_DE_Receive, 0x01, dir, 10, speed, 360000);
            
            char reply[64];
            snprintf(reply, sizeof(reply), "\r\n#Pump1 START (Speed:%d, Dir:%d)#\r\n", speed, dir);
            RS485_Transmit(DEBUG_UART, reply, strlen(reply), RS485_PORT_5);
        }
        else if (strncmp(command_str, "pump1STOP", 9) == 0) {
            ModbusRTU_PumpStop(&huart2, RS485_2_DE_Transmit, RS485_2_DE_Receive, 0x01, 0);
            RS485_Transmit(DEBUG_UART, "\r\n#Pump1STOP START#\r\n", strlen("\r\n#Pump1STOP START#\r\n"), RS485_PORT_5);
        }
        else if (strncmp(command_str, "set_sm_enable", 13) == 0) {
            int enable_val = 1; // 默认值
            
            if (sscanf(command_str, "set_sm_enable %d", &enable_val) == 1) {
                if (enable_val == 0) {
                    g_job_config.state_machine_enable = false; 
                    HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"\r\n#状态机已暂停 (Paused)#\r\n", strlen("\r\n#状态机已暂停 (Paused)#\r\n"), 0xFF);
                } else if (enable_val == 1) {
                    g_job_config.state_machine_enable = true;  
                    HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"\r\n#状态机已恢复运行 (Running)#\r\n", strlen("\r\n#状态机已恢复运行 (Running)#\r\n"), 0xFF);
                } else {
                    HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"\r\n#参数错误，请使用 0(暂停) 或 1(恢复)#\r\n", strlen("\r\n#参数错误，请使用 0(暂停) 或 1(恢复)#\r\n"), 0xFF);
                }
            }
        }
        else {
            char response[] = "\r\n#NAN#\r\n";
            RS485_Transmit(DEBUG_UART, (uint8_t*)response, strlen(response), RS485_PORT_5);
        }
        memset(rx1S.rx_buf, 0x00, RX_BUF_LEN);
        rx1S.data_length = 0;
	}
}

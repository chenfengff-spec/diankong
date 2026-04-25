#include "state_machine.h"
#include "usart.h" // 需要访问huart句柄，如果Log_Event或通信函数用到
#include "gpio.h"  // 需要访问GPIO的宏和函数，如 RUN_LED_GPIO_Port
#include "modbus_rtu.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "config_manager.h" // 需要访问 Log_Save 和 Log_Load 函数
// ============================================================================
// 宏定义
// ============================================================================


// ============================================================================
// 外部访问变量定义
// ============================================================================
volatile SystemState_t currentSystemState = STATE_INITIAL;

SamplingLog_t g_sampling_log;
bool system_self_check_passed = false; // 综合自检结果

// ============================================================================
// 内部状态机变量 (不需外部访问)
// ============================================================================
static bool pump1_comm_ok = false;
static bool pump2_comm_ok = false;
static bool pulse_collector_comm_ok = false;
static bool analog_collector_comm_ok = false;

static uint32_t current_pulse_count = 0; // 从脉冲采集器获取的实时脉冲数


// ============================================================================
// 内部状态处理函数声明 (只在此文件内部使用)
// ============================================================================
static void State_Handle_Initial(void);
static void State_Handle_SelfCheck(void);
static void State_Handle_WaitingForTriggerModeSelection(void);
static void State_Handle_WaitingForTriggerCondition(void);
static void State_Handle_Pump2RunningWaterCollection(void);
static void State_Handle_Pump2StoppedCollectionComplete(void);
static void State_Handle_Pump1RunningCleaning(void);
static void State_Handle_SamplingComplete(void);
static void State_Handle_Error(void);


// ============================================================================
// 辅助函数实现 (可根据需要拆分到各自的驱动文件)
// ============================================================================

/**
  * @brief  Turn on the RUN_LED.
  * @param  None
  * @retval None
  */
void TurnOn_RUN_LED(void)  { HAL_GPIO_WritePin(RUN_LED_GPIO_Port, RUN_LED_Pin, GPIO_PIN_SET); }
/**
  * @brief  Turn off the RUN_LED.
  * @param  None
  * @retval None
  */
void TurnOff_RUN_LED(void) { HAL_GPIO_WritePin(RUN_LED_GPIO_Port, RUN_LED_Pin, GPIO_PIN_RESET); }
/**
  * @brief  Toggle the state of the RUN_LED.
  * @param  None
  * @retval None
  */
void Toggle_RUN_LED(void)  { HAL_GPIO_TogglePin(RUN_LED_GPIO_Port, RUN_LED_Pin); }

void Log_Event(const char* format, ...) {
    char log_buffer[128]; // 足够大的缓冲区来格式化日志消息
    va_list args;

    va_start(args, format);
    vsnprintf(log_buffer, sizeof(log_buffer), format, args);
    va_end(args);

    RS485_5_DE_Transmit(); // 切换到发送模式
    HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"yes \r\n", 6, 100);
    HAL_UART_Transmit(DEBUG_UART, (uint8_t*)log_buffer, strlen(log_buffer), 100);
    HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"\r\n", 2, 100); // 换行
    RS485_5_DE_Receive(); // 切换回接收模式
}

// 设备通信检查占位符实现
bool Communicate_Pump1_Check(void) {
    extern UART_HandleTypeDef huart2; // 声明在 usart.c 中定义的 huart2
    return ModbusRTU_CheckComm(&huart2, 
                            RS485_2_DE_Transmit,
                            RS485_2_DE_Receive,
                            0x01, // Slave address for Pump1
                            0x04, // Function code for reading
                            0x00, //start Address High Byte
                            0xF1, //start Address Low Byte
                            0x00, //quantity high Byte
                            0x01, //quantity low Byte
                             "Pump1");
}
bool Communicate_Pump2_Check(void) {
    extern UART_HandleTypeDef huart3; // 声明在 usart.c 中定义的 huart3
    return ModbusRTU_CheckComm(&huart3, 
                            RS485_3_DE_Transmit,
                            RS485_3_DE_Receive,
                            0x01, // Slave address for Pump1
                            0x04, // Function code for reading
                            0x00, //start Address High Byte
                            0xF1, //start Address Low Byte
                            0x00, //quantity high Byte
                            0x01, //quantity low Byte
                             "Pump2");
}
bool Communicate_PulseCollector_Check(void) {
    extern UART_HandleTypeDef huart4; // 声明在 usart.c 中定义的 huart4
    return ModbusRTU_CheckComm(&huart4, 
                            RS485_4_DE_Transmit,
                            RS485_4_DE_Receive,
                            0x01, // Slave address for AnalogCollector
                            0x03, // Function code for reading
                            0x00, //start Address High Byte
                            0x10, //start Address Low Byte
                            0x00, //quantity high Byte
                            0x02, //quantity low Byte
                             "PulseCollector");
}
bool Communicate_AnalogCollector_Check(void) {
    extern UART_HandleTypeDef huart1; // 声明在 usart.c 中定义的 huart1
    return ModbusRTU_CheckComm(&huart1, 
                            RS485_1_DE_Transmit,
                            RS485_1_DE_Receive,
                            0x01, // Slave address for AnalogCollector
                            0x03, // Function code for reading
                            0x00, //start Address High Byte
                            0x00, //start Address Low Byte
                            0x00, //quantity high Byte
                            0x01, //quantity low Byte
                             "AnalogCollector");
}

// 蠕动泵控制占位符实现
void Pump1_Start(uint16_t rpm) {
    extern UART_HandleTypeDef huart2;
    // 蠕动泵1，地址 0x01。方向 0，加速度 10，速度 rpm，运行时间 2 分钟
    uint32_t runTime_10ms = PUMP1_RUN_TIME_MS / 10; // 2分钟转换为 10ms 单位
    if (!ModbusRTU_PumpStart(&huart2, RS485_2_DE_Transmit, RS485_2_DE_Receive, 0x01, 1, 10, rpm, runTime_10ms)) {
        Log_Event("Pump1 Start FAILED.");
    }
}
void Pump1_Stop(void) {
    extern UART_HandleTypeDef huart2;
    // 蠕动泵1，地址 0x01。加速度 0 (立即停止)
    if (!ModbusRTU_PumpStop(&huart2, RS485_2_DE_Transmit, RS485_2_DE_Receive, 0x01, 0)) {
        Log_Event("Pump1 Stop FAILED.");
    }
}
void Pump2_Start(uint16_t rpm) {
    extern UART_HandleTypeDef huart3;
    // 蠕动泵2，地址 0x01。方向 0，加速度 10，速度 rpm，运行时间 10 分钟
    uint32_t runTime_10ms = PUMP2_RUN_TIME_MS / 10; // 10分钟转换为 10ms 单位
    if (!ModbusRTU_PumpStart(&huart3, RS485_3_DE_Transmit, RS485_3_DE_Receive, 0x01, 1, 10, rpm, runTime_10ms)) {
        Log_Event("Pump2 Start FAILED.");
    }
}
void Pump2_Stop(void) {
    extern UART_HandleTypeDef huart3;
    // 蠕动泵2，地址 0x01。加速度 0 (立即停止)
    if (!ModbusRTU_PumpStop(&huart3, RS485_3_DE_Transmit, RS485_3_DE_Receive, 0x01, 0)) {
        Log_Event("Pump2 Stop FAILED.");
    }
}

// 传感器数据获取占位符实现
/**
  * @brief  从模拟量采集表获取深度值。
  *         读取保持寄存器 0x0002 和 0x0003 的单精度浮点型数据。
  * @param  None
  * @retval 浮点型深度值。如果读取失败，返回 0.0f。
  */
float Get_Depth_Value(void) {
    extern UART_HandleTypeDef huart1; // 模拟量采集表连接到 USART1
    HAL_StatusTypeDef status = HAL_ERROR;
    uint8_t response[9]; // 最小响应帧 (01 03 04 DataByte0..3 CRC) = 3 + 4 + 2 = 9 字节
    uint8_t slave_address = 0x01; // 模拟量采集表的 Modbus 地址
    uint8_t function_code = 0x03; // Read Holding Registers (读取保持寄存器)

    // 请求数据：起始地址 0x0002，寄存器数量 2 (单精度浮点数占用 2 个寄存器)
    uint8_t request_data[4] = {
        0x00, 0x02, // Starting Address: 0x0002 (量程变换高位)
        0x00, 0x02  // Quantity of Registers: 2 (for a single-precision float)
    };

    float depth_value = 0.0f;

    // 发送 Modbus RTU 请求
    Log_Event("Querying Analog Collector for Depth...");
    status = ModbusRTU_Transmit(&huart1, slave_address, function_code, request_data, sizeof(request_data), RS485_1_DE_Transmit, RS485_1_DE_Receive);

    if (status == HAL_OK) {
        status = ModbusRTU_Receive(&huart1, response, slave_address, 100); // 接收超时 100ms
        // 简化验证逻辑：仅检查 ModbusRTU_Receive 返回 HAL_OK。
        if (status == HAL_OK) {
            // 校验响应帧格式：地址 0x01，功能码 0x03，字节数 0x04 (2个寄存器 * 2字节/寄存器)
            if (response[2] == 0x04) // 数据字节数应为 4
            {
                // 将 4 个字节组装成一个 32 位浮点数
                uint32_t temp_float_bytes = (uint32_t)(response[3] << 24) |
                                            (uint32_t)(response[4] << 16) |
                                            (uint32_t)(response[5] << 8)  |
                                            (uint32_t)(response[6]);
                
                // 将 32 位无符号整数转换为浮点数
                // 注意：这里需要通过联合体或类型转换来实现，假设编译器支持 IEEE 754 浮点数
                memcpy(&depth_value, &temp_float_bytes, sizeof(float));

                Log_Event("Analog Collector Depth: %.2f m", depth_value);
                return depth_value;
            } else {
                Log_Event("Analog Collector: Received ACK OK, but data byte count is incorrect (%u instead of 4).", response[2]);
            }
        } else if (status == HAL_TIMEOUT) {
            Log_Event("Analog Collector: Response Timeout.");
        } else { // status == HAL_ERROR (CRC 或地址不匹配)
            Log_Event("Analog Collector: Receive Error.");
        }
    } else {
        Log_Event("Analog Collector: Transmit Error.");
    }
    return 0.0f; // 读取失败返回 0.0f
}
/**
  * @brief  从脉冲信号采集器获取电子流量表的脉冲计数。
  *         读取 DI0 的 32 位计数数据。
  *         应答验证简化：仅检查 ModbusRTU_Receive 返回 HAL_OK。
  * @param  None
  * @retval 32 位脉冲计数。如果读取失败，返回 0。
  */
uint32_t Get_Flow_Pulse_Count(void) {
    extern UART_HandleTypeDef huart4; // 脉冲信号采集器连接到 UART4
    HAL_StatusTypeDef status = HAL_ERROR;
    uint8_t response[11]; // 最小响应帧 (01 03 04 DataHi DataLo DataHi DataLo CRC) = 7 + 4 = 11 字节
    uint8_t slave_address = 0x01; // 脉冲信号采集器的 Modbus 地址
    uint8_t function_code = 0x03; // Read Holding Registers (读取计数数据)

    // 请求数据：起始地址 0x0010，寄存器数量 2 (32位计数)
    uint8_t request_data[4] = {
        0x00, 0x10, // Starting Address: 0x0010
        0x00, 0x02  // Quantity of Registers: 2 (for 32-bit count)
    };

    uint32_t pulse_count = 0;

    // 发送 Modbus RTU 请求
    Log_Event("Querying Pulse Collector (DI0 Count)...");
    status = ModbusRTU_Transmit(&huart4, slave_address, function_code, request_data, sizeof(request_data), RS485_4_DE_Transmit, RS485_4_DE_Receive);

    if (status == HAL_OK) {
        status = ModbusRTU_Receive(&huart4, response, slave_address, 100); // 接收超时 100ms
        // [修改开始]：简化验证逻辑
        if (status == HAL_OK) {
            if (response[2] == 0x04) { // 仍然强烈建议保留这个对数据字节数的检查
                pulse_count = (uint32_t)(response[3] << 24) |
                              (uint32_t)(response[4] << 16) |
                              (uint32_t)(response[5] << 8)  |
                              (uint32_t)(response[6]);
                
                Log_Event("Pulse Collector DI0 Count: %lu", pulse_count);
                return pulse_count;
            } else {
                Log_Event("Pulse Collector: Received ACK OK, but data byte count is incorrect (%u instead of 4).", response[2]);
            }
        } else if (status == HAL_TIMEOUT) {
            Log_Event("Pulse Collector: Response Timeout.");
        } else { // status == HAL_ERROR (CRC 或地址不匹配)
            Log_Event("Pulse Collector: Receive Error.");
        }
        // [修改结束]
    } else {
        Log_Event("Pulse Collector: Transmit Error.");
    }
    return 0; // 读取失败返回 0
}


// ============================================================================
// 状态处理函数实现 (静态函数，仅在此文件内可见)
// ============================================================================

static void State_Handle_Initial(void) {
    Log_Event("System Initialized. Entering Self-Check state.");
    TurnOn_RUN_LED();
    currentSystemState = STATE_SELF_CHECK;
    // 重置所有标志和记录
    memset(&g_sampling_log, 0, sizeof(SamplingLog_t));
    pump1_comm_ok = false;
    pump2_comm_ok = false;
    pulse_collector_comm_ok = false;
    analog_collector_comm_ok = false;
    system_self_check_passed = false;
}

static void State_Handle_SelfCheck(void) {
    static uint32_t self_check_start_tick = 0;
    if (self_check_start_tick == 0) {
        self_check_start_tick = HAL_GetTick();
        Log_Event("Starting system self-check...");
    }

    if (!pump1_comm_ok) {
        pump1_comm_ok = Communicate_Pump1_Check();
        if (!pump1_comm_ok) Log_Event("Self-check: Pump1 comm FAILED.");
    }
    if (!pump2_comm_ok) {
        pump2_comm_ok = Communicate_Pump2_Check();
        if (!pump2_comm_ok) Log_Event("Self-check: Pump2 comm FAILED.");
    }
    if (!pulse_collector_comm_ok) {
        //pulse_collector_comm_ok = Communicate_PulseCollector_Check();
            pulse_collector_comm_ok = true;
        if (!pulse_collector_comm_ok) Log_Event("Self-check: Pulse Collector comm FAILED.");
    }
    if (!analog_collector_comm_ok) {
        analog_collector_comm_ok = Communicate_AnalogCollector_Check();
        if (!analog_collector_comm_ok) Log_Event("Self-check: Analog Collector comm FAILED.");
    }

    if (pump1_comm_ok && pump2_comm_ok && pulse_collector_comm_ok && analog_collector_comm_ok) {
        system_self_check_passed = true;
        Log_Event("System self-check PASSED.");

        g_sampling_log.pump1_comm_ok_log = pump1_comm_ok;
        g_sampling_log.pump2_comm_ok_log = pump2_comm_ok;
        g_sampling_log.pulse_collector_comm_ok_log = pulse_collector_comm_ok;
        g_sampling_log.analog_collector_comm_ok_log = analog_collector_comm_ok;
        g_sampling_log.system_self_check_overall_ok = system_self_check_passed;
        
        currentSystemState = STATE_WAITING_FOR_TRIGGER_MODE_SELECTION;
        self_check_start_tick = 0;
        TurnOff_RUN_LED();
    }
    else if ((HAL_GetTick() - self_check_start_tick > SYSTEM_SELF_CHECK_TIMEOUT_MS) && !system_self_check_passed) {
        Log_Event("System self-check FAILED or TIMEOUT.");

        g_sampling_log.pump1_comm_ok_log = pump1_comm_ok;
        g_sampling_log.pump2_comm_ok_log = pump2_comm_ok;
        g_sampling_log.pulse_collector_comm_ok_log = pulse_collector_comm_ok;
        g_sampling_log.analog_collector_comm_ok_log = analog_collector_comm_ok;
        g_sampling_log.system_self_check_overall_ok = system_self_check_passed; 

        currentSystemState = STATE_ERROR;
        self_check_start_tick = 0;
    }
}

static void State_Handle_WaitingForTriggerModeSelection(void) {
    // 这里不再需要 static bool mode_selected，因为 g_job_config 是全局的
    // 并且调试模式或上位机已经设置了 g_job_config.trigger_mode
    // 只要 g_job_config.trigger_mode 不是 NONE，就可以进入下一个状态

    if (g_job_config.trigger_mode == TRIGGER_MODE_NONE) {
        Log_Event("Waiting for trigger mode selection from HMI/Debug Mode...");
        // 可以在这里添加一些延迟或 LED 闪烁，表示等待中
        HAL_Delay(500); // 示例延迟
    } else {
        Log_Event("Trigger mode selected: %s.", GetTriggerModeString(g_job_config.trigger_mode));
        g_sampling_log.trigger_mode_at_start = g_job_config.trigger_mode; // 记录实际启动的模式
        currentSystemState = STATE_WAITING_FOR_TRIGGER_CONDITION;
    }
}

static void State_Handle_WaitingForTriggerCondition(void) {
    static uint32_t delay_start_tick = 0; // 用于延时触发的计时器
    bool trigger_met = false;
    float current_depth = 0.0f;
    
    // 如果是第一次进入此状态，或者上次的延时计时器被重置了，就初始化它
    if (delay_start_tick == 0) {
        delay_start_tick = HAL_GetTick();
        Log_Event("Waiting for trigger condition: %s...", GetTriggerModeString(g_job_config.trigger_mode));
    }

    switch (g_job_config.trigger_mode) {
        case TRIGGER_MODE_DELAY:
            if (HAL_GetTick() - delay_start_tick >= g_job_config.params.delay_time_ms) {
                trigger_met = true;
                Log_Event("Trigger: Delay condition met after %lu ms.", g_job_config.params.delay_time_ms);
            }
            break;

        case TRIGGER_MODE_DEPTH:
            current_depth = Get_Depth_Value(); // 从模拟量采集表获取深度
            if (current_depth >= g_job_config.params.depth_value) {
                trigger_met = true;
                printf("Trigger: Depth condition met (Current: %.2f m, Set: %.2f m).\r\n", current_depth, g_job_config.params.depth_value);
                Log_Event("Trigger: Depth condition met.");
            } else {
                printf("Current depth: %.2f m, waiting for %.2f m.\r\n", current_depth, g_job_config.params.depth_value);
                HAL_Delay(500); // 示例：每隔 500ms 检查一次深度
            }
            break;

        case TRIGGER_MODE_DEPTH_OR_DELAY:
            current_depth = Get_Depth_Value();
            // 哪个条件先满足就触发
            if (current_depth >= g_job_config.params.depth_value) {
                trigger_met = true;
                printf("Trigger: Depth condition met first (Current: %.2f m, Set: %.2f m).\r\n", current_depth, g_job_config.params.depth_value);
                Log_Event("Trigger: Depth condition met first.");
            } else if (HAL_GetTick() - delay_start_tick >= g_job_config.params.delay_time_ms) {
                trigger_met = true;
                Log_Event("Trigger: Delay condition met first after %lu ms.", g_job_config.params.delay_time_ms);
            } else {
                printf("Current depth: %.2f m (waiting for %.2f m), Elapsed delay: %lu ms (waiting for %lu ms).\r\n",
                       current_depth, g_job_config.params.depth_value,
                       HAL_GetTick() - delay_start_tick, g_job_config.params.delay_time_ms);
                HAL_Delay(500); // 示例：每隔 500ms 检查一次
            }
            break;

        case TRIGGER_MODE_SERIAL_COMMAND:
            Log_Event("Trigger: Waiting for serial command (e.g., 'start_sampling').");
            // 这里的逻辑会复杂一些，需要等待上位机发送特定的启动命令。
            // 可以在 HandleDebugMode_Independent 中添加一个 "start_sampling" 命令，
            // 并在接收到该命令时设置一个标志，State_Handle_WaitingForTriggerCondition 检查这个标志。
            // 例如：extern volatile bool start_sampling_command_received;
            // if (start_sampling_command_received) {
            //     trigger_met = true;
            //     start_sampling_command_received = false; // 清除标志
            //     Log_Event("Trigger: Serial command received.");
            // }
            // [暂时简化]：为演示，这里可以加入一个超时或在 debug 模式下手动切换
            HAL_Delay(1000); // 模拟等待
            break;

        case TRIGGER_MODE_NONE:
            Log_Event("Error: No trigger mode set. Transitioning to ERROR state.");
            currentSystemState = STATE_ERROR;
            break;
    }

    if (trigger_met) {
        g_sampling_log.sampling_start_time = HAL_GetTick(); // 记录启动时间
        Log_Event("Trigger condition met. Starting water sampling (Pump2).");
        currentSystemState = STATE_PUMP2_RUNNING_WATER_COLLECTION;
        delay_start_tick = 0; // 重置计时器
    }
    // 否则继续等待
}

static void State_Handle_Pump2RunningWaterCollection(void) {
    static bool pump2_started = false;
    if (!pump2_started) {
        //if (!Reset_Flow_Pulse_Count()) {
        //    Log_Event("Warning: Failed to reset pulse count before starting Pump2.");
              // 可以选择在这里进入 ERROR 状态，或者继续运行但记录警告
        //}
        Pump2_Start(PUMP2_RPM_DEFAULT);
        g_sampling_log.pump2_start_time = HAL_GetTick();
        current_pulse_count = 0;
        Log_Event("Pump2 started for water collection.");
        pump2_started = true;
    }
    HAL_Delay(1000); // 示例：每隔 500ms 检查一次停止条件
    current_pulse_count = Get_Flow_Pulse_Count();

    if ((HAL_GetTick() - g_sampling_log.pump2_start_time >= PUMP2_RUN_TIME_MS) ||
        (current_pulse_count >= REQUIRED_PULSE_FOR_10L)) {
        Log_Event("Pump2 stop condition met.");
        g_sampling_log.pump2_final_pulse_count = current_pulse_count;
        currentSystemState = STATE_PUMP2_STOPPED_COLLECTION_COMPLETE;
        pump2_started = false;
    }
}

static void State_Handle_Pump2StoppedCollectionComplete(void) {
    Pump2_Stop();
    g_sampling_log.pump2_stop_time = HAL_GetTick();
    Log_Event("Pump2 stopped. Water collection complete.");
    currentSystemState = STATE_PUMP1_RUNNING_CLEANING;
}

static void State_Handle_Pump1RunningCleaning(void) {
    static bool pump1_started = false;
    if (!pump1_started) {
        Pump1_Start(PUMP1_RPM_DEFAULT);
        g_sampling_log.pump1_start_time = HAL_GetTick();
        Log_Event("Pump1 started for cleaning.");
        pump1_started = true;
    }

    if (HAL_GetTick() - g_sampling_log.pump1_start_time >= PUMP1_RUN_TIME_MS) {
        Pump1_Stop();
        g_sampling_log.pump1_stop_time = HAL_GetTick();
        Log_Event("Pump1 stopped. Cleaning complete.");
        currentSystemState = STATE_SAMPLING_COMPLETE;
        pump1_started = false;
    }
}

static void State_Handle_SamplingComplete(void) {
    static bool is_completed_action_done = false; 
    static bool is_log_saved = false;
    if (!is_log_saved) {
        Log_Save(); 
        Log_Event("Log saved to flash.");
        is_log_saved = true;
    }
    if (!is_completed_action_done) {
        Log_Event("Sampling process completed successfully!");
        TurnOn_RUN_LED();
        is_completed_action_done = true; 
    }
}

static void State_Handle_Error(void) {
    Log_Event("ERROR STATE. System halted or in recovery mode.");
    Toggle_RUN_LED();
    HAL_Delay(500);
    // 保持在此状态
}


// ============================================================================
// 状态机初始化与运行函数
// ============================================================================

void StateMachine_Init(void) {
    currentSystemState = STATE_INITIAL;
    g_job_config.trigger_mode = TRIGGER_MODE_NONE;
    memset(&g_sampling_log, 0, sizeof(SamplingLog_t));
    system_self_check_passed = false;
    // 初始化所有RS485的DE引脚为接收模式
    RS485_1_DE_Receive();
    RS485_2_DE_Receive();
    RS485_3_DE_Receive();
    RS485_4_DE_Receive();
    RS485_5_DE_Receive();
    RS485_6_DE_Receive();
    Log_Event("StateMachine initialized.");
}


void StateMachine_Run(void) {
    switch (currentSystemState) {
        case STATE_INITIAL:
            State_Handle_Initial();
            break;
        case STATE_SELF_CHECK:
            State_Handle_SelfCheck();
            break;
        case STATE_WAITING_FOR_TRIGGER_MODE_SELECTION:
            State_Handle_WaitingForTriggerModeSelection();
            break;
        case STATE_WAITING_FOR_TRIGGER_CONDITION:
            State_Handle_WaitingForTriggerCondition();
            break;
        case STATE_PUMP2_RUNNING_WATER_COLLECTION:
            State_Handle_Pump2RunningWaterCollection();
            break;
        case STATE_PUMP2_STOPPED_COLLECTION_COMPLETE:
            State_Handle_Pump2StoppedCollectionComplete();
            break;
        case STATE_PUMP1_RUNNING_CLEANING:
            State_Handle_Pump1RunningCleaning();
            break;
        case STATE_SAMPLING_COMPLETE:
            State_Handle_SamplingComplete();
            break;
        case STATE_ERROR:
            State_Handle_Error();
            break;
        default:
            Log_Event("Unknown state encountered. Transitioning to ERROR state.");
            currentSystemState = STATE_ERROR;
            break;
    }
}

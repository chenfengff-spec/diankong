#include "state_machine.h"
#include "usart.h" // 需要访问huart句柄，如果Log_Event或通信函数用到
#include "gpio.h"  // 需要访问GPIO的宏和函数，如 RUN_LED_GPIO_Port

// ============================================================================
// 宏定义
// ============================================================================
#define PUMP2_RPM_DEFAULT   1000 // 示例转速
#define PUMP1_RPM_DEFAULT   800  // 示例转速

#define REQUIRED_PULSE_FOR_10L  107150UL // 电子流量计流量系数：10715脉冲/升，取水10L脉冲数量107150
#define PUMP2_MAX_RUN_TIME_MS   (10 * 60 * 1000UL) // 10分钟
#define PUMP1_RUN_TIME_MS       (2 * 60 * 1000UL)  // 2分钟

#define SYSTEM_SELF_CHECK_TIMEOUT_MS (5 * 1000UL) // 自检超时时间，例如5秒


// ============================================================================
// 外部访问变量定义
// ============================================================================
volatile SystemState_t currentSystemState = STATE_INITIAL;
TriggerMode_t selectedTriggerMode = TRIGGER_MODE_NONE;
SamplingLog_t g_sampling_log;
bool system_self_check_passed = false; // 综合自检结果

// ============================================================================
// 内部状态机变量 (不需外部访问)
// ============================================================================
static bool pump1_comm_ok = false;
static bool pump2_comm_ok = false;
static bool pulse_collector_comm_ok = false;
static bool analog_collector_comm_ok = false;

static uint32_t pump2_run_start_tick = 0;
static uint32_t pump1_run_start_tick = 0;
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

void Log_Event(const char* event_message) {
    // 实际实现：通过 USART6 (上位机接口) 打印日志
    // extern UART_HandleTypeDef huart6; // 声明外部 huart6 句柄
    // HAL_UART_Transmit(&huart6, (uint8_t*)event_message, strlen(event_message), 100);
    // HAL_UART_Transmit(&huart6, (uint8_t*)"\r\n", 2, 100); // 换行
}

// RS485 DE 引脚控制实现 (假定宏已在 gpio.h 或 main.h 中定义)
/**
  * @brief  Sets RS485_1 DE pin to receive mode. (Analog Collector)
  * @param  None
  * @retval None
  */
void RS485_1_DE_Receive(void) { HAL_GPIO_WritePin(USART1_DE_GPIO_Port, USART1_DE_Pin, GPIO_PIN_RESET); }
/**
  * @brief  Sets RS485_1 DE pin to transmit mode. (Analog Collector)
  * @param  None
  * @retval None
  */
void RS485_1_DE_Transmit(void) { HAL_GPIO_WritePin(USART1_DE_GPIO_Port, USART1_DE_Pin, GPIO_PIN_SET); }
/**
  * @brief  Sets RS485_2 DE pin to receive mode. (Pump 1)
  * @param  None
  * @retval None
  */
void RS485_2_DE_Receive(void) { HAL_GPIO_WritePin(USART2_DE_GPIO_Port, USART2_DE_Pin, GPIO_PIN_RESET); }
/**
  * @brief  Sets RS485_2 DE pin to transmit mode. (Pump 1)
  * @param  None
  * @retval None
  */
void RS485_2_DE_Transmit(void) { HAL_GPIO_WritePin(USART2_DE_GPIO_Port, USART2_DE_Pin, GPIO_PIN_SET); }
/**
  * @brief  Sets RS485_3 DE pin to receive mode. (Pump 2)
  * @param  None
  * @retval None
  */
void RS485_3_DE_Receive(void) { HAL_GPIO_WritePin(USART3_DE_GPIO_Port, USART3_DE_Pin, GPIO_PIN_RESET); }
/**
  * @brief  Sets RS485_3 DE pin to transmit mode. (Pump 2)
  * @param  None
  * @retval None
  */
void RS485_3_DE_Transmit(void) { HAL_GPIO_WritePin(USART3_DE_GPIO_Port, USART3_DE_Pin, GPIO_PIN_SET); }
/**
  * @brief  Sets RS485_4 DE pin to receive mode. (Pulse Collector)
  * @param  None
  * @retval None
  */
void RS485_4_DE_Receive(void) { HAL_GPIO_WritePin(USART4_DE_GPIO_Port, USART4_DE_Pin, GPIO_PIN_RESET); }
/**
  * @brief  Sets RS485_4 DE pin to transmit mode. (Pulse Collector)
  * @param  None
  * @retval None
  */
void RS485_4_DE_Transmit(void) { HAL_GPIO_WritePin(USART4_DE_GPIO_Port, USART4_DE_Pin, GPIO_PIN_SET); }
/**
  * @brief  Sets RS485_6 DE pin to receive mode. (Upper Computer)
  * @param  None
  * @retval None
  */
void RS485_6_DE_Receive(void) { HAL_GPIO_WritePin(USART6_DE_GPIO_Port, USART6_DE_Pin, GPIO_PIN_RESET); }
/**
  * @brief  Sets RS485_6 DE pin to transmit mode. (Upper Computer)
  * @param  None
  * @retval None
  */
void RS485_6_DE_Transmit(void) { HAL_GPIO_WritePin(USART6_DE_GPIO_Port, USART6_DE_Pin, GPIO_PIN_SET); }



// 设备通信检查占位符实现
bool Communicate_Pump1_Check(void) {
    Log_Event("Checking Pump1 comm...");
    HAL_Delay(50);
    return true; // 实际应根据通信结果返回
}
bool Communicate_Pump2_Check(void) {
    Log_Event("Checking Pump2 comm...");
    HAL_Delay(50);
    return true;
}
bool Communicate_PulseCollector_Check(void) {
    Log_Event("Checking Pulse Collector comm...");
    HAL_Delay(50);
    return true;
}
bool Communicate_AnalogCollector_Check(void) {
    Log_Event("Checking Analog Collector comm...");
    HAL_Delay(50);
    return true;
}

// 蠕动泵控制占位符实现
void Pump1_Start(uint16_t rpm) {
    Log_Event("Pump1 started.");
    // 发送RS485指令控制蠕动泵1
}
void Pump1_Stop(void) {
    Log_Event("Pump1 stopped.");
    // 发送RS485指令停止蠕动泵1
}
void Pump2_Start(uint16_t rpm) {
    Log_Event("Pump2 started.");
    // 发送RS485指令控制蠕动泵2
}
void Pump2_Stop(void) {
    Log_Event("Pump2 stopped.");
    // 发送RS485指令停止蠕动泵2
}

// 传感器数据获取占位符实现
float Get_Depth_Value(void) {
    // 通过RS485_1向模拟量采集表查询深度值
    return 10.5f; // 示例深度值
}
uint32_t Get_Flow_Pulse_Count(void) {
    // 通过RS485_4向脉冲信号采集器查询当前脉冲数
    return current_pulse_count; // 示例，实际应从硬件读取
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
        pulse_collector_comm_ok = Communicate_PulseCollector_Check();
        if (!pulse_collector_comm_ok) Log_Event("Self-check: Pulse Collector comm FAILED.");
    }
    if (!analog_collector_comm_ok) {
        analog_collector_comm_ok = Communicate_AnalogCollector_Check();
        if (!analog_collector_comm_ok) Log_Event("Self-check: Analog Collector comm FAILED.");
    }

    if (pump1_comm_ok && pump2_comm_ok && pulse_collector_comm_ok && analog_collector_comm_ok) {
        system_self_check_passed = true;
        Log_Event("System self-check PASSED.");
        currentSystemState = STATE_WAITING_FOR_TRIGGER_MODE_SELECTION;
        self_check_start_tick = 0;
        TurnOff_RUN_LED();
    }
    else if ((HAL_GetTick() - self_check_start_tick > SYSTEM_SELF_CHECK_TIMEOUT_MS) && !system_self_check_passed) {
        Log_Event("System self-check FAILED or TIMEOUT.");
        currentSystemState = STATE_ERROR;
        self_check_start_tick = 0;
    }
}

static void State_Handle_WaitingForTriggerModeSelection(void) {
    static bool mode_selected = false;
    if (!mode_selected) {
        Log_Event("Waiting for trigger mode selection...");
        // 实际应从上位机获取
        selectedTriggerMode = TRIGGER_MODE_DELAY; // 示例：暂时硬编码
        g_sampling_log.trigger_mode_at_start = selectedTriggerMode;
        mode_selected = true;
        Log_Event("Trigger mode selected.");
    }

    if (selectedTriggerMode != TRIGGER_MODE_NONE) {
        currentSystemState = STATE_WAITING_FOR_TRIGGER_CONDITION;
        mode_selected = false;
    }
}

static void State_Handle_WaitingForTriggerCondition(void) {
    Log_Event("Waiting for trigger condition...");
    bool trigger_met = false;
    float current_depth = 0.0f;
    uint32_t current_delay_ms = 0;

    switch (selectedTriggerMode) {
        case TRIGGER_MODE_DELAY:
            if (HAL_GetTick() > 5000) {
                trigger_met = true;
                Log_Event("Trigger: Delay condition met.");
            }
            break;
        case TRIGGER_MODE_DEPTH:
            current_depth = Get_Depth_Value();
            if (current_depth >= 50.0f) {
                trigger_met = true;
                Log_Event("Trigger: Depth condition met.");
            }
            break;
        case TRIGGER_MODE_DEPTH_OR_DELAY:
            current_depth = Get_Depth_Value();
            current_delay_ms = HAL_GetTick();
            if (current_depth >= 50.0f || current_delay_ms >= 10000) {
                trigger_met = true;
                Log_Event("Trigger: Depth or Delay condition met.");
            }
            break;
        case TRIGGER_MODE_SERIAL_COMMAND:
            if (HAL_GetTick() > 8000) { // 模拟等待串口指令
                trigger_met = true;
                Log_Event("Trigger: Simulated serial command received.");
            }
            break;
        case TRIGGER_MODE_NONE:
            Log_Event("Error: No trigger mode set.");
            currentSystemState = STATE_ERROR;
            break;
    }

    if (trigger_met) {
        g_sampling_log.sampling_start_time = HAL_GetTick();
        Log_Event("Trigger condition met. Starting water sampling.");
        currentSystemState = STATE_PUMP2_RUNNING_WATER_COLLECTION;
    }
}

static void State_Handle_Pump2RunningWaterCollection(void) {
    static bool pump2_started = false;
    if (!pump2_started) {
        Pump2_Start(PUMP2_RPM_DEFAULT);
        pump2_run_start_tick = HAL_GetTick();
        current_pulse_count = 0;
        Log_Event("Pump2 started for water collection.");
        pump2_started = true;
    }

    current_pulse_count = Get_Flow_Pulse_Count();

    if ((HAL_GetTick() - pump2_run_start_tick >= PUMP2_MAX_RUN_TIME_MS) ||
        (current_pulse_count >= REQUIRED_PULSE_FOR_10L)) {
        Log_Event("Pump2 stop condition met.");
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
    Log_Event("Sampling process completed successfully!");
    TurnOn_RUN_LED();
    // 保持在此状态
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
    selectedTriggerMode = TRIGGER_MODE_NONE;
    memset(&g_sampling_log, 0, sizeof(SamplingLog_t));
    system_self_check_passed = false;
    // 初始化所有RS485的DE引脚为接收模式
    RS485_1_DE_Receive();
    RS485_2_DE_Receive();
    RS485_3_DE_Receive();
    RS485_4_DE_Receive();
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

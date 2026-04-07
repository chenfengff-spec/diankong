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

void TurnOn_RUN_LED(void)  { HAL_GPIO_WritePin(RUN_LED_GPIO_Port, RUN_LED_Pin, GPIO_PIN_SET); }
void TurnOff_RUN_LED(void) { HAL_GPIO_WritePin(RUN_LED_GPIO_Port, RUN_LED_Pin, GPIO_PIN_RESET); }
void Toggle_RUN_LED(void)  { HAL_GPIO_TogglePin(RUN_LED_GPIO_Port, RUN_LED_Pin); }

void Log_Event(const char* event_message) {
    // 实际实现：通过 USART6 (上位机接口) 打印日志
    // extern UART_HandleTypeDef huart6; // 声明外部 huart6 句柄
    // HAL_UART_Transmit(&huart6, (uint8_t*)event_message, strlen(event_message), 100);
    // HAL_UART_Transmit(&huart6, (uint8_t*)"\r\n", 2, 100); // 换行
}

// RS485 DE 引脚控制实现 (假定宏已在 gpio.h 或 main.h 中定义)
void RS485_1_DE_Receive(void) { HAL_GPIO_WritePin(RS485_1_DE_GPIO_Port, RS485_1_DE_Pin, GPIO_PIN_RESET); }
void RS485_1_DE_Transmit(void) { HAL_GPIO_WritePin(RS485_1_DE_GPIO_Port, RS485_1_DE_Pin, GPIO_PIN_SET); }
void RS485_2_DE_Receive(void) { HAL_GPIO_WritePin(RS485_2_DE_GPIO_Port, RS485_2_DE_Pin, GPIO_PIN_RESET); }
void RS485_2_DE_Transmit(void) { HAL_GPIO_WritePin(RS485_2_DE_GPIO_Port, RS485_2_DE_Pin, GPIO_PIN_SET); }
void RS485_3_DE_Receive(void) { HAL_GPIO_WritePin(RS485_3_DE_GPIO_Port, RS485_3_DE_Pin, GPIO_PIN_RESET); }
void RS485_3_DE_Transmit(void) { HAL_GPIO_WritePin(RS485_3_DE_GPIO_Port, RS485_3_DE_Pin, GPIO_PIN_SET); }
void RS485_4_DE_Receive(void) { HAL_GPIO_WritePin(RS485_4_DE_GPIO_Port, RS485_4_DE_Pin, GPIO_PIN_RESET); }
void RS485_4_DE_Transmit(void) { HAL_GPIO_WritePin(RS485_4_DE_GPIO_Port, RS485_4_DE_Pin, GPIO_PIN_SET); }
void RS485_6_DE_Receive(void) { HAL_GPIO_WritePin(RS485_6_DE_GPIO_Port, RS485_6_DE_Pin, GPIO_PIN_RESET); }
void RS485_6_DE_Transmit(void) { HAL_GPIO_WritePin(RS485_6_DE_GPIO_Port, RS485_6_DE_Pin, GPIO_PIN_SET); }


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

// 其他输入
bool Read_DeepWater_Switch(void) {
    return HAL_GPIO_ReadPin(DEEP_WATER_SWITCH_GPIO_Port, DEEP_WATER_SWITCH_Pin) == GPIO_PIN_SET; // 假设高电平为触发
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
        system_self_check_
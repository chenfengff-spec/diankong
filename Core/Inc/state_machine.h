#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "main.h" // 包含 main.h 以获取HAL库相关定义和GPIO宏
#include <stdbool.h> // 用于 bool 类型
#include <string.h>
#include "modbus_rtu.h"

#define PUMP2_RPM_DEFAULT   375 // 示例转速
#define PUMP1_RPM_DEFAULT   50  // 示例转速

#define REQUIRED_PULSE_FOR_10L  107150UL // 电子流量计流量系数：10715脉冲/升，取水10L脉冲数量107150
#define PUMP2_RUN_TIME_MS       (10UL * 60UL * 1000UL) // 泵2运行10分钟
#define PUMP1_RUN_TIME_MS       (2UL * 60UL * 1000UL)  // 泵1运行2分钟

#define SYSTEM_SELF_CHECK_TIMEOUT_MS (5 * 1000UL) // 自检超时时间，例如5秒

// ============================================================================
// 状态定义
// ============================================================================
typedef enum {
    STATE_INITIAL,
    STATE_SELF_CHECK,
    STATE_WAITING_FOR_TRIGGER_MODE_SELECTION,
    STATE_WAITING_FOR_TRIGGER_CONDITION,
    STATE_PUMP2_RUNNING_WATER_COLLECTION,
    STATE_PUMP2_STOPPED_COLLECTION_COMPLETE,
    STATE_PUMP1_RUNNING_CLEANING,
    STATE_SAMPLING_COMPLETE,
    STATE_DEBUG_MODE,
    STATE_ERROR
} SystemState_t;

// 记录数据结构体
typedef struct {
    uint32_t sampling_start_time;
    TriggerMode_t trigger_mode_at_start;
    uint32_t pump2_stop_time;
    uint32_t pump1_start_time;
    uint32_t pump1_stop_time;
    // 其他需要记录的数据
    bool pump1_comm_ok_log;         // 泵1通信检查结果
    bool pump2_comm_ok_log;         // 泵2通信检查结果
    bool pulse_collector_comm_ok_log; // 脉冲采集器通信检查结果
    bool analog_collector_comm_ok_log; // 模拟量采集器通信检查结果
    bool system_self_check_overall_ok; // 整体自检是否通过
} SamplingLog_t;


// ============================================================================
// 外部访问变量声明
// ============================================================================
extern volatile SystemState_t currentSystemState;
extern SamplingLog_t g_sampling_log;
extern bool system_self_check_passed; // 外部声明，可能在其他模块检查

// ============================================================================
// 函数声明
// ============================================================================

/**
  * @brief  初始化状态机，设置初始状态和相关变量。
  * @param  None
  * @retval None
  */
void StateMachine_Init(void);

/**
  * @brief  状态机的主运行函数，在主循环中周期性调用。
  * @param  None
  * @retval None
  */
void StateMachine_Run(void);

const char* GetTriggerModeString(TriggerMode_t mode);

// 辅助函数声明 (可以在这里声明，也可以在其他驱动模块中声明)
void TurnOn_RUN_LED(void);
void TurnOff_RUN_LED(void);
void Toggle_RUN_LED(void);
void Log_Event(const char* format, ...);



// 设备通信检查占位符
bool Communicate_Pump1_Check(void);
bool Communicate_Pump2_Check(void);
bool Communicate_PulseCollector_Check(void);
bool Communicate_AnalogCollector_Check(void);

// 蠕动泵控制占位符
void Pump1_Start(uint16_t rpm);
void Pump1_Stop(void);
void Pump2_Start(uint16_t rpm);
void Pump2_Stop(void);

// 传感器数据获取占位符
float Get_Depth_Value(void);
uint32_t Get_Flow_Pulse_Count(void);

// 其他输入
bool Read_DeepWater_Switch(void);


#endif // STATE_MACHINE_H

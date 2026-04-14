#ifndef DEBUG_COMMANDS_H
#define DEBUG_COMMANDS_H

#include "main.h" // 包含 main.h 以获取 rxStruct, debugMode, DEBUG_UART 的声明, JobConfig_t, g_job_config
#include <string.h> // for memcmp, strlen
#include <stdio.h>  // for sscanf, printf (if enabled)
#include <stdlib.h> // for atoi, atof

// [新增]：调试命令处理函数声明
void HandleDebugMode_Independent(void);
// [新增]：用于将触发模式枚举转换为字符串的辅助函数（方便打印）
const char* GetTriggerModeString(TriggerMode_t mode);

#endif // DEBUG_COMMANDS_H

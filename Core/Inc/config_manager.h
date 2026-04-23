#ifndef __CONFIG_MANAGER_H
#define __CONFIG_MANAGER_H

#include "main.h"
#include "debug_commands.h" // 包含你定义的 JobConfig_t 结构体

// 函数声明
void Config_Load(void);
void Config_Save(void);

#endif

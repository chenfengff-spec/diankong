/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
#define RX_BUF_LEN 64
typedef struct {
    uint8_t rx_buf[RX_BUF_LEN];
    uint16_t data_length;
} rxStruct;
typedef enum {
    TRIGGER_MODE_NONE,
    TRIGGER_MODE_DELAY,
    TRIGGER_MODE_DEPTH,
    TRIGGER_MODE_DEPTH_OR_DELAY,
    TRIGGER_MODE_SERIAL_COMMAND
} TriggerMode_t;
// [新增]：作业配置结构体（用于调试模式设置）
typedef struct {
    TriggerMode_t trigger_mode;
    union {
        uint32_t delay_time_ms; // 延时时间 (毫秒)
        float depth_value;      // 深度触发值 (米)
    } params;
} JobConfig_t;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void HandleDebugMode_Independent(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define USART2_DE_Pin GPIO_PIN_4
#define USART2_DE_GPIO_Port GPIOA
#define USART4_DE_Pin GPIO_PIN_5
#define USART4_DE_GPIO_Port GPIOA
#define USART3_DE_Pin GPIO_PIN_15
#define USART3_DE_GPIO_Port GPIOE
#define RUN_LED_Pin GPIO_PIN_15
#define RUN_LED_GPIO_Port GPIOB
#define USART6_DE_Pin GPIO_PIN_8
#define USART6_DE_GPIO_Port GPIOC
#define USART1_DE_Pin GPIO_PIN_11
#define USART1_DE_GPIO_Port GPIOA
#define USART5_DE_Pin GPIO_PIN_1
#define USART5_DE_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */
#include "usart.h" // 包含 usart.h 以获取 huart6 的声明
#include <string.h>
// 定义 DEBUG_UART 宏，因为它是全局的调试串口
#define DEBUG_UART &huart6

// [新增]：声明 debugMode 和 rx1S 变量为外部，因为它们在 main.c/usart.c 中定义
extern volatile uint8_t debugMode;
extern rxStruct rx1S;
extern uint8_t aRx1Buffer;

extern JobConfig_t g_job_config;
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

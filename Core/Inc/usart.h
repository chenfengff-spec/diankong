/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
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
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern UART_HandleTypeDef huart4;

extern UART_HandleTypeDef huart5;

extern UART_HandleTypeDef huart1;

extern UART_HandleTypeDef huart2;

extern UART_HandleTypeDef huart3;

extern UART_HandleTypeDef huart6;

/* USER CODE BEGIN Private defines */
void RS485_1_DE_Receive(void);
void RS485_1_DE_Transmit(void);
void RS485_2_DE_Receive(void);
void RS485_2_DE_Transmit(void);
void RS485_3_DE_Receive(void);
void RS485_3_DE_Transmit(void);
void RS485_4_DE_Receive(void);
void RS485_4_DE_Transmit(void);
void RS485_5_DE_Receive(void);
void RS485_5_DE_Transmit(void);
void RS485_6_DE_Receive(void);
void RS485_6_DE_Transmit(void);
typedef enum {
    RS485_PORT_1,
    RS485_PORT_2,
    RS485_PORT_3,
    RS485_PORT_4,
    RS485_PORT_5,
    RS485_PORT_6,
    RS485_PORT_MAX
} RS485_Port_t;

// 定义 DE 引脚控制函数类型
typedef void (*RS485_DE_Control_t)(void);

static const RS485_DE_Control_t transmit_func[] = {
    RS485_1_DE_Transmit,
    RS485_2_DE_Transmit,
    RS485_3_DE_Transmit,
    RS485_4_DE_Transmit,
    RS485_5_DE_Transmit,
    RS485_6_DE_Transmit
};

static const RS485_DE_Control_t receive_func[] = {
    RS485_1_DE_Receive,
    RS485_2_DE_Receive,
    RS485_3_DE_Receive,
    RS485_4_DE_Receive,
    RS485_5_DE_Receive,
    RS485_6_DE_Receive
};
void RS485_Transmit(UART_HandleTypeDef*huart,uint8_t* data, uint16_t size, RS485_Port_t port); // 新增 RS485 发送函数声明
/* USER CODE END Private defines */

void MX_UART4_Init(void);
void MX_UART5_Init(void);
void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_USART3_UART_Init(void);
void MX_USART6_UART_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */


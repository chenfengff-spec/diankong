#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include "stm32f4xx_hal.h" // HAL 库头文件
#include <stdint.h>        // 标准整数类型定义
#include <stdbool.h>        // bool 类型定义

// Modbus RTU 错误码定义
typedef enum {
    MODBUS_OK = 0,
    MODBUS_ERROR,
    MODBUS_TIMEOUT,
    MODBUS_INVALID_CRC,
    MODBUS_INVALID_ADDRESS,
    MODBUS_INVALID_FUNCTION
} ModbusStatus_t;

/**
  * @brief  计算 Modbus RTU 帧的 CRC16 校验码.
  * @param  data: 指向要计算 CRC 校验码的数据的指针.
  * @param  length: 数据的长度 (字节).
  * @retval CRC16 校验码.
  */
uint16_t ModbusRTU_CRC(const uint8_t *data, uint16_t length);

/**
  * @brief  通过指定的 UART/USART 发送 Modbus RTU 帧.
  * @param  huart: HAL 库的 UART_HandleTypeDef 句柄.
  * @param  slave_address: Modbus 从机地址 (1-247).
  * @param  function_code: Modbus 功能码 (例如 0x03, 0x06, 0x16).
  * @param  data: 指向要发送的数据的指针.
  * @param  data_length: 要发送的数据的字节数.
  * @param  de_transmit_func: 用于将 DE 引脚设置为发送模式的函数指针.
  * @param  de_receive_func: 用于将 DE 引脚设置为接收模式的函数指针.
  * @retval HAL_StatusTypeDef: HAL 库状态码 (HAL_OK, HAL_ERROR, HAL_TIMEOUT).
  */
HAL_StatusTypeDef ModbusRTU_Transmit(UART_HandleTypeDef *huart,
                                     uint8_t slave_address,
                                     uint8_t function_code,
                                     const uint8_t *data,
                                     uint16_t data_length,
                                     void (*de_transmit_func)(void),
                                     void (*de_receive_func)(void));

/**
  * @brief  通过指定的 UART/USART 接收 Modbus RTU 响应帧.
  * @param  huart: HAL 库的 UART_HandleTypeDef 句柄.
  * @param  response: 指向接收缓冲区 (至少 8 字节) 的指针.
  * @param  expected_slave_address: 期望的从机地址.
  * @param  timeout: 接收超时时间 (毫秒).
  * @retval HAL_StatusTypeDef:
  *     - HAL_OK: 成功接收到期望的响应.
  *     - HAL_TIMEOUT: 接收超时.
  *     - HAL_ERROR: 接收到错误数据，校验失败，或地址不匹配.
  */
HAL_StatusTypeDef ModbusRTU_Receive(UART_HandleTypeDef *huart,
                                    uint8_t *response,
                                    uint8_t expected_slave_address,
                                    uint32_t timeout);

/**
  * @brief  通用的 Modbus RTU 通信检查函数（简化版，仅检查 CRC 和地址）.
  * @param  huart: HAL 库的 UART_HandleTypeDef 句柄.
  * @param  de_transmit_func: 用于将 DE 引脚设置为发送模式的函数指针.
  * @param  de_receive_func: 用于将 DE 引脚设置为接收模式的函数指针.
  * @param  slave_address: Modbus 从机地址 (1-247).
  * @param  function_code: Modbus 功能码 (例如 0x03, 0x04).
  * @param  start_address_high: 寄存器起始地址高字节
  * @param  start_address_low: 寄存器起始地址低字节
  * @param  quantity_high: 读取寄存器的数量高字节.
  * @param  quantity_low: 读取寄存器的数量低字节
  * @param  log_prefix: 日志信息前缀 (例如 "Pump1", "PulseCollector").
  * @retval true: 通信成功; false: 通信失败.
  */
bool ModbusRTU_CheckComm(UART_HandleTypeDef *huart,
                           void (*de_transmit_func)(void),
                           void (*de_receive_func)(void),
                           uint8_t slave_address,
                           uint8_t function_code,
                           uint8_t start_address_high,
                           uint8_t start_address_low,
                           uint8_t quantity_high,
                           uint8_t quantity_low,
                           const char *log_prefix);

#endif // MODBUS_RTU_H
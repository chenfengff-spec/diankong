#include "modbus_rtu.h"
#include <string.h> // For memcpy
#include "usart.h" 
#include "gpio.h"
#include <stdio.h>

extern void Log_Event(const char* format, ...);
extern UART_HandleTypeDef huart6; // Log_Event 可能需要此句柄

// ============================================================================
// Modbus RTU 通信相关函数实现
// ============================================================================

/**
  * @brief  计算 Modbus RTU 帧的 CRC16 校验码.
  * @param  data: 指向要计算 CRC 校验码的数据的指针.
  * @param  length: 数据的长度 (字节).
  * @retval CRC16 校验码.
  */
uint16_t ModbusRTU_CRC(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

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
                                     void (*de_receive_func)(void)) {
    uint8_t frame[256]; // Modbus RTU 帧缓冲区，请确保足够大
    uint16_t frame_length = 0;
    uint16_t crc;
    HAL_StatusTypeDef status;

    // 1. 组装 Modbus RTU 帧头
    frame[frame_length++] = slave_address;
    frame[frame_length++] = function_code;

    // 2. 拷贝数据
    memcpy(&frame[frame_length], data, data_length);
    frame_length += data_length;

    // 3. 计算 CRC 校验码
    crc = ModbusRTU_CRC(frame, frame_length);
    frame[frame_length++] = (uint8_t)(crc & 0xFF); // CRC Low byte
    frame[frame_length++] = (uint8_t)(crc >> 8);   // CRC High byte

    // 4. 设置 DE 引脚为发送模式
    de_transmit_func(); // e.g., RS485_1_DE_Transmit();

    // 5. 发送 Modbus RTU 帧
    status = HAL_UART_Transmit(huart, frame, frame_length, 100); // 超时设为 100ms

    // 6. 等待发送完成 (重要!)
    if (status == HAL_OK) {
        HAL_UART_StateTypeDef uartState;
        uint32_t tickstart = HAL_GetTick();
        do {
            uartState = HAL_UART_GetState(huart);
            if (HAL_GetTick() - tickstart > 100) {
                status = HAL_TIMEOUT; // 如果超时，也返回错误
                break;
            }
        } while (uartState != HAL_UART_STATE_READY); // 等待 UART 进入就绪状态
    }

    // 7. 确保发送完成，再将 DE 引脚设置为接收模式
    de_receive_func(); // e.g., RS485_1_DE_Receive();

    return status;
}

/**
  * @brief  通过指定的 UART/USART 接收 Modbus RTU 响应帧.
  * @param  huart: HAL 库的 UART_HandleTypeDef 句柄.
  * @param  response: 指向接收缓冲区 (至少 8 字节) 的指针.
  * @param  expected_slave_address: 期望的从机地址.
  * @param  timeout: 接收超时时间 (毫秒).
  * @retval HAL_StatusTypeDef:
  *     - HAL_OK: 成功接收到期望的响应.
  *     - HAL_TIMEOUT: 接收超时.
  *     - HAL_ERROR: 接收到错误数据或校验失败.
  */
HAL_StatusTypeDef ModbusRTU_Receive(UART_HandleTypeDef *huart,
                                    uint8_t *response,
                                    uint8_t expected_slave_address,
                                    uint32_t timeout) {
    uint32_t start_time = HAL_GetTick();
    uint16_t received_length = 0;
    uint16_t crc_received, crc_calculated;

    // 1. 循环接收数据直到超时或接收到完整帧
    while (HAL_GetTick() - start_time < timeout) {
        uint8_t byte;
        if (HAL_UART_Receive(huart, &byte, 1, 1) == HAL_OK) { // 每次接收一个字节，超时1ms
            response[received_length++] = byte;
            // 简化的帧完整性判断：假设至少需要 8 字节才能构成一个完整的Modbus帧 (地址+功能码+至少1字节数据+2字节CRC)
            if (received_length >= 8) {
                // 尝试进行CRC校验和地址检查
                crc_received = (response[received_length - 1] << 8) | response[received_length - 2];
                crc_calculated = ModbusRTU_CRC(response, received_length - 2);

                if (crc_received == crc_calculated && response[0] == expected_slave_address) {
                    // 校验成功，并且地址匹配，认为接收完成
                    return HAL_OK;
                } else {
                    // 校验失败或地址不匹配，清空缓冲区重新接收
                    received_length = 0;
                }
            }
        }
        // 可以添加一些小的延时，降低CPU占用率，但也会增加总的响应时间
        // HAL_Delay(1);
    }
    // 超时
    return HAL_TIMEOUT;
}

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
                           const char *log_prefix) {

    HAL_StatusTypeDef status = HAL_ERROR;
    uint8_t response[8]; // 至少 8 字节，用于存放 Modbus 响应帧

    // 准备 Modbus RTU 查询数据
    uint8_t data[4] = {
        start_address_high, start_address_low, // Starting Address
        quantity_high, quantity_low  // Quantity of Registers
    };

    // 发送 Modbus RTU 查询帧
    Log_Event("Checking %s comm...", log_prefix); // 添加设备名称到日志

    status = ModbusRTU_Transmit(huart, slave_address, function_code, data, sizeof(data), de_transmit_func, de_receive_func);

    // 如果发送成功，则尝试接收响应
    if (status == HAL_OK) {
        status = ModbusRTU_Receive(huart, response, slave_address, 50); // 超时设为 50ms

        // 仅仅检查 ModbusRTU_Receive 的返回值，确保接收成功且校验通过
        if (status == HAL_OK) {
            // 既然收到了数据，就打印通信成功即可
            Log_Event("%s comm OK.", log_prefix);
            return true; // 通信成功
        } else if (status == HAL_TIMEOUT) {
            Log_Event("%s comm: Timeout.", log_prefix);
        } else {
            // status == HAL_ERROR 表明在 ModbusRTU_Receive 内部发生了错误（例如 CRC 校验失败或地址不匹配）
            Log_Event("%s comm: Receive error (CRC or address mismatch).", log_prefix);
        }
    } else {
        Log_Event("%s comm: Transmit error.", log_prefix);
    }
    return false; // 通信失败
}

// 蠕动泵控制通用函数实现 
/**
  * @brief  启动蠕动泵的 Modbus RTU 控制函数。
  *         此版本仅支持指定运行时间模式。应答验证简化：仅检查 ModbusRTU_Receive 返回 HAL_OK。
  * @param  huart: UART 句柄。
  * @param  de_transmit_func: DE 发送函数。
  * @param  de_receive_func: DE 接收函数。
  * @param  slave_address: 从机地址。
  * @param  dir: 方向 (0-1)。
  * @param  acc: 加速度 (0-255)。
  * @param  speed: 速度 (0-3000 RPM)。
  * @param  runTime_10ms: 运行时间，单位 10ms (必须大于0)。
  * @retval true: 控制成功; false: 控制失败。
  */
bool ModbusRTU_PumpStart(UART_HandleTypeDef *huart,
                         void (*de_transmit_func)(void),
                         void (*de_receive_func)(void),
                         uint8_t slave_address,
                         uint8_t dir, uint8_t acc, uint16_t speed, uint32_t runTime_10ms) {
    HAL_StatusTypeDef status = HAL_ERROR;
    uint8_t response[8]; // Modbus 标准应答帧通常为 8 字节

    uint8_t function_code = 0x10; // Write Multiple Holding Registers
    uint8_t start_address_high = 0x00;
    uint8_t start_address_low = 0xF6;
    uint16_t quantity_of_registers = 0x0004; // 4 个寄存器
    uint8_t byte_count = 0x08;              // 8 个字节

    // Modbus RTU 请求帧的数据部分 (不含从机地址和功能码，但包含字节数和实际数据)
    // 重新计算 request_data 的大小，以容纳所有 Modbus 数据
    uint8_t request_data[9 + 4]; // 5 bytes for StartAddr+QtyReg+ByteCount + 4 bytes for dir+acc+speed + 4 bytes for runTime
    
    request_data[0] = start_address_high;
    request_data[1] = start_address_low;
    request_data[2] = (uint8_t)(quantity_of_registers >> 8);
    request_data[3] = (uint8_t)(quantity_of_registers & 0xFF);
    request_data[4] = byte_count;
    request_data[5] = dir;
    request_data[6] = acc;
    request_data[7] = (uint8_t)(speed >> 8);   // Speed High Byte
    request_data[8] = (uint8_t)(speed & 0xFF); // Speed Low Byte
    // 运行时间，单位 10ms
    request_data[9] = (uint8_t)(runTime_10ms >> 24); // runTime Byte 3
    request_data[10] = (uint8_t)(runTime_10ms >> 16); // runTime Byte 2
    request_data[11] = (uint8_t)(runTime_10ms >> 8);  // runTime Byte 1
    request_data[12] = (uint8_t)(runTime_10ms & 0xFF); // runTime Byte 0
    
    // 发送 Modbus RTU 运行指令
    Log_Event("Sending Pump Start command (Addr:0x%02X, Speed:%u, Time:%lu ms)...", slave_address, speed, runTime_10ms * 10);
    // request_data 的实际总长度是 5 (起始地址+寄存器数量+字节数) + 8 (实际数据) = 13 字节
    status = ModbusRTU_Transmit(huart, slave_address, function_code, request_data, 13, de_transmit_func, de_receive_func); 

    if (status == HAL_OK) {
        status = ModbusRTU_Receive(huart, response, slave_address, 100); // 接收超时设为 100ms
        // [修改开始]：简化验证逻辑
        if (status == HAL_OK) {
            Log_Event("Pump 0x%02X Start command ACK OK.", slave_address);
            return true;
        } else if (status == HAL_TIMEOUT) {
            Log_Event("Pump 0x%02X Start command: ACK Timeout.", slave_address);
        } else { // status == HAL_ERROR (CRC 或地址不匹配)
            Log_Event("Pump 0x%02X Start command: ACK Receive error.", slave_address);
        }
        // [修改结束]
    } else {
        Log_Event("Pump 0x%02X Start command: Transmit error.", slave_address);
    }
    return false;
}

/**
  * @brief  停止蠕动泵的 Modbus RTU 控制函数。
  *         应答验证简化：仅检查 ModbusRTU_Receive 返回 HAL_OK。
  * @param  huart: UART 句柄。
  * @param  de_transmit_func: DE 发送函数。
  * @param  de_receive_func: DE 接收函数。
  * @param  slave_address: 从机地址。
  * @param  acc: 加速度 (0立即停止，非0缓慢停止)。
  * @retval true: 控制成功; false: 控制失败。
  */
bool ModbusRTU_PumpStop(UART_HandleTypeDef *huart,
                        void (*de_transmit_func)(void),
                        void (*de_receive_func)(void),
                        uint8_t slave_address,
                        uint8_t acc) {
    HAL_StatusTypeDef status = HAL_ERROR;
    uint8_t response[8]; // Modbus 标准应答帧通常为 8 字节

    uint8_t function_code = 0x10; // Write Multiple Holding Registers
    uint8_t start_address_high = 0x00;
    uint8_t start_address_low = 0xF6;
    uint16_t quantity_of_registers = 0x0002; // 2 个寄存器
    uint8_t byte_count = 0x04;              // 4 个字节

    // Modbus RTU 请求帧的数据部分 (不含从机地址和功能码，但包含字节数和实际数据)
    uint8_t request_data[9]; // 起始地址(2) + 寄存器数量(2) + 字节数(1) + 数据(4)
    request_data[0] = start_address_high;
    request_data[1] = start_address_low;
    request_data[2] = (uint8_t)(quantity_of_registers >> 8);
    request_data[3] = (uint8_t)(quantity_of_registers & 0xFF);
    request_data[4] = byte_count;
    request_data[5] = 0x00; // 方向 (dir) - 停止时通常不关心
    request_data[6] = acc;  // 加速度 (acc) - 0立即停止，非0缓慢停止
    request_data[7] = 0x00; // 速度 High Byte (speed) - 停止为 0
    request_data[8] = 0x00; // 速度 Low Byte (speed) - 停止为 0

    // 发送 Modbus RTU 停止指令
    Log_Event("Sending Pump Stop command (Addr:0x%02X, Acc:%u)...", slave_address, acc);
    // request_data 的实际长度是 5 (协议头) + 4 (数据载荷) = 9 字节
    status = ModbusRTU_Transmit(huart, slave_address, function_code, request_data, 9, de_transmit_func, de_receive_func);

    if (status == HAL_OK) {
        status = ModbusRTU_Receive(huart, response, slave_address, 100); // 接收超时设为 100ms
        // [修改开始]：简化验证逻辑
        if (status == HAL_OK) {
            Log_Event("Pump 0x%02X Stop command ACK OK.", slave_address);
            return true;
        } else if (status == HAL_TIMEOUT) {
            Log_Event("Pump 0x%02X Stop command: ACK Timeout.", slave_address);
        } else { // status == HAL_ERROR (CRC 或地址不匹配)
            Log_Event("Pump 0x%02X Stop command: ACK Receive error.", slave_address);
        }
        // [修改结束]
    } else {
        Log_Event("Pump 0x%02X Stop command: Transmit error.", slave_address);
    }
    return false;
}

#include "config_manager.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdbool.h>

// F427VGT6 Sector 11 的起始地址
#define ADDR_FLASH_SECTOR_11     ((uint32_t)0x080E0000)
#define FLASH_USER_START_ADDR    ADDR_FLASH_SECTOR_11
#define FLASH_USER_END_ADDR      ((uint32_t)0x080FFFFF)

void Config_Save(void) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;

    // 1. 解锁 Flash
    HAL_FLASH_Unlock();

    // 2. 擦除扇区 11
    // 注意：F4 必须先擦除才能写入新的数据
    EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3; // 对应 2.7V - 3.6V
    EraseInitStruct.Sector        = FLASH_SECTOR_11;       // 擦除第 11 扇区
    EraseInitStruct.NbSectors     = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        // 擦除失败处理（可选：打印错误）
        HAL_FLASH_Lock();
        return;
    }

    // 3. 写入数据
    // F4 支持按字节、半字、字、双字写入。这里建议按“字”(32位) 写入。
    uint32_t *pData = (uint32_t *)&g_job_config;
    uint32_t Size = (sizeof(JobConfig_t) + 3) / 4; // 计算需要多少个 32 位字
    uint32_t Address = FLASH_USER_START_ADDR;

    for (uint32_t i = 0; i < Size; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, Address, pData[i]) == HAL_OK) {
            Address += 4;
        } else {
            // 写入失败处理
            break;
        }
    }

    // 4. 上锁
    HAL_FLASH_Lock();
}

void Config_Load(void) {
    // F4 加载数据非常简单，直接从内存地址读取即可
    JobConfig_t *pFlash = (JobConfig_t *)FLASH_USER_START_ADDR;

    // 校验魔数
    if (pFlash->magic_word == 0xABCD1234) {
        memcpy(&g_job_config, pFlash, sizeof(JobConfig_t));
    } else {
        // 默认初始化
        g_job_config.magic_word = 0xABCD1234;
        g_job_config.trigger_mode = TRIGGER_MODE_NONE;
        g_job_config.params.delay_time_ms = 500;
        g_job_config.params.depth_value = 1.0f;
        
        // 第一次运行可以自动保存一次
        Config_Save();
    }
}


#include <stdbool.h>

// ============================================================================
// 日志存储区域定义
// F427VGT6 Sector 10 的起始地址 (128KB)
// ============================================================================
#define ADDR_FLASH_SECTOR_10     ((uint32_t)0x080C0000)
#define FLASH_LOG_START_ADDR     ADDR_FLASH_SECTOR_10
#define LOG_MAGIC_WORD           0x8899AABB  // 运行日志的专属校验标志

/**
 * @brief 将运行日志保存到 Flash (Sector 10)
 */
void Log_Save(void) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;

    // 确保保存前魔数正确
    g_sampling_log.magic_word = LOG_MAGIC_WORD;

    // 1. 解锁 Flash
    HAL_FLASH_Unlock();

    // 2. 擦除扇区 10
    EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3; // 对应 2.7V - 3.6V
    EraseInitStruct.Sector        = FLASH_SECTOR_10;       // 擦除第 10 扇区
    EraseInitStruct.NbSectors     = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        // 擦除失败处理
        HAL_FLASH_Lock();
        return;
    }

    // 3. 写入数据 (按 32 位字写入)
    uint32_t *pData = (uint32_t *)&g_sampling_log;
    uint32_t Size = (sizeof(SamplingLog_t) + 3) / 4; // 计算需要多少个 32 位字
    uint32_t Address = FLASH_LOG_START_ADDR;

    for (uint32_t i = 0; i < Size; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, Address, pData[i]) == HAL_OK) {
            Address += 4;
        } else {
            // 写入失败处理
            break;
        }
    }

    // 4. 上锁
    HAL_FLASH_Lock();
}

/**
 * @brief 从 Flash (Sector 10) 加载历史运行日志
 * @retval true: 找到有效日志; false: 无日志或已损坏
 */
bool Log_Load(void) {
    // 直接从内存映射地址读取
    SamplingLog_t *pFlash = (SamplingLog_t *)FLASH_LOG_START_ADDR;

    // 校验魔数
    if (pFlash->magic_word == LOG_MAGIC_WORD) {
        // 内存拷贝，提取到 RAM 中
        memcpy(&g_sampling_log, pFlash, sizeof(SamplingLog_t));
        return true;
    } else {
        // 如果没有找到合法日志（可能被擦除或者是新板子）
        // 不需要初始化默认日志，直接返回 false 让调用者处理
        return false;
    }
}

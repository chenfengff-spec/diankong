#include "config_manager.h"
#include <string.h>

// 根据芯片型号定义 Flash 地址 (例如 STM32F103C8T6 64KB Flash 的最后一页)
#define FLASH_SAVE_ADDR  0x0800FC00 
#define CONFIG_MAGIC     0xABCD1234  // 校验标志

void Config_Load(void) {
    JobConfig_t *flash_ptr = (JobConfig_t *)FLASH_SAVE_ADDR;
    
    if (flash_ptr->magic_word == CONFIG_MAGIC) {
        memcpy(&g_job_config, flash_ptr, sizeof(JobConfig_t));
    } else {
        // 第一次运行，设置默认参数
        g_job_config.magic_word = CONFIG_MAGIC;
        g_job_config.trigger_mode = TRIGGER_MODE_NONE;
        g_job_config.params.depth_value = 1.0f;
        g_job_config.params.delay_time_ms = 1000;
        // 顺便保存一次初始值
        Config_Save();
    }
}

void Config_Save(void) {
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error;

    HAL_FLASH_Unlock(); // 解锁 Flash

    // 擦除配置所在的页
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = FLASH_SAVE_ADDR;
    erase_init.NbPages = 1;
    
    if (HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK) {
        // 擦除失败处理
        HAL_FLASH_Lock();
        return;
    }

    // 按字（32位）写入结构体
    uint32_t *data_ptr = (uint32_t *)&g_job_config;
    for (uint32_t i = 0; i < (sizeof(JobConfig_t) + 3) / 4; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_SAVE_ADDR + (i * 4), data_ptr[i]);
    }

    HAL_FLASH_Lock(); // 锁定 Flash
}

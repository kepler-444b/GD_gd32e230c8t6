#ifndef _FLASH_H_
#define _FLASH_H_
#include <stdbool.h>
#include "../gpio/gpio.h"
#include "../device/device_manager.h"

#define CONFIG_START_ADDR 0x08008000U // 默认串码存储的地址
#define CONFIG_EXTEN_ADDR (CONFIG_START_ADDR + 1024U)
#define FLASH_PAGE_SIZE   0x400U // 1024 字节 1KB

fmc_state_enum app_flash_write_word(uint32_t address, uint32_t *data, uint32_t length);
fmc_state_enum app_flash_program(uint32_t address, uint32_t *data, uint32_t length, bool erase_first); // 带自动擦除
fmc_state_enum app_flash_read(uint32_t address, uint32_t *data, uint32_t length);

// 擦除页flash
fmc_state_enum app_flash_erase_page(uint32_t page_address);

// 擦除整片flash
fmc_state_enum app_flash_mass_erase(void);

#endif
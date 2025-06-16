#ifndef _FLASH_H_
#define _FLASH_H_
#include <stdbool.h>
#include "../gpio/gpio.h"
#include "../device/device_manager.h"

/*
    2025.5.18 舒东升
    本模块为flash读写功能,
    由于存储的数据较少,读写也不频繁,故而flash空间并不紧张,
    所以存放串码用整页,固定地址,暂不考虑均匀磨损
*/
#define CONFIG_START_ADDR 0x08008000U                 // 默认串码存储的地址
#define CONFIG_EXTEN_ADDR (CONFIG_START_ADDR + 1024U) // 扩展串码存储地址
#define FLASH_PAGE_SIZE   0x400U                      // 1024 字节 1KB

// 以字(32位)为单位写入Flash
fmc_state_enum app_flash_write_word(uint32_t address, uint32_t *data, uint32_t length);
// 以字(32位)为单位写入Flash(带自动擦除选项)
fmc_state_enum app_flash_program(uint32_t address, uint32_t *data, uint32_t length, bool erase_first);
// 以字(32为)为单位读取Flash
fmc_state_enum app_flash_read(uint32_t address, uint32_t *data, uint32_t length);
// 擦除页flash
fmc_state_enum app_flash_erase_page(uint32_t page_address);
// 擦除整片flash
fmc_state_enum app_flash_mass_erase(void);

#endif
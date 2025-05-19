#ifndef _FLASH_H_
#define _FLASH_H_

// 初始化 flash 存储器
fmc_state_enum app_flash_init(void); 

// 写 flash
fmc_state_enum app_flash_write(uint32_t offset, const uint32_t *data, uint32_t size);

// 读 flash
fmc_state_enum app_flash_read(uint32_t offset, uint32_t *buffer, uint32_t size);

// 擦除用户数据的 32 KB flash
fmc_state_enum app_flash_erase_all(void);

// 擦除用户数据的指定范围
static fmc_state_enum app_flash_erase_page_range(uint32_t start_addr, uint32_t end_addr);


#endif
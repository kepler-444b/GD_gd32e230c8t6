#include "gd32e23x.h"
#include "gd32e23x_fmc.h"
#include <stdio.h>
#include <string.h>
#include "flash.h"
#include "../base/debug.h"


#define USER_DATA_START_ADDR        0x08004000U
#define USER_DATA_END_ADDR          0x08004800U
#define USER_DATA_SIZE              (USER_DATA_END_ADDR - USER_DATA_START_ADDR)  // 用户数据区大小 32KB
#define FLASH_PAGE_SIZE             0x400U       // 1024 字节 1KB

static uint32_t app_get_page_address(uint32_t addr);
static bool app_flash_need_erase(uint32_t addr, uint32_t value);
fmc_state_enum app_flash_init(void)
{
    // 检查是否已经解锁
    if(FMC_CTL & FMC_CTL_LK) 
    {
        fmc_unlock();
    }
    
    fmc_prefetch_enable();
    
    // 根据时钟频率设置等待周期
    if(SystemCoreClock > 24000000) 
    {
        fmc_wscnt_set(WS_WSCNT_2);  // 主频>24MHz时使用2等待周期
    } 
    else 
    {
        fmc_wscnt_set(WS_WSCNT_1);  // 默认1等待周期
    }
    
    return fmc_flag_get(FMC_FLAG_BUSY) ? FMC_BUSY : FMC_READY;
}

// 向指定的 Flash 地址写入数据
fmc_state_enum app_flash_write(uint32_t offset, const uint32_t *data, uint32_t size)
{
    if(data == NULL) return FMC_PGERR;
    if((offset % 4) != 0 || (size % 4) != 0) return FMC_PGAERR;
    if((offset + size) > USER_DATA_SIZE) return FMC_PGAERR;
    
    uint32_t addr = USER_DATA_START_ADDR + offset;
    fmc_state_enum ret = FMC_READY;
    
    if(fmc_flag_get(FMC_FLAG_WPERR)) 
    {
        return FMC_WPERR;
    }
    
    fmc_unlock();
    
    // 首先检查哪些页需要擦除
    uint32_t first_page = app_get_page_address(addr);
    uint32_t last_page = app_get_page_address(addr + size - 1);
    
    // 检查范围内的所有页是否需要擦除
    bool need_erase = false;
    for(uint32_t page = first_page; page <= last_page; page += FLASH_PAGE_SIZE) 
    {
        // 检查页内是否有需要擦除的数据
        for(uint32_t i = 0; i < (size / 4); i++) 
        {
            uint32_t current_addr = addr + i*4;
            if(current_addr >= page && current_addr < (page + FLASH_PAGE_SIZE)) 
            {
                if(app_flash_need_erase(current_addr, data[i])) 
                {
                    need_erase = true;
                    break;
                }
            }
        }
        if(need_erase) break;
    }
    
    // 如果需要擦除，执行擦除操作
    if(need_erase) 
    {
        for(uint32_t page = first_page; page <= last_page; page += FLASH_PAGE_SIZE) 
        {
            ret = fmc_page_erase(page);
            if(ret != FMC_READY) 
            {
                fmc_lock();
                return ret;
            }
        }
    }
    
    // 执行写入操作
    for(uint32_t i = 0; i < (size / 4); i++) 
    {
        ret = fmc_word_program(addr, data[i]);
        if(ret != FMC_READY) break;
        addr += 4;
    }
    
    fmc_lock();
    return ret;
}

// 从指定的 Flash 地址偏移处读取指定大小的数据到缓冲区中
fmc_state_enum app_flash_read(uint32_t offset, uint32_t *buffer, uint32_t size)
{
    if(buffer == NULL) return FMC_PGERR;
    if((offset + size) > USER_DATA_SIZE) 
    {
        return FMC_PGAERR;
    }

    uint32_t addr = USER_DATA_START_ADDR + offset;
    
    for(uint32_t i = 0; i < (size / 4); i++) 
    {
        buffer[i] = *(__IO uint32_t*)addr;
        addr += 4;
    }
    
    return FMC_READY;
}

// 擦除指定地址范围内的 Flash 页
static fmc_state_enum app_flash_erase_page_range(uint32_t start_addr, uint32_t end_addr)
{
    if((start_addr % FLASH_PAGE_SIZE) != 0 ||  ((end_addr + 1) % FLASH_PAGE_SIZE) != 0) 
    {
        return FMC_PGAERR;
    }
    
    fmc_state_enum status = FMC_READY;
    fmc_unlock();
    
    for(uint32_t addr = start_addr; addr <= end_addr; addr += FLASH_PAGE_SIZE) 
    {
        status = fmc_page_erase(addr);
        if(status != FMC_READY) break;
    }
    
    fmc_lock();
    return status;
}

// 擦除用户数据区的所有Flash页面
fmc_state_enum app_flash_erase_all(void)
{
    return app_flash_erase_page_range(
        app_get_page_address(USER_DATA_START_ADDR),
        app_get_page_address(USER_DATA_END_ADDR - 1));
}

// 获取指定地址 addr 所在的 Flash 页的起始地址
static uint32_t app_get_page_address(uint32_t addr)
{
    return (addr & ~(FLASH_PAGE_SIZE - 1));
}

// 检查是否需要擦除的辅助函数
static bool app_flash_need_erase(uint32_t addr, uint32_t value)
{
    uint32_t current_value = *(__IO uint32_t*)addr;
    // 如果当前值不是0xFFFFFFFF且与新值不同，则需要擦除
    return (current_value != 0xFFFFFFFF) && (current_value != value);
}

#include "gd32e23x.h"
#include "gd32e23x_fmc.h"
#include <stdio.h>
#include <string.h>
#include "flash.h"
#include "../base/debug.h"
#include "../base/base.h"

fmc_state_enum app_flash_read(uint32_t address, uint32_t *data, uint32_t length)
{
    fmc_state_enum state = FMC_READY;

    if ((address % 4) || (length % 4) || !data) {
        return FMC_PGAERR; // 返回编程对齐错误
    }
    fmc_unlock(); // 解锁Flash操作

    state = fmc_ready_wait(FMC_TIMEOUT_COUNT); // 等待Flash就绪
    if (state != FMC_READY) {
        fmc_lock(); // 如果超时则重新上锁
        return state;
    }

    for (uint32_t i = 0; i < length / 4; i++) { // 逐字读取数据
        data[i] = REG32(address + (i * 4));     // 使用寄存器访问宏读取
    }
    fmc_lock(); // 重新锁定Flash

    return FMC_READY;
}

fmc_state_enum app_flash_write_word(uint32_t address, uint32_t *data, uint32_t length)
{
    fmc_state_enum state = FMC_READY;

    if ((address % 4) || (length % 4) || !data) {
        return FMC_PGAERR;
    }

    fmc_unlock(); // 解锁Flash操作

    for (uint32_t i = 0; i < length / 4; i++) { // 逐字编程
        state = fmc_word_program(address + (i * 4), data[i]);
        if (state != FMC_READY) {
            fmc_lock(); // 出错时重新上锁
            return state;
        }
    }

    fmc_lock(); // 重新锁定Flash

    return FMC_READY;
}

// 以双字(64位)为单位写入Flash
fmc_state_enum app_flash_write_doubleword(uint32_t address, uint64_t *data, uint32_t length)
{
    fmc_state_enum state = FMC_READY;

    if ((address % 4) || (length % 4) || !data) {
        return FMC_PGAERR;
    }

    fmc_unlock(); // 解锁Flash操作

    for (uint32_t i = 0; i < length / 8; i++) { // 逐双字编程
        state = fmc_doubleword_program(address + (i * 8), data[i]);
        if (state != FMC_READY) {
            fmc_lock(); // 出错时重新上锁
            return state;
        }
    }

    fmc_lock(); // 重新锁定Flash

    return FMC_READY;
}

fmc_state_enum app_flash_erase_page(uint32_t page_address)
{
    fmc_state_enum state;

    fmc_unlock(); // 解锁Flash操作
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);

    state = fmc_page_erase(page_address); // 执行页擦除

    fmc_lock(); // 重新锁定Flash

    return state;
}

fmc_state_enum app_flash_mass_erase(void)
{
    fmc_state_enum state;

    fmc_unlock(); // 解锁Flash操作

    state = fmc_mass_erase(); // 执行整片擦除

    fmc_lock(); // 重新锁定Flash

    return state;
}

/*!
    \brief      验证Flash内容是否与预期数据匹配
    \param[in]  address: 验证起始地址
    \param[in]  data: 预期数据指针
    \param[in]  length: 要验证的字节数
    \retval     fmc_state: 验证状态(FMC_READY表示匹配，FMC_PGERR表示不匹配)
*/
fmc_state_enum app_flash_verify(uint32_t address, uint32_t *data, uint32_t length)
{
    fmc_state_enum state = FMC_READY;
    uint32_t read_data;

    if ((address % 4) || (length % 4) || !data) {
        return FMC_PGAERR;
    }

    for (uint32_t i = 0; i < length / 4; i++) { // 逐字读取并比较
        read_data = REG32(address + (i * 4));
        if (read_data != data[i]) {
            state = FMC_PGERR; // 数据不匹配
            break;
        }
    }

    return state;
}

fmc_state_enum app_flash_program(uint32_t address, uint32_t *data, uint32_t length, bool erase_first)
{
    fmc_state_enum state;

    if (erase_first) { // 如果需要先擦除
        uint32_t page_address = address & ~(FLASH_PAGE_SIZE - 1);

        state = app_flash_erase_page(page_address); // 执行页擦除
        if (state != FMC_READY) {
            return state; // 擦除失败直接返回
        }
    }
    return app_flash_write_word(address, data, length); // 执行数据写入
}

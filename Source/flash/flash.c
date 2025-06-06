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

    /* 参数有效性检查 */
    if ((address % 4 != 0) || (length % 4 != 0) || (data == NULL)) {
        return FMC_PGAERR; // 返回编程对齐错误
    }

    /* 解锁Flash操作 */
    fmc_unlock();

    /* 等待Flash就绪 */
    state = fmc_ready_wait(FMC_TIMEOUT_COUNT);
    if (state != FMC_READY) {
        fmc_lock(); // 如果超时则重新上锁
        return state;
    }

    /* 逐字读取数据 */
    for (uint32_t i = 0; i < length / 4; i++) {
        data[i] = REG32(address + (i * 4)); // 使用寄存器访问宏读取
    }

    /* 重新锁定Flash */
    fmc_lock();

    return FMC_READY;
}

/*!
    \brief      以字(32位)为单位写入Flash
    \param[in]  address: 写入起始地址(必须4字节对齐)
    \param[in]  data: 要写入的数据指针
    \param[in]  length: 要写入的字节数(必须是4的倍数)
    \retval     fmc_state: 操作状态
*/
fmc_state_enum app_flash_write_word(uint32_t address, uint32_t *data, uint32_t length)
{
    fmc_state_enum state = FMC_READY;

    /* 参数有效性检查 */
    if ((address % 4 != 0) || (length % 4 != 0) || (data == NULL)) {
        return FMC_PGAERR;
    }

    /* 解锁Flash操作 */
    fmc_unlock();

    /* 逐字编程 */
    for (uint32_t i = 0; i < length / 4; i++) {
        state = fmc_word_program(address + (i * 4), data[i]);
        if (state != FMC_READY) {
            fmc_lock(); // 出错时重新上锁
            return state;
        }
    }

    /* 重新锁定Flash */
    fmc_lock();

    return FMC_READY;
}

/*!
    \brief      以双字(64位)为单位写入Flash
    \param[in]  address: 写入起始地址(必须8字节对齐)
    \param[in]  data: 要写入的数据指针(作为64位值处理)
    \param[in]  length: 要写入的字节数(必须是8的倍数)
    \retval     fmc_state: 操作状态
*/
fmc_state_enum app_flash_write_doubleword(uint32_t address, uint64_t *data, uint32_t length)
{
    fmc_state_enum state = FMC_READY;

    /* 参数有效性检查 */
    if ((address % 8 != 0) || (length % 8 != 0) || (data == NULL)) {
        return FMC_PGAERR;
    }

    /* 解锁Flash操作 */
    fmc_unlock();

    /* 逐双字编程 */
    for (uint32_t i = 0; i < length / 8; i++) {
        state = fmc_doubleword_program(address + (i * 8), data[i]);
        if (state != FMC_READY) {
            fmc_lock(); // 出错时重新上锁
            return state;
        }
    }

    /* 重新锁定Flash */
    fmc_lock();

    return FMC_READY;
}

/*!
    \brief      擦除Flash页
    \param[in]  page_address: 要擦除页的起始地址
    \retval     fmc_state: 操作状态
*/
fmc_state_enum app_flash_erase_page(uint32_t page_address)
{
    fmc_state_enum state;

    /* 解锁Flash操作 */
    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
    /* 执行页擦除 */
    state = fmc_page_erase(page_address);

    /* 重新锁定Flash */
    fmc_lock();

    return state;
}

/*!
    \brief      整片擦除Flash存储器
    \retval     fmc_state: 操作状态
*/
fmc_state_enum app_flash_mass_erase(void)
{
    fmc_state_enum state;

    /* 解锁Flash操作 */
    fmc_unlock();

    /* 执行整片擦除 */
    state = fmc_mass_erase();

    /* 重新锁定Flash */
    fmc_lock();

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

    /* 参数有效性检查 */
    if ((address % 4 != 0) || (length % 4 != 0) || (data == NULL)) {
        return FMC_PGAERR;
    }

    /* 逐字读取并比较 */
    for (uint32_t i = 0; i < length / 4; i++) {
        read_data = REG32(address + (i * 4));
        if (read_data != data[i]) {
            state = FMC_PGERR; // 数据不匹配
            break;
        }
    }

    return state;
}

/*!
    \brief      Flash编程函数(带自动擦除选项)
    \param[in]  address: 写入起始地址(必须4字节对齐)
    \param[in]  data: 要写入的数据指针
    \param[in]  length: 要写入的字节数(必须是4的倍数)
    \param[in]  erase_first: 是否先执行擦除操作
    \retval     fmc_state: 操作状态
*/
fmc_state_enum app_flash_program(uint32_t address, uint32_t *data, uint32_t length, bool erase_first)
{
    fmc_state_enum state;

    /* 如果需要先擦除 */
    if (erase_first) {
        /* 计算页地址(假设已知FLASH_PAGE_SIZE) */
        uint32_t page_address = address & ~(FLASH_PAGE_SIZE - 1);

        /* 执行页擦除 */
        state = app_flash_erase_page(page_address);
        if (state != FMC_READY) {
            return state; // 擦除失败直接返回
        }
    }

    /* 执行数据写入 */
    return app_flash_write_word(address, data, length);
}


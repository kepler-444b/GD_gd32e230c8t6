#ifndef _BASE_H_
#define _BASE_H_
#include "../gpio/gpio.h"
#include <stdio.h>
/*
    2025.5.16 舒东升
    本模块用于各种通用接口
*/

// 一个简单的延时函数,避免使用systick的中断延时函数delay_1ms()
#define APP_DELAY_1MS()                                  \
    do {                                                 \
        for (volatile uint32_t i = 0; i < (1000); i++) { \
            __NOP(); /* 插入空指令防止编译器优化 */      \
        }                                                \
    } while (0)

// 将连续的2个十六进制字符转为1个字节的 uint8_t 值(在协议解析时用到)
#define HEX_TO_BYTE(ptr)                                             \
    (((*(ptr) >= 'A') ? (*(ptr) - 'A' + 10) : (*(ptr) - '0')) << 4 | \
     ((*(ptr + 1) >= 'A') ? (*(ptr + 1) - 'A' + 10) : (*(ptr + 1) - '0')))

/// @brief 用于求一组数据的平均数
/// @param buffer 传入数组
/// @param count  数组长度
/// @return
uint16_t app_calculate_average(const uint16_t *buffer, uint16_t count);

/**
 * @brief 将 uint8_t 数组打包为 uint32_t 数组(小端序)
 * @param input         输入数组(uint8_t* 类型)
 * @param input_count   输入数组的 uint8_t 元素个数
 * @param output        输出数组(uint32_t* 类型)
 * @param output_count  输出数组的 uint32_t 元素个数(需 >= (input_count + 3 / 4) 向上取整)
 * @return true  成功
 * @return false 失败(参数无效或空间不足)
 */
bool app_uint8_to_uint32(const uint8_t *input, size_t input_count, uint32_t *output, size_t output_count);

/**
 * @brief 将 uint32_t 数组解包为 uint8_t 数组(小端序)
 * @param input         输入数组(uint32_t* 类型)
 * @param input_count   输入数组的 uint32_t 元素个数
 * @param output        输出数组(uint8_t* 类型)
 * @param output_count  输出数组的 uint8_t 元素个数(需 >= input_count * 4)
 * @return true  成功
 * @return false 失败(参数无效或空间不足)
 */
bool app_uint32_to_uint8(const uint32_t *input, size_t input_count, uint8_t *output, size_t output_count);

#endif
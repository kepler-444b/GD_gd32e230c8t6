#ifndef _BASE_H_
#define _BASE_H_
#include "../gpio/gpio.h"
#include <stdio.h>
/*
    2025.5.16 舒东升
    本模块用于各种通用接口
*/

#define BIT0(flag) ((bool)((flag) & 0x01)) // 第0位
#define BIT1(flag) ((bool)((flag) & 0x02)) // 第1位
#define BIT2(flag) ((bool)((flag) & 0x04)) // 第2位
#define BIT3(flag) ((bool)((flag) & 0x08)) // 第3位
#define BIT4(flag) ((bool)((flag) & 0x10)) // 第4位
#define BIT5(flag) ((bool)((flag) & 0x20)) // 第5位
#define BIT6(flag) ((bool)((flag) & 0x40)) // 第6位
#define BIT7(flag) ((bool)((flag) & 0x80)) // 第7位

// 一个简单的非阻塞延时,在与plc模块通信时使用
#define APP_DELAY                                        \
    do {                                                 \
        for (volatile uint32_t i = 0; i < (1000); i++) { \
            __NOP(); /* 插入空指令防止编译器优化 */      \
        }                                                \
    } while (0)

// 将连续的2个十六进制字符转为1个字节的 uint8_t 值(在协议解析时用到)
#define HEX_TO_BYTE(ptr)                                             \
    (((*(ptr) >= 'A') ? (*(ptr) - 'A' + 10) : (*(ptr) - '0')) << 4 | \
     ((*(ptr + 1) >= 'A') ? (*(ptr + 1) - 'A' + 10) : (*(ptr + 1) - '0')))

uint16_t app_calculate_average(const uint16_t *buffer, uint16_t count);

// 将 uint8_t 数组打包为 uint32_t 数组(小端序)
bool app_uint8_to_uint32(const uint8_t *input, size_t input_count, uint32_t *output, size_t output_count);

// 将 uint32_t 数组解包为 uint8_t 数组(小端序)
bool app_uint32_to_uint8(const uint32_t *input, size_t input_count, uint8_t *output, size_t output_count);

#endif
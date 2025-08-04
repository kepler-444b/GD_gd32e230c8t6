#ifndef _BASE_H_
#define _BASE_H_
#include "../gpio/gpio.h"
#include <stdio.h>
/*
    2025.5.16 舒东升
    本模块用于各种通用接口
*/

#define BIT0(flag)  ((bool)((flag) & 0x01)) // 第0位
#define BIT1(flag)  ((bool)((flag) & 0x02)) // 第1位
#define BIT2(flag)  ((bool)((flag) & 0x04)) // 第2位
#define BIT3(flag)  ((bool)((flag) & 0x08)) // 第3位
#define BIT4(flag)  ((bool)((flag) & 0x10)) // 第4位
#define BIT5(flag)  ((bool)((flag) & 0x20)) // 第5位
#define BIT6(flag)  ((bool)((flag) & 0x40)) // 第6位
#define BIT7(flag)  ((bool)((flag) & 0x80)) // 第7位

#define L_BIT(byte) ((uint8_t)((byte) & 0x0F))        // 低4位
#define H_BIT(byte) ((uint8_t)(((byte) >> 4) & 0x0F)) // 高4位

// 一个简单的非阻塞延时,在与plc模块通信时使用
#if 0
    #define APP_DELAY                                        \
        do {                                                 \
            for (volatile uint32_t i = 0; i < (1000); i++) { \
                __NOP(); /* 插入空指令防止编译器优化 */      \
            }                                                \
        } while (0)
#endif

// 将连续的2个十六进制字符转为1个字节的 uint8_t 值(在协议解析时用到)
#if defined PLC_HI
static const uint8_t hex_lut[256] = {['0'] = 0, ['1'] = 1, ['2'] = 2, ['3'] = 3, ['4'] = 4, ['5'] = 5, ['6'] = 6, ['7'] = 7, ['8'] = 8, ['9'] = 9, ['A'] = 10, ['B'] = 11, ['C'] = 12, ['D'] = 13, ['E'] = 14, ['F'] = 15, ['a'] = 10, ['b'] = 11, ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15};
    #define HEX_TO_BYTE(ptr) ((hex_lut[*(ptr)] << 4) | hex_lut[*(ptr + 1)])
#endif

#if defined PLC_LHW
    //  将十六进制转为小写的asci
    #define HEX_TO_ASCII(mac, ascii)                                         \
        do {                                                                 \
            for (int i = 0; i < 6; i++) {                                    \
                ascii[2 * i]     = "0123456789abcdef"[(mac[i] >> 4) & 0x0F]; \
                ascii[2 * i + 1] = "0123456789abcdef"[mac[i] & 0x0F];        \
            }                                                                \
        } while (0)

    // 查找第二个 0xFF 力合微模组通信用到
    #define FIND_SECOND_FF(buffer, length, index_var)   \
        do {                                            \
            uint8_t _found_ff_count = 0;                \
            (index_var)             = 0;                \
            for (uint8_t _i = 0; _i < (length); _i++) { \
                if ((buffer)[_i] == 0xFF) {             \
                    _found_ff_count++;                  \
                    if (_found_ff_count == 2) {         \
                        (index_var) = _i + 1;           \
                        break;                          \
                    }                                   \
                }                                       \
            }                                           \
        } while (0)
#endif
uint16_t app_calculate_average(const uint16_t *buffer, uint16_t count);

// 将 uint8_t 数组打包为 uint32_t 数组(小端序)
bool app_uint8_to_uint32(const uint8_t *input, size_t input_count, uint32_t *output, size_t output_count);

// 将 uint32_t 数组解包为 uint8_t 数组(小端序)
bool app_uint32_to_uint8(const uint32_t *input, size_t input_count, uint8_t *output, size_t output_count);

#endif
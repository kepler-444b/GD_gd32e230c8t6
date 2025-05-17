#ifndef _DEBUG_H_
#define _DEBUG_H_ 
#include <stdio.h>
#include <stdbool.h>

/*
    2025.5.14 舒东升
    为了节省芯片资源，暂用通讯的 UART0 进行调试
    在开发阶段开启 APP_DEBUG 宏定义，正式发行版本关闭此宏
    (在整整个工程中用  APP_PRINTF 宏来打印调试信息)
 */

#define APP_DEBUG       // 此宏用来管理整个工程的 debug 信息

#if defined APP_DEBUG
#define APP_PRINTF(...)  printf(__VA_ARGS__)
#define APP_PRINTF_BUF(name, buf, len) do { \
        APP_PRINTF("%s: ", (name)); \
        for (size_t i = 0; i < (len); i++) { \
            APP_PRINTF("%02X ", ((const uint8_t *)(buf))[i]); \
        } \
        APP_PRINTF("\n"); \
    } while (0)
#else
#define APP_PRINTF(...)
#define APP_PRINTF_BUF(name, buf, len)

#endif

#endif
#ifndef _WATCHDOG_H_
#define _WATCHDOG_H_

/*
    2025.5.30 舒东升
    初始化看门狗程序
    (看门狗定时器每3s溢出一次,每1s喂狗一次)
*/
void app_watchdog_init(void);

#endif
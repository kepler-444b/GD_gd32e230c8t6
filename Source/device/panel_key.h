#ifndef _PANEL_KEY_H_
#define _PANEL_KEY_H_

/*
2025.6.26 舒东升

灯控面板程序,支持4,6,8键,可在 device_manager.h 中修改宏定义以切换
8键比较特殊,是用2个4键组成,故而配置信息与adc采集以及处理逻辑都是用了2个4键完成
工程中所有 is_ex 字样都是用于8键面板的扩展
*/
void panel_key_init(void);

#endif
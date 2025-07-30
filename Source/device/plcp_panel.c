#include "plcp_panel.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "timers.h"
#include "plcp_panel_api.h"
#include "MseProcess.h"

#include "../protocol/protocol.h"
#include "../adc/adc.h"
#include "../device/device_manager.h"
#include "../base/base.h"
#include "../config/config.h"
#include "../device/pcb_device.h"
#include "../eventbus/eventbus.h"
#include "../usart/usart.h"
#include "../pwm/pwm.h"
#include "../zero/zero.h"

#define ADC_TO_VOL(adc_val) ((adc_val) * 330 / 4096) // adc值转电压
#define ADC_VOL_NUMBER      10                       // 电压值缓冲区数量
#define MIN_VOL             329                      // 无按键按下时的最小电压值
#define MAX_VOL             330                      // 无按键按下时的最大电压值

typedef struct {
    // 用于处理adc数据
    uint8_t buf_idx; // 缓冲区下标
    uint16_t vol;    // 电压值
    uint16_t vol_buf[ADC_VOL_NUMBER];
} adc_value_t;

typedef struct {
    uint16_t min;
    uint16_t max;
} key_vol_t;

typedef struct { // 用于每个按键的状态

    bool key_press;            // 按键按下
    bool led_w_open;           // 白灯状态
    bool led_w_last;           // 白灯上次状态
    const key_vol_t vol_range; // 按键电压范围

} panel_status_t;

static adc_value_t my_adc_value;
static panel_status_t my_panel_status[KEY_NUMBER] = {
    PANEL_VOL_RANGE_DEF,
};

uint8_t button_num_max_set = 0xff;
uint8_t relay_num_max_set  = 0xff;
uint8_t button_num_max_get = 0xff;
uint8_t relay_num_max_get  = 0xff;
uint8_t sycn_flag;
uint16_t local_did;

static bool inin_timer     = false;
static uint16_t init_count = 0;
static bool sync_timer     = false;
static uint16_t sync_conut = 0;

static bool bl_blink           = false; // 背光灯开始闪烁
static bool bl_blink_status    = false; // 背光灯闪烁状态
static uint32_t bl_blink_count = 0;     // 背光灯闪烁计数

static uint8_t key_press_number       = 0; // 按键按下次数
static uint16_t key_press_clear_count = 0; // 按键按下次数超时清理
static uint32_t key_press_long_count  = 0; // 按键长按计数

static bool mcu_rest = false; // 单片机重启

static uint32_t sync_timer_counter  = 0;
static uint32_t sync_timer_interval = 3000; // 初始3秒

static void plcp_process_key(uint8_t key_number, panel_status_t *temp_status);
static void plcp_panel_timer(TimerHandle_t xTimer);
static void plcp_panel_event_handler(event_type_e event, void *params);
static void plcp_process_panel_adc(panel_status_t *temp_status, adc_value_t *adc_value);
static void init_timer_handler(void);
static void plcp_panel_bl_blink(bool status, bool breathe);
static uint16_t plcp_panel_get_did(void);

void CmdTest_MSE_GET_DID(void);
void CmdTest_MSE_Factory(void);
void CmdTest_MSE_RST(void);
void cmd_switch_init(uint8_t button_mun, uint8_t relay_mun);
void cmd_switch_get_init_info(void);

void plcp_panel_key_init(void)
{
    panel_gpio_init();
    app_plcp_map();
    adc_channel_t my_adc_channel = {0};
    my_adc_channel.adc_channel_0 = true;
    app_adc_init(&my_adc_channel);
    app_zero_init(EXTI_11);
    app_pwm_init(); // 初始化 PWM 模块
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        app_pwm_add_pin(app_get_panel_cfg()[i].led_y_pin);
    };
    app_eventbus_subscribe(plcp_panel_event_handler); // 订阅事件总线
    static StaticTimer_t ReadAdcStaticBuffer;
    static TimerHandle_t ReadAdcTimerHandle = NULL;

    ReadAdcTimerHandle = xTimerCreateStatic("ReadAdcTimer", 1, true, NULL, plcp_panel_timer, &ReadAdcStaticBuffer);

    if (xTimerStart(ReadAdcTimerHandle, 0) != true) {
        // 启动定时器(0表示不阻塞)
        APP_ERROR("timer_start error");
    }

    button_num_max_set = KEY_NUMBER;
    // relay_num_max_set  = RELAY_NUMBER;
    relay_num_max_set = 4;

    local_did  = 0x0000;
    inin_timer = true;
    bl_blink   = true;
}

static void plcp_panel_timer(TimerHandle_t xTimer)
{
    my_adc_value.vol = ADC_TO_VOL(app_get_adc_value()[0]);
    plcp_process_panel_adc(my_panel_status, &my_adc_value);

    if (inin_timer) {
        init_count++;
        if (init_count >= 1000) {
            app_eventbus_publish(EVENT_PLCP_INIT_TIMER, NULL);
            init_count = 0;
        }
    }
    if (!sycn_flag) {
        if (++sync_timer_counter >= sync_timer_interval) {
            sync_timer_counter = 0; // 重置计数器
            APP_PRINTF("sycn_flag\n");
            app_eventbus_publish(EVENT_PLCP_SYCN_FLAG, NULL);
            sync_timer_interval += 3000; // 增加时间间隔(每次增加3秒)
        }
    }
    if (bl_blink) {
        bl_blink_count++;
        if (bl_blink_count % 500 == 0) {
            bl_blink_status = !bl_blink_status;
            APP_PRINTF("key_press_long_count:%d\n", key_press_long_count);
            if (key_press_long_count == 0) {
                plcp_panel_bl_blink(bl_blink_status, false);
            }
        }
    }
}

static void plcp_process_panel_adc(panel_status_t *temp_status, adc_value_t *adc_value)
{
    // APP_PRINTF("%d\n", adc_value->vol);
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_status_t *p_status = &temp_status[i];
        if (adc_value->vol < p_status->vol_range.min || adc_value->vol > p_status->vol_range.max) {
            if (adc_value->vol >= MIN_VOL && adc_value->vol <= MAX_VOL) {
                p_status->key_press = false;
            }
            continue;
        }
        // 填充 vol_buf
        adc_value->vol_buf[adc_value->buf_idx++] = adc_value->vol;
        if (adc_value->buf_idx < ADC_VOL_NUMBER) {
            continue;
        }
        adc_value->buf_idx = 0; // vol_buf 被填充满(下标置0)
        uint16_t new_value = app_calculate_average(adc_value->vol_buf, ADC_VOL_NUMBER);
        if (new_value < p_status->vol_range.min || new_value > p_status->vol_range.max) {
            continue; // 检查平均值是否在有效范围
        }
        if (!p_status->key_press) { // 处理按键按下
            plcp_process_key(i, &my_panel_status[i]);
            if (i == 0) { // 按键1被按下
                key_press_number++;
                key_press_clear_count = 0;
                APP_PRINTF("key_press_number:%d\n", key_press_number);
            }
            p_status->key_press = true;
            continue;
        }
    }

    const key_vol_t *p_range = &temp_status[0].vol_range; // 按键1的电压范围
    bool temp_key_status     = false;                     // 存储按键1的状态
    if (adc_value->vol >= p_range->min && adc_value->vol <= p_range->max) {
        temp_key_status = true;
    }
    if (key_press_number != 0) {
        if (key_press_number >= 3 && temp_key_status) {
            key_press_long_count++;
            if (key_press_long_count >= 6000 && key_press_long_count % 1000 == 0) { // 按下6s后开始呼吸闪烁
                bl_blink_status = !bl_blink_status;
                plcp_panel_bl_blink(bl_blink_status, true);
            }
            if (key_press_long_count >= 8000) {
                key_press_long_count = 0;
                mcu_rest             = true;
            }
        }
        if (!temp_key_status) {
            key_press_clear_count++;
            if (key_press_clear_count >= 500) {
                key_press_long_count  = 0;
                key_press_clear_count = 0;
                key_press_number      = 0;
            }
        }
    }
    if (mcu_rest && !temp_key_status) {
        CmdTest_MSE_Factory();
        vTaskDelay(5);
        NVIC_SystemReset();
    }
}

static void plcp_panel_bl_blink(bool status, bool breathe)
{
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    if (breathe) {
        for (uint8_t i = 0; i < KEY_NUMBER; i++) {
            app_set_pwm_fade(temp_cfg[i].led_y_pin, status ? 500 : 0, 500);
        }
    } else {
        if (plcp_panel_get_did() == 0x0000) {
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                app_set_pwm_fade(temp_cfg[i].led_y_pin, status ? 500 : 0, 50);
            }

        } else {
            APP_PRINTF("get_did_succ\n");
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                app_set_pwm_fade(temp_cfg[i].led_y_pin, 500, 50);
            }
            bl_blink        = false;
            bl_blink_count  = 0;
            bl_blink_status = false;
        }
    }
}

void plcp_panel_event_notify(uint8_t button_id)
{
    uint8_t playload[128];

    memset(playload, 0, sizeof(playload));
    snprintf((char *)playload, sizeof(playload), "{\"button\":\"k%d\"}", button_id + 1);

    // strcpy((char*)playload, "{\"id\":\"_open\",\"name\":\"OPEN\",\"data\":\"1\",\"type\":\"switch\"}");
    // printf("%s\n", (char*)playload);

    APP_SendRSL("@SE200./0/_e", NULL, playload, strlen((char *)playload));
}

static void plcp_process_key(uint8_t key_number, panel_status_t *temp_status)
{
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    static uint8_t s_key_number;

    temp_status->led_w_open = !temp_status->led_w_open;
    if (temp_status->led_w_open != temp_status->led_w_last) {
        APP_SET_GPIO(temp_cfg[key_number].led_w_pin, true);
        vTaskDelay(20);
        APP_SET_GPIO(temp_cfg[key_number].led_w_pin, false);
        temp_status->led_w_last = temp_status->led_w_open;
        s_key_number            = key_number;
        APP_PRINTF("key:%d\n", s_key_number);
        app_eventbus_publish(EVENT_PLCP_NOTIFY, &s_key_number);
        // plcp_panel_event_notify(key_number);
    }
}

static void plcp_panel_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_RECEIVE_CMD: {
            valid_data_t *rc_buf = (valid_data_t *)params;
            APP_UappsMsgProcessing(rc_buf->data, rc_buf->length);
        } break;
        case EVENT_PLCP_NOTIFY:
            plcp_panel_event_notify(*(uint8_t *)params);
            break;
        case EVENT_PLCP_INIT_TIMER:
            init_timer_handler();
            break;
        case EVENT_PLCP_SYCN_FLAG:
            CmdTest_MSE_RST();
            break;
        default:
            return;
    }
}

static void init_timer_handler(void)
{
    static uint8_t step = 0;
    switch (step) {
        case 0:
            CmdTest_MSE_GET_DID();
            step++;
            break;
        case 1:
            // APP_PRINTF("button_num_max_get:%d relay_num_max_get:%d\n", button_num_max_get, relay_num_max_get);
            // APP_PRINTF("button_num_max_set:%d relay_num_max_set:%d\n", button_num_max_set, relay_num_max_set);
            if (button_num_max_get == 0xff || relay_num_max_get == 0xff) {
                cmd_switch_get_init_info();
                break;
            }
            if (button_num_max_set != 0xff && relay_num_max_set != 0xff) {
                if (button_num_max_set != button_num_max_get || relay_num_max_set != relay_num_max_get) {
                    cmd_switch_init(button_num_max_set, relay_num_max_set);
                    // APP_PRINTF("button_num_max_set:%d  relay_num_max_set:%d\n", button_num_max_set, relay_num_max_set);
                    button_num_max_get = 0xff;
                    relay_num_max_get  = 0xff;
                    break;
                }
            }
            step = 0;
            break;
        default:
            step = 0;
            break;
    }
}

uint16_t plcp_panel_get_did(void)
{
    return local_did;
}

bool plcp_panel_set_status(char *aei, uint8_t *stateParam, uint16_t stateParamLen)
{
    APP_PRINTF_BUF("PLCP_RX", stateParam, stateParamLen);
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    uint8_t id;
    uint8_t index;
    index = 3;
    switch (stateParam[0]) {
        case 0x05: {
            index = 3;
            for (id = 0; id < KEY_NUMBER; id++) { // 设置背光灯
                if (stateParam[1] & (0x80 >> id)) {
                    app_set_pwm_fade(temp_cfg[id].led_y_pin, stateParam[index] ? 500 : 0, 500);
                    // APP_PRINTF("led_y_id:%d status:%d\n", id, stateParam[index]);
                    index++;
                }
            }
            for (id = 0; id < KEY_NUMBER; id++) { // 设置指示灯
                if (stateParam[1] & (0x80 >> id)) {
                    // APP_PRINTF("led_w_id:%d status:%d\n", id, stateParam[index]);
                    APP_SET_GPIO(temp_cfg[id].led_w_pin, stateParam[index]);
                    index++;
                }
            }
            sycn_flag = 1;
        } break;
        case 0x04: {
            for (id = 0; id < RELAY_NUMBER; id++) { // 设置继电器
                if (stateParam[1] & (0x80 >> id)) {
                    APP_SET_GPIO(temp_cfg[id].relay_pin[0], stateParam[index]);
                    zero_set_gpio(temp_cfg[id].relay_pin[0], stateParam[index]);
                    // APP_PRINTF("relay_id:%d status:%d\n", id, stateParam[index]);
                    index++;
                }
            }
            sycn_flag = 1;
        } break;
        default:
            break;
    }
    return true;
}
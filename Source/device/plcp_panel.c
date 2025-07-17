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

#define ADC_TO_VOL(adc_val) ((adc_val) * 330 / 4096) // adc值转电压
#define ADC_VOL_NUMBER      10                       // 电压值缓冲区数量
#define MIN_VOL             329                      // 无按键按下时的最小电压值
#define MAX_VOL             330                      // 无按键按下时的最大电压值

bool inin_timer     = false;
uint16_t init_count = 0;
bool sync_timer     = false;
uint16_t sync_conut = 0;

static uint32_t sync_timer_counter  = 0;
static uint32_t sync_timer_interval = 3000; // 初始3秒

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

    bool key_press;  // 按键按下
    bool key_status; // 按键状态

    bool led_w_open; // 白灯状态
    bool led_w_last; // 白灯上次状态

    bool led_y_open; // 黄灯状态
    bool led_y_last; // 黄灯上次状态

    bool relay_open; // 继电状态
    bool relay_last; // 继电器上次状态

    const key_vol_t vol_range; // 按键电压范围

} panel_status_t;

static adc_value_t my_adc_value;
static panel_status_t my_panel_status[KEY_NUMBER] = {
    PANEL_VOL_RANGE_DEF,
};

static void plcp_process_key(uint8_t key_number);
static void plcp_panel_read_adc(TimerHandle_t xTimer);
static void plcp_panel_event_handler(event_type_e event, void *params);
static void plcp_process_panel_adc(panel_status_t *temp_status, adc_value_t *adc_value);
static void init_timer_handler(void);

void CmdTest_MSE_GET_DID(void);
void CmdTest_MSE_Factory(void);
void CmdTest_MSE_RST(void);
void cmd_switch_init(uint8_t button_mun, uint8_t relay_mun);
void cmd_switch_get_init_info(void);
uint8_t button_num_max_set = 0xff;
uint8_t relay_num_max_set  = 0xff;

uint8_t button_num_max_get = 0xff;
uint8_t relay_num_max_get  = 0xff;
uint8_t sycn_flag;
uint16_t local_did;

void plcp_panel_key_init(void)
{

    panel_gpio_init();
    adc_channel_t my_adc_channel = {0};
    my_adc_channel.adc_channel_0 = true;
    app_adc_init(&my_adc_channel);

    app_pwm_init(); // 初始化 PWM 模块
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        app_pwm_add_pin(app_get_panel_cfg()[i].led_y_pin);
    };
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        app_set_pwm_fade(app_get_panel_cfg()[i].led_y_pin, 500, 3000);
    }

    static StaticTimer_t ReadAdcStaticBuffer;
    static TimerHandle_t ReadAdcTimerHandle = NULL;
    app_eventbus_subscribe(plcp_panel_event_handler); // 订阅事件总线
    ReadAdcTimerHandle = xTimerCreateStatic("ReadAdcTimer", 1, true, NULL, plcp_panel_read_adc, &ReadAdcStaticBuffer);

    if (xTimerStart(ReadAdcTimerHandle, 0) != true) {
        // 启动定时器(0表示不阻塞)
        APP_ERROR("timer_start error");
    }

    button_num_max_get = 4;
    relay_num_max_get  = 4;
    inin_timer         = true;

    button_num_max_set = KEY_NUMBER;
    relay_num_max_get  = RELAY_NUMBER;
}

static void plcp_panel_read_adc(TimerHandle_t xTimer)
{
    my_adc_value.vol = ADC_TO_VOL(app_get_adc_value()[0]);
    plcp_process_panel_adc(my_panel_status, &my_adc_value);
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
            p_status->key_status ^= 1;
            plcp_process_key(i);
            p_status->key_press = true;
            continue;
        }
    }
    if (inin_timer == true) {
        init_count++;
        if (init_count >= 1000) {
            init_timer_handler();
            init_count = 0;
        }
    }
    if (!sycn_flag) {
        if (++sync_timer_counter >= sync_timer_interval) {
            sync_timer_counter = 0; // 重置计数器
            APP_PRINTF("sycn_flag\n");
            CmdTest_MSE_RST(); // 执行复位命令
            // 增加时间间隔(每次增加3秒)
            sync_timer_interval += 3000;
        }
    }
}

void plcp_panel_event_notify(uint8_t button_id)
{
    static uint8_t playload[128];

    memset(playload, 0, sizeof(playload));
    snprintf((char *)playload, sizeof(playload), "{\"button\":\"k%d\"}", button_id + 1);

    // strcpy((char*)playload, "{\"id\":\"_open\",\"name\":\"OPEN\",\"data\":\"1\",\"type\":\"switch\"}");
    // printf("%s\n", (char*)playload);

    APP_SendRSL("@SE200./0/_e", NULL, playload, strlen((char *)playload));
}

static void plcp_process_key(uint8_t key_number)
{
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    APP_PRINTF("key:%d\n", key_number);
    my_panel_status[key_number].led_w_open = !my_panel_status[key_number].led_w_open;
    if (my_panel_status[key_number].led_w_open != my_panel_status[key_number].led_w_last) {
        my_panel_status[key_number].led_w_last = my_panel_status[key_number].led_w_open;

        plcp_panel_event_notify(key_number);
    }
}

static void plcp_panel_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_RECEIVE_CMD: {
            valid_data_t *rc_buf = (valid_data_t *)params;
            APP_UappsMsgProcessing(rc_buf->data, rc_buf->length);
        } break;
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
            if (button_num_max_get == 0xff || relay_num_max_get == 0xff) {
                cmd_switch_get_init_info();
                break;
            }
            if (button_num_max_set != 0xff && relay_num_max_set != 0xff) {
                if (button_num_max_set != button_num_max_get || relay_num_max_set != relay_num_max_get) {
                    cmd_switch_init(button_num_max_set, relay_num_max_set);
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

bool plcp_panel_set_status(char *aei, uint8_t *stateParam, uint16_t stateParamLen)
{
    APP_PRINTF_BUF("PLCP_RX", stateParam, stateParamLen);
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    uint8_t id;
    uint8_t index;

    switch (stateParam[0]) {
        case 0x05: {
            index = 7;
            for (id = 0; id < 8; id++) {            // 设置指示灯
                if (stateParam[1] & (0x80 >> id)) { // 前四个需要设置
                    // APP_PRINTF("id:%d  status:%d\n", id, stateParam[index]);
                    APP_SET_GPIO(temp_cfg[id].led_w_pin, stateParam[index]);
                    app_set_pwm_fade(temp_cfg[id].led_y_pin, stateParam[index] ? 0 : 500, 500);
                    index++;
                }
            }
        } break;
        case 0x04: {
            index = 3;
            for (id = 0; id < 4; id++) { // 设置继电器
                if (stateParam[1] & (0x80 >> id)) {
                    APP_SET_GPIO(temp_cfg[id].relay_pin[0], stateParam[index]);
                    APP_PRINTF("%d %s %d\n", id, app_get_gpio_name(temp_cfg[id].relay_pin[0]), stateParam[index]);
                    // printf("relay id %d, index %d\n", id, stateParam[index]);
                    // plcp_panel_realy_process(id, stateParam[index]);
                    index++;
                }
            }
        } break;
        default:
            break;
    }
    sycn_flag = 1;
    return true;
}
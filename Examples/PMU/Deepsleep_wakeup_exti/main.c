/*!
    \file    main.c
    \brief   deepsleep wakeup through exti interrupt
    
    \version 2025-02-10, V2.3.0, firmware for GD32E23x
*/

/*
    Copyright (c) 2025, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this 
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice, 
       this list of conditions and the following disclaimer in the documentation 
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors 
       may be used to endorse or promote products derived from this software without 
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
OF SUCH DAMAGE.
*/

#include "gd32e23x.h"
#include "gd32e230c_eval.h"
#include "systick.h"
#include "main.h"

void led_config(void);

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    /* systick configuration */
    systick_config();
    /* LED configuration */
    led_config();
    /* enable clock */
    rcu_periph_clock_enable(RCU_PMU);
    /* wakeup key configuration */
    gd_eval_key_init(KEY_WAKEUP,KEY_MODE_GPIO);
    /* tamper key EXTI configuration */
    gd_eval_key_init(KEY_TAMPER,KEY_MODE_EXTI);
    /* press wakeup key to enter deepsleep mode and use tamper key to generate an exti interrupt to wakeup mcu */
    while(1){
        if(SET == gpio_input_bit_get(WAKEUP_KEY_GPIO_PORT, WAKEUP_KEY_PIN)){
            pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, WFI_CMD);
        }
    }
}

/*!
    \brief      toggle the LED
    \param[in]  none
    \param[out] none
    \retval     none
*/
void led_spark(void)
{
    static __IO uint32_t timingdelaylocal = 400;

    if (timingdelaylocal != 0x00){
        /* all the LEDs on */
        if(timingdelaylocal < 200){
            gd_eval_led_on(LED2);
        } else {
            /* all the LEDs off */
            gd_eval_led_off(LED2);
        }
        timingdelaylocal--;
    } else {
        timingdelaylocal = 400;
    }
}

/*!
    \brief      LED configuration
    \param[in]  none
    \param[out] none
    \retval     none
*/
void led_config(void)
{
    gd_eval_led_init(LED2);
}

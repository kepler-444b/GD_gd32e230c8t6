/*!
    \file  system_gd32e23x.c
    \brief CMSIS Cortex-M23 Device Peripheral Access Layer Source File for
           GD32E23x Device Series
*/

/* Copyright (c) 2012 ARM LIMITED
   Copyright (c) 2025, GigaDevice Semiconductor Inc.

   All rights reserved.
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
   - Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
   - Neither the name of ARM nor the names of its contributors may be used
     to endorse or promote products derived from this software without
     specific prior written permission.
   *
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
   ---------------------------------------------------------------------------*/

/* This file refers the CMSIS standard, some adjustments are made according to GigaDevice chips */

#include "gd32e23x.h"

/* system frequency define */
#define __IRC8M           (IRC8M_VALUE)            /* internal 8 MHz RC oscillator frequency */
#define __HXTAL           (HXTAL_VALUE)            /* high speed crystal oscillator frequency */
#define __SYS_OSC_CLK     (__IRC8M)                /* main oscillator frequency */

#define VECT_TAB_OFFSET  (uint32_t)0x00            /* vector table base offset */

/* select a system clock by uncommenting the following line */
// 在此选择系统时钟
//#define __SYSTEM_CLOCK_8M_HXTAL              (__HXTAL)
// #define __SYSTEM_CLOCK_8M_IRC8M              (__IRC8M)
//#define __SYSTEM_CLOCK_72M_PLL_HXTAL         (uint32_t)(72000000)
#define __SYSTEM_CLOCK_72M_PLL_IRC8M_DIV2    (uint32_t)(72000000)

/* The following is to prevent Vcore fluctuations caused by frequency switching. 
   It is strongly recommended to include it to avoid issues caused by self-removal. 
*/
#define RCU_MODIFY(__delay)     do{                                     \
                                    volatile uint32_t i,reg;            \
                                    if(0 != __delay){                   \
                                        reg = RCU_CFG0;                 \
                                        reg &= ~(RCU_CFG0_AHBPSC);      \
                                        /* CK_AHB = SYSCLK/2 */         \
                                        reg |= RCU_AHB_CKSYS_DIV2;      \
                                        RCU_CFG0 = reg;                 \
                                        for(i=0; i<__delay; i++){       \
                                        }                               \
                                        reg = RCU_CFG0;                 \
                                        reg &= ~(RCU_CFG0_AHBPSC);      \
                                        reg |= RCU_AHB_CKSYS_DIV4;      \
                                        /* CK_AHB = SYSCLK/4 */         \
                                        RCU_CFG0 = reg;                 \
                                        for(i=0; i<__delay; i++){       \
                                        }                               \
                                    }                                   \
                                }while(0)

#define SEL_IRC8M       0x00
#define SEL_HXTAL       0x01
#define SEL_PLL         0x02

/* set the system clock frequency and declare the system clock configuration function */
#ifdef __SYSTEM_CLOCK_8M_HXTAL
uint32_t SystemCoreClock = __SYSTEM_CLOCK_8M_HXTAL;
static void system_clock_8m_hxtal(void);

#elif defined (__SYSTEM_CLOCK_72M_PLL_HXTAL)
uint32_t SystemCoreClock = __SYSTEM_CLOCK_72M_PLL_HXTAL;
static void system_clock_72m_hxtal(void);

#elif defined (__SYSTEM_CLOCK_72M_PLL_IRC8M_DIV2)
uint32_t SystemCoreClock = __SYSTEM_CLOCK_72M_PLL_IRC8M_DIV2;
static void system_clock_72m_irc8m(void);

#else
uint32_t SystemCoreClock = __SYSTEM_CLOCK_8M_IRC8M;
static void system_clock_8m_irc8m(void);
#endif /* __SYSTEM_CLOCK_8M_HXTAL */

/* configure the system clock */
static void system_clock_config(void);

/* software delay to prevent the impact of Vcore fluctuations.
   It is strongly recommended to include it to avoid issues caused by self-removal. */
static void _soft_delay_(uint32_t time)
{
    __IO uint32_t i;
    for(i=0; i<time*10; i++){
    }
}

/*!
    \brief      setup the microcontroller system, initialize the system
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SystemInit (void)
{
    /* enable IRC8M */
    RCU_CTL0 |= RCU_CTL0_IRC8MEN;
    while(0U == (RCU_CTL0 & RCU_CTL0_IRC8MSTB)){
    }
    if(((RCU_CFG0 & RCU_CFG0_SCSS) == RCU_SCSS_PLL)){
        RCU_MODIFY(0x80);
    }
    RCU_CFG0 &= ~RCU_CFG0_SCS;
    _soft_delay_(100);
    RCU_CTL0 &= ~(RCU_CTL0_HXTALEN | RCU_CTL0_CKMEN | RCU_CTL0_PLLEN | RCU_CTL0_HXTALBPS);
    /* reset RCU */
    RCU_CFG0 &= ~(RCU_CFG0_SCS | RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC |\
                  RCU_CFG0_ADCPSC | RCU_CFG0_CKOUTSEL | RCU_CFG0_CKOUTDIV | RCU_CFG0_PLLDV);
    RCU_CFG0 &= ~(RCU_CFG0_PLLSEL | RCU_CFG0_PLLMF | RCU_CFG0_PLLMF4 | RCU_CFG0_PLLDV);
    RCU_CFG1 &= ~(RCU_CFG1_PREDV);
    RCU_CFG2 &= ~(RCU_CFG2_USART0SEL | RCU_CFG2_ADCSEL);
    RCU_CFG2 &= ~RCU_CFG2_IRC28MDIV;
    RCU_CFG2 &= ~RCU_CFG2_ADCPSC2;
    RCU_CTL1 &= ~RCU_CTL1_IRC28MEN;
    RCU_INT = 0x00000000U;

    /* configure system clock */
    system_clock_config();
    
#ifdef VECT_TAB_SRAM
    nvic_vector_table_set(NVIC_VECTTAB_RAM,VECT_TAB_OFFSET);
#else
    nvic_vector_table_set(NVIC_VECTTAB_FLASH,VECT_TAB_OFFSET);
#endif
}

/*!
    \brief      configure the system clock
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void system_clock_config(void)
{
#ifdef __SYSTEM_CLOCK_8M_HXTAL
    system_clock_8m_hxtal();
#elif defined (__SYSTEM_CLOCK_72M_PLL_HXTAL)
    system_clock_72m_hxtal();
#elif defined (__SYSTEM_CLOCK_72M_PLL_IRC8M_DIV2)
    system_clock_72m_irc8m();
#elif defined (__SYSTEM_CLOCK_72M_PLL_IRC48M_DIV2)
    system_clock_72m_irc48m();
#else
    system_clock_8m_irc8m();
#endif /* __SYSTEM_CLOCK_8M_HXTAL */
}

#ifdef __SYSTEM_CLOCK_8M_HXTAL
/*!
    \brief      configure the system clock to 8M by HXTAL
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void system_clock_8m_hxtal(void)
{
    uint32_t timeout = 0U;
    uint32_t stab_flag = 0U;
    __IO uint32_t reg_temp;

    /* enable HXTAL */
    RCU_CTL0 |= RCU_CTL0_HXTALEN;
    
    /* wait until HXTAL is stable or the startup time is longer than HXTAL_STARTUP_TIMEOUT */
    do{
        timeout++;
        stab_flag = (RCU_CTL0 & RCU_CTL0_HXTALSTB);
    }
    while((0U == stab_flag) && (HXTAL_STARTUP_TIMEOUT != timeout));    
    /* if fail */
    if(0U == (RCU_CTL0 & RCU_CTL0_HXTALSTB)){
        while(1){
        }
    }
    
    /* HXTAL is stable */
    /* AHB = SYSCLK */
    RCU_CFG0 |= RCU_AHB_CKSYS_DIV1;
    /* APB2 = AHB */
    RCU_CFG0 |= RCU_APB2_CKAHB_DIV1;
    /* APB1 = AHB */
    RCU_CFG0 |= RCU_APB1_CKAHB_DIV1;

    reg_temp = RCU_CFG0;
    /* select HXTAL as system clock */
    reg_temp &= ~RCU_CFG0_SCS;
    reg_temp |= RCU_CKSYSSRC_HXTAL;
    RCU_CFG0 = reg_temp;
    
    /* wait until HXTAL is selected as system clock */
    while(RCU_SCSS_HXTAL != (RCU_CFG0 & RCU_CFG0_SCSS)){
    }
}

#elif defined (__SYSTEM_CLOCK_72M_PLL_HXTAL)
/*!
    \brief      configure the system clock to 72M by PLL which selects HXTAL as its clock source
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void system_clock_72m_hxtal(void)
{
    uint32_t timeout = 0U;
    uint32_t stab_flag = 0U;
    __IO uint32_t reg_temp;
    
    /* enable HXTAL */
    RCU_CTL0 |= RCU_CTL0_HXTALEN;

    /* wait until HXTAL is stable or the startup time is longer than HXTAL_STARTUP_TIMEOUT */
    do{
        timeout++;
        stab_flag = (RCU_CTL0 & RCU_CTL0_HXTALSTB);
    }
    while((0U == stab_flag) && (HXTAL_STARTUP_TIMEOUT != timeout));
    /* if fail */
    if(0U == (RCU_CTL0 & RCU_CTL0_HXTALSTB)){
        while(1){
        }
    }
    
    FMC_WS = (FMC_WS & (~FMC_WS_WSCNT)) | WS_WSCNT_2;
    
    /* HXTAL is stable */
    /* AHB = SYSCLK */
    RCU_CFG0 |= RCU_AHB_CKSYS_DIV1;
    /* APB2 = AHB */
    RCU_CFG0 |= RCU_APB2_CKAHB_DIV1;
    /* APB1 = AHB */
    RCU_CFG0 |= RCU_APB1_CKAHB_DIV1;

    /* PLL = HXTAL * 9 = 72 MHz */
    RCU_CFG0 &= ~(RCU_CFG0_PLLSEL | RCU_CFG0_PLLMF | RCU_CFG0_PLLDV);
    RCU_CFG0 |= (RCU_PLLSRC_HXTAL | RCU_PLL_MUL9);

    /* enable PLL */
    RCU_CTL0 |= RCU_CTL0_PLLEN;

    /* wait until PLL is stable */
    while(0U == (RCU_CTL0 & RCU_CTL0_PLLSTB)){
    }

    reg_temp = RCU_CFG0;
    /* select PLL as system clock */
    reg_temp &= ~RCU_CFG0_SCS;
    reg_temp |= RCU_CKSYSSRC_PLL;
    RCU_CFG0 = reg_temp;

    /* wait until PLL is selected as system clock */
    while(RCU_SCSS_PLL != (RCU_CFG0 & RCU_CFG0_SCSS)){
    }
}

#elif defined (__SYSTEM_CLOCK_72M_PLL_IRC8M_DIV2)
/*!
    \brief      configure the system clock to 72M by PLL which selects IRC8M/2 as its clock source
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void system_clock_72m_irc8m(void)
{
    uint32_t timeout = 0U;
    uint32_t stab_flag = 0U;
    __IO uint32_t reg_temp;
    
    /* enable IRC8M */
    RCU_CTL0 |= RCU_CTL0_IRC8MEN;

    /* wait until IRC8M is stable or the startup time is longer than IRC8M_STARTUP_TIMEOUT */
    do{
        timeout++;
        stab_flag = (RCU_CTL0 & RCU_CTL0_IRC8MSTB);
    }
    while((0U == stab_flag) && (IRC8M_STARTUP_TIMEOUT != timeout));

    /* if fail */
    if(0U == (RCU_CTL0 & RCU_CTL0_IRC8MSTB)){
        while(1){
        }
    }

    FMC_WS = (FMC_WS & (~FMC_WS_WSCNT)) | WS_WSCNT_2;
    
    /* AHB = SYSCLK */
    RCU_CFG0 |= RCU_AHB_CKSYS_DIV1;
    /* APB2 = AHB */
    RCU_CFG0 |= RCU_APB2_CKAHB_DIV1;
    /* APB1 = AHB */
    RCU_CFG0 |= RCU_APB1_CKAHB_DIV1;
    /* PLL = (IRC8M/2) * 18 = 72 MHz */
    RCU_CFG0 &= ~(RCU_CFG0_PLLSEL | RCU_CFG0_PLLMF);
    RCU_CFG0 |= (RCU_PLLSRC_IRC8M_DIV2 | RCU_PLL_MUL18);
    
    /* enable PLL */
    RCU_CTL0 |= RCU_CTL0_PLLEN;

    /* wait until PLL is stable */
    while(0U == (RCU_CTL0 & RCU_CTL0_PLLSTB)){
    }

    reg_temp = RCU_CFG0;
    /* select PLL as system clock */
    reg_temp &= ~RCU_CFG0_SCS;
    reg_temp |= RCU_CKSYSSRC_PLL;
    RCU_CFG0 = reg_temp;

    /* wait until PLL is selected as system clock */
    while(RCU_SCSS_PLL != (RCU_CFG0 & RCU_CFG0_SCSS)){
    }
}

#else
/*!
    \brief      configure the system clock to 8M by IRC8M
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void system_clock_8m_irc8m(void)
{
    uint32_t timeout = 0U;
    uint32_t stab_flag = 0U;
    __IO uint32_t reg_temp;
    
    /* enable IRC8M */
    RCU_CTL0 |= RCU_CTL0_IRC8MEN;

    /* wait until IRC8M is stable or the startup time is longer than IRC8M_STARTUP_TIMEOUT */
    do{
        timeout++;
        stab_flag = (RCU_CTL0 & RCU_CTL0_IRC8MSTB);
    }
    while((0U == stab_flag) && (IRC8M_STARTUP_TIMEOUT != timeout));

    /* if fail */
    if(0U == (RCU_CTL0 & RCU_CTL0_IRC8MSTB)){
        while(1){
        }
    }

    /* AHB = SYSCLK */
    RCU_CFG0 |= RCU_AHB_CKSYS_DIV1;
    /* APB2 = AHB */
    RCU_CFG0 |= RCU_APB2_CKAHB_DIV1;
    /* APB1 = AHB */
    RCU_CFG0 |= RCU_APB1_CKAHB_DIV1;
    
    reg_temp = RCU_CFG0;
    /* select IRC8M as system clock */
    reg_temp &= ~RCU_CFG0_SCS;
    reg_temp |= RCU_CKSYSSRC_IRC8M;
    RCU_CFG0 = reg_temp;
    
    /* wait until IRC8M is selected as system clock */
    while(RCU_SCSS_IRC8M != (RCU_CFG0 & RCU_CFG0_SCSS)){
    }
}
#endif /* __SYSTEM_CLOCK_8M_HXTAL */

/*!
    \brief      update the SystemCoreClock with current core clock retrieved from cpu registers
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SystemCoreClockUpdate (void)
{
    uint32_t sws = 0U;
    uint32_t pllmf = 0U, pllmf4 = 0U, pllsel = 0U, prediv = 0U, idx = 0U, clk_exp = 0U;
    /* exponent of AHB clock divider */
    const uint8_t ahb_exp[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};

    sws = GET_BITS(RCU_CFG0, 2, 3);
    switch(sws){
    /* IRC8M is selected as CK_SYS */
    case SEL_IRC8M:
        SystemCoreClock = IRC8M_VALUE;
        break;
    /* HXTAL is selected as CK_SYS */
    case SEL_HXTAL:
        SystemCoreClock = HXTAL_VALUE;
        break;
    /* PLL is selected as CK_SYS */
    case SEL_PLL:
        /* get the value of PLLMF[3:0] */
        pllmf = GET_BITS(RCU_CFG0, 18, 21);
        pllmf4 = GET_BITS(RCU_CFG0, 27, 27);
        /* high 16 bits */
        if(1U == pllmf4){
            pllmf += 17U;
        }else if(15U == pllmf){
            pllmf = 16U;
        } else {
            pllmf += 2U;
        }
        
        /* PLL clock source selection, HXTAL or IRC8M/2 */
        pllsel = GET_BITS(RCU_CFG0, 16, 16);
        if(0U != pllsel){
            prediv = (GET_BITS(RCU_CFG1, 0, 3) + 1U);
            SystemCoreClock = (HXTAL_VALUE / prediv) * pllmf;
        } else {
            SystemCoreClock = (IRC8M_VALUE >> 1) * pllmf;
        }
        break;
    /* IRC8M is selected as CK_SYS */
    default:
        SystemCoreClock = IRC8M_VALUE;
        break;
    }
    /* calculate AHB clock frequency */
    idx = GET_BITS(RCU_CFG0, 4, 7);
    clk_exp = ahb_exp[idx];
    SystemCoreClock >>= clk_exp;
}

#ifdef __FIRMWARE_VERSION_DEFINE
/*!
    \brief      get firmware version
    \param[in]  none
    \param[out] none
    \retval     firmware version
*/
uint32_t gd32e23x_firmware_version_get(void)
{
    return __GD32E23x_STDPERIPH_VERSION;
}
#endif /* __FIRMWARE_VERSION_DEFINE */

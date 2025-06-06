/*!
    \file    main.c
    \brief   master send and slave receive data using interrupt mode

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
#include "gd32e23x_it.h"
#include "gd32e230c_eval.h"
#include "main.h"

volatile uint32_t send_n = 0, receive_n = 0;
uint8_t spi0_send_array[ARRAYSIZE] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA };
uint8_t spi1_receive_array[ARRAYSIZE];

void rcu_config(void);
void gpio_config(void);
void spi_config(void);
ErrStatus memory_compare(uint8_t *src, uint8_t *dst, uint8_t length);

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    /* initialize the LEDs */
    gd_eval_led_init(LED1);

    /* configure NVIC */
    nvic_irq_enable(SPI0_IRQn, 1);
    nvic_irq_enable(SPI1_IRQn, 0);

    /* enable peripheral clock */
    rcu_config();
    /* configure GPIO */
    gpio_config();
    /* configure SPI */
    spi_config();

    /* enable SPI interrupt */
    spi_i2s_interrupt_enable(SPI0, SPI_I2S_INT_TBE);
    spi_i2s_interrupt_enable(SPI1, SPI_I2S_INT_RBNE);

    /* enable SPI */
    spi_enable(SPI1);
    spi_enable(SPI0);

    /* wait transmit completed */
    while(receive_n < ARRAYSIZE);

#if SPI_CRC_ENABLE
    /* check the CRC error status  */
    if(SET != spi_i2s_flag_get(SPI1, SPI_FLAG_CRCERR)) {
        gd_eval_led_on(LED1);
    } else {
        gd_eval_led_off(LED1);
    }
#else
    /* compare receive data with send data */
    if(ERROR != memory_compare(spi1_receive_array, spi0_send_array, ARRAYSIZE)) {
        gd_eval_led_on(LED1);
    } else {
        gd_eval_led_off(LED1);
    }
#endif /* enable CRC function */

    while(1) {
    }
}

/*!
    \brief      configure different peripheral clocks
    \param[in]  none
    \param[out] none
    \retval     none
*/
void rcu_config(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_SPI0);
    rcu_periph_clock_enable(RCU_SPI1);
}

/*!
    \brief      configure the GPIO peripheral
    \param[in]  none
    \param[out] none
    \retval     none
*/
void gpio_config(void)
{
    /* configure SPI0 GPIO: SCK/PA5, MOSI/PA7 */
    gpio_af_set(GPIOA, GPIO_AF_0, GPIO_PIN_5 | GPIO_PIN_7);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_5 | GPIO_PIN_7);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_5 | GPIO_PIN_7);

    /* configure SPI1 GPIO: SCK/PB13, MISO/PB14 */
    gpio_af_set(GPIOB, GPIO_AF_0, GPIO_PIN_13 | GPIO_PIN_14);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_13 | GPIO_PIN_14);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_13 | GPIO_PIN_14);
}

/*!
    \brief      configure the SPI peripheral
    \param[in]  none
    \param[out] none
    \retval     none
*/
void spi_config(void)
{
    spi_parameter_struct  spi_init_struct;
    /* deinitilize SPI and the parameters */
    spi_i2s_deinit(SPI0);
    spi_i2s_deinit(SPI1);
    spi_struct_para_init(&spi_init_struct);

    /* configure SPI0 parameter */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_BDTRANSMIT;
    spi_init_struct.device_mode          = SPI_MASTER;;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_8 ;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;;
    spi_init(SPI0, &spi_init_struct);

    /* configure SPI1 parameter */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_BDRECEIVE;
    spi_init_struct.device_mode          = SPI_SLAVE;
    spi_init(SPI1, &spi_init_struct);

    /* configure SPI1 byte access to FIFO */
    spi_fifo_access_size_config(SPI1, SPI_BYTE_ACCESS);

#if SPI_CRC_ENABLE
    /* configure SPI CRC function */
    spi_crc_length_set(SPI1, SPI_CRC_8BIT);
    spi_crc_polynomial_set(SPI0, 7);
    spi_crc_polynomial_set(SPI1, 7);
    spi_crc_on(SPI0);
    spi_crc_on(SPI1);
#endif /* enable CRC function */
}

/*!
    \brief      memory compare function
    \param[in]  src: source data pointer
    \param[in]  dst: destination data pointer
    \param[in]  length: the compare data length
    \param[out] none
    \retval     ErrStatus: ERROR or SUCCESS
*/
ErrStatus memory_compare(uint8_t *src, uint8_t *dst, uint8_t length)
{
    while(length--) {
        if(*src++ != *dst++) {
            return ERROR;
        }
    }
    return SUCCESS;
}

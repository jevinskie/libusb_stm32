/* This file is the part of the Lightweight USB device Stack for STM32 microcontrollers
 *
 * Copyright ©2016 Dmitry Filimonchuk <dmitrystu[at]gmail[dot]com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "stm32.h"

#include <stdint.h>

static void uart_init_rcc(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_USART1EN | RCC_APB2ENR_AFIOEN;
    AFIO->MAPR |= AFIO_MAPR_USART1_REMAP;
    GPIOB->CRL &= ~(GPIO_CRL_MODE6 | GPIO_CRL_CNF6 | GPIO_CRL_MODE7 | GPIO_CRL_CNF7 );
    GPIOB->CRL    |= ( ( 0x1 << GPIO_CRL_MODE6_Pos ) |
                       ( 0x2 << GPIO_CRL_CNF6_Pos ) |
                       ( 0x0 << GPIO_CRL_MODE7_Pos ) |
                       ( 0x1 << GPIO_CRL_CNF7_Pos ) );
}

static void uart_init(void) {
    const uint32_t sys_clk = 48 * 1000000;
    uart_init_rcc();
    const uint16_t uart_div = sys_clk / 115200;
    USART1->BRR = ((26 << USART_BRR_DIV_Mantissa_Pos) |
                    (0 << USART_BRR_DIV_Fraction_Pos));
    USART1->CR1 |= (USART_CR1_RE | USART_CR1_TE | USART_CR1_UE);
    while (1) {
        while( !( USART1->SR & USART_SR_TXE ) ) {};
        USART1->DR = 'a';
    }
}

static void cdc_init_rcc (void) {
    /* set flash latency 1WS */
    _BMD(FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_1);
    /* use PLL 48MHz clock from 8Mhz HSI */
    _BMD(RCC->CFGR,
         RCC_CFGR_PLLMULL | RCC_CFGR_PLLSRC | RCC_CFGR_USBPRE,
         RCC_CFGR_PLLMULL12 | RCC_CFGR_USBPRE);
    _BST(RCC->CR, RCC_CR_PLLON);
    _WBS(RCC->CR, RCC_CR_PLLRDY);
    /* switch to PLL */
    _BMD(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_PLL);
    _WVL(RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL);
}

void __libc_init_array(void) {

}

void SystemInit(void) {
    cdc_init_rcc();
    uart_init();
}

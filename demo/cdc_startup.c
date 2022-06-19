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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

extern const uint8_t __end__;

void *_sbrk(intptr_t increment) {
  static uint8_t *heap = &__end__;
  uint8_t *res = heap;
  heap += increment;
  return res;
}

int _close(void) {
    return -1;
}

int _fstat(void) {
    return 0;
}

int _isatty(void) {
    return 0;
}

int _lseek(void) {
    return 0;
}

int _read(void) {
    return 0;
}

ssize_t _write(int fd, const void *buf, size_t count) {
    const uint8_t *p = buf;
    ssize_t sz = (ssize_t)count;
    while (count) {
        while (!(USART1->SR & USART_SR_TXE)) {};
        USART1->DR = *p++;
        --count;
    }
    return sz;
}

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
    uart_init_rcc();
    const uint32_t sys_clk = 72 * 1000000;
    const uint32_t uart_div = sys_clk / 115200;
    USART1->BRR = (((uart_div / 16) << USART_BRR_DIV_Mantissa_Pos) |
                    ((uart_div % 16) << USART_BRR_DIV_Fraction_Pos));
    USART1->CR1 |= (USART_CR1_RE | USART_CR1_TE | USART_CR1_UE);
}

static void cdc_init_rcc (void) {
    /* set flash latency 1WS */
    _BMD(FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_1);
    _BST(RCC->CR, RCC_CR_HSEON);
    _WBS(RCC->CR, RCC_CR_HSERDY);
    // use PLL 72 MHz clock from 8Mhz HSE, divide by 1.5 for 48 MHz USB
    _BMD(RCC->CFGR,
         RCC_CFGR_PLLMULL | RCC_CFGR_PLLSRC | RCC_CFGR_USBPRE | RCC_CFGR_PPRE2 | RCC_CFGR_PPRE1 | RCC_CFGR_HPRE,
         RCC_CFGR_PLLMULL9 | RCC_CFGR_PLLSRC | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_HPRE_DIV1);
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
    write(0, "hello\n", sizeof("hello\n") - 1);
    printf("hello world\n");
}

/*  Copyright (C) 2023 NANDO authors
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "spi.h"


inline void spi_select_chip()
{
    GPIO_ResetBits(GPIOA, SPI_FLASH_CS_PIN);
}

inline void spi_deselect_chip()
{
    GPIO_SetBits(GPIOA, SPI_FLASH_CS_PIN);
}

uint8_t spi_send_byte(uint8_t byte)
{
    /* For disable DMA signals. */
    SPI3->CTLR2 = 0;
    /* Loop while DR register in not empty */
    while((SPI3->STATR & SPI_I2S_FLAG_TXE) == RESET);
    /* Send byte through the SPI3 peripheral to generate clock signal */
    SPI3->DATAR = byte;
    /* Wait to receive a byte */
    while((SPI3->STATR & SPI_I2S_FLAG_RXNE) == RESET);
    /* Return the byte read from the SPI bus */
    return SPI3->DATAR;
}

inline uint8_t spi_read_byte()
{
    return spi_send_byte(FLASH_DUMMY_BYTE);
}

void spi_dma_wait(void)
{
    // Wait until the data is received
    while ((DMA2->INTFR & DMA2_FLAG_TC1) == RESET);
    //Disable the DMAs
    DMA2_Channel2->CFGR = 0; // TX
    DMA2_Channel1->CFGR = 0; // RX
    //Clear DMA flags
    DMA2->INTFCR = DMA2_FLAG_TC1 | DMA2_FLAG_TC2;
}

void spi_dma_tx(uint8_t *buf, uint32_t len)
{
    static uint8_t garbage;

    DMA2_Channel1->CNTR = len;
    DMA2_Channel1->MADDR = (uint32_t)&garbage;
    DMA2_Channel1->CFGR = DMA_CFGR1_EN; // RX

    DMA2_Channel2->CNTR = len;
    DMA2_Channel2->MADDR = (uint32_t)buf;
    DMA2_Channel2->CFGR = DMA_CFGR2_EN | DMA_CFGR2_MINC | DMA_CFGR2_DIR;

    SPI3->CTLR2 = SPI_I2S_DMAReq_Tx | SPI_I2S_DMAReq_Rx;

    spi_dma_wait();
}

void spi_dma_rx(uint8_t *buf, uint32_t len) {
    static uint8_t dummy = 0xFF;

    DMA2_Channel1->CNTR = len;
    DMA2_Channel1->MADDR = (uint32_t)buf;
    DMA2_Channel1->CFGR = DMA_CFGR1_EN | DMA_CFGR1_MINC;

    DMA2_Channel2->CNTR = len;
    DMA2_Channel2->MADDR = (uint32_t)&dummy;
    DMA2_Channel2->CFGR = DMA_CFGR2_EN | DMA_CFGR2_DIR;

    SPI3->CTLR2 = SPI_I2S_DMAReq_Tx | SPI_I2S_DMAReq_Rx;

    spi_dma_wait();
}

static void spi_gpio_init()
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    /* Enable SPI peripheral clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, ENABLE);
    /* Remap SPI3 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SPI3, ENABLE);
    /* Configure SPI SCK pin */
    gpio_init.GPIO_Pin = SPI_FLASH_SCK_PIN;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOC, &gpio_init);
    /* Configure SPI MOSI pin */
    gpio_init.GPIO_Pin = SPI_FLASH_MOSI_PIN;
    GPIO_Init(GPIOC, &gpio_init);
    /* Configure SPI MISO pin */
    gpio_init.GPIO_Pin = SPI_FLASH_MISO_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &gpio_init);
    /* Configure SPI CS pin */
    gpio_init.GPIO_Pin = SPI_FLASH_CS_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &gpio_init);
}

static void spi_gpio_uninit()
{
    GPIO_InitTypeDef gpio_init;

    GPIO_PinRemapConfig(GPIO_Remap_SPI3, DISABLE);
    /* Disable SPI peripheral clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3, DISABLE);
    /* Disable SPI SCK pin */
    gpio_init.GPIO_Pin = SPI_FLASH_SCK_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &gpio_init);
    /* Disable SPI MISO pin */
    gpio_init.GPIO_Pin = SPI_FLASH_MISO_PIN;
    GPIO_Init(GPIOC, &gpio_init);
    /* Disable SPI MOSI pin */
    gpio_init.GPIO_Pin = SPI_FLASH_MOSI_PIN;
    GPIO_Init(GPIOC, &gpio_init);
    /* Disable SPI CS pin */
    gpio_init.GPIO_Pin = SPI_FLASH_CS_PIN;
    GPIO_Init(GPIOA, &gpio_init);
}

static uint16_t spi_get_baud_rate_prescaler(uint32_t spi_freq_khz)
{
    uint32_t system_clock_khz = SystemCoreClock / 1000;

    // Max SPI frequency for CH32V307
    if(spi_freq_khz > 36000)
        spi_freq_khz = 36000;

    if (spi_freq_khz >= system_clock_khz / 2)
        return SPI_BaudRatePrescaler_2;
    else if (spi_freq_khz >= system_clock_khz / 4)
        return SPI_BaudRatePrescaler_4;
    else if (spi_freq_khz >= system_clock_khz / 8)
        return SPI_BaudRatePrescaler_8;
    else if (spi_freq_khz >= system_clock_khz / 16)
        return SPI_BaudRatePrescaler_16;
    else if (spi_freq_khz >= system_clock_khz / 32)
        return SPI_BaudRatePrescaler_32;
    else if (spi_freq_khz >= system_clock_khz / 64)
        return SPI_BaudRatePrescaler_64;
    else if (spi_freq_khz >= system_clock_khz / 128)
        return SPI_BaudRatePrescaler_128;
    else
        return SPI_BaudRatePrescaler_256;
}

int spi_init(uint32_t freq)
{
    SPI_InitTypeDef spi_init;

    spi_gpio_init();

    spi_deselect_chip();

    /* Configure SPI */
    spi_init.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi_init.SPI_Mode = SPI_Mode_Master;
    spi_init.SPI_DataSize = SPI_DataSize_8b;
    spi_init.SPI_CPOL = SPI_CPOL_High;
    spi_init.SPI_CPHA = SPI_CPHA_2Edge;
    spi_init.SPI_NSS = SPI_NSS_Soft;
    spi_init.SPI_BaudRatePrescaler =
        spi_get_baud_rate_prescaler(freq);
    spi_init.SPI_FirstBit = SPI_FirstBit_MSB;
    spi_init.SPI_CRCPolynomial = 7;
    SPI_Init(SPI3, &spi_init);

    /* Enable the DMA peripheral */
    RCC->AHBPCENR |= RCC_AHBPeriph_DMA2;
    // Set DMA peripheral addres
    DMA2_Channel2->PADDR = (uint32_t)&(SPI3->DATAR); //TX
    DMA2_Channel1->PADDR = (uint32_t)&(SPI3->DATAR); //RX

    /* Enable SPI */
    SPI_Cmd(SPI3, ENABLE);

    /* Clear data register */
    SPI_I2S_ReceiveData(SPI3);

    return 0;
}

void spi_uninit()
{
    spi_gpio_uninit();
    /* Disable SPI */
    SPI_Cmd(SPI3, DISABLE);
}

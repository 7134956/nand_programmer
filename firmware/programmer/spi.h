/*  Copyright (C) 2023 NANDO authors
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "ch32v30x.h"

#ifndef _SPI_H_
#define _SPI_H_

#define SPI_FLASH_CS_PIN GPIO_Pin_15
#define SPI_FLASH_SCK_PIN GPIO_Pin_10
#define SPI_FLASH_MISO_PIN GPIO_Pin_11
#define SPI_FLASH_MOSI_PIN GPIO_Pin_12

#define FLASH_DUMMY_BYTE 0xA5

void spi_select_chip();
void spi_deselect_chip();
uint8_t spi_send_byte(uint8_t byte);
uint8_t spi_read_byte();
void spi_dma_tx(uint8_t *buf, uint32_t len);
void spi_dma_rx(uint8_t *buf, uint32_t len);
int spi_init(uint32_t freq);
void spi_uninit();

#endif /* _SPI_H_ */

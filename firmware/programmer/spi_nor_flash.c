/*  Copyright (C) 2020 NANDO authors
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "spi_nor_flash.h"
#include "spi.h"

/* 1st addressing cycle */
#define ADDR_1st_CYCLE(ADDR) (uint8_t)((ADDR)& 0xFF)
/* 2nd addressing cycle */
#define ADDR_2nd_CYCLE(ADDR) (uint8_t)(((ADDR)& 0xFF00) >> 8)
/* 3rd addressing cycle */
#define ADDR_3rd_CYCLE(ADDR) (uint8_t)(((ADDR)& 0xFF0000) >> 16)
/* 4th addressing cycle */
#define ADDR_4th_CYCLE(ADDR) (uint8_t)(((ADDR)& 0xFF000000) >> 24)

#define UNDEFINED_CMD 0xFF

typedef struct __attribute__((__packed__))
{
    uint8_t page_offset;
    uint8_t read_cmd;
    uint8_t read_id_cmd;
    uint8_t write_cmd;
    uint8_t write_en_cmd;
    uint8_t erase_cmd;
    uint8_t status_cmd;
    uint8_t busy_bit;
    uint8_t busy_state;
    uint32_t freq;
} spi_conf_t;

static spi_conf_t spi_conf;

static uint32_t spi_flash_read_status()
{
    uint8_t status;
    uint32_t flash_status = FLASH_STATUS_READY;

    spi_select_chip();

    spi_send_byte(spi_conf.status_cmd);

    status = spi_read_byte();

    if (spi_conf.busy_state == 1 && (status & (1 << spi_conf.busy_bit)))
        flash_status = FLASH_STATUS_BUSY;
    else if (spi_conf.busy_state == 0 && !(status & (1 << spi_conf.busy_bit)))
        flash_status = FLASH_STATUS_BUSY;

    spi_deselect_chip();

    return flash_status;
}

static uint32_t spi_flash_get_status()
{
    uint32_t status, timeout = 0x1000000;

    status = spi_flash_read_status();

    /* Wait for an operation to complete or a TIMEOUT to occur */
    while (status == FLASH_STATUS_BUSY && timeout)
    {
        status = spi_flash_read_status();
        timeout --;
    }

    if (!timeout)
        status = FLASH_STATUS_TIMEOUT;

    return status;
}

static void spi_flash_read_id(chip_id_t *chip_id)
{
    spi_select_chip();

    spi_send_byte(spi_conf.read_id_cmd);

    chip_id->maker_id = spi_read_byte();
    chip_id->device_id = spi_read_byte();
    chip_id->third_id = spi_read_byte();
    chip_id->fourth_id = spi_read_byte();
    chip_id->fifth_id = spi_read_byte();
    chip_id->sixth_id = spi_read_byte();

    spi_deselect_chip();
}

static void spi_flash_write_enable()
{
    if (spi_conf.write_en_cmd == UNDEFINED_CMD)
        return;

    spi_select_chip();
    spi_send_byte(spi_conf.write_en_cmd);
    spi_deselect_chip();
}

static void spi_flash_write_page_async(uint8_t *buf, uint32_t page,
    uint32_t page_size)
{
    spi_flash_write_enable();

    spi_select_chip();

    spi_send_byte(spi_conf.write_cmd);

    page = page << spi_conf.page_offset;

    spi_send_byte(ADDR_3rd_CYCLE(page));
    spi_send_byte(ADDR_2nd_CYCLE(page));
    spi_send_byte(ADDR_1st_CYCLE(page));

    spi_dma_tx(buf, page_size);

    spi_deselect_chip();
}

static uint32_t spi_flash_read_data(uint8_t *buf, uint32_t page,
    uint32_t page_offset, uint32_t data_size)
{
    uint32_t addr = (page << spi_conf.page_offset) + page_offset;

    spi_select_chip();

    spi_send_byte(spi_conf.read_cmd);

    spi_send_byte(ADDR_3rd_CYCLE(addr));
    spi_send_byte(ADDR_2nd_CYCLE(addr));
    spi_send_byte(ADDR_1st_CYCLE(addr));

    /* AT45DB requires write of dummy byte after address */
    spi_send_byte(FLASH_DUMMY_BYTE);

    spi_dma_rx(buf, data_size);

    spi_deselect_chip();

    return FLASH_STATUS_READY;
}

static uint32_t spi_flash_read_page(uint8_t *buf, uint32_t page,
    uint32_t page_size)
{
    return spi_flash_read_data(buf, page, 0, page_size);
}

static uint32_t spi_flash_read_spare_data(uint8_t *buf, uint32_t page,
    uint32_t offset, uint32_t data_size)
{
    return FLASH_STATUS_INVALID_CMD;
}

static uint32_t spi_flash_erase_block(uint32_t page)
{
    uint32_t addr = page << spi_conf.page_offset;

    spi_flash_write_enable();

    spi_select_chip();

    spi_send_byte(spi_conf.erase_cmd);

    spi_send_byte(ADDR_3rd_CYCLE(addr));
    spi_send_byte(ADDR_2nd_CYCLE(addr));
    spi_send_byte(ADDR_1st_CYCLE(addr));

    spi_deselect_chip();

    return spi_flash_get_status();
}

static inline bool spi_flash_is_bb_supported()
{
    return false;
}
static int spi_flash_init(void *conf, uint32_t conf_size)
{
    if (conf_size < sizeof(spi_conf_t))
        return -1;
    spi_conf = *(spi_conf_t *)conf;

    spi_init(spi_conf.freq);
    return 0;
}

static void spi_flash_uninit()
{
    spi_uninit();
}

flash_hal_t hal_spi_nor =
{
    .init = spi_flash_init,
    .uninit = spi_flash_uninit,
    .read_id = spi_flash_read_id,
    .erase_block = spi_flash_erase_block,
    .read_page = spi_flash_read_page,
    .read_spare_data = spi_flash_read_spare_data, 
    .write_page_async = spi_flash_write_page_async,
    .read_status = spi_flash_read_status,
    .is_bb_supported = spi_flash_is_bb_supported
};

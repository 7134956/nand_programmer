/*  Copyright (C) 2020 NANDO authors
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "spi_nand_flash.h"
#include "ch32v30x.h"

/* SPI NAND Command Set */
#define _SPI_NAND_OP_GET_FEATURE                0x0F    /* Get Feature */
#define _SPI_NAND_OP_SET_FEATURE                0x1F    /* Set Feature */
#define _SPI_NAND_OP_PAGE_READ                  0x13    /* Load page data into cache of SPI NAND chip */
#define _SPI_NAND_OP_READ_FROM_CACHE_SINGLE     0x03    /* Read data from cache of SPI NAND chip, single speed*/
#define _SPI_NAND_OP_READ_FROM_CACHE_DUAL       0x3B    /* Read data from cache of SPI NAND chip, dual speed*/
#define _SPI_NAND_OP_READ_FROM_CACHE_QUAD       0x6B    /* Read data from cache of SPI NAND chip, quad speed*/
#define _SPI_NAND_OP_WRITE_ENABLE               0x06    /* Enable write data to  SPI NAND chip */
#define _SPI_NAND_OP_WRITE_DISABLE              0x04    /* Reseting the Write Enable Latch (WEL) */
#define _SPI_NAND_OP_PROGRAM_LOAD_SINGLE        0x02    /* Write data into cache of SPI NAND chip with cache reset, single speed */
#define _SPI_NAND_OP_PROGRAM_LOAD_QUAD          0x32    /* Write data into cache of SPI NAND chip with cache reset, quad speed */
#define _SPI_NAND_OP_PROGRAM_LOAD_RAMDOM_SINGLE 0x84    /* Write data into cache of SPI NAND chip, single speed */
#define _SPI_NAND_OP_PROGRAM_LOAD_RAMDON_QUAD   0x34    /* Write data into cache of SPI NAND chip, quad speed */

#define _SPI_NAND_OP_PROGRAM_EXECUTE            0x10    /* Write data from cache into SPI NAND chip */
#define _SPI_NAND_OP_READ_ID                    0x9F    /* Read Manufacture ID and Device ID */
#define _SPI_NAND_OP_BLOCK_ERASE                0xD8    /* Erase Block */
#define _SPI_NAND_OP_RESET                      0xFF    /* Reset */
#define _SPI_NAND_OP_DIE_SELECT                 0xC2    /* Die Select */

/* SPI NAND register address of command set */
#define _SPI_NAND_ADDR_ECC                      0x90    /* Address of ECC Config */
#define _SPI_NAND_ADDR_PROTECT                  0xA0    /* Address of protection */
#define _SPI_NAND_ADDR_FEATURE                  0xB0    /* Address of feature */
#define _SPI_NAND_ADDR_STATUS                   0xC0    /* Address of status */
#define _SPI_NAND_ADDR_FEATURE_4                0xD0    /* Address of status 4 */
#define _SPI_NAND_ADDR_STATUS_5                 0xE0    /* Address of status 5 */
#define _SPI_NAND_ADDR_MANUFACTURE_ID           0x00    /* Address of Manufacture ID */
#define _SPI_NAND_ADDR_DEVICE_ID                0x01    /* Address of Device ID */

/* SPI NAND value of register address of command set */
#define _SPI_NAND_VAL_DISABLE_PROTECTION        0x0     /* Value for disable write protection */
#define _SPI_NAND_VAL_ENABLE_PROTECTION         0x38    /* Value for enable write protection */
#define _SPI_NAND_VAL_OIP                       0x1     /* OIP = Operaton In Progress */
#define _SPI_NAND_VAL_ERASE_FAIL                0x4     /* E_FAIL = Erase Fail */
#define _SPI_NAND_VAL_PROGRAM_FAIL              0x8     /* P_FAIL = Program Fail */


#define SPI_FLASH_CS_PIN GPIO_Pin_15
#define SPI_FLASH_SCK_PIN GPIO_Pin_10
#define SPI_FLASH_MISO_PIN GPIO_Pin_11
#define SPI_FLASH_MOSI_PIN GPIO_Pin_12

#define FLASH_DUMMY_BYTE 0xFF

/* 1st addressing cycle */
#define ADDR_1st_CYCLE(ADDR) (uint8_t)((ADDR)& 0xFF)
/* 2nd addressing cycle */
#define ADDR_2nd_CYCLE(ADDR) (uint8_t)(((ADDR)& 0xFF00) >> 8)
/* 3rd addressing cycle */
#define ADDR_3rd_CYCLE(ADDR) (uint8_t)(((ADDR)& 0xFF0000) >> 16)
/* 4th addressing cycle */
#define ADDR_4th_CYCLE(ADDR) (uint8_t)(((ADDR)& 0xFF000000) >> 24)

#define UNDEFINED_CMD 0xFF

static void spi_flash_chip_init(void);

typedef struct __attribute__((__packed__))
{
    uint32_t spare_offset;
    uint8_t mode_data;
    uint8_t unlock_data;
    uint8_t ecc_err_bits_mask;
    uint8_t ecc_err_bits_state;
    uint8_t read_dummy_prepend;
    uint8_t plane_select_have;
    uint8_t die_select_type;
    uint32_t freq;
} spi_conf_t;

static spi_conf_t spi_conf;

enum
{
    FLASH_OP_EMPTY = 0,
    FLASH_OP_ERASE = 1,
    FLASH_OP_WRITE = 2,
    FLASH_OP_READ  = 3,
    FLASH_OP_SPARE = 4,
};

static uint32_t flash_last_operation = FLASH_OP_EMPTY;
static uint32_t current_die = 0;

static void spi_flash_gpio_init()
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

static void spi_flash_gpio_uninit()
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

static inline void spi_flash_select_chip()
{
    GPIO_ResetBits(GPIOA, SPI_FLASH_CS_PIN);
}

static inline void spi_flash_deselect_chip()
{
    GPIO_SetBits(GPIOA, SPI_FLASH_CS_PIN);
}

static uint16_t spi_flash_get_baud_rate_prescaler(uint32_t spi_freq_khz)
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

static int spi_flash_init(void *conf, uint32_t conf_size)
{
    SPI_InitTypeDef spi_init;

    if (conf_size < sizeof(spi_conf_t))
        return -1;
    spi_conf = *(spi_conf_t *)conf;

    spi_flash_gpio_init();

    spi_flash_deselect_chip();

    /* Configure SPI */
    spi_init.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi_init.SPI_Mode = SPI_Mode_Master;
    spi_init.SPI_DataSize = SPI_DataSize_8b;
    spi_init.SPI_CPOL = SPI_CPOL_High;
    spi_init.SPI_CPHA = SPI_CPHA_2Edge;
    spi_init.SPI_NSS = SPI_NSS_Soft;
    spi_init.SPI_BaudRatePrescaler =
        spi_flash_get_baud_rate_prescaler(spi_conf.freq);
    spi_init.SPI_FirstBit = SPI_FirstBit_MSB;
    spi_init.SPI_CRCPolynomial = 7;
    SPI_Init(SPI3, &spi_init);

    /* Enable SPI */
    SPI_Cmd(SPI3, ENABLE);

    /* Clear data register */
    SPI_I2S_ReceiveData(SPI3);

    spi_flash_chip_init();

    return 0;
}

static void spi_flash_uninit()
{
    spi_flash_gpio_uninit();

    /* Disable SPI */
    SPI_Cmd(SPI3, DISABLE);
}

static uint8_t spi_flash_send_byte(uint8_t byte)
{
    /* Loop while DR register in not emplty */
    while (SPI_I2S_GetFlagStatus(SPI3, SPI_I2S_FLAG_TXE) == RESET);

    /* Send byte through the SPI3 peripheral to generate clock signal */
    SPI_I2S_SendData(SPI3, byte);

    /* Wait to receive a byte */
    while (SPI_I2S_GetFlagStatus(SPI3, SPI_I2S_FLAG_RXNE) == RESET);

    /* Return the byte read from the SPI bus */
    return SPI_I2S_ReceiveData(SPI3);
}

static inline uint8_t spi_flash_read_byte()
{
    return spi_flash_send_byte(FLASH_DUMMY_BYTE);
}

static void spi_flash_set_feature(uint8_t addr, uint8_t data)
{
    spi_flash_select_chip();
    spi_flash_send_byte(_SPI_NAND_OP_SET_FEATURE);
    spi_flash_send_byte(addr);
    spi_flash_send_byte(data);
    spi_flash_deselect_chip();
}

static void spi_flash_get_feature(uint8_t addr, uint8_t *data)
{
    spi_flash_select_chip();
    spi_flash_send_byte(_SPI_NAND_OP_GET_FEATURE);
    spi_flash_send_byte(addr);
    *data = spi_flash_read_byte();
    spi_flash_deselect_chip();
}

static uint32_t spi_flash_read_status()
{
    uint32_t timeout = 0x1000000;//TODO
    uint8_t status;

    do {
        spi_flash_get_feature(_SPI_NAND_ADDR_STATUS, &status);
    } while((status & _SPI_NAND_VAL_OIP) && timeout) ;

    if (!timeout)
        return FLASH_STATUS_TIMEOUT;

    switch(flash_last_operation){
        case FLASH_OP_ERASE:
            if(status & _SPI_NAND_VAL_ERASE_FAIL)
                return FLASH_STATUS_ERROR;
            break;
        case FLASH_OP_WRITE:
            if(status & _SPI_NAND_VAL_PROGRAM_FAIL)
                return FLASH_STATUS_ERROR;
            break;
        case FLASH_OP_READ:
            if((status & spi_conf.ecc_err_bits_mask) == spi_conf.ecc_err_bits_state)
                return FLASH_STATUS_ERROR;
            break;
        case FLASH_OP_SPARE:
        case FLASH_OP_EMPTY:
        default:
            break;
    }
    return FLASH_STATUS_READY;
}

static void spi_flash_select_die_cmd(uint32_t die)
{
    switch(spi_conf.die_select_type) {
    case 1: {
        spi_flash_select_chip();
        spi_flash_send_byte(_SPI_NAND_OP_DIE_SELECT);
        spi_flash_send_byte(die);
        spi_flash_deselect_chip();
        break;
    }
    case 2: {
        uint8_t feature;
        spi_flash_get_feature(_SPI_NAND_ADDR_FEATURE_4, &feature);
        if(die == 0) {
            feature &= ~(0x40);
        } else {
            feature |= 0x40;
        }
        spi_flash_set_feature(_SPI_NAND_ADDR_FEATURE_4, feature);
        break;
    }
    default:
        break;
    }
}

static void spi_flash_select_die(uint32_t page)
{
    uint32_t die = 0;
    if(spi_conf.die_select_type) {
        if(!spi_conf.plane_select_have)
            die = ((page >> 16) & 0xff);
        else
            die = ((page >> 17) & 0xff);
        if (current_die != die) {
            current_die = die;
            spi_flash_select_die_cmd(die);
        }
    }
}

static void spi_flash_read_id(chip_id_t *chip_id)
{
    spi_flash_select_chip();

    spi_flash_send_byte(_SPI_NAND_OP_READ_ID);
    spi_flash_send_byte(_SPI_NAND_ADDR_MANUFACTURE_ID);

    chip_id->maker_id = spi_flash_read_byte();
    chip_id->device_id = spi_flash_read_byte();
    chip_id->third_id = spi_flash_read_byte();
    chip_id->fourth_id = spi_flash_read_byte();
    chip_id->fifth_id = spi_flash_read_byte();
    chip_id->sixth_id = spi_flash_read_byte();

    spi_flash_deselect_chip();
}

static void spi_flash_chip_init(void)
{
    if(spi_conf.die_select_type) {
        spi_flash_select_die_cmd(0);
        if(spi_conf.mode_data != UNDEFINED_CMD)
            spi_flash_set_feature(_SPI_NAND_ADDR_FEATURE, spi_conf.mode_data);
        if(spi_conf.unlock_data != UNDEFINED_CMD)
            spi_flash_set_feature(_SPI_NAND_ADDR_PROTECT, spi_conf.unlock_data);
        spi_flash_select_die_cmd(1);
    }
    if(spi_conf.mode_data != UNDEFINED_CMD)
        spi_flash_set_feature(_SPI_NAND_ADDR_FEATURE, spi_conf.mode_data);
    if(spi_conf.unlock_data != UNDEFINED_CMD)
        spi_flash_set_feature(_SPI_NAND_ADDR_PROTECT, spi_conf.unlock_data);
}

static void spi_flash_write_enable()
{
    spi_flash_select_chip();
    spi_flash_send_byte(_SPI_NAND_OP_WRITE_ENABLE);
    spi_flash_deselect_chip();
}

//static void spi_flash_write_disable()
//{
//    spi_flash_select_chip();
//    spi_flash_send_byte(_SPI_NAND_OP_WRITE_DISABLE);
//    spi_flash_deselect_chip();
//}

static void spi_flash_program_load(uint8_t *buf, uint32_t page_size, uint32_t page)
{
    uint32_t i;
    uint32_t addr = 0;
    spi_flash_select_chip();

    spi_flash_send_byte(_SPI_NAND_OP_PROGRAM_LOAD_SINGLE);

    if(spi_conf.plane_select_have) {
        if((page >> 6)& (0x1))
            spi_flash_send_byte(ADDR_2nd_CYCLE(addr) | (0x10));
        else
            spi_flash_send_byte(ADDR_2nd_CYCLE(addr) & (0xef));
    } else {
        spi_flash_send_byte(ADDR_2nd_CYCLE(addr));
    }

    spi_flash_send_byte(ADDR_1st_CYCLE(addr));

    for (i = 0; i < page_size; i++)
        spi_flash_send_byte(buf[i]);

    spi_flash_deselect_chip();
}

static void spi_flash_write_page_async(uint8_t *buf, uint32_t page,
    uint32_t page_size)
{
    spi_flash_select_die(page);

    spi_flash_program_load(buf, page_size, page);

    spi_flash_write_enable();

    spi_flash_select_chip();
    spi_flash_send_byte(_SPI_NAND_OP_PROGRAM_EXECUTE);
    flash_last_operation = FLASH_OP_WRITE;
    spi_flash_send_byte(ADDR_3rd_CYCLE(page));
    spi_flash_send_byte(ADDR_2nd_CYCLE(page));
    spi_flash_send_byte(ADDR_1st_CYCLE(page));
    spi_flash_deselect_chip();
//    spi_flash_wait_operation_end();

//    spi_flash_write_disable();
}

static uint32_t spi_flash_load_page_into_cache(uint32_t page)
{
    spi_flash_select_die(page);

    spi_flash_select_chip();
    spi_flash_send_byte(_SPI_NAND_OP_PAGE_READ);
    flash_last_operation = FLASH_OP_READ;
    spi_flash_send_byte(ADDR_3rd_CYCLE(page));
    spi_flash_send_byte(ADDR_2nd_CYCLE(page));
    spi_flash_send_byte(ADDR_1st_CYCLE(page));
    spi_flash_deselect_chip();

    return spi_flash_read_status();
}

static uint32_t spi_flash_read_page(uint8_t *buf, uint32_t page, uint32_t data_size)
{
    uint32_t status = spi_flash_load_page_into_cache(page);
    uint32_t data_offset = 0;

    spi_flash_select_chip();
    spi_flash_send_byte(_SPI_NAND_OP_READ_FROM_CACHE_SINGLE);

    if(spi_conf.read_dummy_prepend)
        spi_flash_send_byte(FLASH_DUMMY_BYTE);

    if(spi_conf.plane_select_have) {
        if((page >> 6)& (0x1))
            spi_flash_send_byte(ADDR_2nd_CYCLE(data_offset) | (0x10));
        else
            spi_flash_send_byte(ADDR_2nd_CYCLE(data_offset) & (0xef));
    } else {
        spi_flash_send_byte(ADDR_2nd_CYCLE(data_offset));
    }

    spi_flash_send_byte(ADDR_1st_CYCLE(data_offset));

    if(!spi_conf.read_dummy_prepend)
        spi_flash_send_byte(FLASH_DUMMY_BYTE);

    for(uint32_t i = 0; i < data_size; i++)
          buf[i] = spi_flash_read_byte();

    spi_flash_deselect_chip();
    return status;
}

static uint32_t spi_flash_read_spare_data(uint8_t *buf, uint32_t page,
    uint32_t offset, uint32_t data_size)
{
    uint32_t status;

    spi_flash_select_die(page);

    spi_flash_select_chip();
    spi_flash_send_byte(_SPI_NAND_OP_PAGE_READ);
    flash_last_operation = FLASH_OP_SPARE;
    spi_flash_send_byte(ADDR_3rd_CYCLE(page));
    spi_flash_send_byte(ADDR_2nd_CYCLE(page));
    spi_flash_send_byte(ADDR_1st_CYCLE(page));
    spi_flash_deselect_chip();
    status = spi_flash_read_status();

    spi_flash_select_chip();
    spi_flash_send_byte(_SPI_NAND_OP_READ_FROM_CACHE_SINGLE);

    if(spi_conf.read_dummy_prepend)
        spi_flash_send_byte(FLASH_DUMMY_BYTE);

    offset += spi_conf.spare_offset;
    if(spi_conf.plane_select_have) {
        if((page >> 6)& (0x1))
            spi_flash_send_byte(ADDR_2nd_CYCLE(offset) | (0x10));
        else
            spi_flash_send_byte(ADDR_2nd_CYCLE(offset) & (0xef));
    } else {
        spi_flash_send_byte(ADDR_2nd_CYCLE(offset));
    }
    spi_flash_send_byte(ADDR_1st_CYCLE(offset));

    if(!spi_conf.read_dummy_prepend)
        spi_flash_send_byte(FLASH_DUMMY_BYTE);

    for(uint32_t i = 0; i < data_size; i++)
          buf[i] = spi_flash_read_byte();

    spi_flash_deselect_chip();
    return status;
}

static uint32_t spi_flash_erase_block(uint32_t page)
{
    spi_flash_select_die(page);

    spi_flash_write_enable();

    spi_flash_select_chip();

    spi_flash_send_byte(_SPI_NAND_OP_BLOCK_ERASE);
    flash_last_operation = FLASH_OP_ERASE;

    spi_flash_send_byte(ADDR_3rd_CYCLE(page));
    spi_flash_send_byte(ADDR_2nd_CYCLE(page));
    spi_flash_send_byte(ADDR_1st_CYCLE(page));

    spi_flash_deselect_chip();

    return spi_flash_read_status();
}

static inline bool spi_flash_is_bb_supported()
{
    return true;
}


flash_hal_t hal_spi_nand =
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

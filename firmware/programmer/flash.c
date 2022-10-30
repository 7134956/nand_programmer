/*  Copyright (C) 2020 NANDO authors
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "ch32v30x.h"

void flash_write_fast(uint32_t addr, uint8_t *data)
{
    __disable_irq();
    RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV2;
    FLASH_Unlock_Fast();
    FLASH_ErasePage_Fast(addr);
    FLASH_ProgramPage_Fast(addr, (uint32_t *)data);
    FLASH_Lock_Fast();
    RCC->CFGR0 &= ~(uint32_t)RCC_HPRE_DIV2;
    __enable_irq();
}

int flash_read(uint32_t addr, uint8_t *data, uint32_t data_len)
{
    uint32_t i;

    for(i = 0; i < data_len; i++)
        data[i] = *(uint8_t *) (addr + i);

    return i;
}

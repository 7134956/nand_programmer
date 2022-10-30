/*  Copyright (C) 2020 NANDO authors
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#ifndef _FLASH_H_
#define _FLASH_H_

void flash_write_fast(uint32_t addr, uint8_t *data);
int flash_read(uint32_t addr, uint8_t *data, uint32_t data_len);

#endif

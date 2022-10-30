/*  Copyright (C) 2020 NANDO authors
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "uart.h"
#include "version.h"
#include <stdio.h>
#include <string.h>

/* Flash layout
 * ------------ 0x08000000
 * |bootloader |
 * |14K        |
 * -------------0x08003800
 * |data       |
 * |2K         |
 * ------------ 0x08004000
 * |image1     |   
 * |120K       |
 * -------------0x08022000
 * |image2     |
 * |120K       |
 * -------------0x08040000
 */

#define STR(a) #a
#define MAKE_STR(a) STR(a)
#define VERSION "\r\nBootloader ver: " MAKE_STR(SW_VERSION_MAJOR) "." \
    MAKE_STR(SW_VERSION_MINOR) "." MAKE_STR(SW_VERSION_BUILD) "\r\n"

#define APP1_ADDRESS_OFFSET 0x4000
#define APP1_ADDRESS APP1_ADDRESS_OFFSET
#define APP2_ADDRESS_OFFSET 0x22000
#define APP2_ADDRESS APP2_ADDRESS_OFFSET

#define BOOT_DATA_ADDRESS 0x00003800

typedef void (*app_func_t)(void);
typedef struct __attribute__((__packed__))
{
    uint8_t active_image;
} config_t;

int main()
{
    app_func_t app;
    uint32_t jump_addr;
    volatile config_t *config = (config_t *)BOOT_DATA_ADDRESS;

    uart_init();
    print(VERSION);

    print("Start application: ");
    if (!config->active_image)
    {
        print("0\r\n");
        jump_addr = APP1_ADDRESS;
    }
    else
    {
        print("1\r\n");
        jump_addr = APP2_ADDRESS;
    }
    app = (app_func_t)jump_addr;
    app();

    return 0;
}

/*
 * Copyright (c) 2019 xiaofan <xfan1024@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2019-09-19     xiaofan         the first version
 */

#ifndef __modem_device_h__
#define __modem_device_h__

#include <rtthread.h>
#include <rtdevice.h>
#include <ppp/ppp.h>

void m6312_attach(const char *device_name, rt_base_t power_pin);

struct modem
{
    struct netif pppif;
    ppp_pcb *ppp;
    struct rt_completion comp;

    struct rt_serial_device *serial;
    rt_err_t (*prepare)(struct modem *modem);
};

struct rt_serial_device* modem_open_serial(const char *device_name);
void modem_attach(struct modem *modem);

#endif

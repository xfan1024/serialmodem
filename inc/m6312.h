/*
 * Copyright (c) 2019 xiaofan <xfan1024@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2019-09-19     xiaofan         the first version
 */

#ifndef __modem_m6312_h__
#define __modem_m6312_h__

#include "modem.h"

void m6312_attach(const char *device_name, rt_base_t power_pin);

#endif

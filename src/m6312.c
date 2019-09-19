/*
 * Copyright (c) 2019 xiaofan <xfan1024@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2019-09-19     xiaofan         the first version
 */

#include <m6312.h>
#include <chat.h>

#define DBG_TAG    "m6312"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

struct modem_m6312
{
    struct modem modem;
    rt_base_t power_pin;
};

#define M6312_POWER_ON  PIN_HIGH
#define M6312_POWER_OFF PIN_LOW


#define M6312_SET_APN       "AT+CGDCONT=1,\"IP\",\"" MODEM_APN "\""
#define M6312_SET_ATD       "ATD" MODEM_NUMBER
#define M6312_SOFT_RESET    "AT+CMRESET\r"

static void m6312_reset_chip(struct modem *modem)
{
    struct modem_m6312 *m6312 = (struct modem_m6312*)modem;
    if (m6312->power_pin > 0)
    {
        rt_pin_write(m6312->power_pin, M6312_POWER_OFF);
        rt_thread_mdelay(500);
        rt_pin_write(m6312->power_pin, M6312_POWER_ON);
    }
    else
    {
        rt_device_write(&modem->serial->parent, 0, "\r", 1);
        rt_thread_mdelay(500);
        rt_device_write(&modem->serial->parent, 0, M6312_SOFT_RESET, sizeof(M6312_SOFT_RESET)-1);
    }

    rt_thread_mdelay(5000);
}

static rt_err_t m6312_chat(struct modem *modem)
{
    static const struct modem_chat_data mcd[] =
    {
        { "AT",             MODEM_CHAT_RESP_OK,         10,     1},
        { "ATE0V1",         MODEM_CHAT_RESP_OK,         1,      1},
        { "ATS0=0",         MODEM_CHAT_RESP_OK,         1,      1},
        { M6312_SET_APN,    MODEM_CHAT_RESP_OK,         1,      5},
        { M6312_SET_ATD,    MODEM_CHAT_RESP_CONNECT,    1,      30},
    };
    return modem_chat(modem->serial, mcd, sizeof(mcd)/sizeof(mcd[0]));
}

static rt_err_t m6312_prepare(struct modem *modem)
{
    m6312_reset_chip(modem);
    return m6312_chat(modem);
}

void m6312_attach(const char *device_name, rt_base_t power_pin)
{
    struct rt_serial_device *serial;
    struct modem_m6312 *m6312;

    serial = modem_open_serial(device_name);
    if (!serial)
        return;

    m6312 = rt_malloc(sizeof(struct modem_m6312));
    if (!m6312)
        goto err;

    m6312->modem.prepare = m6312_prepare;
    m6312->modem.serial = serial;
    modem_attach(&m6312->modem);
    return;
err:
    if (serial)
        rt_device_close(&serial->parent);
    if (m6312)
        rt_free(m6312);
}

#ifdef MODEM_AUTO_ATTACH

static int m6312_auto_attach()
{
    m6312_attach(MODEM_DEVICE_NAME, MODEM_POWER_PIN);
    return RT_EOK;
}

INIT_APP_EXPORT(m6312_auto_attach);

#endif

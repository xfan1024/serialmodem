/*
 * Copyright (c) 2019 xiaofan <xfan1024@live.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2019-09-19     xiaofan         the first version
 */

#include <modem.h>
#include <pppnetif.h>
#include <lwip/dns.h>
#include <pppapi.h>

#define DBG_TAG    "modem"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

#ifndef MODEM_SERIAL_READ_MAX
#define MODEM_SERIAL_READ_MAX 48
#endif

#ifndef MODEM_STACK_SIZE
#define MODEM_STACK_SIZE (2000 + MODEM_SERIAL_READ_MAX)
#endif

#ifndef MODEM_THREAD_PRIORITY
#define MODEM_THREAD_PRIORITY (RT_THREAD_PRIORITY_MAX - 2)
#endif

#ifdef MODEM_RECONFIGURE_SERIAL
static void modem_reconfigure_serial(struct rt_serial_device *serial)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    config.baud_rate = MODEM_BAUD_RATE;
#ifdef MODEM_PARITY_NONE
    config.parity   = PARITY_NONE;
#endif
#ifdef MODEM_PARITY_ODD
    config.parity   = PARITY_ODD;
#endif
#ifdef MODEM_PARITY_EVEN
    config.parity   = PARITY_EVEN;
#endif
    serial->ops->configure(serial, &config);
}
#endif

struct rt_serial_device* modem_open_serial(const char *device_name)
{
    rt_uint16_t open_flag;
    rt_device_t device;
    rt_err_t err;

    device = rt_device_find(device_name);
    if (device == RT_NULL) {
        LOG_E("can not found: %s", device_name);
        return RT_NULL;
    }
    open_flag = RT_DEVICE_OFLAG_RDWR;
    if (device->flag & RT_DEVICE_FLAG_DMA_RX)
        open_flag |= RT_DEVICE_FLAG_DMA_RX;
    else if (device->flag & RT_DEVICE_FLAG_INT_RX)
        open_flag |= RT_DEVICE_FLAG_INT_RX;

    if (device->flag & RT_DEVICE_FLAG_DMA_TX)
        open_flag |= RT_DEVICE_FLAG_DMA_TX;
    else if (device->flag & RT_DEVICE_FLAG_INT_TX)
        open_flag |= RT_DEVICE_FLAG_INT_TX;

    err = rt_device_open(device, open_flag);
    if (err)
    {
        LOG_E("open(%s, 0x%x) fail", device_name, open_flag);
        return RT_NULL;
    }
#ifdef MODEM_RECONFIGURE_SERIAL
    modem_reconfigure_serial((struct rt_serial_device*)device);
#endif
    return (struct rt_serial_device*)device;
}

static rt_err_t modem_serial_cb(rt_device_t dev, rt_size_t size)
{
    struct rt_serial_device *serial = (struct rt_serial_device*)dev;
    struct modem *modem = serial->user_data;
    rt_completion_done(&modem->comp);
    return RT_EOK;
}

static u32_t modem_output_cb(ppp_pcb *ppp, u8_t *data, u32_t len, void *ctx)
{
    struct netif *pppif = ppp_netif(ppp);
    struct modem *modem = rt_container_of(pppif, struct modem, pppif);
    if (len)
    {
        LOG_D("send %u bytes", len);
        return rt_device_write(&modem->serial->parent, 0, data, len);
    }
    return 0;
}


static void modem_link_status_cb(ppp_pcb *ppp, int errCode, void *ctx)
{
    struct netif *pppif = ppp_netif(ppp);
    struct modem *modem = rt_container_of(pppif, struct modem, pppif);

    LWIP_UNUSED_ARG(ctx);

    switch(errCode)
    {
        case PPPERR_NONE: {             /* No error. */
            ppp_netdev_add(&modem->pppif);
            LOG_D("pppLinkStatusCallback: PPPERR_NONE");
#if LWIP_IPV4
            LOG_D("   our_ipaddr  = %s", ip4addr_ntoa(netif_ip4_addr(pppif)));
            LOG_D("   his_ipaddr  = %s", ip4addr_ntoa(netif_ip4_gw(pppif)));
            LOG_D("   netmask     = %s", ip4addr_ntoa(netif_ip4_netmask(pppif)));
#endif /* LWIP_IPV4 */
#if LWIP_DNS
            LOG_D("   dns1        = %s", ipaddr_ntoa(dns_getserver(0)));
            LOG_D("   dns2        = %s", ipaddr_ntoa(dns_getserver(1)));
#endif /* LWIP_DNS */
#if PPP_IPV6_SUPPORT
            LOG_D("   our6_ipaddr = %s", ip6addr_ntoa(netif_ip6_addr(pppif, 0)));
#endif /* PPP_IPV6_SUPPORT */
            break;
        }
        case PPPERR_PARAM: {           /* Invalid parameter. */
            LOG_D("pppLinkStatusCallback: PPPERR_PARAM");
            break;
        }
        case PPPERR_OPEN: {            /* Unable to open PPP session. */
            LOG_D("pppLinkStatusCallback: PPPERR_OPEN");
            break;
        }
        case PPPERR_DEVICE: {          /* Invalid I/O device for PPP. */
            LOG_D("pppLinkStatusCallback: PPPERR_DEVICE");
            break;
        }
        case PPPERR_ALLOC: {           /* Unable to allocate resources. */
            LOG_D("pppLinkStatusCallback: PPPERR_ALLOC");
            break;
        }
        case PPPERR_USER: {            /* User interrupt. */
            LOG_D("pppLinkStatusCallback: PPPERR_USER");
            break;
        }
        case PPPERR_CONNECT: {         /* Connection lost. */
            LOG_D("pppLinkStatusCallback: PPPERR_CONNECT");
            break;
        }
        case PPPERR_AUTHFAIL: {        /* Failed authentication challenge. */
            LOG_D("pppLinkStatusCallback: PPPERR_AUTHFAIL");
            break;
        }
        case PPPERR_PROTOCOL: {        /* Failed to meet protocol. */
            LOG_D("pppLinkStatusCallback: PPPERR_PROTOCOL");
            break;
        }
        case PPPERR_PEERDEAD: {        /* Connection timeout */
            LOG_D("pppLinkStatusCallback: PPPERR_PEERDEAD");
            break;
        }
        case PPPERR_IDLETIMEOUT: {     /* Idle Timeout */
            LOG_D("pppLinkStatusCallback: PPPERR_IDLETIMEOUT");
            break;
        }
        case PPPERR_CONNECTTIME: {     /* Max connect time reached */
            LOG_D("pppLinkStatusCallback: PPPERR_CONNECTTIME");
            break;
        }
        case PPPERR_LOOPBACK: {        /* Loopback detected */
            LOG_D("pppLinkStatusCallback: PPPERR_LOOPBACK");
            break;
        }
        default: {
            LOG_D("pppLinkStatusCallback: unknown errCode %d", errCode);
            break;
        }
    }

    if (errCode != PPPERR_NONE)
    {
        rt_completion_done(&modem->comp);
    }
}

static void modem_thread_entry(void *params)
{
    struct modem *modem = params;
    char rxbuf[MODEM_SERIAL_READ_MAX];
    rt_size_t rxlen;
    err_t lwip_err;

    modem->serial->user_data = modem;
    rt_completion_init(&modem->comp);
    rt_device_set_rx_indicate(&modem->serial->parent, modem_serial_cb);

    while (1)
    {
        if (modem->prepare && modem->prepare(modem))
        {
            LOG_W("Modem is not ready, try again after 30s");
            rt_thread_mdelay(30000);
            continue;
        }

        // start ppp connection
        modem->ppp = pppos_create(&modem->pppif, modem_output_cb, modem_link_status_cb, NULL);
        if (modem->ppp == RT_NULL)
        {
            LOG_E("create ppp fail");
            return;
        }
        ppp_set_usepeerdns(modem->ppp, 1);
        lwip_err = pppapi_connect(modem->ppp, 0);
        if (lwip_err != ERR_OK)
        {
            LOG_E("ppp_connect fail");
            return;
        }

        netif_set_default(&modem->pppif);

        // check serial rx event and ppp connection broken error event
        do
        {
            rt_completion_wait(&modem->comp, RT_WAITING_FOREVER);
            // handle serial data coming
            do
            {
                rxlen = rt_device_read(&modem->serial->parent, 0, rxbuf, sizeof(rxbuf));
                if (rxlen)
                {
                    LOG_D("recv %u bytes", rxlen);
                    pppos_input_tcpip(modem->ppp, (u8_t*)rxbuf, rxlen);
                }
            } while (rxlen > 0);

            // handle ppp connection broken
            if (modem->ppp->err_code != PPPERR_NONE)
                break;
        } while (1);

        ppp_netdev_del(&modem->pppif);

        if (modem->ppp->phase != PPP_PHASE_DEAD)
            pppapi_close(modem->ppp, 1);

        while (pppapi_free(modem->ppp) != ERR_OK)
        {
            LOG_W("ppp free fail, try again later");
            rt_thread_mdelay(1000);
        }
        LOG_D("ppp was freed");
    }
}

void modem_attach(struct modem *modem)
{
    rt_thread_t tid;
    tid = rt_thread_create("modem", modem_thread_entry, modem,
        MODEM_STACK_SIZE, MODEM_THREAD_PRIORITY, 2);
    if (tid == RT_NULL)
    {
        LOG_E("create modem thread fail");
        return;
    }

    rt_thread_startup(tid);
}

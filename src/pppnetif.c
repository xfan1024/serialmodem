/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2019-08-15     xiangxistu      the first version
 */

#include "lwip/opt.h"
#include "pppnetif.h"

#ifdef RT_USING_NETDEV

#include "lwip/ip.h"
#include "lwip/netdb.h"
#include <netdev.h>

static int ppp_netdev_set_up(struct netdev *netif)
{
    netif_set_up((struct netif *)netif->user_data);
    return ERR_OK;
}

static int ppp_netdev_set_down(struct netdev *netif)
{
    netif_set_down((struct netif *)netif->user_data);
    return ERR_OK;
}

static int ppp_netdev_set_addr_info(struct netdev *netif, ip_addr_t *ip_addr, ip_addr_t *netmask, ip_addr_t *gw)
{
    if (ip_addr && netmask && gw)
    {
        netif_set_addr((struct netif *)netif->user_data, ip_2_ip4(ip_addr), ip_2_ip4(netmask), ip_2_ip4(gw));
    }
    else
    {
        if (ip_addr)
        {
            netif_set_ipaddr((struct netif *)netif->user_data, ip_2_ip4(ip_addr));
        }

        if (netmask)
        {
            netif_set_netmask((struct netif *)netif->user_data, ip_2_ip4(netmask));
        }

        if (gw)
        {
            netif_set_gw((struct netif *)netif->user_data, ip_2_ip4(gw));
        }
    }

    return ERR_OK;
}

#ifdef RT_LWIP_DNS
static int ppp_netdev_set_dns_server(struct netdev *netif, uint8_t dns_num, ip_addr_t *dns_server)
{
    extern void dns_setserver(uint8_t dns_num, const ip_addr_t *dns_server);
    dns_setserver(dns_num, dns_server);
    return ERR_OK;
}
#endif /* RT_LWIP_DNS */

#ifdef RT_USING_FINSH
#ifdef RT_LWIP_USING_PING
extern int lwip_ping_recv(int s, int *ttl);
extern err_t lwip_ping_send(int s, ip_addr_t *addr, int size);

int ppp_netdev_ping(struct netdev *netif, const char *host, size_t data_len,
                        uint32_t timeout, struct netdev_ping_resp *ping_resp)
{
    int s, ttl, recv_len, result = 0;
    int elapsed_time;
    rt_tick_t recv_start_tick;
#if LWIP_VERSION_MAJOR >= 2U
    struct timeval recv_timeout = { timeout / RT_TICK_PER_SECOND, timeout % RT_TICK_PER_SECOND };
#else
    int recv_timeout = timeout * 1000UL / RT_TICK_PER_SECOND;
#endif
    ip_addr_t target_addr;
    struct addrinfo hint, *res = RT_NULL;
    struct sockaddr_in *h = RT_NULL;
    struct in_addr ina;

    RT_ASSERT(netif);
    RT_ASSERT(host);
    RT_ASSERT(ping_resp);

    rt_memset(&hint, 0x00, sizeof(hint));

    /* convert URL to IP */
    if (lwip_getaddrinfo(host, RT_NULL, &hint, &res) != 0)
    {
        return -RT_ERROR;
    }
    rt_memcpy(&h, &res->ai_addr, sizeof(struct sockaddr_in *));
    rt_memcpy(&ina, &h->sin_addr, sizeof(ina));
    lwip_freeaddrinfo(res);
    if (inet_aton(inet_ntoa(ina), &target_addr) == 0)
    {
        return -RT_ERROR;
    }
    rt_memcpy(&(ping_resp->ip_addr), &target_addr, sizeof(ip_addr_t));

    /* new a socket */
    if ((s = lwip_socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP)) < 0)
    {
        return -RT_ERROR;
    }

    lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

    if (lwip_ping_send(s, &target_addr, data_len) == ERR_OK)
    {
        recv_start_tick = rt_tick_get();
        if ((recv_len = lwip_ping_recv(s, &ttl)) >= 0)
        {
            elapsed_time = (rt_tick_get() - recv_start_tick) * 1000UL / RT_TICK_PER_SECOND;
            ping_resp->data_len = recv_len;
            ping_resp->ttl = ttl;
            ping_resp->ticks = elapsed_time;
        }
        else
        {
            result = -RT_ETIMEOUT;
            goto __exit;
        }
    }
    else
    {
        result = -RT_ETIMEOUT;
        goto __exit;
    }

__exit:
    lwip_close(s);

    return result;
}
#endif /* RT_LWIP_USING_PING */

#if defined (RT_LWIP_TCP) || defined (RT_LWIP_UDP)
void ppp_netdev_netstat(struct netdev *netif)
{
    extern void list_tcps(void);
    extern void list_udps(void);

#ifdef RT_LWIP_TCP
    list_tcps();
#endif
#ifdef RT_LWIP_UDP
    list_udps();
#endif
}
#endif /* RT_LWIP_TCP || RT_LWIP_UDP */
#endif /* RT_USING_FINSH */

const struct netdev_ops ppp_netdev_ops =
{
    ppp_netdev_set_up,
    ppp_netdev_set_down,

    ppp_netdev_set_addr_info,
#ifdef RT_LWIP_DNS
    ppp_netdev_set_dns_server,
#else
    NULL,
#endif /* RT_LWIP_DNS */

    NULL,

#ifdef RT_USING_FINSH
#ifdef RT_LWIP_USING_PING
    ppp_netdev_ping,
#else
    NULL,
#endif /* RT_LWIP_USING_PING */

#if defined (RT_LWIP_TCP) || defined (RT_LWIP_UDP)
    ppp_netdev_netstat,
#endif /* RT_LWIP_TCP || RT_LWIP_UDP */
#endif /* RT_USING_FINSH */
};

rt_err_t ppp_netdev_add(struct netif *ppp_netif)
{
#define LWIP_NETIF_NAME_LEN 2
    int result = 0;
    struct netdev *netdev = RT_NULL;
    char name[LWIP_NETIF_NAME_LEN + 1] = {0};

    RT_ASSERT(ppp_netif);

    netdev = (struct netdev *)rt_calloc(1, sizeof(struct netdev));
    if (netdev == RT_NULL)
    {
        return -ERR_IF;
    }

#ifdef SAL_USING_LWIP
    extern int sal_lwip_netdev_set_pf_info(struct netdev *netdev);

    /* set the lwIP network interface device protocol family information */
    sal_lwip_netdev_set_pf_info(netdev);
#endif /* SAL_USING_LWIP */

    rt_strncpy(name, ppp_netif->name, LWIP_NETIF_NAME_LEN);
    result = netdev_register(netdev, name, (void *)ppp_netif);

    /* Update netdev info after registered */
    netdev->flags = ppp_netif->flags;
    netdev->mtu = ppp_netif->mtu;
    netdev->ops = &ppp_netdev_ops;
    netdev->hwaddr_len =  0;
    netdev->ip_addr = ppp_netif->ip_addr;
    netdev->gw = ppp_netif->gw;
    netdev->netmask = ppp_netif->netmask;

    {
        extern const ip_addr_t* dns_getserver(u8_t numdns);

        netdev_low_level_set_dns_server(netdev, 0, dns_getserver(0));
        netdev_low_level_set_dns_server(netdev, 1, dns_getserver(1));
    }


    return result;
}

void ppp_netdev_del(struct netif *ppp_netif)
{
    char name[LWIP_NETIF_NAME_LEN + 1];
    struct netdev *netdev;

    RT_ASSERT(ppp_netif);

    rt_strncpy(name, ppp_netif->name, LWIP_NETIF_NAME_LEN);
    netdev = netdev_get_by_name(name);
    if (netdev)
    {
        netdev_unregister(netdev);
        rt_free(netdev);
    }
}

#endif  /* RT_USING_NETDEV */

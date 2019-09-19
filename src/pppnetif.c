/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author          Notes
 * 2019-08-15     xiangxistu      the first version
 * 2019-09-19     xiaofan         use lwip_netdev_ops instead of ppp_netdev_ops
 */

#include "lwip/opt.h"
#include "pppnetif.h"

#ifdef RT_USING_NETDEV

#include "lwip/ip.h"
#include "lwip/netdb.h"
#include <netdev.h>

extern const struct netdev_ops lwip_netdev_ops;

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
    netdev->ops = &lwip_netdev_ops;
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

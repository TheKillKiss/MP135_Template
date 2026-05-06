/**
 * @file
 * Ethernet network interface for lwIP.
 */

#ifndef LWIP_ARCH_ETHERNETIF_H
#define LWIP_ARCH_ETHERNETIF_H

#include "lwip/err.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

err_t ethernetif_init(struct netif *netif);
void ethernetif_input(struct netif *netif);
void ethernetif_update_link(struct netif *netif);
void ethernetif_notify_rx(void);

#ifdef __cplusplus
}
#endif

#endif /* LWIP_ARCH_ETHERNETIF_H */

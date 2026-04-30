/**
 * @file
 * Ethernet Interface Skeleton
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/*
 * This file is a skeleton for developing Ethernet network interface
 * drivers for lwIP. Add code to the low_level functions and do a
 * search-and-replace for the word "ethernetif" to replace it with
 * something that better describes your network interface.
 */

#include "lwip/opt.h"

// #if 0 /* don't build, this is only a skeleton, see previous comment */

#include "ethernetif.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "netif/ppp/pppoe.h"

#include "main.h"
#include "eth.h"
#include "bsp_YT8531C.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'n'

#define ETH_TX_BUFFER_SIZE    1536U
#define ETH_RX_BUFFER_SIZE    1524U
#define ETH_RX_BUFFER_COUNT   (ETH_RX_DESC_CNT * 2U)

static uint8_t eth_tx_buffer[ETH_TX_BUFFER_SIZE];
static uint8_t eth_rx_buffer[ETH_RX_BUFFER_COUNT][ETH_RX_BUFFER_SIZE];

extern ETH_HandleTypeDef heth1;
extern ETH_TxPacketConfigTypeDef TxConfig;

static yt8531c_Object_t YT8531C;
static ETH_BufferTypeDef eth_rx_app_buffer[ETH_RX_BUFFER_COUNT];
static uint32_t eth_rx_buffer_index;

/**
 * Helper struct to hold private data used to operate your ethernet interface.
 * Keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif.
 * But this is only an example, anyway...
 */
struct ethernetif {
  struct eth_addr *ethaddr;
  /* Add whatever per-interface state that is needed here. */
};

static int32_t ETH_PHY_IO_Init(void)
{
    return 0;
}

static int32_t ETH_PHY_IO_DeInit(void)
{
    return 0;
}

static int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t Reg, uint32_t *pValue)
{
    if(HAL_ETH_ReadPHYRegister(&heth1, DevAddr, Reg, pValue) != HAL_OK) {
      return YT8531C_STATUS_READ_ERROR;
    }

    return YT8531C_STATUS_OK;
}

static int32_t ETH_PHY_IO_GetTick(void)
{
    return (int32_t)HAL_GetTick();
}

static int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t Reg, uint32_t Value)
{
    if (HAL_ETH_WritePHYRegister(&heth1, DevAddr, Reg, Value) != HAL_OK) {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

static err_t ethernetif_phy_init(void)
{
    yt8531c_IOCtx_t ioctx;

    ioctx.Init     = ETH_PHY_IO_Init;
    ioctx.DeInit   = ETH_PHY_IO_DeInit;
    ioctx.ReadReg  = ETH_PHY_IO_ReadReg;
    ioctx.WriteReg = ETH_PHY_IO_WriteReg;
    ioctx.GetTick  = ETH_PHY_IO_GetTick;

    if (YT8531C_RegisterBusIO(&YT8531C, &ioctx) != YT8531C_STATUS_OK) {
        return ERR_IF;
    }

    if (YT8531C_Init(&YT8531C) != YT8531C_STATUS_OK) {
        return ERR_IF;
    }

    if (YT8531C_XtalInit() != YT8531C_STATUS_OK) {
        return ERR_IF;
    }

    if (YT8531C_ConfigRGMII_Delay(&YT8531C) != YT8531C_STATUS_OK) {
        return ERR_IF;
    }

    YT8531C_LED_Init();

    if (YT8531C_DisablePowerDownMode(&YT8531C) != YT8531C_STATUS_OK) {
        return ERR_IF;
    }

    if (YT8531C_StartAutoNego(&YT8531C) != YT8531C_STATUS_OK) {
        return ERR_IF;
    }

    return ERR_OK;
}

void HAL_ETH_RxAllocateCallback(ETH_HandleTypeDef *heth, uint8_t **buff)
{
    (void)heth;

    *buff = eth_rx_buffer[eth_rx_buffer_index];
    eth_rx_buffer_index++;

    if (eth_rx_buffer_index >= ETH_RX_BUFFER_COUNT) {
        eth_rx_buffer_index = 0U;
    }
}

void HAL_ETH_RxLinkCallback(ETH_HandleTypeDef *heth,
                            void **pStart,
                            void **pEnd,
                            uint8_t *buff,
                            uint16_t Length)
{
    ETH_BufferTypeDef *rx_buffer;
    uint32_t buffer_start;
    uint32_t buffer_offset;
    uint32_t index;

    (void)heth;

    if ((buff == NULL) || (Length == 0U)) {
        return;
    }

    buffer_start = (uint32_t)&eth_rx_buffer[0][0];
    if (((uint32_t)buff < buffer_start) ||
        ((uint32_t)buff >= (buffer_start + sizeof(eth_rx_buffer)))) {
        return;
    }

    buffer_offset = (uint32_t)buff - buffer_start;
    index = buffer_offset / ETH_RX_BUFFER_SIZE;

    rx_buffer = &eth_rx_app_buffer[index];
    rx_buffer->buffer = buff;
    rx_buffer->len = Length;
    rx_buffer->next = NULL;

    if (*pStart == NULL) {
        *pStart = rx_buffer;
    } else {
        ((ETH_BufferTypeDef *)(*pEnd))->next = rx_buffer;
    }

    *pEnd = rx_buffer;
}

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void low_level_init(struct netif *netif)
{
    struct ethernetif *ethernetif = netif->state;
    int32_t link_state;

    (void)ethernetif;

    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    netif->hwaddr[0] = 0x00;
    netif->hwaddr[1] = 0x80;
    netif->hwaddr[2] = 0xE1;
    netif->hwaddr[3] = 0x00;
    netif->hwaddr[4] = 0x00;
    netif->hwaddr[5] = 0x00;

    netif->mtu = 1500;

    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

    MX_ETH1_Init();

    if (ethernetif_phy_init() != ERR_OK) {
        return;
    }

    link_state = YT8531C_GetLinkState(&YT8531C);

    if ((link_state == YT8531C_STATUS_10MBITS_FULLDUPLEX)  ||
        (link_state == YT8531C_STATUS_10MBITS_HALFDUPLEX)  ||
        (link_state == YT8531C_STATUS_100MBITS_FULLDUPLEX) ||
        (link_state == YT8531C_STATUS_100MBITS_HALFDUPLEX) ||
        (link_state == YT8531C_STATUS_1000MBITS_FULLDUPLEX)||
        (link_state == YT8531C_STATUS_1000MBITS_HALFDUPLEX)) {

        netif_set_link_up(netif);
        HAL_ETH_Start_IT(&heth1);
    } else {
        netif_set_link_down(netif);
        HAL_ETH_Stop_IT(&heth1);
    }
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become available since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q;
    uint8_t *buffer;
    uint32_t framelen = 0;
    ETH_BufferTypeDef txbuffer;
    HAL_StatusTypeDef hal_status;

    (void)netif;

#if ETH_PAD_SIZE
    pbuf_remove_header(p, ETH_PAD_SIZE);
#endif

    if (p->tot_len > ETH_TX_BUFFER_SIZE) {
#if ETH_PAD_SIZE
        pbuf_add_header(p, ETH_PAD_SIZE);
#endif
        LINK_STATS_INC(link.lenerr);
        return ERR_BUF;
    }

    buffer = eth_tx_buffer;

    for (q = p; q != NULL; q = q->next) {
        memcpy(&buffer[framelen], q->payload, q->len);
        framelen += q->len;
    }

#if defined(CACHE_USE)
    // SCB_CleanDCache_by_Addr((uint32_t *)eth_tx_buffer, (int32_t)framelen);
#endif

    memset(&txbuffer, 0, sizeof(txbuffer));
    txbuffer.buffer = eth_tx_buffer;
    txbuffer.len    = framelen;
    txbuffer.next   = NULL;

    TxConfig.Length = framelen;
    TxConfig.TxBuffer = &txbuffer;
    TxConfig.pData = NULL;

    hal_status = HAL_ETH_Transmit(&heth1, &TxConfig, 100U);

#if ETH_PAD_SIZE
    pbuf_add_header(p, ETH_PAD_SIZE);
#endif

    if (hal_status != HAL_OK) {
        LINK_STATS_INC(link.err);
        return ERR_IF;
    }

    MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);

    if (((u8_t *)p->payload)[0] & 1) {
        MIB2_STATS_NETIF_INC(netif, ifoutnucastpkts);
    } else {
        MIB2_STATS_NETIF_INC(netif, ifoutucastpkts);
    }

    LINK_STATS_INC(link.xmit);

    return ERR_OK;
}

static struct pbuf *low_level_input(struct netif *netif)
{
    struct pbuf *p = NULL;
    struct pbuf *q;
    ETH_BufferTypeDef *rx_buffer;
    ETH_BufferTypeDef *rx_buffer_it;
    uint16_t len = 0;
    uint16_t copied = 0;
    uint8_t *payload;

    (void)netif;

    rx_buffer = NULL;

    if (HAL_ETH_ReadData(&heth1, (void **)&rx_buffer) != HAL_OK) {
        return NULL;
    }

    if (rx_buffer == NULL) {
        return NULL;
    }

    rx_buffer_it = rx_buffer;

    while (rx_buffer_it != NULL) {
        len += rx_buffer_it->len;
        rx_buffer_it = rx_buffer_it->next;
    }

    if (len == 0U) {
        return NULL;
    }

#if ETH_PAD_SIZE
    len += ETH_PAD_SIZE;
#endif

    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p == NULL) {
        LINK_STATS_INC(link.memerr);
        LINK_STATS_INC(link.drop);
        MIB2_STATS_NETIF_INC(netif, ifindiscards);
        return NULL;
    }

#if ETH_PAD_SIZE
    pbuf_remove_header(p, ETH_PAD_SIZE);
#endif

    rx_buffer_it = rx_buffer;
    q = p;
    copied = 0;

    while ((q != NULL) && (rx_buffer_it != NULL)) {
        uint16_t copy_len;
        uint16_t remain_in_q;

        payload = (uint8_t *)q->payload;
        remain_in_q = q->len;

        while ((remain_in_q > 0U) && (rx_buffer_it != NULL)) {
            copy_len = rx_buffer_it->len;

            if (copy_len > remain_in_q) {
                copy_len = remain_in_q;
            }

#if defined(CACHE_USE)
            // SCB_InvalidateDCache_by_Addr((uint32_t *)rx_buffer_it->buffer,
                                          // (int32_t)rx_buffer_it->len);
#endif

            memcpy(payload, rx_buffer_it->buffer, copy_len);

            payload += copy_len;
            copied += copy_len;
            remain_in_q -= copy_len;

            rx_buffer_it = rx_buffer_it->next;
        }

        q = q->next;
    }

#if ETH_PAD_SIZE
    pbuf_add_header(p, ETH_PAD_SIZE);
#endif

    MIB2_STATS_NETIF_ADD(netif, ifinoctets, p->tot_len);

    if (((u8_t *)p->payload)[0] & 1) {
        MIB2_STATS_NETIF_INC(netif, ifinnucastpkts);
    } else {
        MIB2_STATS_NETIF_INC(netif, ifinucastpkts);
    }

    LINK_STATS_INC(link.recv);

    return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
void
ethernetif_input(struct netif *netif)
{
  // struct ethernetif *ethernetif;
  // struct eth_hdr *ethhdr;
  struct pbuf *p;

  // ethernetif = netif->state;

  while (1) {
    /* move received packet into a new pbuf */
    p = low_level_input(netif);
    /* if no packet could be read, silently ignore this */
    if (p == NULL) {
      break;
    }

    /* pass all packets to ethernet_input, which decides what packets it supports */
    if (netif->input(p, netif) != ERR_OK) {
      LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
      pbuf_free(p);
    }
  }
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
ethernetif_init(struct netif *netif)
{
  struct ethernetif *ethernetif;

  LWIP_ASSERT("netif != NULL", (netif != NULL));

  ethernetif = mem_malloc(sizeof(struct ethernetif));
  if (ethernetif == NULL) {
    LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: out of memory\n"));
    return ERR_MEM;
  }

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 1000000000);

  netif->state = ethernetif;
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
#if LWIP_IPV4
  netif->output = etharp_output;
#endif /* LWIP_IPV4 */
#if LWIP_IPV6
  netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */
  netif->linkoutput = low_level_output;

  ethernetif->ethaddr = (struct eth_addr *) & (netif->hwaddr[0]);

  /* initialize the hardware */
  low_level_init(netif);

  return ERR_OK;
}

void ethernetif_update_link(struct netif *netif)
{
    int32_t link_state;

    link_state = YT8531C_GetLinkState(&YT8531C);

    switch (link_state) {
    case YT8531C_STATUS_10MBITS_FULLDUPLEX:
    case YT8531C_STATUS_10MBITS_HALFDUPLEX:
    case YT8531C_STATUS_100MBITS_FULLDUPLEX:
    case YT8531C_STATUS_100MBITS_HALFDUPLEX:
    case YT8531C_STATUS_1000MBITS_FULLDUPLEX:
    case YT8531C_STATUS_1000MBITS_HALFDUPLEX:
        if (!netif_is_link_up(netif)) {
            netif_set_link_up(netif);
            HAL_ETH_Start_IT(&heth1);
        }
        break;

    case YT8531C_STATUS_LINK_DOWN:
    default:
        if (netif_is_link_up(netif)) {
            HAL_ETH_Stop_IT(&heth1);
            netif_set_link_down(netif);
        }
        break;
    }
}

// #endif /* 0 */

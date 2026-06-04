#include "lwip.h"
#include "main.h"
#include "ethernetif.h"
#include "lwip/tcpip.h"
#include "lwip/apps/lwiperf.h"
#include "os.h"

static OS_TCB  EthIfTaskTCB;
static CPU_STK EthIfTaskStk[ETHIF_TASK_STK_SIZE];
static OS_FLAG_GRP EthIfFlags;
static void   *IperfSession;

struct netif gnetif;

static void EthIfTask(void *p_arg);
static void IperfReport(void *arg,
                        enum lwiperf_report_type report_type,
                        const ip_addr_t *local_addr,
                        u16_t local_port,
                        const ip_addr_t *remote_addr,
                        u16_t remote_port,
                        u32_t bytes_transferred,
                        u32_t ms_duration,
                        u32_t bandwidth_kbitpsec);

void LwIP_Init(void)
{
    OS_ERR err;
    ip_addr_t ipaddr;
    ip_addr_t netmask;
    ip_addr_t gw;

    OSFlagCreate(&EthIfFlags, "Eth If Flags", 0u, &err);
    if (err != OS_ERR_NONE) {
        Error_Handler();
    }

    tcpip_init(NULL, NULL);

#if LWIP_DHCP
    ip_addr_set_zero_ip4(&ipaddr);
    ip_addr_set_zero_ip4(&netmask);
    ip_addr_set_zero_ip4(&gw);
#else
    IP4_ADDR(&ipaddr,  192, 168, 6, 6);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw,      192, 168, 6, 1);
#endif

    netif_add(&gnetif,
              &ipaddr,
              &netmask,
              &gw,
              NULL,
              ethernetif_init,
              tcpip_input);

    netif_set_default(&gnetif);
    netif_set_up(&gnetif);

#if LWIP_DHCP
    if (dhcp_start(&gnetif) != ERR_OK) {
        Error_Handler();
    }
#endif

    IperfSession = lwiperf_start_tcp_server_default(IperfReport, NULL);
    if (IperfSession == NULL) {
        Error_Handler();
    }

    OSTaskCreate((OS_TCB     *)&EthIfTaskTCB,
                 (CPU_CHAR   *)"EthIf Task",
                 (OS_TASK_PTR )EthIfTask,
                 (void       *)0,
                 (OS_PRIO     )ETHIF_TASK_PRIO,
                 (CPU_STK    *)&EthIfTaskStk[0],
                 (CPU_STK_SIZE)ETHIF_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)ETHIF_TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                 (OS_ERR     *)&err);

    if (err != OS_ERR_NONE) {
        Error_Handler();
    }
}

static void EthIfTask(void *p_arg)
{
    OS_ERR err;
    CPU_TS ts;
    OS_FLAGS flags;

    (void)p_arg;

    while (1) {
        flags = OSFlagPend(&EthIfFlags,
                           ETHIF_FLAG_ALL,
                           500u,
                           OS_OPT_PEND_FLAG_SET_ANY |
                           OS_OPT_PEND_FLAG_CONSUME |
                           OS_OPT_PEND_BLOCKING,
                           &ts,
                           &err);

        if (err == OS_ERR_NONE) {
            if ((flags & ETHIF_FLAG_LINK) != 0u) {
                ethernetif_update_link(&gnetif);
            }

            if (((flags & ETHIF_FLAG_RX) != 0u) && netif_is_link_up(&gnetif)) {
                ethernetif_input(&gnetif);
            }
        } else if (err == OS_ERR_TIMEOUT) {
            ethernetif_update_link(&gnetif);
        }
    }
}

void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
    (void)heth;

    ethernetif_notify_rx();
}

void ethernetif_notify_rx(void)
{
    OS_ERR err;

    OSFlagPost(&EthIfFlags,
               ETHIF_FLAG_RX,
               OS_OPT_POST_FLAG_SET,
               &err);
}

void ethernetif_notify_link(void)
{
    OS_ERR err;

    OSFlagPost(&EthIfFlags,
               ETHIF_FLAG_LINK,
               OS_OPT_POST_FLAG_SET,
               &err);
}

static void IperfReport(void *arg,
                        enum lwiperf_report_type report_type,
                        const ip_addr_t *local_addr,
                        u16_t local_port,
                        const ip_addr_t *remote_addr,
                        u16_t remote_port,
                        u32_t bytes_transferred,
                        u32_t ms_duration,
                        u32_t bandwidth_kbitpsec)
{
    (void)arg;
    (void)report_type;
    (void)local_addr;
    (void)local_port;
    (void)remote_addr;
    (void)remote_port;
    (void)bytes_transferred;
    (void)ms_duration;
    (void)bandwidth_kbitpsec;
}

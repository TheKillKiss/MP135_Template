#ifndef APP_LWIP_H
#define APP_LWIP_H


#define ETHIF_TASK_PRIO        6u
#define ETHIF_TASK_STK_SIZE    1024u
#define ETHIF_FLAG_RX          ((OS_FLAGS)0x01u)
#define ETHIF_FLAG_LINK        ((OS_FLAGS)0x02u)
#define ETHIF_FLAG_ALL         (ETHIF_FLAG_RX | ETHIF_FLAG_LINK)

void LwIP_Init(void);

#endif

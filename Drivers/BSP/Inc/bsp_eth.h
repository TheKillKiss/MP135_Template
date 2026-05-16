#ifndef BSP_ETH_H__
#define BSP_ETH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rtconfig.h"
#include "main.h"

extern ETH_HandleTypeDef heth1;
extern ETH_TxPacketConfigTypeDef TxConfig;

int BSP_ETH_Init(void);

#ifdef __cplusplus
}
#endif

#endif

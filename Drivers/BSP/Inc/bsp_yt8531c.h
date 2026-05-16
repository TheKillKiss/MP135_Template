#ifndef BSP_YT8531C_H__
#define BSP_YT8531C_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define YT8531C_STATUS_READ_ERROR            ((int32_t)-5)
#define YT8531C_STATUS_WRITE_ERROR           ((int32_t)-4)
#define YT8531C_STATUS_ADDRESS_ERROR         ((int32_t)-3)
#define YT8531C_STATUS_RESET_TIMEOUT         ((int32_t)-2)
#define YT8531C_STATUS_ERROR                 ((int32_t)-1)
#define YT8531C_STATUS_OK                    ((int32_t) 0)

#define YT8531C_STATUS_LINK_DOWN             ((int32_t) 1)
#define YT8531C_STATUS_100MBITS_FULLDUPLEX   ((int32_t) 2)
#define YT8531C_STATUS_100MBITS_HALFDUPLEX   ((int32_t) 3)
#define YT8531C_STATUS_10MBITS_FULLDUPLEX    ((int32_t) 4)
#define YT8531C_STATUS_10MBITS_HALFDUPLEX    ((int32_t) 5)
#define YT8531C_STATUS_AUTONEGO_NOTDONE      ((int32_t) 6)
#define YT8531C_STATUS_1000MBITS_FULLDUPLEX  ((int32_t) 7)
#define YT8531C_STATUS_1000MBITS_HALFDUPLEX  ((int32_t) 8)

#define YT8531C_MAX_DEV_ADDR                 ((uint32_t)31U)

#define YT8531C_BCR                          ((uint16_t)0x0000U)
#define YT8531C_BSR                          ((uint16_t)0x0001U)
#define YT8531C_PHYI1R                       ((uint16_t)0x0002U)
#define YT8531C_PHYI2R                       ((uint16_t)0x0003U)
#define YT8531C_PHYSCSR                      ((uint16_t)0x0011U)
#define YT8531C_IMR                          ((uint16_t)0x0012U)
#define YT8531C_ISR                          ((uint16_t)0x0013U)

#define YT8531C_PHY_ID1_EXPECTED             ((uint16_t)0x4F51U)
#define YT8531C_PHY_ID2_OUI_MSB_EXPECTED     ((uint16_t)0x003AU)
#define YT8531C_PHY_ID2_TYPE_NO_EXPECTED     ((uint16_t)0x0011U)

#define YT8531C_BCR_SOFT_RESET               ((uint16_t)0x8000U)
#define YT8531C_BCR_SPEED_SEL_LSB            ((uint16_t)0x2000U)
#define YT8531C_BCR_AUTONEGO_EN              ((uint16_t)0x1000U)
#define YT8531C_BCR_POWER_DOWN               ((uint16_t)0x0800U)
#define YT8531C_BCR_RESTART_AUTONEGO         ((uint16_t)0x0200U)
#define YT8531C_BCR_DUPLEX_MODE              ((uint16_t)0x0100U)
#define YT8531C_BCR_SPEED_SEL_MSB            ((uint16_t)0x0040U)

#define YT8531C_SPEEDSEL_10M                 ((uint16_t)0x0000U)
#define YT8531C_SPEEDSEL_100M                (YT8531C_BCR_SPEED_SEL_LSB)
#define YT8531C_SPEEDSEL_1000M               (YT8531C_BCR_SPEED_SEL_MSB)

#define YT8531C_BSR_LINK_STATUS              ((uint16_t)0x0004U)

#define YT8531C_PHYSCSR_SPEED_MODE_MASK      ((uint16_t)0xC000U)
#define YT8531C_PHYSCSR_SPEED_10M            ((uint16_t)0x0000U)
#define YT8531C_PHYSCSR_SPEED_100M           ((uint16_t)0x4000U)
#define YT8531C_PHYSCSR_SPEED_1000M          ((uint16_t)0x8000U)
#define YT8531C_PHYSCSR_DUPLEX_FULL          ((uint16_t)0x2000U)
#define YT8531C_PHYSCSR_RESOLVED             ((uint16_t)0x0800U)
#define YT8531C_PHYSCSR_LINK_REALTIME        ((uint16_t)0x0400U)

#define YT8531C_EXT_REG_ADDR                 ((uint16_t)0x001EU)
#define YT8531C_EXT_REG_DATA                 ((uint16_t)0x001FU)
#define YT8531C_PHY_ADDR                     ((uint16_t)0x0000U)

typedef int32_t (*yt8531c_Init_Func)(void);
typedef int32_t (*yt8531c_DeInit_Func)(void);
typedef int32_t (*yt8531c_ReadReg_Func)(uint32_t dev_addr, uint32_t reg, uint32_t *value);
typedef int32_t (*yt8531c_WriteReg_Func)(uint32_t dev_addr, uint32_t reg, uint32_t value);
typedef int32_t (*yt8531c_GetTick_Func)(void);

typedef struct
{
    yt8531c_Init_Func Init;
    yt8531c_DeInit_Func DeInit;
    yt8531c_WriteReg_Func WriteReg;
    yt8531c_ReadReg_Func ReadReg;
    yt8531c_GetTick_Func GetTick;
} yt8531c_IOCtx_t;

typedef struct
{
    uint32_t DevAddr;
    uint32_t Is_Initialized;
    yt8531c_IOCtx_t IO;
    void *pData;
} yt8531c_Object_t;

int32_t YT8531C_RegisterBusIO(yt8531c_Object_t *obj, yt8531c_IOCtx_t *ioctx);
int32_t YT8531C_Init(yt8531c_Object_t *obj);
int32_t YT8531C_DeInit(yt8531c_Object_t *obj);
int32_t YT8531C_DisablePowerDownMode(yt8531c_Object_t *obj);
int32_t YT8531C_EnablePowerDownMode(yt8531c_Object_t *obj);
int32_t YT8531C_StartAutoNego(yt8531c_Object_t *obj);
int32_t YT8531C_GetLinkState(yt8531c_Object_t *obj);
int32_t YT8531C_SetLinkState(yt8531c_Object_t *obj, uint32_t link_state);
int32_t YT8531C_ConfigRGMII_Delay(yt8531c_Object_t *obj);
int32_t YT8531C_XtalInit(void);
void YT8531C_LED_Init(void);

#ifdef __cplusplus
}
#endif

#endif

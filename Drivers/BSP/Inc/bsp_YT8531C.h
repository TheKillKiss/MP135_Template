/* yt8531c.h */
#ifndef YT8531C_H
#define YT8531C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ----------------------------- Status codes ----------------------------- */
#define YT8531C_STATUS_READ_ERROR            ((int32_t)-5)
#define YT8531C_STATUS_WRITE_ERROR           ((int32_t)-4)
#define YT8531C_STATUS_ADDRESS_ERROR         ((int32_t)-3)
#define YT8531C_STATUS_RESET_TIMEOUT         ((int32_t)-2)
#define YT8531C_STATUS_ERROR                 ((int32_t)-1)
#define YT8531C_STATUS_OK                    ((int32_t) 0)

/* Link states (keep compatible style) */
#define YT8531C_STATUS_LINK_DOWN             ((int32_t) 1)
#define YT8531C_STATUS_AUTONEGO_NOTDONE      ((int32_t) 6)

/* We extend to include 1000M modes */
#define YT8531C_STATUS_10MBITS_FULLDUPLEX    ((int32_t) 4)
#define YT8531C_STATUS_10MBITS_HALFDUPLEX    ((int32_t) 5)
#define YT8531C_STATUS_100MBITS_FULLDUPLEX   ((int32_t) 2)
#define YT8531C_STATUS_100MBITS_HALFDUPLEX   ((int32_t) 3)
#define YT8531C_STATUS_1000MBITS_FULLDUPLEX  ((int32_t) 7)
#define YT8531C_STATUS_1000MBITS_HALFDUPLEX  ((int32_t) 8) /* usually not used, but reserved */

/* --------------------------- PHY constants --------------------------- */
#define YT8531C_MAX_DEV_ADDR                 ((uint32_t)31U)

/* Clause 22 registers */
#define YT8531C_BCR                          ((uint16_t)0x0000U)
#define YT8531C_BSR                          ((uint16_t)0x0001U)
#define YT8531C_PHYI1R                       ((uint16_t)0x0002U) /* PHY ID1 */
#define YT8531C_PHYI2R                       ((uint16_t)0x0003U) /* PHY ID2 */
#define YT8531C_ANAR                         ((uint16_t)0x0004U)
#define YT8531C_ANLPAR                       ((uint16_t)0x0005U)
#define YT8531C_ANER                         ((uint16_t)0x0006U)

#define YT8531C_PHYSFCR                      ((uint16_t)0x0010U) /* PHY Specific Function Control */
#define YT8531C_PHYSCSR                      ((uint16_t)0x0011U) /* PHY Specific Status */

#define YT8531C_IMR                          ((uint16_t)0x0012U) /* Interrupt Mask */
#define YT8531C_ISR                          ((uint16_t)0x0013U) /* Interrupt Status */

/* PHY ID expected values (from datasheet defaults) */
#define YT8531C_PHY_ID1_EXPECTED             ((uint16_t)0x4F51U)
#define YT8531C_PHY_ID2_OUI_MSB_EXPECTED     ((uint16_t)0x003AU) /* bits15:10 */
#define YT8531C_PHY_ID2_TYPE_NO_EXPECTED     ((uint16_t)0x0011U) /* bits9:4  */

/* ------------------------ BCR bit definitions ------------------------ */
#define YT8531C_BCR_SOFT_RESET               ((uint16_t)0x8000U) /* bit15 */
#define YT8531C_BCR_LOOPBACK                 ((uint16_t)0x4000U) /* bit14 */
#define YT8531C_BCR_SPEED_SEL_LSB            ((uint16_t)0x2000U) /* bit13 */
#define YT8531C_BCR_AUTONEGO_EN              ((uint16_t)0x1000U) /* bit12 */
#define YT8531C_BCR_POWER_DOWN               ((uint16_t)0x0800U) /* bit11 */
#define YT8531C_BCR_ISOLATE                  ((uint16_t)0x0400U) /* bit10 */
#define YT8531C_BCR_RESTART_AUTONEGO         ((uint16_t)0x0200U) /* bit9  */
#define YT8531C_BCR_DUPLEX_MODE              ((uint16_t)0x0100U) /* bit8  */
#define YT8531C_BCR_SPEED_SEL_MSB            ((uint16_t)0x0040U) /* bit6  */

/* Speed selection encoding uses bit6 (MSB) and bit13 (LSB) when autoneg disabled */
#define YT8531C_SPEEDSEL_10M                 ((uint16_t)0x0000U) /* bit6=0 bit13=0 */
#define YT8531C_SPEEDSEL_100M                (YT8531C_BCR_SPEED_SEL_LSB) /* bit6=0 bit13=1 */
#define YT8531C_SPEEDSEL_1000M               (YT8531C_BCR_SPEED_SEL_MSB) /* bit6=1 bit13=0 */

/* ------------------------ BSR bit definitions ------------------------ */
#define YT8531C_BSR_LINK_STATUS              ((uint16_t)0x0004U) /* bit2 */
#define YT8531C_BSR_AUTONEGO_CPLT            ((uint16_t)0x0020U) /* bit5 */

/* -------------------- PHY Specific Status (0x11) --------------------- */
#define YT8531C_PHYSCSR_SPEED_MODE_MASK      ((uint16_t)0xC000U) /* bits15:14 */
#define YT8531C_PHYSCSR_SPEED_10M            ((uint16_t)0x0000U) /* 00 */
#define YT8531C_PHYSCSR_SPEED_100M           ((uint16_t)0x4000U) /* 01 */
#define YT8531C_PHYSCSR_SPEED_1000M          ((uint16_t)0x8000U) /* 10 */

#define YT8531C_PHYSCSR_DUPLEX_FULL          ((uint16_t)0x2000U) /* bit13: 1=full */
#define YT8531C_PHYSCSR_RESOLVED             ((uint16_t)0x0800U) /* bit11 */
#define YT8531C_PHYSCSR_LINK_REALTIME        ((uint16_t)0x0400U) /* bit10 */

/* -------------------- Interrupt bits (mask & status) -------------------- */
#define YT8531C_INT_AUTONEGO_ERR             ((uint16_t)0x8000U) /* bit15 */
#define YT8531C_INT_SPEED_CHANGED            ((uint16_t)0x4000U) /* bit14 */
#define YT8531C_INT_DUPLEX_CHANGED           ((uint16_t)0x2000U) /* bit13 */
#define YT8531C_INT_PAGE_RECEIVED            ((uint16_t)0x1000U) /* bit12 */
#define YT8531C_INT_LINK_FAILED              ((uint16_t)0x0800U) /* bit11 */
#define YT8531C_INT_LINK_SUCCEED             ((uint16_t)0x0400U) /* bit10 */
#define YT8531C_INT_WOL                      ((uint16_t)0x0040U) /* bit6  */
#define YT8531C_INT_WIRESPEED_DOWNGRADED     ((uint16_t)0x0020U) /* bit5  */
#define YT8531C_INT_POLARITY_CHANGED         ((uint16_t)0x0002U) /* bit1  */
#define YT8531C_INT_JABBER_HAPPENED          ((uint16_t)0x0001U) /* bit0  */

/* -------------------- EXT Registers -------------------- */
#define YT8531C_EXT_REG_ADDR                 ((uint16_t)0x0001E)
#define YT8531C_EXT_REG_DATA                 ((uint16_t)0x0001F)
#define YT8531C_PHY_ADDR                     ((uint16_t)0x0)
/* ----------------------------- IO interface ----------------------------- */
typedef int32_t  (*yt8531c_Init_Func)     (void);
typedef int32_t  (*yt8531c_DeInit_Func)   (void);
typedef int32_t  (*yt8531c_ReadReg_Func)  (uint32_t DevAddr, uint32_t Reg, uint32_t *pValue);
typedef int32_t  (*yt8531c_WriteReg_Func) (uint32_t DevAddr, uint32_t Reg, uint32_t Value);
typedef int32_t  (*yt8531c_GetTick_Func)  (void);

typedef struct
{
  yt8531c_Init_Func      Init;
  yt8531c_DeInit_Func    DeInit;
  yt8531c_WriteReg_Func  WriteReg;
  yt8531c_ReadReg_Func   ReadReg;
  yt8531c_GetTick_Func   GetTick;
} yt8531c_IOCtx_t;

typedef struct
{
  uint32_t         DevAddr;
  uint32_t         Is_Initialized;
  yt8531c_IOCtx_t  IO;
  void            *pData;
} yt8531c_Object_t;

/* ----------------------------- APIs ----------------------------- */
int32_t  YT8531C_RegisterBusIO(yt8531c_Object_t *pObj, yt8531c_IOCtx_t *ioctx);
int32_t  YT8531C_Init(yt8531c_Object_t *pObj);
int32_t  YT8531C_DeInit(yt8531c_Object_t *pObj);

int32_t  YT8531C_DisablePowerDownMode(yt8531c_Object_t *pObj);
int32_t  YT8531C_EnablePowerDownMode(yt8531c_Object_t *pObj);

int32_t  YT8531C_StartAutoNego(yt8531c_Object_t *pObj);
int32_t  YT8531C_GetLinkState(yt8531c_Object_t *pObj);
int32_t  YT8531C_SetLinkState(yt8531c_Object_t *pObj, uint32_t LinkState);

int32_t  YT8531C_EnableLoopbackMode(yt8531c_Object_t *pObj);
int32_t  YT8531C_DisableLoopbackMode(yt8531c_Object_t *pObj);

int32_t  YT8531C_EnableIT(yt8531c_Object_t *pObj, uint32_t InterruptMask);
int32_t  YT8531C_DisableIT(yt8531c_Object_t *pObj, uint32_t InterruptMask);
int32_t  YT8531C_ClearIT(yt8531c_Object_t *pObj, uint32_t InterruptMask);
int32_t  YT8531C_GetITStatus(yt8531c_Object_t *pObj, uint32_t InterruptMask, uint32_t *pStatus);

int32_t YT8531C_ConfigRGMII_Delay(yt8531c_Object_t *pObj);

int32_t YT8531C_XtalInit(void);
void YT8531C_LED_Init(void);

void YT_CHECK(yt8531c_Object_t *pObj);

#ifdef __cplusplus
}
#endif

#endif /* YT8531C_H */

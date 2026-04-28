/* yt8531c.c */
#include "bsp_YT8531C.h"

/*
 * YT8531C PHY driver (Clause 22)
 * - PHY ID check based on datasheet defaults (ID1=0x4F51, ID2 OUI_MSB=0x3A, Type_No=0x11)
 * - Link decode from PHY Specific Status Register 0x11:
 *   Speed_mode[15:14], Duplex[13], Resolved[11], Link realtime[10]
 * - Interrupt Mask/Status 0x12/0x13
 */
   
extern ETH_HandleTypeDef heth1;

static int32_t yt8531c_check_id(yt8531c_Object_t *pObj, uint32_t addr)
{
    uint32_t id1 = 0, id2 = 0;

    if (pObj->IO.ReadReg(addr, YT8531C_PHYI1R, &id1) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }
    if (pObj->IO.ReadReg(addr, YT8531C_PHYI2R, &id2) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    /* ID1 must match exactly */
    if (((uint16_t)id1) != YT8531C_PHY_ID1_EXPECTED) {
        return YT8531C_STATUS_ERROR;
    }

    /* ID2: bits15:10 OUI MSB, bits9:4 type number, bits3:0 revision */
    {
        uint16_t id2_oui_msb = (uint16_t)((id2 >> 10) & 0x3Fu);
        uint16_t id2_type_no = (uint16_t)((id2 >> 4)  & 0x3Fu);

        if ((id2_oui_msb != YT8531C_PHY_ID2_OUI_MSB_EXPECTED) ||
            (id2_type_no != YT8531C_PHY_ID2_TYPE_NO_EXPECTED)) {
            return YT8531C_STATUS_ERROR;
        }
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_RegisterBusIO(yt8531c_Object_t *pObj, yt8531c_IOCtx_t *ioctx)
{
    if (!pObj || !ioctx || !ioctx->ReadReg || !ioctx->WriteReg || !ioctx->GetTick) {
        return YT8531C_STATUS_ERROR;
    }

    pObj->IO.Init     = ioctx->Init;
    pObj->IO.DeInit   = ioctx->DeInit;
    pObj->IO.ReadReg  = ioctx->ReadReg;
    pObj->IO.WriteReg = ioctx->WriteReg;
    pObj->IO.GetTick  = ioctx->GetTick;

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_Init(yt8531c_Object_t *pObj)
{
    uint32_t addr;
    int32_t status = YT8531C_STATUS_OK;

    if (!pObj) {
        return YT8531C_STATUS_ERROR;
    }

    if (pObj->Is_Initialized) {
        return YT8531C_STATUS_OK;
    }

    if (pObj->IO.Init) {
        (void)pObj->IO.Init();
    }

    pObj->DevAddr = YT8531C_MAX_DEV_ADDR + 1u;

    /*
     * Datasheet notes PHY always responds to address 0 in common cases.
     * For robustness we try 0 first, then scan 0..31.
     */
    status = yt8531c_check_id(pObj, 0u);
    if (status == YT8531C_STATUS_OK) {
        pObj->DevAddr = 0u;
    } else {
        for (addr = 0u; addr <= YT8531C_MAX_DEV_ADDR; addr++) {
            if (yt8531c_check_id(pObj, addr) == YT8531C_STATUS_OK) {
                pObj->DevAddr = addr;
                status = YT8531C_STATUS_OK;
                break;
            }
        }
    }

    if (pObj->DevAddr > YT8531C_MAX_DEV_ADDR) {
        return YT8531C_STATUS_ADDRESS_ERROR;
    }

    pObj->Is_Initialized = 1u;
    return YT8531C_STATUS_OK;
}

int32_t YT8531C_DeInit(yt8531c_Object_t *pObj)
{
    if (!pObj) {
        return YT8531C_STATUS_ERROR;
    }

    if (pObj->Is_Initialized) {
        if (pObj->IO.DeInit) {
            if (pObj->IO.DeInit() < 0) {
                return YT8531C_STATUS_ERROR;
            }
        }
        pObj->Is_Initialized = 0u;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_DisablePowerDownMode(yt8531c_Object_t *pObj)
{
    uint32_t val = 0;

    if (!pObj) {
        return YT8531C_STATUS_ERROR;
    }

    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_BCR, &val) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    val &= (uint32_t)(~YT8531C_BCR_POWER_DOWN);

    if (pObj->IO.WriteReg(pObj->DevAddr, YT8531C_BCR, val) < 0) {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_EnablePowerDownMode(yt8531c_Object_t *pObj)
{
    uint32_t val = 0;

    if (!pObj) {
        return YT8531C_STATUS_ERROR;
    }

    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_BCR, &val) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    val |= (uint32_t)YT8531C_BCR_POWER_DOWN;

    if (pObj->IO.WriteReg(pObj->DevAddr, YT8531C_BCR, val) < 0) {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_StartAutoNego(yt8531c_Object_t *pObj)
{
    uint32_t val = 0;

    if (!pObj) {
        return YT8531C_STATUS_ERROR;
    }

    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_BCR, &val) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    val |= (uint32_t)YT8531C_BCR_AUTONEGO_EN;
    val |= (uint32_t)YT8531C_BCR_RESTART_AUTONEGO;

    if (pObj->IO.WriteReg(pObj->DevAddr, YT8531C_BCR, val) < 0) {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_GetLinkState(yt8531c_Object_t *pObj)
{
    uint32_t bsr = 0;
    uint32_t psr = 0;

    if (!pObj) {
        return YT8531C_STATUS_ERROR;
    }

    /* Read BSR twice as standard practice */
    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_BSR, &bsr) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }
    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_BSR, &bsr) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    if (((uint16_t)bsr & YT8531C_BSR_LINK_STATUS) == 0u) {
        return YT8531C_STATUS_LINK_DOWN;
    }

    /* PHY Specific Status contains resolved speed/duplex and realtime link */
    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_PHYSCSR, &psr) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    /* If not resolved yet, report autoneg not done */
    if ((((uint16_t)psr & YT8531C_PHYSCSR_RESOLVED) == 0u) ||
        (((uint16_t)psr & YT8531C_PHYSCSR_LINK_REALTIME) == 0u)) {
        return YT8531C_STATUS_AUTONEGO_NOTDONE;
    }

    {
        uint16_t psr16 = (uint16_t)psr;
        uint16_t speed = (uint16_t)(psr16 & YT8531C_PHYSCSR_SPEED_MODE_MASK);
        uint8_t  full  = (uint8_t)((psr16 & YT8531C_PHYSCSR_DUPLEX_FULL) ? 1u : 0u);

        if (speed == YT8531C_PHYSCSR_SPEED_1000M) {
            return full ? YT8531C_STATUS_1000MBITS_FULLDUPLEX : YT8531C_STATUS_1000MBITS_HALFDUPLEX;
        } else if (speed == YT8531C_PHYSCSR_SPEED_100M) {
            return full ? YT8531C_STATUS_100MBITS_FULLDUPLEX : YT8531C_STATUS_100MBITS_HALFDUPLEX;
        } else { /* 10M or reserved treated as 10M */
            return full ? YT8531C_STATUS_10MBITS_FULLDUPLEX : YT8531C_STATUS_10MBITS_HALFDUPLEX;
        }
    }
}

int32_t YT8531C_SetLinkState(yt8531c_Object_t *pObj, uint32_t LinkState)
{
    uint32_t bcr = 0;
    int32_t status = YT8531C_STATUS_OK;

    if (!pObj) {
        return YT8531C_STATUS_ERROR;
    }

    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_BCR, &bcr) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    /* Disable autoneg for force mode */
    bcr &= (uint32_t)(~YT8531C_BCR_AUTONEGO_EN);

    /* Clear speed selection bits and duplex bit */
    bcr &= (uint32_t)(~(YT8531C_BCR_SPEED_SEL_LSB | YT8531C_BCR_SPEED_SEL_MSB | YT8531C_BCR_DUPLEX_MODE));

    switch (LinkState) {
        case YT8531C_STATUS_10MBITS_FULLDUPLEX:
            bcr |= (uint32_t)YT8531C_BCR_DUPLEX_MODE;
            bcr |= (uint32_t)YT8531C_SPEEDSEL_10M;
            break;

        case YT8531C_STATUS_10MBITS_HALFDUPLEX:
            bcr |= (uint32_t)YT8531C_SPEEDSEL_10M;
            break;

        case YT8531C_STATUS_100MBITS_FULLDUPLEX:
            bcr |= (uint32_t)YT8531C_BCR_DUPLEX_MODE;
            bcr |= (uint32_t)YT8531C_SPEEDSEL_100M;
            break;

        case YT8531C_STATUS_100MBITS_HALFDUPLEX:
            bcr |= (uint32_t)YT8531C_SPEEDSEL_100M;
            break;

        case YT8531C_STATUS_1000MBITS_FULLDUPLEX:
            bcr |= (uint32_t)YT8531C_BCR_DUPLEX_MODE;
            bcr |= (uint32_t)YT8531C_SPEEDSEL_1000M;
            break;

        case YT8531C_STATUS_1000MBITS_HALFDUPLEX:
            /* If your design doesn't support 1000 half, you can reject it */
            bcr |= (uint32_t)YT8531C_SPEEDSEL_1000M;
            break;

        default:
            status = YT8531C_STATUS_ERROR;
            break;
    }

    if (status != YT8531C_STATUS_OK) {
        return status;
    }

    if (pObj->IO.WriteReg(pObj->DevAddr, YT8531C_BCR, bcr) < 0) {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_EnableLoopbackMode(yt8531c_Object_t *pObj)
{
    uint32_t bcr = 0;

    if (!pObj) {
        return YT8531C_STATUS_ERROR;
    }

    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_BCR, &bcr) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    bcr |= (uint32_t)YT8531C_BCR_LOOPBACK;

    if (pObj->IO.WriteReg(pObj->DevAddr, YT8531C_BCR, bcr) < 0) {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_DisableLoopbackMode(yt8531c_Object_t *pObj)
{
    uint32_t bcr = 0;

    if (!pObj) {
        return YT8531C_STATUS_ERROR;
    }

    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_BCR, &bcr) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    bcr &= (uint32_t)(~YT8531C_BCR_LOOPBACK);

    if (pObj->IO.WriteReg(pObj->DevAddr, YT8531C_BCR, bcr) < 0) {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_EnableIT(yt8531c_Object_t *pObj, uint32_t InterruptMask)
{
    uint32_t imr = 0;

    if (!pObj) {
        return YT8531C_STATUS_ERROR;
    }

    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_IMR, &imr) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    imr |= (uint32_t)InterruptMask;

    if (pObj->IO.WriteReg(pObj->DevAddr, YT8531C_IMR, imr) < 0) {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_DisableIT(yt8531c_Object_t *pObj, uint32_t InterruptMask)
{
    uint32_t imr = 0;

    if (!pObj) {
        return YT8531C_STATUS_ERROR;
    }

    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_IMR, &imr) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    imr &= (uint32_t)(~InterruptMask);

    if (pObj->IO.WriteReg(pObj->DevAddr, YT8531C_IMR, imr) < 0) {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_ClearIT(yt8531c_Object_t *pObj, uint32_t InterruptMask)
{
    uint32_t isr = 0;

    if (!pObj) {
        return YT8531C_STATUS_ERROR;
    }

    /* ISR is Read-Clear per datasheet; read it to clear latched bits */
    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_ISR, &isr) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    /* Optionally: ensure requested bits were present, but clearing is done by read */
    (void)InterruptMask;
    return YT8531C_STATUS_OK;
}

int32_t YT8531C_GetITStatus(yt8531c_Object_t *pObj, uint32_t InterruptMask, uint32_t *pStatus)
{
    uint32_t isr = 0;

    if (!pObj || !pStatus) {
        return YT8531C_STATUS_ERROR;
    }

    if (pObj->IO.ReadReg(pObj->DevAddr, YT8531C_ISR, &isr) < 0) {
        return YT8531C_STATUS_READ_ERROR;
    }

    *pStatus = (isr & InterruptMask);
    return YT8531C_STATUS_OK;
}

int32_t YT8531C_ConfigRGMII_Delay(yt8531c_Object_t *pObj)
{
  uint32_t val;

  pObj->IO.WriteReg(pObj->DevAddr, 0x1E, 0xA003);

  pObj->IO.ReadReg(pObj->DevAddr, 0x1F, &val);

  val |= (1 << 1); 
  val |= (1 << 2); 

  pObj->IO.WriteReg(pObj->DevAddr, 0x1F, val);

  return YT8531C_STATUS_OK;
}

int32_t YT8531C_ReadReg(uint32_t DevAddr, uint32_t Reg, uint32_t *pValue)
{
  if (pValue == NULL) {
    return YT8531C_STATUS_ERROR;
  }

  if (HAL_ETH_ReadPHYRegister(&heth1, DevAddr, Reg, pValue) != HAL_OK) {
    return YT8531C_STATUS_READ_ERROR;
  }

  return YT8531C_STATUS_OK;
}

int32_t YT8531C_WriteExtReg(uint16_t reg, uint16_t value)
{
    //uint32_t val;
    if(HAL_ETH_WritePHYRegister(&heth1, YT8531C_PHY_ADDR, YT8531C_EXT_REG_ADDR, reg) != HAL_OK)
        return YT8531C_STATUS_ERROR;
    if(HAL_ETH_WritePHYRegister(&heth1, YT8531C_PHY_ADDR, YT8531C_EXT_REG_DATA, value) != HAL_OK)
        return YT8531C_STATUS_ERROR;

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_ReadExtReg(uint16_t reg, uint16_t *value)
{
    uint32_t val;

    if(HAL_ETH_WritePHYRegister(&heth1, YT8531C_PHY_ADDR, YT8531C_EXT_REG_ADDR, reg) != HAL_OK)
        return YT8531C_STATUS_ERROR;
    if(HAL_ETH_ReadPHYRegister(&heth1, YT8531C_PHY_ADDR, YT8531C_EXT_REG_DATA, &val) != HAL_OK)
        return YT8531C_STATUS_ERROR;
    
    *value = (uint16_t)val;
    return YT8531C_STATUS_OK;
}

int32_t YT8531C_XtalInit(void)
{
    uint16_t val = 0;
    HAL_Delay(50);

    do{
        if(YT8531C_WriteExtReg(0xA012, 0x0088) != YT8531C_STATUS_OK)
            return YT8531C_STATUS_ERROR;
        HAL_Delay(100);

        if(YT8531C_ReadExtReg(0xA012, &val) != YT8531C_STATUS_OK)
            return YT8531C_STATUS_ERROR;

        HAL_Delay(20);
    }while(val != 0x0088);

    if(YT8531C_WriteExtReg(0xA012, 0x00D0) != YT8531C_STATUS_OK)
        return YT8531C_STATUS_ERROR;

    return YT8531C_STATUS_OK;
}

void YT8531C_LED_Init(void)
{
    YT8531C_WriteExtReg(0xA00D, 0x2600);
    YT8531C_WriteExtReg(0xA00C, 0x0030);
    YT8531C_WriteExtReg(0xA00E, 0x0040);
}


void YT_CHECK(yt8531c_Object_t *pObj)
{
  uint32_t v;

  YT8531C_ReadReg(pObj->DevAddr, YT8531C_BSR, &v);
  printf("BSR      = 0x%04lx\r\n", v);

  YT8531C_ReadReg(pObj->DevAddr, YT8531C_PHYSCSR, &v);
  printf("PHYSCSR  = 0x%04lx\r\n", v);

  YT8531C_ReadReg(pObj->DevAddr, YT8531C_BCR, &v);
  printf("BCR      = 0x%04lx\r\n", v);

  YT8531C_ReadReg(pObj->DevAddr, YT8531C_PHYI1R, &v);
  printf("PHYID1   = 0x%04lx\r\n", v);

  YT8531C_ReadReg(pObj->DevAddr, YT8531C_PHYI2R, &v);
  printf("PHYID2   = 0x%04lx\r\n", v);

}


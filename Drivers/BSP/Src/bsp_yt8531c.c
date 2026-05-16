#include "bsp_yt8531c.h"
#include "rtconfig.h"

#ifdef BSP_USING_ETH

extern ETH_HandleTypeDef heth1;

static int32_t yt8531c_check_id(yt8531c_Object_t *obj, uint32_t addr)
{
    uint32_t id1 = 0;
    uint32_t id2 = 0;
    uint16_t id2_oui_msb;
    uint16_t id2_type_no;

    if (obj->IO.ReadReg(addr, YT8531C_PHYI1R, &id1) < 0)
    {
        return YT8531C_STATUS_READ_ERROR;
    }
    if (obj->IO.ReadReg(addr, YT8531C_PHYI2R, &id2) < 0)
    {
        return YT8531C_STATUS_READ_ERROR;
    }

    if ((uint16_t)id1 != YT8531C_PHY_ID1_EXPECTED)
    {
        return YT8531C_STATUS_ERROR;
    }

    id2_oui_msb = (uint16_t)((id2 >> 10) & 0x3FU);
    id2_type_no = (uint16_t)((id2 >> 4) & 0x3FU);
    if ((id2_oui_msb != YT8531C_PHY_ID2_OUI_MSB_EXPECTED) ||
        (id2_type_no != YT8531C_PHY_ID2_TYPE_NO_EXPECTED))
    {
        return YT8531C_STATUS_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_RegisterBusIO(yt8531c_Object_t *obj, yt8531c_IOCtx_t *ioctx)
{
    if ((obj == 0) || (ioctx == 0) || (ioctx->ReadReg == 0) ||
        (ioctx->WriteReg == 0) || (ioctx->GetTick == 0))
    {
        return YT8531C_STATUS_ERROR;
    }

    obj->IO.Init = ioctx->Init;
    obj->IO.DeInit = ioctx->DeInit;
    obj->IO.ReadReg = ioctx->ReadReg;
    obj->IO.WriteReg = ioctx->WriteReg;
    obj->IO.GetTick = ioctx->GetTick;

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_Init(yt8531c_Object_t *obj)
{
    uint32_t addr;

    if (obj == 0)
    {
        return YT8531C_STATUS_ERROR;
    }

    if (obj->Is_Initialized != 0U)
    {
        return YT8531C_STATUS_OK;
    }

    if (obj->IO.Init != 0)
    {
        (void)obj->IO.Init();
    }

    obj->DevAddr = YT8531C_MAX_DEV_ADDR + 1U;
    if (yt8531c_check_id(obj, 0U) == YT8531C_STATUS_OK)
    {
        obj->DevAddr = 0U;
    }
    else
    {
        for (addr = 0U; addr <= YT8531C_MAX_DEV_ADDR; addr++)
        {
            if (yt8531c_check_id(obj, addr) == YT8531C_STATUS_OK)
            {
                obj->DevAddr = addr;
                break;
            }
        }
    }

    if (obj->DevAddr > YT8531C_MAX_DEV_ADDR)
    {
        return YT8531C_STATUS_ADDRESS_ERROR;
    }

    obj->Is_Initialized = 1U;
    return YT8531C_STATUS_OK;
}

int32_t YT8531C_DeInit(yt8531c_Object_t *obj)
{
    if (obj == 0)
    {
        return YT8531C_STATUS_ERROR;
    }

    if (obj->Is_Initialized != 0U)
    {
        if ((obj->IO.DeInit != 0) && (obj->IO.DeInit() < 0))
        {
            return YT8531C_STATUS_ERROR;
        }
        obj->Is_Initialized = 0U;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_DisablePowerDownMode(yt8531c_Object_t *obj)
{
    uint32_t val = 0;

    if (obj == 0)
    {
        return YT8531C_STATUS_ERROR;
    }
    if (obj->IO.ReadReg(obj->DevAddr, YT8531C_BCR, &val) < 0)
    {
        return YT8531C_STATUS_READ_ERROR;
    }

    val &= (uint32_t)(~YT8531C_BCR_POWER_DOWN);
    if (obj->IO.WriteReg(obj->DevAddr, YT8531C_BCR, val) < 0)
    {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_EnablePowerDownMode(yt8531c_Object_t *obj)
{
    uint32_t val = 0;

    if (obj == 0)
    {
        return YT8531C_STATUS_ERROR;
    }
    if (obj->IO.ReadReg(obj->DevAddr, YT8531C_BCR, &val) < 0)
    {
        return YT8531C_STATUS_READ_ERROR;
    }

    val |= (uint32_t)YT8531C_BCR_POWER_DOWN;
    if (obj->IO.WriteReg(obj->DevAddr, YT8531C_BCR, val) < 0)
    {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_StartAutoNego(yt8531c_Object_t *obj)
{
    uint32_t val = 0;

    if (obj == 0)
    {
        return YT8531C_STATUS_ERROR;
    }
    if (obj->IO.ReadReg(obj->DevAddr, YT8531C_BCR, &val) < 0)
    {
        return YT8531C_STATUS_READ_ERROR;
    }

    val |= (uint32_t)YT8531C_BCR_AUTONEGO_EN;
    val |= (uint32_t)YT8531C_BCR_RESTART_AUTONEGO;
    if (obj->IO.WriteReg(obj->DevAddr, YT8531C_BCR, val) < 0)
    {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_GetLinkState(yt8531c_Object_t *obj)
{
    uint32_t bsr = 0;
    uint32_t psr = 0;
    uint16_t speed;
    uint16_t psr16;
    uint8_t full;

    if (obj == 0)
    {
        return YT8531C_STATUS_ERROR;
    }

    if (obj->IO.ReadReg(obj->DevAddr, YT8531C_BSR, &bsr) < 0)
    {
        return YT8531C_STATUS_READ_ERROR;
    }
    if (obj->IO.ReadReg(obj->DevAddr, YT8531C_BSR, &bsr) < 0)
    {
        return YT8531C_STATUS_READ_ERROR;
    }
    if (((uint16_t)bsr & YT8531C_BSR_LINK_STATUS) == 0U)
    {
        return YT8531C_STATUS_LINK_DOWN;
    }

    if (obj->IO.ReadReg(obj->DevAddr, YT8531C_PHYSCSR, &psr) < 0)
    {
        return YT8531C_STATUS_READ_ERROR;
    }

    psr16 = (uint16_t)psr;
    if (((psr16 & YT8531C_PHYSCSR_RESOLVED) == 0U) ||
        ((psr16 & YT8531C_PHYSCSR_LINK_REALTIME) == 0U))
    {
        return YT8531C_STATUS_AUTONEGO_NOTDONE;
    }

    speed = (uint16_t)(psr16 & YT8531C_PHYSCSR_SPEED_MODE_MASK);
    full = (uint8_t)((psr16 & YT8531C_PHYSCSR_DUPLEX_FULL) ? 1U : 0U);

    if (speed == YT8531C_PHYSCSR_SPEED_1000M)
    {
        return full ? YT8531C_STATUS_1000MBITS_FULLDUPLEX : YT8531C_STATUS_1000MBITS_HALFDUPLEX;
    }
    if (speed == YT8531C_PHYSCSR_SPEED_100M)
    {
        return full ? YT8531C_STATUS_100MBITS_FULLDUPLEX : YT8531C_STATUS_100MBITS_HALFDUPLEX;
    }

    return full ? YT8531C_STATUS_10MBITS_FULLDUPLEX : YT8531C_STATUS_10MBITS_HALFDUPLEX;
}

int32_t YT8531C_SetLinkState(yt8531c_Object_t *obj, uint32_t link_state)
{
    uint32_t bcr = 0;

    if (obj == 0)
    {
        return YT8531C_STATUS_ERROR;
    }
    if (obj->IO.ReadReg(obj->DevAddr, YT8531C_BCR, &bcr) < 0)
    {
        return YT8531C_STATUS_READ_ERROR;
    }

    bcr &= (uint32_t)(~YT8531C_BCR_AUTONEGO_EN);
    bcr &= (uint32_t)(~(YT8531C_BCR_SPEED_SEL_LSB |
                        YT8531C_BCR_SPEED_SEL_MSB |
                        YT8531C_BCR_DUPLEX_MODE));

    switch (link_state)
    {
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
        bcr |= (uint32_t)YT8531C_SPEEDSEL_1000M;
        break;
    default:
        return YT8531C_STATUS_ERROR;
    }

    if (obj->IO.WriteReg(obj->DevAddr, YT8531C_BCR, bcr) < 0)
    {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

int32_t YT8531C_ConfigRGMII_Delay(yt8531c_Object_t *obj)
{
    uint32_t val = 0;

    if (obj == 0)
    {
        return YT8531C_STATUS_ERROR;
    }

    if (obj->IO.WriteReg(obj->DevAddr, YT8531C_EXT_REG_ADDR, 0xA003U) < 0)
    {
        return YT8531C_STATUS_WRITE_ERROR;
    }
    if (obj->IO.ReadReg(obj->DevAddr, YT8531C_EXT_REG_DATA, &val) < 0)
    {
        return YT8531C_STATUS_READ_ERROR;
    }

    val |= (1UL << 1);
    val |= (1UL << 2);

    if (obj->IO.WriteReg(obj->DevAddr, YT8531C_EXT_REG_DATA, val) < 0)
    {
        return YT8531C_STATUS_WRITE_ERROR;
    }

    return YT8531C_STATUS_OK;
}

static int32_t yt8531c_write_ext_reg(uint16_t reg, uint16_t value)
{
    if (HAL_ETH_WritePHYRegister(&heth1, YT8531C_PHY_ADDR, YT8531C_EXT_REG_ADDR, reg) != HAL_OK)
    {
        return YT8531C_STATUS_ERROR;
    }
    if (HAL_ETH_WritePHYRegister(&heth1, YT8531C_PHY_ADDR, YT8531C_EXT_REG_DATA, value) != HAL_OK)
    {
        return YT8531C_STATUS_ERROR;
    }

    return YT8531C_STATUS_OK;
}

static int32_t yt8531c_read_ext_reg(uint16_t reg, uint16_t *value)
{
    uint32_t val = 0;

    if (HAL_ETH_WritePHYRegister(&heth1, YT8531C_PHY_ADDR, YT8531C_EXT_REG_ADDR, reg) != HAL_OK)
    {
        return YT8531C_STATUS_ERROR;
    }
    if (HAL_ETH_ReadPHYRegister(&heth1, YT8531C_PHY_ADDR, YT8531C_EXT_REG_DATA, &val) != HAL_OK)
    {
        return YT8531C_STATUS_ERROR;
    }

    *value = (uint16_t)val;
    return YT8531C_STATUS_OK;
}

int32_t YT8531C_XtalInit(void)
{
    uint16_t val = 0;

    HAL_Delay(50U);

    do
    {
        if (yt8531c_write_ext_reg(0xA012U, 0x0088U) != YT8531C_STATUS_OK)
        {
            return YT8531C_STATUS_ERROR;
        }
        HAL_Delay(100U);

        if (yt8531c_read_ext_reg(0xA012U, &val) != YT8531C_STATUS_OK)
        {
            return YT8531C_STATUS_ERROR;
        }
        HAL_Delay(20U);
    } while (val != 0x0088U);

    if (yt8531c_write_ext_reg(0xA012U, 0x00D0U) != YT8531C_STATUS_OK)
    {
        return YT8531C_STATUS_ERROR;
    }

    return YT8531C_STATUS_OK;
}

void YT8531C_LED_Init(void)
{
    (void)yt8531c_write_ext_reg(0xA00DU, 0x2600U);
    (void)yt8531c_write_ext_reg(0xA00CU, 0x0030U);
    (void)yt8531c_write_ext_reg(0xA00EU, 0x0040U);
}

#endif

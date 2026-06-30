#ifndef APP_TIME_H
#define APP_TIME_H

#include "stm32mp13xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t AppTime_GetCounterHz(void)
{
    return __get_CNTFRQ();
}

static inline uint64_t AppTime_GetPhysicalValue(void)
{
    return PL1_GetCurrentPhysicalValue();
}

static inline uint64_t AppTime_PhysicalValueToUs(uint64_t ticks)
{
    uint64_t freq = (uint64_t)AppTime_GetCounterHz();

    if (freq == 0u) {
        return 0u;
    }

    return ((ticks / freq) * 1000000ULL) + (((ticks % freq) * 1000000ULL) / freq);
}

static inline uint64_t AppTime_PhysicalValueToMs(uint64_t ticks)
{
    uint64_t freq = (uint64_t)AppTime_GetCounterHz();

    if (freq == 0u) {
        return 0u;
    }

    return ((ticks / freq) * 1000ULL) + (((ticks % freq) * 1000ULL) / freq);
}

static inline uint64_t AppTime_GetUs(void)
{
    return AppTime_PhysicalValueToUs(AppTime_GetPhysicalValue());
}

static inline uint64_t AppTime_GetMs(void)
{
    return AppTime_PhysicalValueToMs(AppTime_GetPhysicalValue());
}

static inline uint64_t AppTime_PhysicalDiff(uint64_t start, uint64_t end)
{
    return end - start;
}

static inline uint64_t AppTime_PhysicalDiffToUs(uint64_t start, uint64_t end)
{
    return AppTime_PhysicalValueToUs(AppTime_PhysicalDiff(start, end));
}

static inline uint64_t AppTime_PhysicalDiffToMs(uint64_t start, uint64_t end)
{
    return AppTime_PhysicalValueToMs(AppTime_PhysicalDiff(start, end));
}

#ifdef __cplusplus
}
#endif

#endif /* APP_TIME_H */

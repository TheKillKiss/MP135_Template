# STM32MP13xx MMU 内存区划总结

本文档根据当前工程中的 `Drivers/CMSIS/Device/Source/mmu_stm32mp13xx.c` 整理，并结合
`Drivers/CMSIS/Device/Source/IAR/linker/stm32mp13xx_a7_ddr.icf` 中的 DDR/ETH 链接区域。

表格按“最终生效后的地址区划”编写，不再描述“先写一个大范围，再由后面的区域覆盖”的初始化过程。

## 通用属性

除非表格中特别说明，当前 MMU 描述符使用以下通用属性：

| 属性 | 当前值 |
| --- | --- |
| Domain | 0 |
| 安全属性 | Secure |
| Global | Global |
| ECC | Disabled |
| DACR | Domain 0 为 Client，其他 domain 为 No Access |
| TTBR0 | IAR 下为 `__section_begin("TTB") \| 9U` |

## 描述符含义

| 描述符 | 内存类型 | Cache 策略 | Share 属性 | 访问权限 | 执行权限 | 说明 |
| --- | --- | --- | --- | --- | --- | --- |
| `DESCRIPTOR_FAULT` | Fault | 不适用 | 不适用 | 不可访问 | 不可执行 | 未映射区域，访问会触发 abort。 |
| `Sect_Device_RO` | Device/strongly ordered 风格 | Non-cacheable | Non-shared | RO | XN | 用于 FMC NOR、QSPI memory window。 |
| `Sect_Device_RW` | Device/strongly ordered 风格 | Non-cacheable | Non-shared | RW | XN | 用于 FMC NAND memory window。 |
| `Sect_SO` | Strongly ordered | Non-cacheable | Strongly ordered | RW | XN | 用于外设寄存器窗口。 |
| `Sect_Normal_Shared` | Normal | Inner/outer WB/WA | Shared | RW | 可执行 | 用于普通 DDR 区。 |
| `Sect_Normal_NoneCacheable` | Normal | Non-cacheable | Non-shared | RW | 可执行 | 用于当前 ETH no-cache 区。注意该描述符没有 XN。 |
| `Page_4k_Device_RW` | Shared device | Non-cacheable | Non-shared | RW | XN | 用于 GIC 4KB 页映射。 |
| `Page_4k_Normal_Cod` | Normal | Inner/outer WB/WA | Shared | RW | 可执行 | 用于代码页。源码注释说明设为 RW 是为了调试器兼容。 |
| `Page_4k_Normal_RO` | Normal | Inner/outer WB/WA | Shared | RO | XN | 用于 DDR 中的只读数据页。 |
| `Page_4k_Normal_RW` | Normal | Inner/outer WB/WA | Shared | RW | XN | 用于代码/RO data 所在 1MB 窗口的基础页映射。 |
| `Page_4k_Normal_RW_NonCacheable` | Normal | Non-cacheable | Shared | RW | XN | 用于 SYSRAM、SRAM 数据页。 |
| `Page_4k_Normal_RO_NonCacheable` | Normal | Non-cacheable | Shared | RO | XN | 用于 SYSRAM 启动时的只读数据页。 |

## 最终生效的 MMU 区划

| 地址范围 | 大小 | 最终描述符 | 内存/cache 状态 | 访问权限 | 执行权限 | 说明 |
| --- | ---: | --- | --- | --- | --- | --- |
| `0x00000000-0x2FEFFFFF` | 767MB | `DESCRIPTOR_FAULT` | Fault | 不可访问 | 不可执行 | 当前没有显式映射。 |
| `0x2FF00000-0x2FFDFFFF` | 896KB | `DESCRIPTOR_FAULT` | Fault | 不可访问 | 不可执行 | SYSRAM 所在 1MB 窗口内未使用部分。 |
| `0x2FFE0000-0x2FFFFFFF` | 128KB | `Page_4k_Normal_RW_NonCacheable` | Normal，non-cacheable，shared | RW | XN | SYSRAM 实际映射区。若代码从 SYSRAM 运行，其中代码/RO data 页会被进一步细分，见后文。 |
| `0x30000000-0x30007FFF` | 32KB | `Page_4k_Normal_RW_NonCacheable` | Normal，non-cacheable，shared | RW | XN | SRAM1/2/3 实际映射区。 |
| `0x30008000-0x3FFFFFFF` | 255.96875MB | `DESCRIPTOR_FAULT` | Fault | 不可访问 | 不可执行 | SRAM 窗口剩余未映射区域，以及 `0x30100000` 到外设前的空洞。 |
| `0x40000000-0x5FFFFFFF` | 512MB | `Sect_SO` | Strongly ordered，non-cacheable | RW | XN | 外设寄存器窗口。 |
| `0x60000000-0x6FFFFFFF` | 256MB | `Sect_Device_RO` | Device/strongly ordered 风格，non-cacheable | RO | XN | FMC NOR memory window。 |
| `0x70000000-0x7FFFFFFF` | 256MB | `Sect_Device_RO` | Device/strongly ordered 风格，non-cacheable | RO | XN | QSPI memory window。 |
| `0x80000000-0x8FFFFFFF` | 256MB | `Sect_Device_RW` | Device/strongly ordered 风格，non-cacheable | RW | XN | FMC NAND memory window。 |
| `0x90000000-0x9FFFFFFF` | 256MB | `DESCRIPTOR_FAULT` | Fault | 不可访问 | 不可执行 | 当前没有显式映射。 |
| `0xA0000000-0xA0020FFF` | 132KB | `DESCRIPTOR_FAULT` | Fault | 不可访问 | 不可执行 | GIC 所在 1MB 窗口内，GIC 前方未映射部分。 |
| `0xA0021000-0xA0027FFF` | 28KB | `Page_4k_Device_RW` | Shared device，non-cacheable | RW | XN | GIC distributor/interface/virtual interface 相关页。 |
| `0xA0028000-0xBFFFFFFF` | 511.84375MB | `DESCRIPTOR_FAULT` | Fault | 不可访问 | 不可执行 | GIC 窗口剩余未映射区域，以及 DDR 前的空洞。 |
| `0xC0000000-0xDF1FFFFF` | 498MB | `Sect_Normal_Shared`，代码/RO data 页可能进一步细分 | Normal，WB/WA cacheable，shared | RW；RO data 页为 RO | 普通 DDR 可执行；RO data 页 XN | DDR 普通区。当前 linker 的 `DDR1_region` 放置 `.intvec`、RO code、RO data。 |
| `0xDF200000-0xDF2FFFFF` | 1MB | `Sect_Normal_NoneCacheable` | Normal，non-cacheable，non-shared | RW | 可执行 | ETH DMA no-cache 区。linker 中对应 `ETH_NOCACHE_region`，用于放置 `ETH_NOCACHE` section。 |
| `0xDF300000-0xFFFFFFFF` | 208MB | `Sect_Normal_Shared` | Normal，WB/WA cacheable，shared | RW | 可执行 | DDR 普通区。linker 中 `DDR2_region`、`DDR3_region` 位于该范围内；`TTB`、heap、栈等在 `DDR3_region`。 |

## 当前 DDR 启动时的代码/只读数据页

当前 DDR 链接脚本中 `.intvec` 从 `0xC0000000` 开始，因此正常会按 DDR 启动路径处理代码和只读数据。最终生效属性如下：

| 地址范围 | 粒度 | 最终描述符 | 内存/cache 状态 | 访问权限 | 执行权限 | 说明 |
| --- | ---: | --- | --- | --- | --- | --- |
| `aligned_CodeStart` 所在 1MB 窗口 | 4KB 页 | `Page_4k_Normal_RW` | Normal，WB/WA cacheable，shared | RW | XN | 仅当 code + RO data 可放入同一个 1MB 对齐窗口时建立。当前通常是 `0xC0000000-0xC00FFFFF`。 |
| `text_start_addr` 到 `text_end_addr` | 4KB 页 | `Page_4k_Normal_Cod` | Normal，WB/WA cacheable，shared | RW | 可执行 | 代码页。 |
| `rodata_start_addr` 到 `rodata_end_addr` | 4KB 页 | `Page_4k_Normal_RO` | Normal，WB/WA cacheable，shared | RO | XN | 只读数据页。 |

如果 code + RO data 不能放进同一个 1MB 对齐窗口，代码中的这部分 4KB 页细分会跳过，代码/RO data 会继续使用 DDR 普通区的 section 属性。

## 如果代码从 SYSRAM 运行

当 `.intvec` 地址小于 `DRAM_MEM_BASE` 时，代码会走 SYSRAM 启动路径。最终生效属性如下：

| 地址范围 | 粒度 | 最终描述符 | 内存/cache 状态 | 访问权限 | 执行权限 | 说明 |
| --- | ---: | --- | --- | --- | --- | --- |
| `0x2FF00000-0x2FFDFFFF` | 4KB 页 | `DESCRIPTOR_FAULT` | Fault | 不可访问 | 不可执行 | SYSRAM 所在 1MB 窗口内未使用部分。 |
| `0x2FFE0000-0x2FFFFFFF` | 4KB 页 | `Page_4k_Normal_RW_NonCacheable` | Normal，non-cacheable，shared | RW | XN | SYSRAM 数据基础映射。 |
| `text_start_addr` 到 `text_end_addr` | 4KB 页 | `Page_4k_Normal_Cod` | Normal，WB/WA cacheable，shared | RW | 可执行 | SYSRAM 中的代码页。 |
| `rodata_start_addr` 到 `rodata_end_addr` | 4KB 页 | `Page_4k_Normal_RO_NonCacheable` | Normal，non-cacheable，shared | RO | XN | SYSRAM 中的只读数据页。 |

## 当前 linker 区域与 MMU 的关系

| Linker 符号/区域 | 地址范围 | 用途 | MMU 最终状态 |
| --- | --- | --- | --- |
| `DDR1_region` | `0xC0000000-0xDF1FFFFF` | `.intvec`、RO code、RO data | DDR 普通 cacheable 区；代码/RO data 可能被 4KB 页进一步细分。 |
| `ETH_NOCACHE_region` | `0xDF200000-0xDF2FFFFF` | 放置 `ETH_NOCACHE` section，用于 ETH DMA descriptor/buffer | Normal non-cacheable 区，必须与 `ETH_NOCACHE_BASE`、`ETH_NOCACHE_SIZE_MB` 保持一致。 |
| `DDR2_region` | `0xDF37B000-0xDF3F7FFF` | 普通 readwrite 数据 | DDR 普通 cacheable 区。 |
| `DDR3_region` | `0xDF3F8000-0xDF3FFFFF` | `TTB`、heap、reserved、异常栈 | DDR 普通 cacheable 区；`TTB` 由 linker 保证 16KB 对齐。 |

## 使用注意

| 主题 | 注意事项 |
| --- | --- |
| ETH DMA | ETH DMA 描述符和 buffer 必须真的链接到 `ETH_NOCACHE` section。否则它们仍在 cacheable DDR 中，打开 DCache 后仍然需要 clean/invalidate。 |
| no-cache 区粒度 | 当前 ETH no-cache 区使用 1MB section 映射，因此 `ETH_NOCACHE_BASE` 必须 1MB 对齐，`ETH_NOCACHE_SIZE_MB` 单位是 MB。 |
| ETH no-cache 区仍可执行 | `section_normal_nc()` 生成的 section 描述符是 executable。功能上不影响 ETH 数据区使用，但从约束角度看，后续更推荐自定义 non-cacheable + XN 的数据区描述符。 |
| DDR 中的空隙 | linker 没有放置对象的 DDR 空隙，在 MMU 角度仍是 cacheable DDR；只是当前链接脚本不会主动把普通段放进去。 |

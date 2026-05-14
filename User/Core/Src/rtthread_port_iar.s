        MODULE  rtthread_port_iar

Mode_SVC        EQU     0x13
I_Bit           EQU     0x80
F_Bit           EQU     0x40

        SECTION .text:CODE:NOROOT(2)
        ARM

        PUBLIC  rt_hw_interrupt_disable
rt_hw_interrupt_disable
        MRS     R0, CPSR
        CPSID   i
        BX      LR

        PUBLIC  rt_hw_interrupt_enable
rt_hw_interrupt_enable
        MSR     CPSR_cxsf, R0
        BX      LR

        PUBLIC  rt_hw_context_switch_to
rt_hw_context_switch_to
        CLREX
        MSR     CPSR_c, #0xD3
        LDR     SP, [R0]
        B       rt_hw_context_switch_exit

        PUBLIC  rt_hw_context_switch
rt_hw_context_switch
        CLREX
        STMFD   SP!, {LR}
        STMFD   SP!, {R0-R12, LR}
        MRS     R4, CPSR
        TST     LR, #0x01
        ORRNE   R4, R4, #0x20
        STMFD   SP!, {R4}
        STR     SP, [R0]
        LDR     SP, [R1]
        B       rt_hw_context_switch_exit

        PUBLIC  rt_hw_context_switch_interrupt
        EXTERN  rt_thread_switch_interrupt_flag
        EXTERN  rt_interrupt_from_thread
        EXTERN  rt_interrupt_to_thread
rt_hw_context_switch_interrupt
        CLREX
        LDR     R12, =rt_thread_switch_interrupt_flag
        LDR     R3, [R12]
        CMP     R3, #1
        BEQ     _reswitch
        LDR     R3, =rt_interrupt_from_thread
        STR     R0, [R3]
        MOV     R3, #1
        STR     R3, [R12]
_reswitch
        LDR     R12, =rt_interrupt_to_thread
        STR     R1, [R12]
        BX      LR

        PUBLIC  rt_hw_context_switch_exit
rt_hw_context_switch_exit
        LDMFD   SP!, {R1}
        MSR     SPSR_cxsf, R1
        LDMFD   SP!, {R0-R12, LR, PC}^

        PUBLIC  RTThread_IRQ_Handler
        EXTERN  rt_interrupt_enter
        EXTERN  rt_hw_trap_irq
        EXTERN  rt_interrupt_leave
RTThread_IRQ_Handler
        STMFD   SP!, {R0-R12, LR}

        BL      rt_interrupt_enter
        BL      rt_hw_trap_irq
        BL      rt_interrupt_leave

        LDR     R0, =rt_thread_switch_interrupt_flag
        LDR     R1, [R0]
        CMP     R1, #1
        BEQ     rt_hw_context_switch_interrupt_do

        LDMFD   SP!, {R0-R12, LR}
        SUBS    PC, LR, #4

rt_hw_context_switch_interrupt_do
        MOV     R1, #0
        STR     R1, [R0]

        MOV     R1, SP
        ADD     SP, SP, #16
        LDMFD   SP!, {R4-R12, LR}
        MRS     R0, SPSR
        SUB     R2, LR, #4

        MSR     CPSR_c, #0xD3

        STMFD   SP!, {R2}
        STMFD   SP!, {R4-R12, LR}
        LDMFD   R1, {R1-R4}
        STMFD   SP!, {R1-R4}
        STMFD   SP!, {R0}

        LDR     R4, =rt_interrupt_from_thread
        LDR     R5, [R4]
        STR     SP, [R5]

        LDR     R6, =rt_interrupt_to_thread
        LDR     R6, [R6]
        LDR     SP, [R6]

        LDMFD   SP!, {R4}
        MSR     SPSR_cxsf, R4
        LDMFD   SP!, {R0-R12, LR, PC}^

        PUBLIC  _thread_start
_thread_start
        MOV     R10, LR
        BLX     R1
        BLX     R10
_thread_halt
        B       _thread_halt

        EXTERN  rt_hw_trap_swi
        EXTERN  rt_hw_trap_undef
        EXTERN  rt_hw_trap_pabt
        EXTERN  rt_hw_trap_dabt
        EXTERN  rt_hw_trap_resv

        PUBLIC  FreeRTOS_SWI_Handler
FreeRTOS_SWI_Handler
        SUB     SP, SP, #68
        STMIA   SP, {R0-R12}
        MOV     R0, SP
        MRS     R6, SPSR
        STR     LR, [R0, #60]
        STR     R6, [R0, #64]
        AND     R1, R6, #0x1F
        CMP     R1, #0x10
        BEQ     _swi_save_usr_sys
        CMP     R1, #0x1F
        BEQ     _swi_save_usr_sys
        MSR     CPSR_c, #0xD3
        ADD     R2, SP, #68
        STR     R2, [R0, #52]
        STR     LR, [R0, #56]
        B       _swi_call
_swi_save_usr_sys
        MSR     CPSR_c, #0xDF
        STR     SP, [R0, #52]
        STR     LR, [R0, #56]
        MSR     CPSR_c, #0xD3
_swi_call
        BL      rt_hw_trap_swi
_swi_halt
        B       _swi_halt

        PUBLIC  Undef_Handler
Undef_Handler
        SUB     SP, SP, #68
        STMIA   SP, {R0-R12}
        MOV     R0, SP
        MRS     R6, SPSR
        STR     LR, [R0, #60]
        STR     R6, [R0, #64]
        AND     R1, R6, #0x1F
        CMP     R1, #0x10
        BEQ     _undef_save_usr_sys
        CMP     R1, #0x1F
        BEQ     _undef_save_usr_sys
        MSR     CPSR_c, #0xD3
        STR     SP, [R0, #52]
        STR     LR, [R0, #56]
        B       _undef_call
_undef_save_usr_sys
        MSR     CPSR_c, #0xDF
        STR     SP, [R0, #52]
        STR     LR, [R0, #56]
        MSR     CPSR_c, #0xD3
_undef_call
        BL      rt_hw_trap_undef
_undef_halt
        B       _undef_halt

        PUBLIC  PAbt_Handler
PAbt_Handler
        SUB     SP, SP, #68
        STMIA   SP, {R0-R12}
        MOV     R0, SP
        MRS     R6, SPSR
        STR     LR, [R0, #60]
        STR     R6, [R0, #64]
        AND     R1, R6, #0x1F
        CMP     R1, #0x10
        BEQ     _pabt_save_usr_sys
        CMP     R1, #0x1F
        BEQ     _pabt_save_usr_sys
        MSR     CPSR_c, #0xD3
        STR     SP, [R0, #52]
        STR     LR, [R0, #56]
        B       _pabt_call
_pabt_save_usr_sys
        MSR     CPSR_c, #0xDF
        STR     SP, [R0, #52]
        STR     LR, [R0, #56]
        MSR     CPSR_c, #0xD3
_pabt_call
        BL      rt_hw_trap_pabt
_pabt_halt
        B       _pabt_halt

        PUBLIC  DAbt_Handler
DAbt_Handler
        SUB     SP, SP, #68
        STMIA   SP, {R0-R12}
        MOV     R0, SP
        MRS     R6, SPSR
        STR     LR, [R0, #60]
        STR     R6, [R0, #64]
        AND     R1, R6, #0x1F
        CMP     R1, #0x10
        BEQ     _dabt_save_usr_sys
        CMP     R1, #0x1F
        BEQ     _dabt_save_usr_sys
        MSR     CPSR_c, #0xD3
        STR     SP, [R0, #52]
        STR     LR, [R0, #56]
        B       _dabt_call
_dabt_save_usr_sys
        MSR     CPSR_c, #0xDF
        STR     SP, [R0, #52]
        STR     LR, [R0, #56]
        MSR     CPSR_c, #0xD3
_dabt_call
        BL      rt_hw_trap_dabt
_dabt_halt
        B       _dabt_halt

        PUBLIC  FIQ_Handler
FIQ_Handler
        SUB     SP, SP, #68
        STMIA   SP, {R0-R12}
        MOV     R0, SP
        MRS     R6, SPSR
        STR     LR, [R0, #60]
        STR     R6, [R0, #64]
        AND     R1, R6, #0x1F
        CMP     R1, #0x10
        BEQ     _fiq_save_usr_sys
        CMP     R1, #0x1F
        BEQ     _fiq_save_usr_sys
        MSR     CPSR_c, #0xD3
        STR     SP, [R0, #52]
        STR     LR, [R0, #56]
        B       _fiq_call
_fiq_save_usr_sys
        MSR     CPSR_c, #0xDF
        STR     SP, [R0, #52]
        STR     LR, [R0, #56]
        MSR     CPSR_c, #0xD3
_fiq_call
        BL      rt_hw_trap_resv
_fiq_halt
        B       _fiq_halt

        END

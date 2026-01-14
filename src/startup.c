#include <stdint.h>

// External symbols from linker script
extern uint32_t _sdata, _edata, _sbss, _ebss, _estack;
extern int main(void);

// Minimal startup code for Cortex-M3
void Reset_Handler(void)
{
    // Copy data section from flash to RAM
    uint32_t *src = &_sdata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }
    
    // Zero BSS section
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }
    
    // Call main
    main();
    
    // Infinite loop if main returns
    while(1);
}

// Default handler
void Default_Handler(void)
{
    while(1);
}

// Weak aliases for all interrupt handlers
void NMI_Handler(void)              __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)         __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)              __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)         __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)           __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)          __attribute__((weak, alias("Default_Handler")));

// Vector table
__attribute__((section(".isr_vector")))
const uint32_t vector_table[] = {
    (uint32_t)&_estack,             // Initial stack pointer
    (uint32_t)Reset_Handler,        // Reset handler
    (uint32_t)NMI_Handler,          // NMI handler
    (uint32_t)HardFault_Handler,    // Hard fault handler
    (uint32_t)MemManage_Handler,    // MPU fault handler
    (uint32_t)BusFault_Handler,     // Bus fault handler
    (uint32_t)UsageFault_Handler,   // Usage fault handler
    0,                               // Reserved
    0,                               // Reserved
    0,                               // Reserved
    0,                               // Reserved
    (uint32_t)SVC_Handler,          // SVCall handler
    (uint32_t)DebugMon_Handler,     // Debug monitor handler
    0,                               // Reserved
    (uint32_t)PendSV_Handler,       // PendSV handler
    (uint32_t)SysTick_Handler,      // SysTick handler
};

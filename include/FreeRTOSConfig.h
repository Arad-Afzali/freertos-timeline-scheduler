#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ===== Core Configuration ===== */
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     1
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0
#define configUSE_TICK_HOOK                     1

/* ===== CPU and Timing Configuration ===== */
#define configCPU_CLOCK_HZ                      ((unsigned long)12000000)
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configMAX_PRIORITIES                    (5)
#define configMINIMAL_STACK_SIZE                ((unsigned short)256)
#define configMAX_TASK_NAME_LEN                 (16)

/* ===== Memory Configuration ===== */
#define configTOTAL_HEAP_SIZE                   ((size_t)(20 * 1024))
#define configCHECK_FOR_STACK_OVERFLOW          2

/* ===== Task Configuration ===== */
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_ALTERNATIVE_API               0
#define configQUEUE_REGISTRY_SIZE               10
#define configUSE_QUEUE_SETS                    0

/* ===== Timeline Scheduler Configuration ===== */
#define configUSE_CYCLIC_EXECUTIVE              1

/* ===== Software Timer Configuration ===== */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

/* ===== Co-routine Configuration ===== */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         (2)

/* ===== Interrupt Priority Configuration (Cortex-M3) ===== */
/* LM3S6965 implements 3 priority bits (8 priority levels) */
#ifndef __NVIC_PRIO_BITS
    #define __NVIC_PRIO_BITS 3
#endif

#define configKERNEL_INTERRUPT_PRIORITY         (7 << (8 - __NVIC_PRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5 << (8 - __NVIC_PRIO_BITS))
#define configMAX_API_CALL_INTERRUPT_PRIORITY   configMAX_SYSCALL_INTERRUPT_PRIORITY

/* ===== Runtime Statistics Configuration ===== */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

/* ===== Stack Depth Type ===== */
#define configSTACK_DEPTH_TYPE                  uint16_t

/* ===== Message Buffer Configuration ===== */
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t

/* ===== Include Task API Functions ===== */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xEventGroupSetBitFromISR        1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  1
#define INCLUDE_xTaskResumeFromISR              1

/* ===== Cortex-M Specific Definitions ===== */
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS 4
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         7
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5

/* ===== Assertion Configuration ===== */
extern void vAssertCalled(const char *file, int line);
#define configASSERT(x) if((x) == 0) { vAssertCalled(__FILE__, __LINE__); }

/* ===== Scheduler Configuration ===== */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

#endif /* FREERTOS_CONFIG_H */

/**
 * @file main.c
 * @brief Timeline Scheduler - Test Harness and System Integration
 * @author Stefano (QA & Environment Lead)
 * 
 * This file integrates all components:
 * - Lifecycle Manager (Ali)
 * - Timeline Configuration (Arad + Ali)
 * - Trace System (Alessandro)
 * - Test Tasks (Stefano)
 */

#include "FreeRTOS.h"
#include "task.h"
#include "scheduler_config.h"
#include "lifecycle_manager.h"
#include "trace.h"
#include <stdio.h>

/* ===== Assert handler ===== */
void vAssertCalled(const char *file, int line)
{
    taskDISABLE_INTERRUPTS();
    printf("[ASSERT FAILED] %s:%d\n", file, line);
    for(;;);
}

/* ===== Test Configuration Flags ===== */
volatile uint32_t g_test_mode = 1;          // 1 = test mode (print trace after 3 frames)
volatile uint32_t g_test_complete = 0;      // Test completion flag
volatile uint32_t g_test_errors = 0;        // Error counter
volatile uint32_t g_test_frame_count = 0;   // Frame counter for testing

/* ===== Example HRT Task Functions ===== */

/**
 * @brief Hard Real-Time Task A
 * Simulates a deterministic HRT workload
 */
static void Task_HRT_A(void *pvParameters)
{
    (void)pvParameters;
    
    // Wait for activation from cyclic executive (Ali's kernel)
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    
    // Log task start
    vTraceTaskStart(xTaskGetCurrentTaskHandle());
    printf("[HRT_A] Activated at tick %u\n", (unsigned)xTaskGetTickCount());
    
    // Simulate deterministic work (e.g., sensor reading)
    volatile uint32_t counter = 0;
    for (uint32_t i = 0; i < 10000; i++)
    {
        counter++;
    }
    
    // Log task completion
    vTraceTaskComplete(xTaskGetCurrentTaskHandle(), pdFALSE, 0);
    printf("[HRT_A] Completed at tick %u\n", (unsigned)xTaskGetTickCount());
    
    // Suspend self forever - lifecycle manager will delete/recreate at frame reset
    for(;;)
    {
        vTaskSuspend(NULL);
    }
}

/**
 * @brief Hard Real-Time Task B
 * Another HRT task for testing non-overlapping execution
 */
static void Task_HRT_B(void *pvParameters)
{
    (void)pvParameters;
    
    // Wait for activation from cyclic executive
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    
    vTraceTaskStart(xTaskGetCurrentTaskHandle());
    printf("[HRT_B] Activated at tick %u\n", (unsigned)xTaskGetTickCount());
    
    // Simulate work
    volatile uint32_t counter = 0;
    for (uint32_t i = 0; i < 15000; i++)
    {
        counter++;
    }
    
    vTraceTaskComplete(xTaskGetCurrentTaskHandle(), pdFALSE, 0);
    printf("[HRT_B] Completed at tick %u\n", (unsigned)xTaskGetTickCount());
    
    // Suspend self forever
    for(;;)
    {
        vTaskSuspend(NULL);
    }
}

/* ===== Example SRT Task Functions ===== */

/**
 * @brief Soft Real-Time Task X
 * Runs during idle gaps left by HRT tasks
 */
static void Task_SRT_X(void *pvParameters)
{
    (void)pvParameters;
    
    vTraceTaskStart(xTaskGetCurrentTaskHandle());
    
    // Simulate best-effort work
    volatile uint32_t counter = 0;
    for (uint32_t i = 0; i < 50000; i++)
    {
        counter++;
        
        // SRT tasks can be preempted - check frequently
        if (i % 1000 == 0)
        {
            taskYIELD(); // Allow HRT preemption
        }
    }
    
    vTraceTaskComplete(xTaskGetCurrentTaskHandle(), pdFALSE, 0);
    
    // Suspend self forever - lifecycle manager will delete at frame reset
    for(;;)
    {
        vTaskSuspend(NULL);
    }
}

/**
 * @brief Soft Real-Time Task Y
 */
static void Task_SRT_Y(void *pvParameters)
{
    (void)pvParameters;
    
    vTraceTaskStart(xTaskGetCurrentTaskHandle());
    
    volatile uint32_t counter = 0;
    for (uint32_t i = 0; i < 30000; i++)
    {
        counter++;
        if (i % 1000 == 0)
        {
            taskYIELD();
        }
    }
    
    vTraceTaskComplete(xTaskGetCurrentTaskHandle(), pdFALSE, 0);
    
    // Suspend self forever - lifecycle manager will delete at frame reset
    for(;;)
    {
        vTaskSuspend(NULL);
    }
}

/* ===== Timeline Configuration ===== */

/**
 * @brief Test configuration with HRT and SRT tasks
 * 
 * Major Frame = 1000 ticks (1000ms with 1ms tick rate)
 * 
 * Timeline:
 * - Task_HRT_A: 100-200ms (HRT, Subframe 1)
 * - Task_HRT_B: 300-450ms (HRT, Subframe 2)
 * - Task_SRT_X: Idle time (SRT)
 * - Task_SRT_Y: Idle time (SRT)
 */
static TimelineTaskConfig_t xTestTasks[] = 
{
    // HRT Task A - Subframe 1 (100-200ms)
    {
        .pcTaskName = "HRT_A",
        .pxTaskFunction = Task_HRT_A,
        .xType = HARD_RT,
        .ulStart_time = 100,    // Start at 100ms
        .ulEnd_time = 200,      // Deadline at 200ms
        .ulSubframe_id = 1,
        .usStackDepth = configMINIMAL_STACK_SIZE
    },
    
    // HRT Task B - Subframe 2 (300-450ms)
    {
        .pcTaskName = "HRT_B",
        .pxTaskFunction = Task_HRT_B,
        .xType = HARD_RT,
        .ulStart_time = 300,    // Start at 300ms
        .ulEnd_time = 450,      // Deadline at 450ms
        .ulSubframe_id = 2,
        .usStackDepth = configMINIMAL_STACK_SIZE
    },
    
    // SRT Task X - Runs during idle
    {
        .pcTaskName = "SRT_X",
        .pxTaskFunction = Task_SRT_X,
        .xType = SOFT_RT,
        .ulStart_time = 0,      // No fixed start time
        .ulEnd_time = 0,        // No deadline
        .ulSubframe_id = 0,     // No subframe assignment
        .usStackDepth = configMINIMAL_STACK_SIZE
    },
    
    // SRT Task Y - Runs during idle
    {
        .pcTaskName = "SRT_Y",
        .pxTaskFunction = Task_SRT_Y,
        .xType = SOFT_RT,
        .ulStart_time = 0,
        .ulEnd_time = 0,
        .ulSubframe_id = 0,
        .usStackDepth = configMINIMAL_STACK_SIZE
    }
};

static TimelineConfig_t xTestConfig = 
{
    .pxTasks = xTestTasks,
    .ulNumTasks = sizeof(xTestTasks) / sizeof(TimelineTaskConfig_t)
};

/* ===== System Initialization ===== */

/**
 * @brief Initialize all system components
 */
static BaseType_t prvSystemInit(void)
{
    // 1. Initialize Trace System (Alessandro's module)
    vTraceInitialize();
    vTraceSetTicksPerMs(1);  // 1 tick = 1ms with configTICK_RATE_HZ = 1000
    
    printf("\n========================================\n");
    printf("Timeline Scheduler - Test Harness\n");
    printf("QA & Environment: Stefano\n");
    printf("========================================\n\n");
    
    // 2. Validate Configuration (from Arad's scheduler_config.h)
    vConfigureScheduler(&xTestConfig);
    
    printf("[INIT] Timeline configuration validated\n");
    printf("[INIT] Total tasks: %lu\n", xTestConfig.ulNumTasks);
    printf("[INIT] Major frame: %lu ticks\n", (unsigned long)MAJOR_FRAME_TICKS);
    
    // 3. Initialize Lifecycle Manager (Ali's module)
    if (xLifecycleManagerInit(&xTestConfig) != pdPASS)
    {
        printf("[ERROR] Failed to initialize Lifecycle Manager\n");
        g_test_errors++;
        return pdFAIL;
    }
    
    printf("[INIT] Lifecycle Manager initialized\n");
    
    // 4. Log kernel initialization
    vTraceKernelInitialize(xTestConfig.ulNumTasks, MAJOR_FRAME_TICKS);
    
    return pdPASS;
}

/* ===== Main Entry Point ===== */

int main(void)
{
    printf("\n[BOOT] Timeline Scheduler Starting...\n\n");
    
    // Initialize system
    if (prvSystemInit() != pdPASS)
    {
        printf("[FATAL] System initialization failed!\n");
        while(1)
        {
            __asm("NOP");
        }
    }
    
    printf("[START] Starting FreeRTOS Scheduler...\n\n");
    
    // Start Lifecycle Manager (Ali's API)
    vLifecycleManagerStart();
    
    // Start FreeRTOS scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    printf("[FATAL] Scheduler failed to start!\n");
    g_test_errors++;
    
    while(1)
    {
        __asm("NOP");
    }
    
    return 0;
}

/* ===== FreeRTOS Hook Functions ===== */

/**
 * @brief Tick Hook - called every tick
 * Used for frame counting and testing
 */
void vApplicationTickHook(void)
{
    static TickType_t xLastTick = 0;
    TickType_t xCurrentTick = xTaskGetTickCount();
    
    // Check for major frame boundary
    if ((xCurrentTick % MAJOR_FRAME_TICKS) == 0 && xCurrentTick != xLastTick)
    {
        g_test_frame_count++;
        xLastTick = xCurrentTick;
        
        // Stop after X frames for testing (optional)
        if (g_test_mode && g_test_frame_count >= 3)
        {
            printf("\n[TEST] Completed %lu frames\n", g_test_frame_count);
            vTracePrintAllEvents();
            vTraceGetStatistics();
            g_test_complete = 1;
        }
    }
}

/**
 * @brief Malloc Failed Hook
 */
void vApplicationMallocFailedHook(void)
{
    printf("[FATAL] Malloc failed!\n");
    g_test_errors++;
    taskDISABLE_INTERRUPTS();
    for(;;);
}

/**
 * @brief Stack Overflow Hook
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("[FATAL] Stack overflow in task: %s\n", pcTaskName);
    g_test_errors++;
    taskDISABLE_INTERRUPTS();
    for(;;);
}

/**
 * @brief Idle Hook
 */
void vApplicationIdleHook(void)
{
    // Can add CPU idle tracking here if needed
    __asm("WFI"); // Wait For Interrupt - low power mode
}

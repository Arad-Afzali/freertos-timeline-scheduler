#ifndef TRACE_H
#define TRACE_H

#include "FreeRTOS.h"
#include "task.h"

/* ===== Configuration ===== */
#define TRACE_MAX_EVENTS 512
#define TRACE_ENABLED 1

/* ===== Event Types ===== */
typedef enum {
    TRACE_EVENT_KERNEL_INIT = 0,
    TRACE_EVENT_KERNEL_SCHEDULE = 1,
    TRACE_EVENT_KERNEL_ACTIVATE = 2,
    TRACE_EVENT_TASK_RELEASE = 3,           /* NEW: Task release/activation */
    TRACE_EVENT_TASK_START = 4,
    TRACE_EVENT_TASK_COMPLETE = 5,
    TRACE_EVENT_DEADLINE_MISS = 6,
    TRACE_EVENT_TASK_TERMINATED = 7,
    TRACE_EVENT_TASK_OVERRUN = 8,           /* NEW: Period overrun */
    TRACE_EVENT_FRAME_START = 9,
    TRACE_EVENT_FRAME_END = 10,
    TRACE_EVENT_SUBFRAME_START = 11,
    TRACE_EVENT_SUBFRAME_END = 12,
    TRACE_EVENT_CPU_IDLE_START = 13,
    TRACE_EVENT_CPU_IDLE_END = 14
} TraceEventType_t;

/* ===== Kernel Decision Types ===== */
#define TRACE_KERNEL_DECISION_RUN 0
#define TRACE_KERNEL_DECISION_PREEMPT 1
#define TRACE_KERNEL_DECISION_IDLE 2
#define TRACE_KERNEL_DECISION_SUSPEND 3
#define TRACE_KERNEL_DECISION_ERROR 4

/* ===== Overrun Policy Types ===== */
#define TRACE_OVERRUN_POLICY_SKIP 0
#define TRACE_OVERRUN_POLICY_KILL 1
#define TRACE_OVERRUN_POLICY_CATCHUP 2

/* ===== Trace Event Structure ===== */
typedef struct {
    uint32_t ulTimestamp;       /* Tick when event occurred */
    uint8_t ucEventType;        /* TraceEventType_t */
    TaskHandle_t xTaskHandle;   /* Task involved (NULL if none) */
    uint32_t ulParam1;          /* Event-specific parameter */
    uint32_t ulParam2;          /* Event-specific parameter */
    uint32_t ulParam3;          /* Event-specific parameter */
} TraceEvent_t;

/* ===== Public API: Initialization & Control ===== */
void vTraceInitialize(void);
void vTraceReset(void);
void vTraceEnable(BaseType_t xEnable);
BaseType_t xTraceIsEnabled(void);

/**
 * @brief Set tick to ms conversion factor
 * @param ulTicksPerMs Number of ticks per millisecond (default: 1)
 */
void vTraceSetTicksPerMs(uint32_t ulTicksPerMs);

/* ===== Lifecycle Management Events ===== */

/**
 * @brief Log task release/activation
 * @param xTaskHandle Task being released
 */
void vTraceTaskRelease(TaskHandle_t xTaskHandle);

void vTraceTaskStart(TaskHandle_t xTaskHandle);
void vTraceTaskComplete(TaskHandle_t xTaskHandle, BaseType_t xDeadlineMissed, uint32_t ulDeadlineTime);
void vTraceTaskDeadlineMiss(TaskHandle_t xTaskHandle, uint32_t ulExpectedTime, uint32_t ulActualTime);
void vTraceTaskForceTerminated(TaskHandle_t xTaskHandle, uint8_t ucReason, uint32_t ulExecutionTime);

/**
 * @brief Log period overrun event
 * @param xTaskHandle Task that overran
 * @param ucPolicy Policy applied (SKIP/KILL/CATCHUP)
 */
void vTraceTaskOverrun(TaskHandle_t xTaskHandle, uint8_t ucPolicy);

/* ===== Kernel Scheduler Events ===== */
void vTraceKernelInitialize(uint32_t ulNumTasks, uint32_t ulMajorFrameTicks);
void vTraceKernelSchedule(TaskHandle_t xTaskHandle, uint32_t ulStartTime, uint32_t ulEndTime, uint8_t ucDecision);
void vTraceKernelTaskActivation(TaskHandle_t xTaskHandle, uint32_t ulActivationTime);

/* ===== System Events ===== */
void vTraceFrameStart(uint32_t ulFrameNumber);
void vTraceFrameEnd(uint32_t ulFrameNumber);
void vTraceSubframeStart(uint32_t ulSubframeId);
void vTraceSubframeEnd(uint32_t ulSubframeId);
void vTraceCpuIdleStart(void);
void vTraceCpuIdleEnd(void);

/* ===== Output & Analysis ===== */
void vTracePrintAllEvents(void);
void vTracePrintEventsSince(uint32_t ulTick);
uint32_t uxTraceGetEventCount(void);
uint32_t uxTraceGetEventCountSince(uint32_t ulTick);
BaseType_t xTraceGetEventAtIndex(uint32_t ulIndex, TraceEvent_t *pxEvent);
void vTraceGetStatistics(void);

#endif /* TRACE_H */

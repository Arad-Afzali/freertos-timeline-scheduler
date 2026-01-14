#include "trace.h"
#include <stdio.h>
#include <string.h>

/* ===== Ring Buffer State ===== */
static TraceEvent_t xTraceBuffer[TRACE_MAX_EVENTS];
static volatile uint32_t ulWriteIndex = 0;
static volatile uint32_t ulEventCount = 0;  /* NEW: manteniamo il conteggio */
static volatile BaseType_t xTraceEnabled = pdTRUE;
static volatile BaseType_t xTraceInitialized = pdFALSE;
static volatile uint32_t ulTicksPerMs = 1;

/* ===== Internal Macros ===== */
#define TRACE_NEXT_INDEX(idx) (((idx) + 1) % TRACE_MAX_EVENTS)
#define TICKS_TO_MS(ticks) ((ticks) / ulTicksPerMs)

/* ===== Private Functions ===== */
static void prvTraceAddEvent(TraceEvent_t *pxEvent)
{
    if (!xTraceEnabled || !xTraceInitialized)
        return;

    taskENTER_CRITICAL();
    xTraceBuffer[ulWriteIndex] = *pxEvent;
    ulWriteIndex = TRACE_NEXT_INDEX(ulWriteIndex);

    if (ulEventCount < TRACE_MAX_EVENTS) {
        ulEventCount++;
    }
    taskEXIT_CRITICAL();
}

static const char *prvGetEventTypeName(uint8_t ucType)
{
    switch (ucType) {
        case TRACE_EVENT_KERNEL_INIT: return "KERNEL_INIT";
        case TRACE_EVENT_KERNEL_SCHEDULE: return "KERNEL_SCHEDULE";
        case TRACE_EVENT_KERNEL_ACTIVATE: return "KERNEL_ACTIVATE";
        case TRACE_EVENT_TASK_RELEASE: return "RELEASE";
        case TRACE_EVENT_TASK_START: return "START";
        case TRACE_EVENT_TASK_COMPLETE: return "COMPLETE";
        case TRACE_EVENT_DEADLINE_MISS: return "DEADLINE_MISS";
        case TRACE_EVENT_TASK_TERMINATED: return "TERMINATED";
        case TRACE_EVENT_TASK_OVERRUN: return "OVERRUN";
        case TRACE_EVENT_FRAME_START: return "FRAME_START";
        case TRACE_EVENT_FRAME_END: return "FRAME_END";
        case TRACE_EVENT_SUBFRAME_START: return "SUBFRAME_START";
        case TRACE_EVENT_SUBFRAME_END: return "SUBFRAME_END";
        case TRACE_EVENT_CPU_IDLE_START: return "CPU_IDLE_START";
        case TRACE_EVENT_CPU_IDLE_END: return "CPU_IDLE_END";
        default: return "UNKNOWN";
    }
}

static const char *prvGetTaskName(TaskHandle_t xHandle)
{
    if (xHandle == NULL)
        return "IDLE";
    return pcTaskGetName(xHandle);
}

static const char *prvGetPolicyName(uint8_t ucPolicy)
{
    switch (ucPolicy) {
        case TRACE_OVERRUN_POLICY_SKIP: return "SKIP";
        case TRACE_OVERRUN_POLICY_KILL: return "KILL";
        case TRACE_OVERRUN_POLICY_CATCHUP: return "CATCHUP";
        default: return "UNKNOWN";
    }
}

static void prvPrintEvent(TraceEvent_t *pxEvent)
{
    uint32_t ulTimeMs = TICKS_TO_MS(pxEvent->ulTimestamp);
    const char *pcTaskName = prvGetTaskName(pxEvent->xTaskHandle);
    const char *pcEventName = prvGetEventTypeName(pxEvent->ucEventType);

    switch (pxEvent->ucEventType) {
        case TRACE_EVENT_TASK_RELEASE:
        case TRACE_EVENT_TASK_START:
        case TRACE_EVENT_TASK_COMPLETE:
            printf("%lu %s %s\n", ulTimeMs, pcTaskName, pcEventName);
            break;

        case TRACE_EVENT_DEADLINE_MISS:
            printf("%lu %s %s (D=%lu, tick %lu)\n", 
                   ulTimeMs, pcTaskName, pcEventName,
                   TICKS_TO_MS(pxEvent->ulParam1), pxEvent->ulTimestamp);
            break;

        case TRACE_EVENT_TASK_TERMINATED:
            printf("%lu ms %s deadline miss terminated\n", 
                   ulTimeMs, pcTaskName);
            break;

        case TRACE_EVENT_TASK_OVERRUN:
            printf("%lu %s %s %s\n", 
                   ulTimeMs, pcTaskName, pcEventName,
                   prvGetPolicyName(pxEvent->ulParam1));
            break;

        case TRACE_EVENT_CPU_IDLE_START:
            printf("%lu CPU IDLE START\n", ulTimeMs);
            break;

        case TRACE_EVENT_CPU_IDLE_END:
            printf("%lu CPU IDLE END\n", ulTimeMs);
            break;

        case TRACE_EVENT_FRAME_START:
            printf("%lu FRAME %lu START\n", ulTimeMs, pxEvent->ulParam1);
            break;

        case TRACE_EVENT_FRAME_END:
            printf("%lu FRAME %lu END\n", ulTimeMs, pxEvent->ulParam1);
            break;

        case TRACE_EVENT_SUBFRAME_START:
            printf("%lu SUBFRAME %lu START\n", ulTimeMs, pxEvent->ulParam1);
            break;

        case TRACE_EVENT_SUBFRAME_END:
            printf("%lu SUBFRAME %lu END\n", ulTimeMs, pxEvent->ulParam1);
            break;

        default:
            printf("%lu %s %s\n", ulTimeMs, pcTaskName, pcEventName);
            break;
    }
}

/* ===== Public API: Initialization & Control ===== */
void vTraceInitialize(void)
{
    taskENTER_CRITICAL();
    memset(xTraceBuffer, 0, sizeof(xTraceBuffer));
    ulWriteIndex = 0;
    ulEventCount = 0;
    xTraceEnabled = pdTRUE;
    xTraceInitialized = pdTRUE;
    taskEXIT_CRITICAL();
}

void vTraceReset(void)
{
    taskENTER_CRITICAL();
    memset(xTraceBuffer, 0, sizeof(xTraceBuffer));
    ulWriteIndex = 0;
    ulEventCount = 0;
    taskEXIT_CRITICAL();
}

void vTraceEnable(BaseType_t xEnable)
{
    taskENTER_CRITICAL();
    xTraceEnabled = xEnable;
    taskEXIT_CRITICAL();
}

BaseType_t xTraceIsEnabled(void)
{
    return xTraceEnabled;
}

void vTraceSetTicksPerMs(uint32_t ulTicksPerMsValue)
{
    if (ulTicksPerMsValue > 0) {
        ulTicksPerMs = ulTicksPerMsValue;
    }
}

/* ===== Lifecycle Management Events ===== */
void vTraceTaskRelease(TaskHandle_t xTaskHandle)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_TASK_RELEASE;
    xEvent.xTaskHandle = xTaskHandle;
    prvTraceAddEvent(&xEvent);
}

void vTraceTaskStart(TaskHandle_t xTaskHandle)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_TASK_START;
    xEvent.xTaskHandle = xTaskHandle;
    prvTraceAddEvent(&xEvent);
}

void vTraceTaskComplete(TaskHandle_t xTaskHandle, BaseType_t xDeadlineMissed, uint32_t ulDeadlineTime)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_TASK_COMPLETE;
    xEvent.xTaskHandle = xTaskHandle;
    xEvent.ulParam1 = xDeadlineMissed;
    xEvent.ulParam2 = ulDeadlineTime;
    prvTraceAddEvent(&xEvent);
}

void vTraceTaskDeadlineMiss(TaskHandle_t xTaskHandle, uint32_t ulExpectedTime, uint32_t ulActualTime)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_DEADLINE_MISS;
    xEvent.xTaskHandle = xTaskHandle;
    xEvent.ulParam1 = ulExpectedTime;
    xEvent.ulParam2 = ulActualTime;
    prvTraceAddEvent(&xEvent);
}

void vTraceTaskForceTerminated(TaskHandle_t xTaskHandle, uint8_t ucReason, uint32_t ulExecutionTime)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_TASK_TERMINATED;
    xEvent.xTaskHandle = xTaskHandle;
    xEvent.ulParam1 = ucReason;
    xEvent.ulParam2 = ulExecutionTime;
    prvTraceAddEvent(&xEvent);
}

void vTraceTaskOverrun(TaskHandle_t xTaskHandle, uint8_t ucPolicy)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_TASK_OVERRUN;
    xEvent.xTaskHandle = xTaskHandle;
    xEvent.ulParam1 = ucPolicy;
    prvTraceAddEvent(&xEvent);
}

/* ===== Kernel Scheduler Events ===== */
void vTraceKernelInitialize(uint32_t ulNumTasks, uint32_t ulMajorFrameTicks)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_KERNEL_INIT;
    xEvent.xTaskHandle = NULL;
    xEvent.ulParam1 = ulNumTasks;
    xEvent.ulParam2 = ulMajorFrameTicks;
    prvTraceAddEvent(&xEvent);
}

void vTraceKernelSchedule(TaskHandle_t xTaskHandle, uint32_t ulStartTime, uint32_t ulEndTime, uint8_t ucDecision)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_KERNEL_SCHEDULE;
    xEvent.xTaskHandle = xTaskHandle;
    xEvent.ulParam1 = ulStartTime;
    xEvent.ulParam2 = ulEndTime;
    xEvent.ulParam3 = ucDecision;
    prvTraceAddEvent(&xEvent);
}

void vTraceKernelTaskActivation(TaskHandle_t xTaskHandle, uint32_t ulActivationTime)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_KERNEL_ACTIVATE;
    xEvent.xTaskHandle = xTaskHandle;
    xEvent.ulParam1 = ulActivationTime;
    prvTraceAddEvent(&xEvent);
}

/* ===== System Events ===== */
void vTraceFrameStart(uint32_t ulFrameNumber)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_FRAME_START;
    xEvent.xTaskHandle = NULL;
    xEvent.ulParam1 = ulFrameNumber;
    prvTraceAddEvent(&xEvent);
}

void vTraceFrameEnd(uint32_t ulFrameNumber)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_FRAME_END;
    xEvent.xTaskHandle = NULL;
    xEvent.ulParam1 = ulFrameNumber;
    prvTraceAddEvent(&xEvent);
}

void vTraceSubframeStart(uint32_t ulSubframeId)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_SUBFRAME_START;
    xEvent.xTaskHandle = NULL;
    xEvent.ulParam1 = ulSubframeId;
    prvTraceAddEvent(&xEvent);
}

void vTraceSubframeEnd(uint32_t ulSubframeId)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_SUBFRAME_END;
    xEvent.xTaskHandle = NULL;
    xEvent.ulParam1 = ulSubframeId;
    prvTraceAddEvent(&xEvent);
}

void vTraceCpuIdleStart(void)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_CPU_IDLE_START;
    xEvent.xTaskHandle = NULL;
    prvTraceAddEvent(&xEvent);
}

void vTraceCpuIdleEnd(void)
{
    TraceEvent_t xEvent = {0};
    xEvent.ulTimestamp = xTaskGetTickCount();
    xEvent.ucEventType = TRACE_EVENT_CPU_IDLE_END;
    xEvent.xTaskHandle = NULL;
    prvTraceAddEvent(&xEvent);
}

/* ===== Output & Analysis ===== */
void vTracePrintAllEvents(void)
{
    printf("\n========== TRACE LOG ==========\n");

    for (uint32_t i = 0; i < TRACE_MAX_EVENTS; i++) {
        TraceEvent_t *pxEvent = &xTraceBuffer[i];
        if (pxEvent->ucEventType == 0)
            continue;

        prvPrintEvent(pxEvent);
    }

    printf("========== END TRACE LOG ==========\n\n");
}

void vTracePrintEventsSince(uint32_t ulTick)
{
    printf("\n========== TRACE EVENTS SINCE TICK %lu ==========\n", ulTick);

    for (uint32_t i = 0; i < TRACE_MAX_EVENTS; i++) {
        TraceEvent_t *pxEvent = &xTraceBuffer[i];
        if (pxEvent->ucEventType != 0 && pxEvent->ulTimestamp >= ulTick) {
            prvPrintEvent(pxEvent);
        }
    }

    printf("==========================================\n\n");
}

uint32_t uxTraceGetEventCount(void)
{
    return ulEventCount;
}

uint32_t uxTraceGetEventCountSince(uint32_t ulTick)
{
    uint32_t ulCount = 0;
    for (uint32_t i = 0; i < TRACE_MAX_EVENTS; i++) {
        if (xTraceBuffer[i].ucEventType != 0 && xTraceBuffer[i].ulTimestamp >= ulTick)
            ulCount++;
    }
    return ulCount;
}

BaseType_t xTraceGetEventAtIndex(uint32_t ulIndex, TraceEvent_t *pxEvent)
{
    if (ulIndex >= TRACE_MAX_EVENTS || pxEvent == NULL)
        return pdFALSE;
    *pxEvent = xTraceBuffer[ulIndex];
    return pdTRUE;
}

void vTraceGetStatistics(void)
{
    uint32_t ulDeadlineMisses = 0;
    uint32_t ulTasksCompleted = 0;
    uint32_t ulTasksTerminated = 0;
    uint32_t ulOverruns = 0;
    uint32_t ulMinTick = UINT32_MAX;
    uint32_t ulMaxTick = 0;

    for (uint32_t i = 0; i < TRACE_MAX_EVENTS; i++) {
        TraceEvent_t *pxEvent = &xTraceBuffer[i];
        if (pxEvent->ucEventType == 0)
            continue;

        if (pxEvent->ulTimestamp < ulMinTick)
            ulMinTick = pxEvent->ulTimestamp;
        if (pxEvent->ulTimestamp > ulMaxTick)
            ulMaxTick = pxEvent->ulTimestamp;

        switch (pxEvent->ucEventType) {
            case TRACE_EVENT_DEADLINE_MISS:
                ulDeadlineMisses++;
                break;
            case TRACE_EVENT_TASK_COMPLETE:
                ulTasksCompleted++;
                break;
            case TRACE_EVENT_TASK_TERMINATED:
                ulTasksTerminated++;
                break;
            case TRACE_EVENT_TASK_OVERRUN:
                ulOverruns++;
                break;
            default:
                break;
        }
    }

    printf("\n========== TRACE STATISTICS ==========\n");
    printf("Total Events:        %lu\n", ulEventCount);
    printf("Time Window:         %lu ms (%lu ticks)\n", 
           TICKS_TO_MS(ulMaxTick - ulMinTick), (ulMaxTick - ulMinTick));
    printf("Deadline Misses:     %lu\n", ulDeadlineMisses);
    printf("Period Overruns:     %lu\n", ulOverruns);
    printf("Tasks Completed:     %lu\n", ulTasksCompleted);
    printf("Tasks Terminated:    %lu\n", ulTasksTerminated);
    printf("======================================\n\n");
}
// timeline_config.h
// Timeline Scheduler configuration types and declarations
// Internal structures for the Kernel (Ali) to use.

#ifndef TIMELINE_CONFIG_H
#define TIMELINE_CONFIG_H

#include "FreeRTOS.h"
#include "task.h"

// Internal schedule table entry
typedef struct {
    TaskHandle_t xHandle;     // The FreeRTOS task handle
    uint32_t ulStartTime;
    uint32_t ulEndTime;
    uint8_t  ucTaskType;      // 0=HRT, 1=SRT
} TimelineScheduleEntry_t;

#define MAX_TIMELINE_TASKS 16

// Timeline control block - exposed to the kernel
typedef struct {
    TimelineScheduleEntry_t xEntries[MAX_TIMELINE_TASKS];
    uint32_t ulTaskCount;
    uint32_t ulMajorFrameTicks;
} TimelineControlBlock_t;

// Global timeline control block
extern TimelineControlBlock_t xSystemTimeline;

#endif // TIMELINE_CONFIG_H

#ifndef SCHEDULER_CONFIG_H
#define SCHEDULER_CONFIG_H

// Include FreeRTOS headers so we can use the types
#include "FreeRTOS.h"
#include "task.h"

// This is how long our major frame lasts (in ticks)
#define MAJOR_FRAME_TICKS 1000

// This enum tells us if a task is hard real-time or soft real-time
typedef enum
{
    HARD_RT,  // Hard real-time - must finish on time
    SOFT_RT   // Soft real-time - more flexible timing
} TimelineTaskType_t;

// This struct holds all the info we need for one task in our timeline
typedef struct
{
    const char* pcTaskName;              // Name of the task
    TaskFunction_t pxTaskFunction;       // The function to run
    TimelineTaskType_t xType;            // Is it Hard or Soft RT?
    uint32_t ulStart_time;               // When it starts in the frame
    uint32_t ulEnd_time;                 // When it must finish
    uint32_t ulSubframe_id;              // Which sub-frame it is in
    configSTACK_DEPTH_TYPE usStackDepth; // How much stack it needs
} TimelineTaskConfig_t;

// This struct holds our entire timeline configuration
// It points to an array of tasks and tells us how many tasks there are
typedef struct
{
    TimelineTaskConfig_t* pxTasks;  // Pointer to the array of tasks
    uint32_t ulNumTasks;            // Number of tasks in the array
} TimelineConfig_t;

// Function to set up the scheduler with our configuration
void vConfigureScheduler(TimelineConfig_t* pxConfig);

#endif // SCHEDULER_CONFIG_H

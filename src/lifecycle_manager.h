#ifndef LIFECYCLE_MANAGER_H
#define LIFECYCLE_MANAGER_H

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "scheduler_config.h"

/* ===== Runtime State Tracking ===== */

/**
 * @brief Runtime state of a single task in the timeline
 */
typedef struct
{
    TaskHandle_t xTaskHandle;           /* Handle to the FreeRTOS task */
    TimelineTaskConfig_t* pxConfig;     /* Pointer to task configuration */
    TimerHandle_t xDeadlineTimer;       /* Timer for HRT deadline enforcement (NULL for SRT) */
    TickType_t xStartTick;              /* Tick when task was started */
    BaseType_t xIsRunning;              /* pdTRUE if task is currently running */
    BaseType_t xIsCompleted;            /* pdTRUE if task finished within deadline */
    BaseType_t xDeadlineMissed;         /* pdTRUE if task was terminated for missing deadline */
} TaskRuntimeState_t;

/**
 * @brief Global timeline manager state
 */
typedef struct
{
    TaskRuntimeState_t* pxRuntimeStates;    /* Array of runtime states for all tasks */
    TimelineConfig_t* pxConfig;             /* Pointer to original configuration */
    uint32_t ulNumTasks;                    /* Total number of tasks */
    TickType_t xFrameStartTick;             /* Tick when current frame started */
    uint32_t ulCurrentFrame;                /* Current frame number (for debugging) */
    TaskHandle_t xManagerTaskHandle;        /* Handle to the Timeline Manager task */
    TimerHandle_t xFrameResetTimer;         /* Timer for major frame reset */
    BaseType_t xIsInitialized;              /* pdTRUE if manager is initialized */
} TimelineManager_t;

/* ===== Public API ===== */

/**
 * @brief Initialize the Lifecycle Manager with a timeline configuration
 * 
 * This function:
 * - Allocates runtime state tracking structures
 * - Creates all tasks in suspended state
 * - Sets up deadline timers for HRT tasks
 * - Creates the Timeline Manager task
 * - Starts the major frame timer
 * 
 * @param pxConfig Pointer to timeline configuration (must remain valid)
 * @return pdPASS on success, pdFAIL on error
 */
BaseType_t xLifecycleManagerInit( TimelineConfig_t* pxConfig );

/**
 * @brief Start the timeline scheduler
 * 
 * Called after initialization to begin timeline execution.
 * This activates the first frame and starts task scheduling.
 */
void vLifecycleManagerStart( void );

/**
 * @brief Monitor and enforce HRT task deadlines
 * 
 * Checks all running HRT tasks and terminates any that have
 * exceeded their ulEnd_time deadline.
 * 
 * Called periodically by the Timeline Manager task.
 */
void vMonitorHRTDeadlines( void );

/**
 * @brief Schedule SRT tasks during idle gaps
 * 
 * Determines if any HRT task is active. If not, resumes the next
 * SRT task in the fixed execution order. If HRT becomes active,
 * SRT tasks are automatically preempted due to priority.
 * 
 * Called by the Timeline Manager task.
 */
void vScheduleSRTTasks( void );

/**
 * @brief Handle major frame reset and reinitialization
 * 
 * Called at the end of each major frame (MAJOR_FRAME_TICKS).
 * Deletes all tasks and recreates them from the original configuration
 * to ensure deterministic behavior across frames.
 * 
 * This is the "destroy & recreate" approach.
 */
void vHandleMajorFrameReset( void );

/**
 * @brief Check if a task is past its deadline
 * 
 * @param pxTask Runtime state of task to check
 * @return pdTRUE if task is past deadline, pdFALSE otherwise
 */
BaseType_t bIsTaskPastDeadline( TaskRuntimeState_t* pxTask );

/**
 * @brief Terminate a task that has exceeded its deadline
 * 
 * Performs clean termination:
 * - Stops deadline timer
 * - Deletes the task
 * - Logs the deadline miss
 * - Updates runtime state
 * 
 * @param pxTask Runtime state of task to terminate
 */
void vTerminateTask( TaskRuntimeState_t* pxTask );

/**
 * @brief Check if any HRT task is currently active
 * 
 * @return pdTRUE if any HRT task is running, pdFALSE otherwise
 */
BaseType_t bIsAnyHRTTaskActive( void );

/**
 * @brief Get the current frame time (ticks since frame start)
 * 
 * @return Number of ticks elapsed in current major frame
 */
TickType_t xGetCurrentFrameTime( void );

/**
 * @brief Notification that an HRT task has started
 * 
 * Called by kernel when an HRT task begins execution.
 * Starts the deadline timer and updates runtime state.
 * 
 * @param ulTaskIndex Index of task in configuration array
 */
void vNotifyHRTTaskStarted( uint32_t ulTaskIndex );

/**
 * @brief Notification that an HRT task has completed
 * 
 * Called bykernel (or by the task itself) when it finishes.
 * Stops the deadline timer and marks task as completed.
 * 
 * @param ulTaskIndex Index of task in configuration array
 */
void vNotifyHRTTaskCompleted( uint32_t ulTaskIndex );

/* ===== Internal Functions (exposed for testing/integration) ===== */

/**
 * @brief Main loop of the Timeline Manager task
 * 
 * Periodically checks:
 * - HRT deadline enforcement
 * - SRT task scheduling
 * - Frame timing
 * 
 * @param pvParameters Unused
 */
void vTimelineManagerTask( void* pvParameters );

/**
 * @brief Callback for HRT deadline timers
 * 
 * Invoked when an HRT task's deadline expires.
 * Terminates the task if still running.
 * 
 * @param xTimer Timer handle that expired
 */
void vDeadlineTimerCallback( TimerHandle_t xTimer );

/**
 * @brief Callback for major frame reset timer
 * 
 * Invoked when the major frame period expires.
 * Triggers frame reset and reinitialization.
 * 
 * @param xTimer Timer handle that expired
 */
void vFrameResetTimerCallback( TimerHandle_t xTimer );

/* ===== Configuration Constants ===== */

/* Priority levels for different task types */
#define TIMELINE_MANAGER_PRIORITY       ( configMAX_PRIORITIES - 1 )  /* Highest: 4 */
#define HRT_TASK_PRIORITY               ( configMAX_PRIORITIES - 2 )  /* High: 3 */
#define SRT_TASK_PRIORITY               ( 1 )                         /* Low: 1 */

/* Stack size for Timeline Manager task */
#define TIMELINE_MANAGER_STACK_SIZE     ( 512 )

/* How often the Timeline Manager checks system state (in ticks) */
#define MANAGER_CHECK_PERIOD_TICKS      ( 1 )  /* Check every 1ms */

#endif /* LIFECYCLE_MANAGER_H */


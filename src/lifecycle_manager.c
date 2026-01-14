
#include "lifecycle_manager.h"
#include <string.h>
#include <stdio.h>

/* ===== Global State ===== */

/* Single global instance of the timeline manager */
static TimelineManager_t xTimelineManager;

/* Index for tracking which SRT task to run next */
static uint32_t ulCurrentSRTIndex = 0;

/* ===== Logging Macros ===== */

#define LOG_DEADLINE_MISS( taskName, tick ) \
    printf( "[DEADLINE_MISS] Task '%s' terminated at tick %u\n", (taskName), (unsigned int)(tick) )

#define LOG_TASK_STARTED( taskName, tick ) \
    printf( "[TASK_START] Task '%s' started at tick %u\n", (taskName), (unsigned int)(tick) )

#define LOG_TASK_COMPLETED( taskName, tick ) \
    printf( "[TASK_COMPLETE] Task '%s' completed at tick %u\n", (taskName), (unsigned int)(tick) )

#define LOG_FRAME_RESET( frameNum, tick ) \
    printf( "[FRAME_RESET] Frame %u completed at tick %u, resetting...\n", (unsigned int)(frameNum), (unsigned int)(tick) )

#define LOG_ERROR( msg ) \
    printf( "[ERROR] %s\n", (msg) )

/* ===== Forward Declarations ===== */

static BaseType_t prvCreateAllTasks( void );
static void prvDeleteAllTasks( void );
static void prvResetRuntimeStates( void );
static TaskRuntimeState_t* prvFindRuntimeStateByHandle( TaskHandle_t xHandle );

/* ===== Public API Implementation ===== */

BaseType_t xLifecycleManagerInit( TimelineConfig_t* pxConfig )
{
    if( pxConfig == NULL || pxConfig->pxTasks == NULL || pxConfig->ulNumTasks == 0 )
    {
        LOG_ERROR( "Invalid configuration passed to xLifecycleManagerInit" );
        return pdFAIL;
    }

    /* Initialize manager structure */
    memset( &xTimelineManager, 0, sizeof( TimelineManager_t ) );
    xTimelineManager.pxConfig = pxConfig;
    xTimelineManager.ulNumTasks = pxConfig->ulNumTasks;
    xTimelineManager.ulCurrentFrame = 0;
    xTimelineManager.xFrameStartTick = 0;

    /* Allocate runtime state array */
    xTimelineManager.pxRuntimeStates = ( TaskRuntimeState_t* ) pvPortMalloc(
        pxConfig->ulNumTasks * sizeof( TaskRuntimeState_t )
    );

    if( xTimelineManager.pxRuntimeStates == NULL )
    {
        LOG_ERROR( "Failed to allocate runtime state array" );
        return pdFAIL;
    }

    /* Initialize runtime states */
    prvResetRuntimeStates();

    /* Create all tasks */
    if( prvCreateAllTasks() != pdPASS )
    {
        LOG_ERROR( "Failed to create tasks" );
        vPortFree( xTimelineManager.pxRuntimeStates );
        return pdFAIL;
    }

    /* Create major frame reset timer */
    xTimelineManager.xFrameResetTimer = xTimerCreate(
        "FrameReset",                    /* Timer name */
        MAJOR_FRAME_TICKS,               /* Period */
        pdFALSE,                         /* One-shot (not auto-reload) */
        ( void* ) 0,                     /* Timer ID */
        vFrameResetTimerCallback         /* Callback function */
    );

    if( xTimelineManager.xFrameResetTimer == NULL )
    {
        LOG_ERROR( "Failed to create frame reset timer" );
        prvDeleteAllTasks();
        vPortFree( xTimelineManager.pxRuntimeStates );
        return pdFAIL;
    }

    /* Create Timeline Manager task */
    BaseType_t xResult = xTaskCreate(
        vTimelineManagerTask,             /* Task function */
        "TimelineMgr",                    /* Task name */
        TIMELINE_MANAGER_STACK_SIZE,      /* Stack size */
        NULL,                             /* Parameters */
        TIMELINE_MANAGER_PRIORITY,        /* Priority (highest) */
        &xTimelineManager.xManagerTaskHandle  /* Task handle */
    );

    if( xResult != pdPASS )
    {
        LOG_ERROR( "Failed to create Timeline Manager task" );
        xTimerDelete( xTimelineManager.xFrameResetTimer, 0 );
        prvDeleteAllTasks();
        vPortFree( xTimelineManager.pxRuntimeStates );
        return pdFAIL;
    }

    xTimelineManager.xIsInitialized = pdTRUE;
    printf( "[INIT] Lifecycle Manager initialized with %u tasks\n", 
            (unsigned int)pxConfig->ulNumTasks );

    return pdPASS;
}

void vLifecycleManagerStart( void )
{
    if( !xTimelineManager.xIsInitialized )
    {
        LOG_ERROR( "Cannot start: Lifecycle Manager not initialized" );
        return;
    }

    /* Record frame start time */
    xTimelineManager.xFrameStartTick = xTaskGetTickCount();
    xTimelineManager.ulCurrentFrame = 1;

    /* Start the major frame timer */
    xTimerStart( xTimelineManager.xFrameResetTimer, 0 );

    printf( "[START] Timeline scheduler started at tick %u\n", 
            (unsigned int)xTimelineManager.xFrameStartTick );

    /* Note: The Timeline Manager task is already created and will start running
     * based on its priority. Ali's kernel will trigger HRT tasks at their start times. */
}

void vMonitorHRTDeadlines( void )
{
    TickType_t xCurrentTick = xTaskGetTickCount();
    TickType_t xFrameTime = xCurrentTick - xTimelineManager.xFrameStartTick;

    for( uint32_t i = 0; i < xTimelineManager.ulNumTasks; i++ )
    {
        TaskRuntimeState_t* pxTask = &xTimelineManager.pxRuntimeStates[ i ];

        /* Only check HRT tasks that are running */
        if( pxTask->pxConfig->xType == HARD_RT && pxTask->xIsRunning )
        {
            if( xFrameTime >= pxTask->pxConfig->ulEnd_time )
            {
                /* Task has exceeded its deadline - terminate it */
                vTerminateTask( pxTask );
            }
        }
    }
}

void vScheduleSRTTasks( void )
{
    /* Check if any HRT task is active */
    if( bIsAnyHRTTaskActive() )
    {
        /* HRT task is running - SRT tasks should be suspended or will be
         * automatically preempted due to lower priority. Nothing to do here. */
        return;
    }

    /* Find next SRT task to run */
    for( uint32_t attempts = 0; attempts < xTimelineManager.ulNumTasks; attempts++ )
    {
        uint32_t ulIndex = ulCurrentSRTIndex;
        TaskRuntimeState_t* pxTask = &xTimelineManager.pxRuntimeStates[ ulIndex ];

        /* Move to next task for next call */
        ulCurrentSRTIndex = ( ulCurrentSRTIndex + 1 ) % xTimelineManager.ulNumTasks;

        /* Check if this is an SRT task and not completed */
        if( pxTask->pxConfig->xType == SOFT_RT && !pxTask->xIsCompleted )
        {
            /* Resume this SRT task if suspended */
            if( eTaskGetState( pxTask->xTaskHandle ) == eSuspended )
            {
                vTaskResume( pxTask->xTaskHandle );
            }

            /* Only run one SRT task at a time, it will be preempted if HRT starts */
            break;
        }
    }
}

void vHandleMajorFrameReset( void )
{
    TickType_t xCurrentTick = xTaskGetTickCount();
    
    LOG_FRAME_RESET( xTimelineManager.ulCurrentFrame, xCurrentTick );

    /* Delete all tasks */
    prvDeleteAllTasks();

    /* Reset runtime states */
    prvResetRuntimeStates();

    /* Recreate all tasks from original configuration */
    if( prvCreateAllTasks() != pdPASS )
    {
        LOG_ERROR( "Critical: Failed to recreate tasks during frame reset!" );
        /* This is a critical error - system cannot continue */
        configASSERT( 0 );
    }

    /* Update frame counter and start time */
    xTimelineManager.ulCurrentFrame++;
    xTimelineManager.xFrameStartTick = xCurrentTick;

    /* Reset SRT task index */
    ulCurrentSRTIndex = 0;

    /* Restart the frame reset timer for next frame */
    xTimerReset( xTimelineManager.xFrameResetTimer, 0 );

    printf( "[RESET_COMPLETE] Frame %u started at tick %u\n",
            (unsigned int)xTimelineManager.ulCurrentFrame,
            (unsigned int)xCurrentTick );
}

BaseType_t bIsTaskPastDeadline( TaskRuntimeState_t* pxTask )
{
    if( pxTask == NULL || pxTask->pxConfig->xType != HARD_RT )
    {
        return pdFALSE;
    }

    TickType_t xCurrentTick = xTaskGetTickCount();
    TickType_t xFrameTime = xCurrentTick - xTimelineManager.xFrameStartTick;

    return ( xFrameTime >= pxTask->pxConfig->ulEnd_time ) ? pdTRUE : pdFALSE;
}

void vTerminateTask( TaskRuntimeState_t* pxTask )
{
    if( pxTask == NULL || pxTask->xTaskHandle == NULL )
    {
        return;
    }

    TickType_t xCurrentTick = xTaskGetTickCount();

    /* Stop deadline timer if it exists */
    if( pxTask->xDeadlineTimer != NULL )
    {
        xTimerStop( pxTask->xDeadlineTimer, 0 );
    }

    /* Log the deadline miss */
    LOG_DEADLINE_MISS( pxTask->pxConfig->pcTaskName, xCurrentTick );

    /* Delete the task */
    vTaskDelete( pxTask->xTaskHandle );

    /* Update runtime state */
    pxTask->xTaskHandle = NULL;
    pxTask->xIsRunning = pdFALSE;
    pxTask->xDeadlineMissed = pdTRUE;
}

BaseType_t bIsAnyHRTTaskActive( void )
{
    for( uint32_t i = 0; i < xTimelineManager.ulNumTasks; i++ )
    {
        TaskRuntimeState_t* pxTask = &xTimelineManager.pxRuntimeStates[ i ];

        if( pxTask->pxConfig->xType == HARD_RT && pxTask->xIsRunning )
        {
            return pdTRUE;
        }
    }

    return pdFALSE;
}

TickType_t xGetCurrentFrameTime( void )
{
    TickType_t xCurrentTick = xTaskGetTickCount();
    return ( xCurrentTick - xTimelineManager.xFrameStartTick );
}

void vNotifyHRTTaskStarted( uint32_t ulTaskIndex )
{
    if( ulTaskIndex >= xTimelineManager.ulNumTasks )
    {
        return;
    }

    TaskRuntimeState_t* pxTask = &xTimelineManager.pxRuntimeStates[ ulTaskIndex ];
    TickType_t xCurrentTick = xTaskGetTickCount();

    pxTask->xIsRunning = pdTRUE;
    pxTask->xStartTick = xCurrentTick;

    LOG_TASK_STARTED( pxTask->pxConfig->pcTaskName, xCurrentTick );

    /* Start deadline timer for HRT tasks */
    if( pxTask->pxConfig->xType == HARD_RT && pxTask->xDeadlineTimer != NULL )
    {
        TickType_t xDeadlinePeriod = pxTask->pxConfig->ulEnd_time - 
                                      ( xCurrentTick - xTimelineManager.xFrameStartTick );
        
        /* Change timer period to exact deadline and start it */
        xTimerChangePeriod( pxTask->xDeadlineTimer, xDeadlinePeriod, 0 );
        xTimerStart( pxTask->xDeadlineTimer, 0 );
    }
}

void vNotifyHRTTaskCompleted( uint32_t ulTaskIndex )
{
    if( ulTaskIndex >= xTimelineManager.ulNumTasks )
    {
        return;
    }

    TaskRuntimeState_t* pxTask = &xTimelineManager.pxRuntimeStates[ ulTaskIndex ];
    TickType_t xCurrentTick = xTaskGetTickCount();

    pxTask->xIsRunning = pdFALSE;
    pxTask->xIsCompleted = pdTRUE;

    LOG_TASK_COMPLETED( pxTask->pxConfig->pcTaskName, xCurrentTick );

    /* Stop deadline timer */
    if( pxTask->xDeadlineTimer != NULL )
    {
        xTimerStop( pxTask->xDeadlineTimer, 0 );
    }
}

/* ===== Task and Timer Callbacks ===== */

void vTimelineManagerTask( void* pvParameters )
{
    ( void ) pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for( ;; )
    {
        /* Wait for next check period */
        vTaskDelayUntil( &xLastWakeTime, MANAGER_CHECK_PERIOD_TICKS );

        /* Monitor HRT deadlines and terminate violators */
        vMonitorHRTDeadlines();

        /* Schedule SRT tasks during idle time */
        vScheduleSRTTasks();

        /* Frame reset is handled by timer callback */
    }
}

void vDeadlineTimerCallback( TimerHandle_t xTimer )
{
    /* Find which task this timer belongs to */
    for( uint32_t i = 0; i < xTimelineManager.ulNumTasks; i++ )
    {
        TaskRuntimeState_t* pxTask = &xTimelineManager.pxRuntimeStates[ i ];

        if( pxTask->xDeadlineTimer == xTimer )
        {
            /* This task's deadline has expired */
            if( pxTask->xIsRunning )
            {
                vTerminateTask( pxTask );
            }
            break;
        }
    }
}

void vFrameResetTimerCallback( TimerHandle_t xTimer )
{
    ( void ) xTimer;
    
    /* Trigger major frame reset */
    vHandleMajorFrameReset();
}

/* ===== Private Helper Functions ===== */

static BaseType_t prvCreateAllTasks( void )
{
    for( uint32_t i = 0; i < xTimelineManager.ulNumTasks; i++ )
    {
        TimelineTaskConfig_t* pxConfig = &xTimelineManager.pxConfig->pxTasks[ i ];
        TaskRuntimeState_t* pxState = &xTimelineManager.pxRuntimeStates[ i ];

        /* Determine priority based on task type */
        UBaseType_t uxPriority = ( pxConfig->xType == HARD_RT ) ? 
                                 HRT_TASK_PRIORITY : SRT_TASK_PRIORITY;

        /* Create the task */
        BaseType_t xResult = xTaskCreate(
            pxConfig->pxTaskFunction,       /* Task function */
            pxConfig->pcTaskName,           /* Task name */
            pxConfig->usStackDepth,         /* Stack size */
            NULL,                           /* Parameters */
            uxPriority,                     /* Priority */
            &pxState->xTaskHandle           /* Task handle */
        );

        if( xResult != pdPASS )
        {
            LOG_ERROR( "Failed to create task" );
            return pdFAIL;
        }

        /* Suspend task initially */
        vTaskSuspend( pxState->xTaskHandle );

        /* Create deadline timer for HRT tasks */
        if( pxConfig->xType == HARD_RT )
        {
            char timerName[ configMAX_TASK_NAME_LEN ];
            snprintf( timerName, sizeof( timerName ), "DL_%s", pxConfig->pcTaskName );

            pxState->xDeadlineTimer = xTimerCreate(
                timerName,                   /* Timer name */
                pxConfig->ulEnd_time,        /* Initial period */
                pdFALSE,                     /* One-shot */
                ( void* ) i,                 /* Timer ID = task index */
                vDeadlineTimerCallback       /* Callback */
            );

            if( pxState->xDeadlineTimer == NULL )
            {
                LOG_ERROR( "Failed to create deadline timer" );
                return pdFAIL;
            }
        }
        else
        {
            pxState->xDeadlineTimer = NULL;
        }

        printf( "[CREATE] Task '%s' created (Type: %s, Priority: %u)\n",
                pxConfig->pcTaskName,
                ( pxConfig->xType == HARD_RT ) ? "HRT" : "SRT",
                (unsigned int)uxPriority );
    }

    return pdPASS;
}

static void prvDeleteAllTasks( void )
{
    for( uint32_t i = 0; i < xTimelineManager.ulNumTasks; i++ )
    {
        TaskRuntimeState_t* pxState = &xTimelineManager.pxRuntimeStates[ i ];

        /* Delete deadline timer if exists */
        if( pxState->xDeadlineTimer != NULL )
        {
            xTimerDelete( pxState->xDeadlineTimer, 0 );
            pxState->xDeadlineTimer = NULL;
        }

        /* Delete task if exists */
        if( pxState->xTaskHandle != NULL )
        {
            vTaskDelete( pxState->xTaskHandle );
            pxState->xTaskHandle = NULL;
        }
    }
}

static void prvResetRuntimeStates( void )
{
    for( uint32_t i = 0; i < xTimelineManager.ulNumTasks; i++ )
    {
        TaskRuntimeState_t* pxState = &xTimelineManager.pxRuntimeStates[ i ];

        pxState->xTaskHandle = NULL;
        pxState->pxConfig = &xTimelineManager.pxConfig->pxTasks[ i ];
        pxState->xDeadlineTimer = NULL;
        pxState->xStartTick = 0;
        pxState->xIsRunning = pdFALSE;
        pxState->xIsCompleted = pdFALSE;
        pxState->xDeadlineMissed = pdFALSE;
    }
}

static TaskRuntimeState_t* prvFindRuntimeStateByHandle( TaskHandle_t xHandle )
{
    for( uint32_t i = 0; i < xTimelineManager.ulNumTasks; i++ )
    {
        if( xTimelineManager.pxRuntimeStates[ i ].xTaskHandle == xHandle )
        {
            return &xTimelineManager.pxRuntimeStates[ i ];
        }
    }

    return NULL;
}

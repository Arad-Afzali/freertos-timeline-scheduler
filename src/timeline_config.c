// timeline_config.c
// Timeline Scheduler configuration - creates tasks and builds the schedule table.

#include "FreeRTOS.h"
#include "task.h"
#include "scheduler_config.h" // Public API (Phase 1)
#include "timeline_config.h"  // Internal Kernel Structures (Phase 2)

TimelineControlBlock_t xSystemTimeline = { 0 };

/*-----------------------------------------------------------*/

// Sort schedule entries by start time using insertion sort.
static void prvSortScheduleEntriesByStartTime( TimelineScheduleEntry_t *pxEntries,
                                               uint32_t ulCount )
{
    uint32_t ulI, ulJ;
    TimelineScheduleEntry_t xTemp;

    if( pxEntries == NULL || ulCount <= 1 )
    {
        return;
    }

    for( ulI = 1; ulI < ulCount; ulI++ )
    {
        xTemp = pxEntries[ ulI ];
        ulJ = ulI;

        while( ulJ > 0 && pxEntries[ ulJ - 1 ].ulStartTime > xTemp.ulStartTime )
        {
            pxEntries[ ulJ ] = pxEntries[ ulJ - 1 ];
            ulJ--;
        }

        pxEntries[ ulJ ] = xTemp;
    }
}

/*-----------------------------------------------------------*/

void vConfigureScheduler( TimelineConfig_t *pxConfig )
{
    uint32_t ulIndex, ulCheckIndex;
    TimelineTaskConfig_t *pxTask, *pxOther;
    TaskHandle_t xHandle;

    // Basic validation
    configASSERT( pxConfig != NULL );
    configASSERT( pxConfig->ulNumTasks > 0 );
    configASSERT( pxConfig->pxTasks != NULL );
    configASSERT( pxConfig->ulNumTasks <= MAX_TIMELINE_TASKS );

    // Validate time windows
    for( ulIndex = 0; ulIndex < pxConfig->ulNumTasks; ulIndex++ )
    {
        pxTask = &pxConfig->pxTasks[ ulIndex ];

        // Ensure task fits within the major frame
        configASSERT( pxTask->ulEnd_time <= MAJOR_FRAME_TICKS );
        // Ensure start time is before end time
        configASSERT( pxTask->ulStart_time < pxTask->ulEnd_time );
    }

    // Check for HRT overlaps
    for( ulIndex = 0; ulIndex < pxConfig->ulNumTasks; ulIndex++ )
    {
        pxTask = &pxConfig->pxTasks[ ulIndex ];

        if( pxTask->xType != HARD_RT )
        {
            continue;
        }

        for( ulCheckIndex = ulIndex + 1; ulCheckIndex < pxConfig->ulNumTasks; ulCheckIndex++ )
        {
            pxOther = &pxConfig->pxTasks[ ulCheckIndex ];

            if( pxOther->xType != HARD_RT )
            {
                continue;
            }

            // Windows overlap if: Start1 < End2 AND Start2 < End1
            if( pxTask->ulStart_time < pxOther->ulEnd_time &&
                pxOther->ulStart_time < pxTask->ulEnd_time )
            {
                // Overlap detected!
                configASSERT( pdFALSE );
            }
        }
    }

    // Create tasks and populate schedule table
    xSystemTimeline.ulTaskCount = 0;
    xSystemTimeline.ulMajorFrameTicks = MAJOR_FRAME_TICKS;

    for( ulIndex = 0; ulIndex < pxConfig->ulNumTasks; ulIndex++ )
    {
        pxTask = &pxConfig->pxTasks[ ulIndex ];
        xHandle = NULL;

        BaseType_t xResult = xTaskCreate( pxTask->pxTaskFunction,
                         pxTask->pcTaskName,
                         pxTask->usStackDepth,
                         NULL,
                         tskIDLE_PRIORITY + 1,
                         &xHandle );
        
        configASSERT( xResult == pdPASS );

        // Suspend immediately - our scheduler will resume at the right time
        vTaskSuspend( xHandle );

        xSystemTimeline.xEntries[ ulIndex ].xHandle = xHandle;
        xSystemTimeline.xEntries[ ulIndex ].ulStartTime = pxTask->ulStart_time;
        xSystemTimeline.xEntries[ ulIndex ].ulEndTime = pxTask->ulEnd_time;
        xSystemTimeline.xEntries[ ulIndex ].ucTaskType = (uint8_t)pxTask->xType;
        xSystemTimeline.ulTaskCount++;
    }

    // Sort by start time so the kernel can iterate linearly
    prvSortScheduleEntriesByStartTime( xSystemTimeline.xEntries,
                                       xSystemTimeline.ulTaskCount );
}

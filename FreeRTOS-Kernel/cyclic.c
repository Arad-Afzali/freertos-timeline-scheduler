#include "cyclic.h"


static TickType_t MajorFrameTicks = 0;
static TickType_t MajorStartTick  = 0;

static const HRT_Entry_t * Table  = NULL;
static UBaseType_t Count          = 0;
static UBaseType_t NextIndex      = 0;

void vCyclicExecInit( TickType_t majorFrameTicks, const HRT_Entry_t * table,UBaseType_t count )
{
    MajorFrameTicks = majorFrameTicks;
    MajorStartTick  = xTaskGetTickCount();

    Table = table;
    Count = count;
    NextIndex = 0;
}

BaseType_t xCyclicExecTickHookFromISR( TickType_t newNowTick )
{

    if( MajorFrameTicks == 0 || Table == NULL || Count == 0 )
        return pdFALSE;

    TickType_t rel = ( newNowTick - MajorStartTick ) % MajorFrameTicks;

    if( ( rel == 0 ) && ( !(newNowTick == MajorStartTick) ) )
    {
        MajorStartTick = newNowTick;
        NextIndex = 0;
    }

    BaseType_t Awake = pdFALSE;

    while( ( NextIndex < Count ) &&
           ( Table[NextIndex].ulStartTimeTicks == rel ) )
    {
        BaseType_t HigherPriorityTask = pdFALSE;
        vTaskNotifyGiveFromISR( Table[NextIndex].Task, &HigherPriorityTask );
        if( HigherPriorityTask == pdTRUE ) Awake = pdTRUE;
        NextIndex++;
    }

    return Awake;
}

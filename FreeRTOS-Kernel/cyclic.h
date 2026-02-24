#pragma once
#include "FreeRTOS.h"
#include "task.h"

typedef struct
{ 
    
    TaskHandle_t Task;
    TickType_t   ulStartTimeTicks;

} HRT_Entry_t;

void vCyclicExecInit( TickType_t majorFrameTick,const HRT_Entry_t * Table,UBaseType_t count );
BaseType_t xCyclicExecTickHookFromISR( TickType_t xnowyick );

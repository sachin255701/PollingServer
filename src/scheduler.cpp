#include "scheduler.h"

#define schedTHREAD_LOCAL_STORAGE_POINTER_INDEX 0
#define schedUSE_TCB_ARRAY 1



/* Extended Task control block for managing periodic tasks within this library. */
typedef struct xExtended_TCB
{
  TaskFunction_t pvTaskCode;    /* Function pointer to the code that will be run periodically. */
  const char *pcName;       /* Name of the task. */
  UBaseType_t uxStackDepth;       /* Stack size of the task. */
  void *pvParameters;       /* Parameters to the task function. */
  UBaseType_t uxPriority;     /* Priority of the task. */
  TaskHandle_t *pxTaskHandle;   /* Task handle for the task. */
  TickType_t xReleaseTime;    /* Release time of the task. */
  TickType_t xRelativeDeadline; /* Relative deadline of the task. */
  TickType_t xAbsoluteDeadline; /* Absolute deadline of the task. */
  TickType_t xPeriod;       /* Task period. */
  TickType_t xLastWakeTime;     /* Last time stamp when the task was running. */
  TickType_t xMaxExecTime;    /* Worst-case execution time of the task. */
  TickType_t xExecTime;     /* Current execution time of the task. */

  BaseType_t xWorkIsDone;     /* pdFALSE if the job is not finished, pdTRUE if the job is finished. */

  #if( schedUSE_TCB_ARRAY == 1 )
    BaseType_t xPriorityIsSet;  /* pdTRUE if the priority is assigned. */
    BaseType_t xInUse;      /* pdFALSE if this extended TCB is empty. */
  #endif

  #if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
    BaseType_t xExecutedOnce; /* pdTRUE if the task has executed once. */
  #endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

  #if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 || schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
    TickType_t xAbsoluteUnblockTime; /* The task will be unblocked at this time if it is blocked by the scheduler task. */
  #endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME || schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

  #if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
    BaseType_t xSuspended;    /* pdTRUE if the task is suspended. */
    BaseType_t xMaxExecTimeExceeded; /* pdTRUE when execTime exceeds maxExecTime. */
  #endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
  
  #if (POLLING_SERVER == 1)
    BaseType_t isPollingServer;
  #endif
  
} SchedTCB_t;


#if( POLLING_SERVER == 1)
static TaskHandle_t pollingSeverTaskHandle = NULL;

AJTCB_t aperiodicTCBQueue [schedMAX_NUMBER_OF_APERIODIC_TASKS] =  {0};
BaseType_t queueHead = 0;
BaseType_t queueTail = 0;
BaseType_t aperiodicJobCounter = 0;
#endif /* POLLING_SERVER */



#if( schedUSE_TCB_ARRAY == 1 )
  static BaseType_t prvGetTCBIndexFromHandle( TaskHandle_t xTaskHandle );
  static void prvInitTCBArray( void );
  /* Find index for an empty entry in xTCBArray. Return -1 if there is no empty entry. */
  static BaseType_t prvFindEmptyElementIndexTCB( void );
  /* Remove a pointer to extended TCB from xTCBArray. */
  static void prvDeleteTCBFromArray( BaseType_t xIndex );
#endif /* schedUSE_TCB_ARRAY */


static TickType_t xSystemStartTime = 0;

static void prvPeriodicTaskCode( void *pvParameters );
static void prvCreateAllTasks( void );


#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS)
  static void prvSetFixedPriorities( void );    
#endif /* schedSCHEDULING_POLICY_RMS */

#if( schedUSE_SCHEDULER_TASK == 1 )
  static void prvSchedulerCheckTimingError( TickType_t xTickCount, SchedTCB_t *pxTCB );
  static void prvSchedulerFunction( void );
  static void prvCreateSchedulerTask( void );
  static void prvWakeScheduler( void );

  #if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
    static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB );
    static void prvDeadlineMissedHook( SchedTCB_t *pxTCB, TickType_t xTickCount );
    static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount );       
  #endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

  #if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
    static void prvExecTimeExceedHook( TickType_t xTickCount, SchedTCB_t *pxCurrentTask );
  #endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
  
#endif /* schedUSE_SCHEDULER_TASK */



#if( schedUSE_TCB_ARRAY == 1 )
  /* Array for extended TCBs. */
  static SchedTCB_t xTCBArray[ schedMAX_NUMBER_OF_PERIODIC_TASKS ] = { 0 };
  /* Counter for number of periodic tasks. */
  static BaseType_t xTaskCounter = 0;
#endif /* schedUSE_TCB_ARRAY */

#if( schedUSE_SCHEDULER_TASK )
  static TickType_t xSchedulerWakeCounter = 0;
  static TaskHandle_t xSchedulerHandle = NULL;
#endif /* schedUSE_SCHEDULER_TASK */


#if( POLLING_SERVER == 1)
  
  /*void aperiodicTaskQueueInit(void) {
    aperiodicJobsQueue = xQueueCreate(schedMAX_NUMBER_OF_APERIODIC_TASKS, sizeof(AJTCB_t));
  }*/
  
  BaseType_t getEmptyIndexInQueue (void) {
    BaseType_t retVal = -1;
    
    if(aperiodicJobCounter == schedMAX_NUMBER_OF_APERIODIC_TASKS)  {
      /* Queue Full */
      return retVal;
    }
    
    retVal = queueTail;
    
    queueTail = (queueTail + 1) % schedMAX_NUMBER_OF_APERIODIC_TASKS;
    
    return retVal;
  }
  
  
  BaseType_t createAperiodicJob(TaskFunction_t pvTaskCode, \
                const char *pcName, void *pvParameters, \
                TaskHandle_t *pxCreatedTask) {
    BaseType_t index;
 
    AJTCB_t *aperiodicTCB;
  index = getEmptyIndexInQueue();
  
  if(index == -1) {
    return pdFALSE;
  }
  
  
  aperiodicTCB = &aperiodicTCBQueue[index];

    aperiodicTCB->pvTaskCode = pvTaskCode;
    aperiodicTCB->pcName = pcName;
    aperiodicTCB->pvParameters = pvParameters;
    aperiodicTCB->pxTaskHandle = pxCreatedTask;
  
  aperiodicJobCounter++;
  
  return pdTRUE;
  }

  void executeAperiodicJob(void) {
    BaseType_t retVal;
    AJTCB_t *aperiodicTCB;

    /*Serial.begin(9600);
    Serial.println("Polling Server Start");
    Serial.end();*/
      
    for (; ; ) {
      if(aperiodicJobCounter == 0) {
        return;
      } 
      else {
        aperiodicTCB = &aperiodicTCBQueue[queueHead];
        queueHead = (queueHead + 1) % schedMAX_NUMBER_OF_APERIODIC_TASKS;
        aperiodicJobCounter--;
        aperiodicTCB -> pvTaskCode(aperiodicTCB -> pvParameters );
      }
    }
  }
#endif /* POLLING_SERVER */


#if( schedUSE_TCB_ARRAY == 1 )
  /* Returns index position in xTCBArray of TCB with same task handle as parameter. */
  static BaseType_t prvGetTCBIndexFromHandle( TaskHandle_t xTaskHandle )
  {
  static BaseType_t xIndex = 0;
  BaseType_t xIterator;

    for( xIterator = 0; xIterator < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIterator++ )
    {
    
      if( pdTRUE == xTCBArray[ xIndex ].xInUse && *xTCBArray[ xIndex ].pxTaskHandle == xTaskHandle )
      {
        return xIndex;
      }
    
      xIndex++;
      if( schedMAX_NUMBER_OF_PERIODIC_TASKS == xIndex )
      {
        xIndex = 0;
      }
    }
    return -1;
  }

  /* Initializes xTCBArray. */
  static void prvInitTCBArray( void )
  {
  UBaseType_t uxIndex;
    for( uxIndex = 0; uxIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; uxIndex++)
    {
      xTCBArray[ uxIndex ].xInUse = pdFALSE;
    }
  }

  /* Find index for an empty entry in xTCBArray. Returns -1 if there is no empty entry. */
  static BaseType_t prvFindEmptyElementIndexTCB( void )
  {
  BaseType_t xIndex;
    for( xIndex = 0; xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIndex++ )
    {
      if( pdFALSE == xTCBArray[ xIndex ].xInUse )
      {
        return xIndex;
      }
    }

    return -1;
  }

  /* Remove a pointer to extended TCB from xTCBArray. */
  static void prvDeleteTCBFromArray( BaseType_t xIndex )
  {
    configASSERT( xIndex >= 0 && xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS );
    configASSERT( pdTRUE == xTCBArray[ xIndex ].xInUse );

    if( xTCBArray[ pdTRUE == xIndex].xInUse )
    {
      xTCBArray[ xIndex ].xInUse = pdFALSE;
      xTaskCounter--;
    }
  }
  
#endif /* schedUSE_TCB_ARRAY */


/* The whole function code that is executed by every periodic task.
 * This function wraps the task code specified by the user. */
static void prvPeriodicTaskCode( void *pvParameters )
{
  SchedTCB_t *pxThisTask; 
  TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();  

    configASSERT( xCurrentTaskHandle );
    
  BaseType_t xIndex;
  
  BaseType_t xFlag = 0;
  
  for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
  {
    pxThisTask = &xTCBArray[ xIndex ];
    if( uxTaskPriorityGet( xCurrentTaskHandle ) == pxThisTask->uxPriority){
      xFlag = 1;
      break;
    } 
  }
  /*for (xIndex = 0; xIndex < xTaskCounter; xIndex++)
    {
        pxThisTask = &xTCBArray[xIndex];
        if (*(pxThisTask->pxTaskHandle) == xCurrentTaskHandle) {
            break;
        }
    }*/

  if( 0 != pxThisTask->xReleaseTime )
  {
    vTaskDelayUntil( &pxThisTask->xLastWakeTime, pxThisTask->xReleaseTime );
  }

  #if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
    pxThisTask->xExecutedOnce = pdTRUE;
  #endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
  if( 0 == pxThisTask->xReleaseTime )
  {
    pxThisTask->xLastWakeTime = xSystemStartTime;
  }

  for( ; ; )
  {   
    pxThisTask->xWorkIsDone = pdFALSE;    

    pxThisTask->xAbsoluteDeadline = pxThisTask->xLastWakeTime + pxThisTask->xRelativeDeadline;

    /* Execute the task function specified by the user. */
    pxThisTask->pvTaskCode( pvParameters );
    pxThisTask->xWorkIsDone = pdTRUE;

    //pxThisTask->xAbsoluteDeadline = pxThisTask->xLastWakeTime + pxThisTask->xRelativeDeadline + pxThisTask->xPeriod;
    pxThisTask->xExecTime = 0;
    vTaskDelayUntil( &pxThisTask->xLastWakeTime, pxThisTask->xPeriod );
  }
}

/* Creates a periodic task. */
void vSchedulerPeriodicTaskCreate( TaskFunction_t pvTaskCode, const char *pcName, UBaseType_t uxStackDepth, void *pvParameters, UBaseType_t uxPriority,
    TaskHandle_t *pxCreatedTask, TickType_t xPhaseTick, TickType_t xPeriodTick, TickType_t xMaxExecTimeTick, TickType_t xDeadlineTick )
{
  taskENTER_CRITICAL();
  
  SchedTCB_t *pxNewTCB;
  #if( schedUSE_TCB_ARRAY == 1 )
    BaseType_t xIndex = prvFindEmptyElementIndexTCB();
    configASSERT( xTaskCounter < schedMAX_NUMBER_OF_PERIODIC_TASKS );
    configASSERT( xIndex != -1 );
    pxNewTCB = &xTCBArray[ xIndex ];  
  #endif /* schedUSE_TCB_ARRAY */

  pxNewTCB->pvTaskCode = pvTaskCode;
  pxNewTCB->pcName = pcName;
  pxNewTCB->uxStackDepth = uxStackDepth;
  pxNewTCB->pvParameters = pvParameters;
  pxNewTCB->uxPriority = uxPriority;
  pxNewTCB->pxTaskHandle = pxCreatedTask;
  pxNewTCB->xReleaseTime = xPhaseTick;
  pxNewTCB->xPeriod = xPeriodTick;
  pxNewTCB->xMaxExecTime = xMaxExecTimeTick;
  pxNewTCB->xRelativeDeadline = xDeadlineTick;
  pxNewTCB->xAbsoluteDeadline = pxNewTCB->xRelativeDeadline + pxNewTCB->xReleaseTime;    
  pxNewTCB->xWorkIsDone = pdTRUE;
  pxNewTCB->xExecTime = 0;    
  
  #if( schedUSE_TCB_ARRAY == 1 )
    pxNewTCB->xInUse = pdTRUE;
  #endif /* schedUSE_TCB_ARRAY */
  
  #if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
    pxNewTCB->xPriorityIsSet = pdFALSE; 
  #endif /* schedSCHEDULING_POLICY */
  
  #if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
    pxNewTCB->xExecutedOnce = pdFALSE;
  #endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
  
  #if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
    pxNewTCB->xSuspended = pdFALSE;
    pxNewTCB->xMaxExecTimeExceeded = pdFALSE;
  #endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */ 

  #if (POLLING_SERVER == 1)
    pxNewTCB->isPollingServer = pdFALSE;
  #endif

  #if( schedUSE_TCB_ARRAY == 1 )
    xTaskCounter++; 
  #endif /* schedUSE_TCB_SORTED_LIST */
  taskEXIT_CRITICAL();  
}

/* Deletes a periodic task. */
void vSchedulerPeriodicTaskDelete( TaskHandle_t xTaskHandle )
{
  if( xTaskHandle != NULL )
  {
    #if( schedUSE_TCB_ARRAY == 1 )
      prvDeleteTCBFromArray( prvGetTCBIndexFromHandle( xTaskHandle ) );   
    #endif /* schedUSE_TCB_ARRAY */
  }
  else
  {
    #if( schedUSE_TCB_ARRAY == 1 )
      prvDeleteTCBFromArray( prvGetTCBIndexFromHandle( xTaskGetCurrentTaskHandle() ) );   
    #endif /* schedUSE_TCB_ARRAY */
  }
  
  vTaskDelete( xTaskHandle );
}

/* Creates all periodic tasks stored in TCB array, or TCB list. */
static void prvCreateAllTasks( void )
{
  SchedTCB_t *pxTCB;

  #if( schedUSE_TCB_ARRAY == 1 )
    BaseType_t xIndex;
    for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
    {
      configASSERT( pdTRUE == xTCBArray[ xIndex ].xInUse );
      pxTCB = &xTCBArray[ xIndex ];

      BaseType_t xReturnValue = xTaskCreate( prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle);         
    } 
  #endif /* schedUSE_TCB_ARRAY */
}

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
  /* Initiazes fixed priorities of all periodic tasks with respect to RMS policy. */
static void prvSetFixedPriorities( void )
{
  BaseType_t xIter, xIndex;
  TickType_t xShortest, xPreviousShortest=0;
  SchedTCB_t *pxShortestTaskPointer, *pxTCB;

  #if( schedUSE_SCHEDULER_TASK == 1 )
    BaseType_t xHighestPriority = schedSCHEDULER_PRIORITY; 
  #else
    BaseType_t xHighestPriority = configMAX_PRIORITIES;
  #endif /* schedUSE_SCHEDULER_TASK */

  for( xIter = 0; xIter < xTaskCounter; xIter++ )
  {
    xShortest = portMAX_DELAY;

    /* search for shortest period/deadline */
    for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
    {
      pxTCB = &xTCBArray[ xIndex ];
      configASSERT( pdTRUE == pxTCB->xInUse );
      if(pdTRUE == pxTCB->xPriorityIsSet)
      {
        continue;
      }

      #if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS )
        if( pxTCB->xPeriod <= xShortest )
        {
          xShortest = pxTCB->xPeriod;
          pxShortestTaskPointer = pxTCB;
        }
      #elif( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
        if( pxTCB->xRelativeDeadline <= xShortest )
        {
          xShortest = pxTCB->xRelativeDeadline;
          pxShortestTaskPointer = pxTCB;
        }
      #endif /* schedSCHEDULING_POLICY */
    }
    configASSERT( -1 <= xHighestPriority );
    if( xPreviousShortest != xShortest )
    {
      xHighestPriority--;
    }

    /* set highest priority to task with xShortest period (the highest priority is configMAX_PRIORITIES-1) */
    pxShortestTaskPointer->uxPriority = xHighestPriority;
    pxShortestTaskPointer->xPriorityIsSet = pdTRUE;

    xPreviousShortest = xShortest;    
  }
}
#endif /* schedSCHEDULING_POLICY */


#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )

  /* Recreates a deleted task that still has its information left in the task array (or list). */
  static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB )
  {
    BaseType_t xReturnValue = xTaskCreate( prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle);                             
    
    if( pdPASS == xReturnValue )
    { 
      /* This must be set to false so that the task does not miss the deadline immediately when it is created. */
      pxTCB->xExecutedOnce = pdFALSE;
      #if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
        pxTCB->xSuspended = pdFALSE;
        pxTCB->xMaxExecTimeExceeded = pdFALSE;
      #endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
      
    }   
  }

  /* Called when a deadline of a periodic task is missed.
   * Deletes the periodic task that has missed it's deadline and recreate it.
   * The periodic task is released during next period. */
  static void prvDeadlineMissedHook( SchedTCB_t *pxTCB, TickType_t xTickCount )
  {    
    /* Delete the pxTask and recreate it. */
    vTaskDelete( *pxTCB->pxTaskHandle );
    pxTCB->xExecTime = 0;
    prvPeriodicTaskRecreate( pxTCB );

    pxTCB->xReleaseTime = pxTCB->xLastWakeTime + pxTCB->xPeriod;
    /* Need to reset lastWakeTime for correct release. */
    pxTCB->xLastWakeTime = 0;
    pxTCB->xAbsoluteDeadline = pxTCB->xRelativeDeadline + pxTCB->xReleaseTime;    
  }

  /* Checks whether given task has missed deadline or not. */
  static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount )
  {    
      /*Serial.begin(9600);
      Serial.println(pxTCB->pcName);
      Serial.println((pxTCB->xAbsoluteDeadline));
      Serial.println((xTickCount));
      Serial.end();*/
    if( ( NULL != pxTCB ) && ( pdFALSE == pxTCB->xWorkIsDone ) && ( pdTRUE == pxTCB->xExecutedOnce ) )
    {
      if( ( signed ) ( pxTCB->xAbsoluteDeadline - xTickCount ) < 0 )
      {
        Serial.begin(9600);
        Serial.print("Missed Deadline ");
        Serial.println(pxTCB->pcName);
        Serial.end();
        /* Serial.println(pxTCB->pcName);*/
        /* Deadline is missed. */
        prvDeadlineMissedHook( pxTCB, xTickCount );
      }
    }
  } 
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */


#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )

  /* Called if a periodic task has exceeded it's worst-case execution time.
   * The periodic task is blocked until next period. A context switch to
   * the scheduler task occur to block the periodic task. */
  static void prvExecTimeExceedHook( TickType_t xTickCount, SchedTCB_t *pxCurrentTask )
  {
    pxCurrentTask->xMaxExecTimeExceeded = pdTRUE;
    /* Is not suspended yet, but will be suspended by the scheduler later. */
    pxCurrentTask->xSuspended = pdTRUE;
    pxCurrentTask->xAbsoluteUnblockTime = pxCurrentTask->xLastWakeTime + pxCurrentTask->xPeriod;
    pxCurrentTask->xExecTime = 0;
    #if (POLLING_SERVER == 1)  
      if( pdTRUE == pxCurrentTask->isPollingServer )
      {
        pxCurrentTask->xAbsoluteDeadline = pxCurrentTask->xAbsoluteUnblockTime + pxCurrentTask->xRelativeDeadline;
      }
    #endif
    BaseType_t xHigherPriorityTaskWoken; 
    vTaskNotifyGiveFromISR( xSchedulerHandle, &xHigherPriorityTaskWoken ); 
    xTaskResumeFromISR(xSchedulerHandle);    
  }

#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */


#if (POLLING_SERVER == 1)
  void prvCreatePollingServerTask (void) {
    taskENTER_CRITICAL();
  
    SchedTCB_t *pxNewTCB;
    #if( schedUSE_TCB_ARRAY == 1 )
      BaseType_t xIndex = prvFindEmptyElementIndexTCB();
      configASSERT( xTaskCounter < schedMAX_NUMBER_OF_PERIODIC_TASKS );
      configASSERT( xIndex != -1 );
      pxNewTCB = &xTCBArray[ xIndex ];  
    #endif /* schedUSE_TCB_ARRAY */

    pxNewTCB->pvTaskCode = (TaskFunction_t) executeAperiodicJob;
    pxNewTCB->pcName = "Server";
    pxNewTCB->uxStackDepth = configMINIMAL_STACK_SIZE;
    pxNewTCB->pvParameters = NULL;
    pxNewTCB->uxPriority = POLLING_SERVER_PRIORITY;
    pxNewTCB->pxTaskHandle = &pollingSeverTaskHandle;
    pxNewTCB->xReleaseTime = 0;
    pxNewTCB->xPeriod = POLLING_SERVER_PERIOD;
    pxNewTCB->xMaxExecTime = POLLING_SERVER_MAX_EXEC_TIME;
    pxNewTCB->xRelativeDeadline = POLLING_SERVER_RELATIVE_DEADLINE;
    pxNewTCB->xAbsoluteDeadline = pxNewTCB->xRelativeDeadline + pxNewTCB->xReleaseTime;    
    pxNewTCB->xWorkIsDone = pdTRUE;
    pxNewTCB->xExecTime = 0;    
    
    #if( schedUSE_TCB_ARRAY == 1 )
      pxNewTCB->xInUse = pdTRUE;
    #endif /* schedUSE_TCB_ARRAY */
    
    #if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
      pxNewTCB->xPriorityIsSet = pdFALSE; 
    #endif /* schedSCHEDULING_POLICY */
    
    #if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
      pxNewTCB->xExecutedOnce = pdFALSE;
    #endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
    
    #if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
      pxNewTCB->xSuspended = pdFALSE;
      pxNewTCB->xMaxExecTimeExceeded = pdFALSE;
    #endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */ 

    #if (POLLING_SERVER == 1)
      pxNewTCB->isPollingServer = pdTRUE;
    #endif

    #if( schedUSE_TCB_ARRAY == 1 )
      xTaskCounter++; 
    #endif /* schedUSE_TCB_SORTED_LIST */
    taskEXIT_CRITICAL();  
    
  }

#endif

#if( schedUSE_SCHEDULER_TASK == 1 )
  /* Called by the scheduler task. Checks all tasks for any enabled
   * Timing Error Detection feature. */
  static void prvSchedulerCheckTimingError( TickType_t xTickCount, SchedTCB_t *pxTCB )
  {
    #if( schedUSE_TCB_ARRAY == 1 )
      if( pdFALSE == pxTCB->xInUse )
      {
        return;
      }
    #endif
    
    #if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )      
      
      /* Since lastWakeTime is updated to next wake time when the task is delayed, tickCount > lastWakeTime implies that
       * the task has not finished it's job this period. */

       #if (POLLING_SERVER == 1)
         if( pxTCB->isPollingServer != pdTRUE ) {
       #endif /* POLLING_SERVER */
            if( ( signed ) ( xTickCount - pxTCB->xLastWakeTime ) > 0 )
            {
              pxTCB->xWorkIsDone = pdFALSE;
            }

            prvCheckDeadline( pxTCB, xTickCount );
       #if (POLLING_SERVER == 1)
        }
      #endif /* POLLING_SERVER */
    #endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
    

    #if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
      if( pdTRUE == pxTCB->xMaxExecTimeExceeded )
      {        
        pxTCB->xMaxExecTimeExceeded = pdFALSE;
        Serial.begin(9600);
        Serial.print("Suspend Task ");
        Serial.println(pxTCB->pcName);
        Serial.end();
        vTaskSuspend( *pxTCB->pxTaskHandle );
      }
      if( pdTRUE == pxTCB->xSuspended )
      {     
        if( ( signed ) ( pxTCB->xAbsoluteUnblockTime - xTickCount ) <= 0 )
        {
          pxTCB->xSuspended = pdFALSE;
          pxTCB->xLastWakeTime = xTickCount;
          Serial.begin(9600);
          Serial.print("Resume Task ");
          Serial.println(pxTCB->pcName);
          Serial.end();
          vTaskResume( *pxTCB->pxTaskHandle );
        }
      }
    #endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

    return;
  }

  /* Function code for the scheduler task. */
  static void prvSchedulerFunction( void *pvParameters )
  {   
    for( ; ; )
    { 
      #if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
        TickType_t xTickCount = xTaskGetTickCount();        
        SchedTCB_t *pxTCB;

        #if( schedUSE_TCB_ARRAY == 1 )
          BaseType_t xIndex;
          for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
          {
            pxTCB = &xTCBArray[ xIndex ];
            prvSchedulerCheckTimingError( xTickCount, pxTCB );
          }       
        #endif
      
      #endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

      ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
    }
  }

  /* Creates the scheduler task. */
  static void prvCreateSchedulerTask( void )
  {
    xTaskCreate( (TaskFunction_t) prvSchedulerFunction, "Scheduler", schedSCHEDULER_TASK_STACK_SIZE, NULL, schedSCHEDULER_PRIORITY, &xSchedulerHandle );                
  }
#endif /* schedUSE_SCHEDULER_TASK */


#if( schedUSE_SCHEDULER_TASK == 1 )
  /* Wakes up (context switches to) the scheduler task. */
  static void prvWakeScheduler( void )
  {
    BaseType_t xHigherPriorityTaskWoken;
    vTaskNotifyGiveFromISR( xSchedulerHandle, &xHigherPriorityTaskWoken );
    xTaskResumeFromISR(xSchedulerHandle);    
  }

  /* Called every software tick. */
  void vApplicationTickHook()
  {            
    SchedTCB_t *pxCurrentTask;
    TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();

    configASSERT(xCurrentTaskHandle != xSchedulerHandle);

    BaseType_t xIndex;
    for (xIndex = 0; xIndex < xTaskCounter; xIndex++)
    {
        /* your implementation goes here */
        pxCurrentTask = &xTCBArray[xIndex];

        if (*(pxCurrentTask->pxTaskHandle) == xCurrentTaskHandle) {
            break;
        }
    }

    if( xCurrentTaskHandle != xSchedulerHandle && xCurrentTaskHandle != xTaskGetIdleTaskHandle())
    {
      pxCurrentTask->xExecTime++;
      #if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
        if( pxCurrentTask->xMaxExecTime <= pxCurrentTask->xExecTime )
        {
          if( pdFALSE == pxCurrentTask->xMaxExecTimeExceeded )
          {
            if( pdFALSE == pxCurrentTask->xSuspended )
            {            
              prvExecTimeExceedHook( xTaskGetTickCountFromISR(), pxCurrentTask );
            }
          }
        }
      #endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
    }

    #if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )    
      xSchedulerWakeCounter++;      
      if( xSchedulerWakeCounter == schedSCHEDULER_TASK_PERIOD )
      {
        xSchedulerWakeCounter = 0;        
        prvWakeScheduler();
      }
    #endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
  }
#endif /* schedUSE_SCHEDULER_TASK */

/* This function must be called before any other function call from this module. */
void vSchedulerInit( void )
{
  #if( schedUSE_TCB_ARRAY == 1 )
    prvInitTCBArray();
  #endif /* schedUSE_TCB_ARRAY */
}

/* Starts scheduling tasks. All periodic tasks (including polling server) must
 * have been created with API function before calling this function. */
void vSchedulerStart( void )
{ 
  #if (POLLING_SERVER == 1)
    prvCreatePollingServerTask();
  #endif /* POLLING_SERVER */
  
  #if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS)
    prvSetFixedPriorities();  
  #endif /* schedSCHEDULING_POLICY */

  #if( schedUSE_SCHEDULER_TASK == 1 )
    prvCreateSchedulerTask();
  #endif /* schedUSE_SCHEDULER_TASK */

  prvCreateAllTasks();
    
  xSystemStartTime = xTaskGetTickCount();
  vTaskStartScheduler();
}

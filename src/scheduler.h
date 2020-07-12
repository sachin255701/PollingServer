#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <projdefs.h>
#include <timers.h>
#include <list.h>
#include <croutine.h>
#include <portable.h>
#include <stack_macros.h>
#include <mpu_wrappers.h>
#include <FreeRTOSVariant.h>
#include <message_buffer.h>
#include <semphr.h>
#include <FreeRTOSConfig.h>
#include <stream_buffer.h>
#include <portmacro.h>
#include <event_groups.h>
#include <queue.h>
#include <Arduino.h>



#ifdef __cplusplus
extern "C" {
#endif

/* The scheduling policy can be chosen from one of these. */
#define schedSCHEDULING_POLICY_RMS 1 		/* Rate-monotonic scheduling */


/* Configure scheduling policy by setting this define to the appropriate one. */
#define schedSCHEDULING_POLICY schedSCHEDULING_POLICY_RMS

/* Maximum number of periodic tasks that can be created. (Scheduler task is
 * not included, but Polling Server is included) */
#define schedMAX_NUMBER_OF_PERIODIC_TASKS 5

/* Maximum number of aperiodic tasks that can be created. */
#define schedMAX_NUMBER_OF_APERIODIC_TASKS 2

/* Set this define to 1 to enable Timing-Error-Detection for detecting tasks
 * that have missed their deadlines. Tasks that have missed their deadlines
 * will be deleted, recreated and restarted during next period. */
#define schedUSE_TIMING_ERROR_DETECTION_DEADLINE 1

/* Set this define to 1 to enable Timing-Error-Detection for detecting tasks
 * that have exceeded their worst-case execution time. Tasks that have exceeded
 * their worst-case execution time will be preempted until next period. */
#define schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME 1

#define schedUSE_SCHEDULER_TASK 1

#define POLLING_SERVER 1


#if( POLLING_SERVER == 1)
#define POLLING_SERVER_PRIORITY     ( configMAX_PRIORITIES - 2 )
#define POLLING_SERVER_PERIOD        pdMS_TO_TICKS( 200 )
#define POLLING_SERVER_MAX_EXEC_TIME        pdMS_TO_TICKS( 100 )
#define POLLING_SERVER_RELATIVE_DEADLINE        pdMS_TO_TICKS( 200 )


/* Aperiodic Task control block for managing periodic tasks within this library. */
typedef struct aperiodicExtended_TCB
{
  TaskFunction_t pvTaskCode;    /* Function pointer to the code that will be run periodically. */
  const char *pcName;       /* Name of the task. */
  void *pvParameters;       /* Parameters to the task function. */
  TaskHandle_t *pxTaskHandle;   /* Task handle for the task. */
} AJTCB_t;

  /* Wrapper funtions for queue insertion and deletion */
  BaseType_t createAperiodicJob(TaskFunction_t pvTaskCode, \
                                   const char *pcName, void *pvParameters, \
                   TaskHandle_t *pxCreatedTask);

  static BaseType_t getEmptyIndexInQueue (void);
  static void executeAperiodicJob(void);

#endif

#if( schedUSE_SCHEDULER_TASK == 1 )
	/* Priority of the scheduler task. */
	#define schedSCHEDULER_PRIORITY ( configMAX_PRIORITIES - 1 )
	/* Stack size of the scheduler task. */
	#define schedSCHEDULER_TASK_STACK_SIZE 200
	/* The period of the scheduler task in software ticks. */
	#define schedSCHEDULER_TASK_PERIOD pdMS_TO_TICKS( 200 )	
#endif /* schedUSE_SCHEDULER_TASK */

/* This function must be called before any other function call from scheduler.h. */
void vSchedulerInit( void );

/* Creates a periodic task.
 *
 * pvTaskCode: The task function.
 * pcName: Name of the task.
 * usStackDepth: Stack size of the task in words, not bytes.
 * pvParameters: Parameters to the task function.
 * uxPriority: Priority of the task. (Only used when scheduling policy is set to manual)
 * pxCreatedTask: Pointer to the task handle.
 * xPhaseTick: Phase given in software ticks. Counted from when vSchedulerStart is called.
 * xPeriodTick: Period given in software ticks.
 * xMaxExecTimeTick: Worst-case execution time given in software ticks.
 * xDeadlineTick: Relative deadline given in software ticks.
 * */
void vSchedulerPeriodicTaskCreate( TaskFunction_t pvTaskCode, const char *pcName, UBaseType_t uxStackDepth, void *pvParameters, UBaseType_t uxPriority,
		TaskHandle_t *pxCreatedTask, TickType_t xPhaseTick, TickType_t xPeriodTick, TickType_t xMaxExecTimeTick, TickType_t xDeadlineTick );

/* Deletes a periodic task associated with the given task handle. */
void vSchedulerPeriodicTaskDelete( TaskHandle_t xTaskHandle );

/* Starts scheduling tasks. */
void vSchedulerStart( void );

#ifdef __cplusplus
}
#endif


#endif /* SCHEDULER_H_ */

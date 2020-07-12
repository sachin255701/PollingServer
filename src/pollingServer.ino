#include "scheduler.h"

TaskHandle_t xHandle1 = NULL;
TaskHandle_t xHandle2 = NULL;

TaskHandle_t xAperiodicTaskHandle1 = NULL;
TaskHandle_t xAperiodicTaskHandle2 = NULL;

void loop() {}


static void testFunc1( void *pvParameters ){

	(void) pvParameters;
  int i, a;
  Serial.begin(9600);
  Serial.println("t1 start");
  Serial.end();
  static int counter = 0;
  counter++;
 
  if (counter % 2 == 0) {
    createAperiodicJob(aperiodicTaskFunc1, "apt1", NULL, &xAperiodicTaskHandle1);
  }
  
  for( i = 0; i < 10000; i++ )
  {
    a = 1 + i*i*i*i;
  }

  Serial.begin(9600);
  Serial.println("t1 end");
  Serial.end();
    
}

static void aperiodicTaskFunc1( void *pvParameters ){
  
  (void) pvParameters;  
  float i, a;  
  
  Serial.begin(9600);
  Serial.println("Aperiodic Task 1 Start");
  Serial.end();
  
  for(i = 0; i < 10000; i++ )
  {
    a = 1 + a * a * i;  
  }  
  Serial.begin(9600);
  Serial.println("Aperiodic Task 1 End");
  Serial.end();

}

static void aperiodicTaskFunc2( void *pvParameters ){
  
  (void) pvParameters;  
  float i, a;  
  
  Serial.begin(9600);
  Serial.println("Aperiodic Task 2 Start");
  Serial.end();
  
  for(i = 0; i < 10000; i++ )
  {
    a = 1 + a * a * i;  
  }  
  Serial.begin(9600);
  Serial.println("Aperiodic Task 2 End");
  Serial.end();

}


static void testFunc2( void *pvParameters ){
  
  (void) pvParameters;  
  float i, a;  
  
  Serial.begin(9600);
  Serial.println("t2 start");
  Serial.end();
  
  for(i = 0; i < 10000; i++ )
  {
    a = 1 + i*i*i*i;
  }  
  Serial.begin(9600);
  Serial.println("t2 end");
  Serial.end();

}



int main( void )
{

  char c1 = 'a';
  char c2 = 'b';

  vSchedulerInit();
  vSchedulerPeriodicTaskCreate(testFunc1, "t1", configMINIMAL_STACK_SIZE, &c1, 1, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(800), pdMS_TO_TICKS(200), pdMS_TO_TICKS(800));  
  vSchedulerPeriodicTaskCreate(testFunc2, "t2", configMINIMAL_STACK_SIZE, &c2, 2, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(400), pdMS_TO_TICKS(200), pdMS_TO_TICKS(400));

  vSchedulerStart(); 
  for( ;; );
}

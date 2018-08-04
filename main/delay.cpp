#include "esp_system.h"
#include "esp_task_wdt.h"
#include "delay.h"

/****************************************************************************/
/*  Delay routine in ms which uses hardware timer                           */
/*  Function : DelayMs                                                      */
/*      Parameters                                                          */
/*          Input   :   number of ms in 16bits => 65,535sec max             */
/*          Output  :   Nothing                                             */
/****************************************************************************/
void DelayMs(HWORD cmpt_ms)
{
  esp_task_wdt_reset();
  vTaskDelay(cmpt_ms / portTICK_PERIOD_MS);
}

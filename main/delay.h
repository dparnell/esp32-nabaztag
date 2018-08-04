#ifndef _DELAY_H_
#define _DELAY_H_

#include "common.h"
/*************/
/* Functions */
/*************/
void DelayMs(HWORD cmpt_ms);

extern volatile int counter_timer;
extern volatile int counter_timer_s;

#endif

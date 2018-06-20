/*
 * timer64.h
 *
 *  Created on: 2018-5-29
 *      Author: ws
 */

#ifndef TIMER64_H_
#define TIMER64_H_

typedef void (*CallbackFxn)(UArg arg);
extern void setup_TIMER (unsigned int timer_id, unsigned long int TIMER_PERIOD, CallbackFxn Fxn);
extern void add_timer(unsigned int timer_id, unsigned long int TIMER_PERIOD);

#endif /* TIMER64_H_ */

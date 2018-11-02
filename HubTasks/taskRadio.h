/*
 * taskRF.h
 *
 *  Created on: Jul 5, 2018
 *      Author: Dillon
 */

#ifndef HUBTASKS_TASKRADIO_H_
#define HUBTASKS_TASKRADIO_H_

#include <ti/sysbios/knl/Task.h>

extern uint8_t ieeeAddr[8];

Void radioTask(UArg arg0, UArg arg1);
extern void radioTaskEvtPost(uint32_t evt);

#endif /* HUBTASKS_TASKRADIO_H_ */

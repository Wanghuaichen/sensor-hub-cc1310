/*
 * taskUART.h
 *
 *  Created on: Jul 5, 2018
 *      Author: Dillon
 */

#ifndef HUBTASKS_TASKUART_H_
#define HUBTASKS_TASKUART_H_

#include <ti/sysbios/knl/Task.h>

#define UART_RX_BUFF_LEN        128
#define UART_TX_BUFF_LEN        128

Void uartTask(UArg arg0, UArg arg1);

#endif /* HUBTASKS_TASKUART_H_ */

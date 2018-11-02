/*
 * Copyright (c) 2016-2018, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,

 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== main_tirtos.c ========
 */
#include <stdint.h>

/* POSIX Header files */
#include <pthread.h>

/* RTOS header files */
#include <ti/sysbios/BIOS.h>

/* Example/Board Header files */
#include "Board.h"

/* Task Execution */
#include "HubTasks/taskUART.h"
#include "HubTasks/taskRadio.h"
#include "HubTasks/taskNodes.h"

/* Application Header*/
#include "HubTasks/HubData.h"

/* Stack size in bytes */
#define UART_TASK   1024
#define RADIO_TASK  1024
#define NODES_TASK  1024

Task_Handle taskUART_H;
Task_Handle taskRadio_H;
Task_Handle taskNodes_H;


/*
 *  ======== main ========
 */
int main(void)
{
    Error_Block eb;
    Task_Params taskParams;

    /* Call driver init functions */
    Board_initGeneral();
    GPIO_init();

    Error_init(&eb);

    Task_Params_init(&taskParams);
    taskParams.priority = 4;
    taskParams.stackSize = UART_TASK;
    taskUART_H = Task_create(uartTask, &taskParams, &eb);

    Task_Params_init(&taskParams);
    taskParams.priority = 5;
    taskParams.stackSize = RADIO_TASK;
    taskRadio_H = Task_create(radioTask, &taskParams, &eb);

    Task_Params_init(&taskParams);
    taskParams.priority = 3;
    taskParams.stackSize = NODES_TASK;
    taskNodes_H = Task_create(nodesTask, &taskParams, &eb);

    BIOS_start();

    return (0);
}

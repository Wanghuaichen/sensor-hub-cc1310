/*
 * HubData.h
 *
 *  Created on: Jul 5, 2018
 *      Author: Dillon
 */

#ifndef HUBTASKS_HUBDATA_H_
#define HUBTASKS_HUBDATA_H_

 /* Standard C Libraries */
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* Board Header file */
#include "Board.h"

/* BIOS */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Swi.h>
#include <xdc/runtime/Error.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/hal/Seconds.h>

/* Driver header files */
#include <ti/drivers/GPIO.h>

/* For System printf */
#include <xdc/runtime/System.h>



//max number of nodes is dependent on number of addresses available for Rx filter
//update max Rx addresses for more nodes. currently 20
#define MAX_NODES   20
#define G_CLOSE     "CLOSE"
#define G_OPEN      "OPEN"

// max number of tries for garage command
#define MAX_G_CMND  3

typedef struct
{
        bool        set;
        char        type;
        char        data[50];
        uint32_t    dtime;
        uint8_t     addrA[8];
} sensorNode;

extern sensorNode nodes[MAX_NODES];
extern uint8_t numNodes;

/* Sensor Controller */
extern char appSCdataStr[];

/* For Radio Task*/
extern Event_Handle radioEvent;
#define RF_RX_EVT           Event_Id_00
#define RF_TX_EVT           Event_Id_01
#define RF_RX_FILT_EVT      Event_Id_02
#define RF_GARAGE_EVT       Event_Id_03
#define GARAGE_EVT_DONE     Event_Id_04
#define GARAGE_EVT_CANCEL   Event_Id_05

#define RF_EVT_ALL          Event_Id_00|\
                            Event_Id_01|\
                            Event_Id_02|\
                            Event_Id_03|\
                            Event_Id_04|\
                            Event_Id_05


/* Message to update Node Data */
#define NUM_NDATA_MSGS 3

typedef struct {
    uint8_t addrA[8];
    char    data[50];
} nodeDataMsgObj;

typedef struct {
    Mailbox_MbxElem  elem;      /* Mailbox header        */
    nodeDataMsgObj      obj;      /* Application's mailbox */
} MailboxNdatmsgObj;

extern MailboxNdatmsgObj nodeDataMboxBuff[NUM_NDATA_MSGS];

extern Mailbox_Struct nodeDatambxS;
extern Mailbox_Handle nodeDatambxH;

/* Message to send command to garage */
#define NUM_GCMND_MSGS   2

typedef struct {
    uint8_t addrA[8];
    uint8_t cmnd[40];
} nodeGcmndMsgObj;

typedef struct {
    Mailbox_MbxElem  elem;      /* Mailbox header        */
    nodeGcmndMsgObj   obj;      /* Application's mailbox */
} MailboxGcmndmsgObj;

extern MailboxGcmndmsgObj nodeGcmndMboxBuff[NUM_GCMND_MSGS];
extern Mailbox_Struct nodeGcmndmbxS;
extern Mailbox_Handle nodeGcmndmbxH;

/* Message to show status of garage command */
#define NUM_GSTAT_MSGS   2

typedef struct {
    uint8_t addrA[8];
    uint8_t stat[10];
} nodeGStatMsgObj;

typedef struct {
    Mailbox_MbxElem  elem;      /* Mailbox header        */
    nodeGStatMsgObj   obj;      /* Application's mailbox */
} MailboxGStatmsgObj;

extern MailboxGStatmsgObj nodeGStatMboxBuff[NUM_GSTAT_MSGS];
extern Mailbox_Struct nodeGStatmbxS;
extern Mailbox_Handle nodeGStatmbxH;

/* Max retries for garage command */
#define GARAGE_CMD_MAX_RETRY    2

/* Utility Functions */
uint8_t getNodeRef(uint8_t addrN[]);
uint16_t getValidRefs(char refs[]);
bool checkIfAdded(uint8_t addr[], uint8_t *ref);

/* Radio task global functions */
extern void radioTaskEvtPost(uint32_t evt);

#endif /* HUBTASKS_HUBDATA_H_ */












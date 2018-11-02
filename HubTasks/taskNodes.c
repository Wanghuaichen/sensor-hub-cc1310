/*
 * taskNodes.c
 *
 *  Created on: Jul 5, 2018
 *      Author: Dillon
 */

#include "taskNodes.h"
#include"HubData.h"
#include "SensorControllerStudio/appSensorController.h"

MailboxNdatmsgObj nodeDataMboxBuff[NUM_NDATA_MSGS];

Mailbox_Struct nodeDatambxS;
Mailbox_Handle nodeDatambxH;

extern sensorNode nodes[MAX_NODES];
extern uint8_t numNodes;

void initNodesTask(void);

Void nodesTask(UArg arg0, UArg arg1)
{
    initNodesTask();

    nodeDataMsgObj msg;
    uint8_t ref;
    while(true)
    {
        Mailbox_pend(nodeDatambxH, &msg, BIOS_WAIT_FOREVER);
        ref = getNodeRef(msg.addrA);
        if(ref < MAX_NODES){//valid if ref is less than max nodes
            //update node data
            memcpy(nodes[ref].data, msg.data, strlen((char*)msg.data));
            nodes[ref].dtime = Seconds_get();
        }

    }
}

void initNodesTask(void)
{
    Mailbox_Params mbxParams;

    /* Construct a Mailbox instance */
    Mailbox_Params_init(&mbxParams);
    mbxParams.buf = (Ptr)nodeDataMboxBuff;
    mbxParams.bufSize = sizeof(nodeDataMboxBuff);
    Mailbox_construct(&nodeDatambxS, sizeof(nodeDataMsgObj), NUM_NDATA_MSGS, &mbxParams, NULL);
    nodeDatambxH = Mailbox_handle(&nodeDatambxS);

    scInit();//start sensor controller
}

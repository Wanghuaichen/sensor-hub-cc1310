/*
 * taskRadio.c
 *
 *  Created on: Jul 5, 2018
 *      Author: Dillon
 */
#include "taskRadio.h"
#include "HubData.h"
#include "easylink/EasyLink.h"
#include "encryption/messageEncryption.h"

extern sensorNode nodes[MAX_NODES];
extern uint8_t numNodes;

typedef enum {
    radio_RX_EVT_CB,
    radio_RX_SEM_CB,
    radio_TX_EVT_CB,
    radio_TX_SEM_CB
} radioCBsrc;

extern uint8_t ieeeAddr[8] = {0};

Event_Handle radioEvent;
Event_Struct structEvent;

Swi_Struct rfRxDoneSwiS;
Swi_Handle rfRxDoneSwiH;

Swi_Struct rfTxDoneSwiS;
Swi_Handle rfTxDoneSwiH;

EasyLink_RxPacket rfRxPacket = {0};

extern MailboxNdatmsgObj nodeDataMboxBuff[NUM_NDATA_MSGS];
extern Mailbox_Struct nodeDatambxS;
extern Mailbox_Handle nodeDatambxH;

extern MailboxGcmndmsgObj nodeGcmndMboxBuff[NUM_GCMND_MSGS];
extern Mailbox_Struct nodeGcmndmbxS;
extern Mailbox_Handle nodeGcmndmbxH;

extern MailboxGStatmsgObj nodeGStatMboxBuff[NUM_GSTAT_MSGS];
extern Mailbox_Struct nodeGStatmbxS;
extern Mailbox_Handle nodeGStatmbxH;

static void eLinkInit(void);

static void rxDoneCb(EasyLink_RxPacket * rxPacket, EasyLink_Status status);
static void rfRxProcessFxn(UArg a0, UArg a1);

static void txDoneCb(EasyLink_Status status);
static void rfTxProcessFxn(UArg a0, UArg a1);

static void radioStartRxEvt(void);
//static bool radioRxBlock(uint32_t timeout);

static void radioStartTxEvt(EasyLink_TxPacket *txPacket);
//static bool radioTxBlock(EasyLink_TxPacket *txPacket);

static uint8_t updateRxFilter(void);

void radioTaskEvtPost(uint32_t evt);

Void radioTask(UArg arg0, UArg arg1)
{
    eLinkInit();
    updateRxFilter();
    uint32_t events = 0;
    EasyLink_TxPacket txPacket = {0};
    nodeGcmndMsgObj msgCmndGarage;
    bool enterRx = true;
    while(true)
    {
        if(enterRx){ // flag must be true
            radioStartRxEvt(); // will return EasyLink_Status_Busy_Error if radio is busy
        }
        enterRx = true;
        GPIO_write(Board_GPIO_RLED, 0);
        events = Event_pend(radioEvent, Event_Id_NONE, RF_EVT_ALL, BIOS_WAIT_FOREVER);

        if(events & RF_RX_EVT){
            //if packet received successfully
            if(rfRxPacket.len > 0){
                //data received from node
                nodeDataMsgObj msg = {0};
                memcpy(msg.addrA, rfRxPacket.dstAddr, 8);
                memcpy(msg.data, rfRxPacket.payload, rfRxPacket.len);
                Mailbox_post(nodeDatambxH, &msg, BIOS_NO_WAIT);
            }
        }

        if(events & RF_TX_EVT){
            //not used, radio will enter rx mode
        }

        if(events & RF_RX_FILT_EVT){
            //node added or removed
            updateRxFilter();
        }

        if(events & RF_GARAGE_EVT){
            //command to send to garage node
            //should already be formatted correctly

            bool msgRead = Mailbox_pend(nodeGcmndmbxH, &msgCmndGarage, BIOS_NO_WAIT);

            if(msgRead){
                EasyLink_abort();//cancel any current transactions
                txPacket.len = strlen((char*)msgCmndGarage.cmnd);
                memcpy(txPacket.dstAddr, msgCmndGarage.addrA, 8);
                memcpy(txPacket.payload, msgCmndGarage.cmnd, txPacket.len);
                EasyLink_enableRxAddrFilter(msgCmndGarage.addrA, 8, 1);//only receive from garage node
                radioStartTxEvt(&txPacket); // start transmitt
                // do not enter Rx mode
                enterRx = false;
            }

        }

        if(events & GARAGE_EVT_DONE){
            bool success = false;
            if(memcmp(txPacket.payload, rfRxPacket.payload, txPacket.len) == 0){
                //success
                success = true;
            }
            //post message showing if command was successful
            nodeGStatMsgObj msgStat;
            memcpy(msgStat.addrA, msgCmndGarage.addrA, 8);

            if(success){
                strcpy((char*)msgStat.stat, "SUCCESS");
            }
            else{
                strcpy((char*)msgStat.stat, "FAILURE");
            }
            Mailbox_post(nodeGStatmbxH, &msgStat, BIOS_NO_WAIT);
            updateRxFilter();//restore Rx filter for all nodes
        }

        if(events & GARAGE_EVT_CANCEL){
            EasyLink_abort();//cancel any current transactions
            updateRxFilter();//restore Rx filter for all nodes
        }

    }//while(true)
}//task

static void rxDoneCb(EasyLink_RxPacket * rxPacket, EasyLink_Status status)
{
    GPIO_write(Board_GPIO_RLED, 1);//turn on LED to show data being processed
    if (status == EasyLink_Status_Success)
    {
        //copy data to global struct
        memcpy(rfRxPacket.payload, rxPacket->payload, rxPacket->len);
        memcpy(rfRxPacket.dstAddr, rxPacket->dstAddr, 8);
        rfRxPacket.len = rxPacket->len;
    }
    else if(status == EasyLink_Status_Aborted)
    {
        return;
    }

    Swi_Params swiParams;
    Swi_getAttrs(rfRxDoneSwiH, NULL, &swiParams);
    swiParams.arg1 = status;
    Swi_setAttrs(rfRxDoneSwiH, NULL, &swiParams);
    Swi_post(rfRxDoneSwiH);
}

static void rfRxProcessFxn(UArg a0, UArg a1)
{
    int8_t cryptStat = CRYPTOCC26XX_STATUS_ERROR;

    if(a1 == EasyLink_Status_Success){
        cryptStat = decryptMessage(rfRxPacket.payload, &rfRxPacket.len);//decrypt message
        rfRxPacket.payload[rfRxPacket.len] = '\0';//ensure end of string char is present
    }
    else{
        radioStartRxEvt();
        return;
    }

    //if decryption failed empty data
    if(cryptStat != CRYPTOCC26XX_STATUS_SUCCESS){
        rfRxPacket.payload[0] = 0;
        rfRxPacket.len = 0;
    }

    if(rfRxPacket.payload[0] == 'G' && rfRxPacket.payload[2] == 'C'){
        // received data is response to garage command
        Event_post(radioEvent, GARAGE_EVT_DONE);
    }
    else{
        Event_post(radioEvent, RF_RX_EVT);
    }
}

static void txDoneCb(EasyLink_Status status)
{
    GPIO_write(Board_GPIO_RLED, 1);//turn on LED to show data being processed
    Swi_Params swiParams;
    Swi_getAttrs(rfTxDoneSwiH, NULL, &swiParams);
    swiParams.arg1 = status;
    Swi_setAttrs(rfTxDoneSwiH, NULL, &swiParams);
    Swi_post(rfTxDoneSwiH);
}

static void rfTxProcessFxn(UArg a0, UArg a1)
{
    Event_post(radioEvent, RF_TX_EVT);
}

static void radioStartRxEvt(void)
{
    EasyLink_abort();//cancel any current transactions
    Swi_Params swiParams;
    Swi_getAttrs(rfRxDoneSwiH, NULL, &swiParams);
    swiParams.arg0 = radio_RX_EVT_CB;
    Swi_setAttrs(rfRxDoneSwiH, NULL, &swiParams);
    EasyLink_receiveAsync(rxDoneCb, 0);//put in Rx mode starting now
}

//static bool radioRxBlock(uint32_t timeout)
//{
//    EasyLink_abort();//cancel any current transactions
//    Swi_Params swiParams;
//    Swi_getAttrs(rfRxDoneSwiH, NULL, &swiParams);
//    swiParams.arg0 = radio_RX_SEM_CB;
//    Swi_setAttrs(rfRxDoneSwiH, NULL, &swiParams);
//    EasyLink_receiveAsync(rxDoneCb, 0);//put in Rx mode starting now
//    return Semaphore_pend(rxDoneSem, timeout);
//}


static void radioStartTxEvt(EasyLink_TxPacket *txPacket)
{
    EasyLink_abort();//cancel any current transactions
    EasyLink_TxPacket txPacketEncrypted = {0};
    memcpy(txPacketEncrypted.dstAddr, txPacket->dstAddr, 8);
    addEncryptedPayload(&txPacketEncrypted, txPacket->payload, txPacket->len);
    EasyLink_transmitCcaAsync(&txPacketEncrypted, txDoneCb);//send packet and listen before talk
}


// static bool radioTxBlock(EasyLink_TxPacket *txPacket)
//{
//     EasyLink_abort();//cancel any current transactions
//     EasyLink_TxPacket txPacketEncrypted = {0};
//     Swi_Params swiParams;
//     Swi_getAttrs(rfTxDoneSwiH, NULL, &swiParams);
//     swiParams.arg0 = radio_TX_SEM_CB;
//     Swi_setAttrs(rfTxDoneSwiH, NULL, &swiParams);
//     memcpy(txPacketEncrypted.dstAddr, txPacket->dstAddr, 8);
//     addEncryptedPayload(&txPacketEncrypted, txPacket->payload, txPacket->len);
//     EasyLink_transmitCcaAsync(&txPacketEncrypted, txDoneCb);//send packet and listen before talk
//     return Semaphore_pend(txDoneSem, BIOS_WAIT_FOREVER);
//}

static uint8_t updateRxFilter(void)
 {
    EasyLink_abort();//cancel any current transactions
    uint8_t rxAddrFilter[EASYLINK_MAX_ADDR_SIZE * EASYLINK_MAX_ADDR_FILTERS];
    uint16_t rxAddrCnt = 0;
    uint16_t i;

    if(numNodes == 0){
     EasyLink_enableRxAddrFilter(ieeeAddr, 8, 1);//only receive on own address
     return 0;
    }

    for(i = 0; i < MAX_NODES; i++){
     if(nodes[i].set){
         memcpy(&rxAddrFilter[EASYLINK_MAX_ADDR_SIZE * rxAddrCnt++], nodes[i].addrA, EASYLINK_MAX_ADDR_SIZE);
     }
    }

    if(rxAddrCnt == 0){
     return 1;
    }

    EasyLink_enableRxAddrFilter(rxAddrFilter, 8, rxAddrCnt);//filter to only receive on valid nodes

    return 0;
 }

void radioTaskEvtPost(uint32_t evt)
{
    Event_post(radioEvent, evt);
}

static void eLinkInit(void)
{
    EasyLink_Params easyLink_params;
    EasyLink_Params_init(&easyLink_params);

    easyLink_params.ui32ModType = EasyLink_Phy_50kbps2gfsk;

    /* Initialize EasyLink */
    if(EasyLink_init(&easyLink_params) != EasyLink_Status_Success)
    {
        System_printf("\r\nEasyLink_init FAIL\r\n");
        while(1);
    }

    /* Set output power to 12dBm */
    EasyLink_Status pwrStatus = EasyLink_setRfPower(12);

    if(pwrStatus != EasyLink_Status_Success)
    {
        System_printf("\r\nThere was a problem setting the transmission power\r\n");
        while(1);
    }

    EasyLink_setCtrl(EasyLink_Ctrl_AddSize, 8);//set address length to 8 bytes

    EasyLink_getIeeeAddr(ieeeAddr);

    // SWI Initialization
    Swi_Params swiParams;
    Swi_Params_init(&swiParams);
    swiParams.priority = 5;
    swiParams.arg0 = radio_RX_EVT_CB;
    Swi_construct(&rfRxDoneSwiS, rfRxProcessFxn, &swiParams, NULL);
    rfRxDoneSwiH = Swi_handle(&rfRxDoneSwiS);

    Swi_Params_init(&swiParams);
    swiParams.priority = 5;
    Swi_construct(&rfTxDoneSwiS, rfTxProcessFxn, &swiParams, NULL);
    rfTxDoneSwiH = Swi_handle(&rfTxDoneSwiS);

    /* Create a semaphore for Async */
    Semaphore_Params params1, params2;
    Error_Block eb;

    /* Init params */
    Semaphore_Params_init(&params1);
    Semaphore_Params_init(&params2);
    Error_init(&eb);

    /* Event handle for task */
    Event_Params eventParams;
    Event_Params_init(&eventParams);
    Event_construct(&structEvent, &eventParams);
    radioEvent = Event_handle(&structEvent);

    //initialize encryption module
    messageEncrypt_Init(ieeeAddr);
}

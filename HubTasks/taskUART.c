/*
 * taskUART.c
 *
 *  Created on: Jul 5, 2018
 *      Author: Dillon
 */

#include "taskUART.h"
#include "HubData.h"
#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTCC26XX.h>

UART_Handle uart;

uint8_t uartRxBuff[UART_RX_BUFF_LEN] = {0};
uint8_t receivedCmnd[64] = {0};
uint8_t* command = receivedCmnd;

bool uartTxDone = true;

Semaphore_Handle uartCmndRxSem;

/* Mailbox for sending commands to garage node */
MailboxGcmndmsgObj nodeGcmndMboxBuff[NUM_GCMND_MSGS];
Mailbox_Struct nodeGcmndmbxS;
Mailbox_Handle nodeGcmndmbxH;

/* Mailbox to receive status of garage command */
MailboxGStatmsgObj nodeGStatMboxBuff[NUM_GSTAT_MSGS];
Mailbox_Struct nodeGStatmbxS;
Mailbox_Handle nodeGStatmbxH;

static void uartInit(void);
static void uartWriteBuff(char utd[], size_t len);

static void uartWriteCallback(UART_Handle handle, void *buf, size_t count);
static void uartReadCallback(UART_Handle handle, void *buf, size_t count);

static void appAddNode(uint8_t *hubCmnd);
static void appRemoveNode(uint8_t *hubCmnd);
static void appDataRequest(uint8_t *hubCmnd);
static void appTimeSetGet(uint8_t *hubCmnd);
static void appCheckReference(uint8_t *hubCmnd);
static void appGetReferences(uint8_t *hubCmnd);
static void appGarageNodeCmnd(uint8_t *hubCmnd);

static bool checkCommand(uint8_t *cmd);

static uint16_t charReplace(char *string, char char2repl, char newChar);
static int16_t charPosition(char c, char *str);

Void uartTask(UArg arg0, UArg arg1)
{
    uartInit();
    GPIO_write(Board_GPIO_GLED, 0);

    int16_t cmdRef = 0;

    while(true)
    {
        UART_read(uart, uartRxBuff, UART_RX_BUFF_LEN);//start uart Rx
        GPIO_write(Board_GPIO_GLED, 0);
        Semaphore_pend(uartCmndRxSem, BIOS_WAIT_FOREVER);

        cmdRef = charPosition('>', (char*) receivedCmnd);

        if(cmdRef > 0){
            // reply with reference if received
            uartWriteBuff((char*) receivedCmnd, cmdRef + 1);
            command = &receivedCmnd[cmdRef + 1];

        } else{
            command = receivedCmnd;
        }

        //ensure command is valid
        if(!checkCommand(command)){
            command[0] = 'X';
        }

        switch(command[0]){
        case 'A'://add node
            appAddNode(command);
            break;
        case 'R'://remove node
            appRemoveNode(command);
            break;
        case 'D'://node data request
            appDataRequest(command);
            break;
        case 'L'://get local temp
            uartWriteBuff(appSCdataStr, strlen(appSCdataStr));
            break;
        case 't'://set or get time
            appTimeSetGet(command);
            break;
        case 'C':
            appCheckReference(command);
            break;
        case 'f':
            appGetReferences(command);
            break;
        case 'G':
            appGarageNodeCmnd(command);
            break;
        default:
            uartWriteBuff("X#UKNOWN_COMMAND#\n", 18);
            break;
        }
    }

}

static void uartWriteCallback(UART_Handle handle, void *buf, size_t count)
{
    uartTxDone = true;
    uartWriteBuff(NULL, 0);//ensure write buffer is empty
}

static void uartReadCallback(UART_Handle handle, void *buf, size_t count)
{
    GPIO_write(Board_GPIO_GLED, 1);//turn on LED to show data being processed
    memcpy(receivedCmnd, uartRxBuff, count);
    // ensure end of string is present
    receivedCmnd[count] = '\0';
    Semaphore_post(uartCmndRxSem);
}

static void uartInit(void)
{
    UART_init();
    UART_Params uartParams;

    /* Create a UART with data processing off. */
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.baudRate = 9600;
    uartParams.writeMode = UART_MODE_CALLBACK;
    uartParams.writeCallback = uartWriteCallback;
    uartParams.readMode = UART_MODE_CALLBACK;
    uartParams.readCallback = uartReadCallback;


    uart = UART_open(Board_UART0, &uartParams);

    if (uart == NULL) {
        /* UART_open() failed */
        System_printf("\r\nUART_open FAIL\r\n");
        while (1);
    }

    UART_control(uart, UARTCC26XX_CMD_RETURN_PARTIAL_ENABLE, NULL);

    Error_Block eb;
    Semaphore_Params params;

    Semaphore_Params_init(&params);
    Error_init(&eb);

    uartCmndRxSem = Semaphore_create(0, &params, &eb);

    Mailbox_Params mbxParams;

    /* Construct a Mailbox instance for garage commands */
    Mailbox_Params_init(&mbxParams);
    //mbxParams.readerEvent = radioEvent;//post event for radio task when message is available
    //mbxParams.readerEventId = RF_GARAGE_EVT;
    mbxParams.buf = (Ptr)nodeGcmndMboxBuff;
    mbxParams.bufSize = sizeof(nodeGcmndMboxBuff);
    Mailbox_construct(&nodeGcmndmbxS, sizeof(nodeGcmndMsgObj), NUM_GCMND_MSGS, &mbxParams, NULL);
    nodeGcmndmbxH = Mailbox_handle(&nodeGcmndmbxS);

    /* Construct a Mailbox instance for garage command status */
    Mailbox_Params_init(&mbxParams);
    //mbxParams.readerEvent = RF_GARAGE_EVT;//post event for radio task when message is available
    mbxParams.buf = (Ptr)nodeGStatMboxBuff;
    mbxParams.bufSize = sizeof(nodeGStatMboxBuff);
    Mailbox_construct(&nodeGStatmbxS, sizeof(nodeGStatMsgObj), NUM_GSTAT_MSGS, &mbxParams, NULL);
    nodeGStatmbxH = Mailbox_handle(&nodeGStatmbxS);
}

/*Buffer data to ensure uart write is not called before it has finished*/
static void uartWriteBuff(char utd[], size_t len)
{
    static uint8_t buff[UART_TX_BUFF_LEN] = {0};
    static uint8_t buffPos = 0;
    //uint8_t i;

    if(len > 0){
        memcpy(&buff[buffPos], utd, len);
        buffPos += len;
    }

    if(uartTxDone && buffPos > 0){
        uartTxDone = false;
        UART_write(uart, buff, buffPos);//write data
        buffPos = 0;
    }
}

static void appAddNode(uint8_t *hubCmnd)
{
    char naddr[20] = {'\0'};
    uint8_t newAddr[8] = {0};
    uint8_t i, nNodeS;

    if(hubCmnd[0] != 'A'){
        uartWriteBuff("X##\n", 4);
        return;
    }

    for(i = 0; hubCmnd[i + 2] != '#' && i < UART_RX_BUFF_LEN; i++){
        naddr[i] = hubCmnd[i + 2];//copy new addr
    }

    if(i >= UART_RX_BUFF_LEN){//bad command
        uartWriteBuff("X##\n", 4);
        return;
    }

    uint64_t laddr = strtoull(naddr, '\0', 16);

    for(i = 0; i < 8; i++){//copy bytes from addr
        naddr[i] = laddr & 0x00FF;
        laddr >>= 8;
    }

    for(i = 0; i < 8; i++){//reverse and copy addr
        newAddr[i] = naddr[7 - i];
    }

    //check if address is already added
    //add it if needed
    if(!checkIfAdded(newAddr, &nNodeS)){
        for(i = 0; i < MAX_NODES; i++){
            if(!nodes[i].set){//find available struct for node
                nNodeS = i;
                break;
            }
        }

        numNodes++;//increment number of nodes counter

        nodes[nNodeS].set = true;
        memcpy(nodes[nNodeS].addrA, newAddr, 8);
        nodes[nNodeS].data[0] = 0;//clear 1st data character
        nodes[nNodeS].dtime = 0;//clear time
        nodes[nNodeS].type = 0;//clear type
    }

    //add address to string
    for(i = 0; i < 8; i++){
        System_sprintf(&naddr[i*2], "%02x", nodes[nNodeS].addrA[i]);
    }
    //reply with added address in proper format
    uartWriteBuff("A#", 2);
    uartWriteBuff(naddr, strlen(naddr));//send address
    System_sprintf(naddr, "#%d", nNodeS);
    uartWriteBuff(naddr, strlen(naddr));//send ref number
    uartWriteBuff("#\n", 2);

    radioTaskEvtPost(RF_RX_FILT_EVT);//post event for Rx filter to be updated
}

static void appRemoveNode(uint8_t *hubCmnd)
{
    char rRefS[4] = {'\0'};
    char raddr[20];
    uint16_t rRef, i;

    for(i = 0; hubCmnd[i + 2] != '#' && i < UART_RX_BUFF_LEN; i++){
        rRefS[i] = hubCmnd  [i + 2];//copy ref number
    }

    if(i >= UART_RX_BUFF_LEN){//bad command
        uartWriteBuff("X##\n", 4);
        return;
    }

    rRef = atoi(rRefS);//convert string to decimal

    uartWriteBuff("R#", 2);
    if(!nodes[rRef].set){//reference is invalid
        uartWriteBuff("0", 1);
    }
    else{
        nodes[rRef].set = false;
        numNodes--;
        //add address to string
        for(i = 0; i < 8; i++){
            System_sprintf(&raddr[i*2], "%02x", nodes[rRef].addrA[i]);
        }
        uartWriteBuff(raddr, strlen(raddr));
    }
    uartWriteBuff("#\n", 2);

    radioTaskEvtPost(RF_RX_FILT_EVT);//post event for Rx filter to be updated
}

static void appDataRequest(uint8_t *hubCmnd)
{
    char rRefS[4] = {'\0'};
    char tStr[40];
    uint16_t rRef, i;

    for(i = 0; hubCmnd[i + 2] != '#' && i < UART_RX_BUFF_LEN; i++){
        rRefS[i] = hubCmnd[i + 2];//copy ref number
    }

    if(i >= UART_RX_BUFF_LEN){//bad command
        uartWriteBuff("X##\n", 4);
        return;
    }

    // handle local data request
    if(rRefS[0] == 'L'){
        // Local string format:
        // System_sprintf(appSCdataStr,"L#%f,%f#%d#\n", temperatureC, temperatureF, Seconds_get());
        uartWriteBuff("D#", 2);
        strcpy(tStr, appSCdataStr);
        tStr[0] = 'T';
        tStr[1] = '-';
        charReplace(tStr, '\n', '\0');//remove end of line char
        uartWriteBuff(tStr, strlen(tStr));
        uartWriteBuff("L#\n", 3);
        return;
    }

    rRef = atoi(rRefS);//convert string to decimal

    if(!nodes[rRef].set){//node isn't valid
        uartWriteBuff("X#NO_NODE#\n", 11);
    }
    else if(strlen(nodes[rRef].data) == 0){//no data for node
        uartWriteBuff("X#NO_DATA#0#\n", 13);
    }
    else{
        uartWriteBuff("D#", 2);
        strcpy(tStr, nodes[rRef].data);
        charReplace(tStr, '#', '-');//replace # with - for data string
        uartWriteBuff(tStr, strlen(tStr));//write out data
        System_sprintf(tStr, "#%d#%d#\n", nodes[rRef].dtime, rRef);//add time, ref number, and end of command symbols
        uartWriteBuff(tStr, strlen(tStr));
    }
}

static void appTimeSetGet(uint8_t *hubCmnd)
{
    char ntime[20] = {'\0'};
    uint8_t i;
    uint32_t nt;

    if(hubCmnd[2] == 'G'){//get time
        uartWriteBuff("t#", 2);
        uartWriteBuff("G", 1);
        System_sprintf(ntime, "%d", Seconds_get());
        uartWriteBuff(ntime, strlen(ntime));
        uartWriteBuff("#\n", 2);
        return;
    }

    for(i = 0; hubCmnd[i + 2] != '#' && i < UART_RX_BUFF_LEN; i++){
        ntime[i] = hubCmnd[i + 2];//copy new time
    }

    if(i >= UART_RX_BUFF_LEN){//bad command
        uartWriteBuff("X##\n", 4);
        return;
    }

    nt = strtoul(ntime, '\0', 10);
    if(nt > 0){//check if nt is valid
        Seconds_set(nt);//set new time
        nt = Seconds_get();
    }

    uartWriteBuff("t#", 2);
    System_sprintf(ntime, "%d", nt);
    uartWriteBuff(ntime, strlen(ntime));
    uartWriteBuff("#\n", 2);
}

static void appCheckReference(uint8_t *hubCmnd)
{
    char rRefS[4] = {'\0'};
    char addr[20];
    uint16_t rRef, i;

    for(i = 0; hubCmnd[i + 2] != '#' && i < UART_RX_BUFF_LEN; i++){
        rRefS[i] = hubCmnd[i + 2];//copy ref number
    }

    if(i >= UART_RX_BUFF_LEN){//bad command
        uartWriteBuff("X##\n", 4);
        return;
    }

    rRef = atoi(rRefS);//convert string to decimal

    uartWriteBuff("C#", 2);
    if(!nodes[rRef].set){//reference is invalid
        uartWriteBuff("0", 1);
    }
    else{
        //add address to string
        for(i = 0; i < 8; i++){
            System_sprintf(&addr[i*2], "%02x", nodes[rRef].addrA[i]);
        }
        uartWriteBuff(addr, strlen(addr));
    }
    System_sprintf(addr, "#%d", rRef);
    uartWriteBuff(addr, strlen(addr));//send ref number
    uartWriteBuff("#\n", 2);
}

static void appGetReferences(uint8_t *hubCmnd)
{
    char refs[3*MAX_NODES];
    uint8_t nodeCnt;
    nodeCnt = getValidRefs(refs);

    uartWriteBuff("f#", 2);

    if(nodeCnt == 0){
        //no valid references
        uartWriteBuff("NONE", 4);
    }
    else
    {
        uartWriteBuff(refs, strlen(refs));
    }
    uartWriteBuff("#\n", 2);
}

static void appGarageNodeCmnd(uint8_t *hubCmnd)
{
    uint16_t i;
    char rbuff[10] = {0};
    uint8_t ref;

    if(hubCmnd[2] != 'C'){
        uartWriteBuff("G#BAD_COMMAND#\n", 15);
        return;
    }

    for(i = 0; hubCmnd[i + 4] != '#' && i < UART_RX_BUFF_LEN; i++){
        rbuff[i] = hubCmnd[i + 4];
    }

    ref = atoi(rbuff);

    if(!nodes[ref].set){
        uartWriteBuff("G#BAD_REF#\n", 11);
        return;
    }

    uint8_t ros = ++i + 4;

    for(i = 0;  hubCmnd[i + ros] != '#' && i < UART_RX_BUFF_LEN; i++){
        rbuff[i] = hubCmnd[i + ros];
    }
    rbuff[i] = '\0';//ensure end of string character is added

    if(i >= UART_RX_BUFF_LEN){
        uartWriteBuff("G#BAD_G_COMMAND#\n", 17);
        return;
    }

    nodeGcmndMsgObj msgCmnd;
    memcpy(msgCmnd.addrA, nodes[ref].addrA, 8);

    char addr[20] = {0};
    //add address to string
    for(i = 0; i < 8; i++){
        System_sprintf(&addr[i*2], "%02x", nodes[ref].addrA[i]);
    }

    if(strcmp(rbuff, "CLOSE") == 0){
        //send close command
        System_sprintf((char*)msgCmnd.cmnd, "G#C#CLOSE#%d#", Seconds_get());
    }
    else if(strcmp(rbuff, "OPEN") == 0){
        //open garage
        System_sprintf((char*)msgCmnd.cmnd, "G#C#OPEN#%d#", Seconds_get());
    }
    else{
        uartWriteBuff("G#C#", 4);
        uartWriteBuff(addr, strlen(addr));
        uartWriteBuff("#", 1);
        uartWriteBuff("BAD_COMMAND", 11);
        uartWriteBuff("#\n", 2);
        return;
    }


    uint8_t retryCnt;
    nodeGStatMsgObj msgStat;
    bool msgRead, success = false;

    for(retryCnt = 0; retryCnt < GARAGE_CMD_MAX_RETRY; retryCnt++){
        //post command for RF task, should also post RF_GARAGE_EVT event
        Mailbox_post(nodeGcmndmbxH, &msgCmnd, BIOS_NO_WAIT);

        radioTaskEvtPost(RF_GARAGE_EVT);//post event

        //wait 500ms for response
        msgRead = Mailbox_pend(nodeGStatmbxH, &msgStat, 500000/Clock_tickPeriod);

        if(msgRead && strcmp((char*)msgStat.stat, "SUCCESS") == 0 && memcmp(msgStat.addrA, msgCmnd.addrA, 8) == 0){
            success = true;
            break;
        }

        radioTaskEvtPost(GARAGE_EVT_CANCEL);//post cancel event and retry
    }

    uartWriteBuff("G#C#", 4);
    uartWriteBuff(addr, strlen(addr));
    uartWriteBuff("#", 1);

    if(success){
        uartWriteBuff(rbuff, strlen(rbuff));
    }
    else{
        radioTaskEvtPost(GARAGE_EVT_CANCEL);//post event
        uartWriteBuff("NO_RESPONSE", 11);
    }
    uartWriteBuff("#\n", 2);
}

static uint16_t charReplace(char *string, char char2repl, char newChar)
{
    uint16_t i, replCnt = 0;

    for(i = 0; string[i] != '\0'; i++){
        if(string[i] == char2repl){
            string[i] = newChar;
            replCnt++;
        }
    }

    return replCnt;
}

/* Find first occurance of character in string */
static int16_t charPosition(char c, char *str)
{
    uint16_t i;
    for(i = 0; str[i] != '\0'; i++){
        if(str[i] == c)
            return i;
    }
    return -1; // character not found
}

/*Ensure all commands are valid*/
static bool checkCommand(uint8_t *cmd)
{
    int16_t pos1, pos2;
    uint32_t num;
    uint8_t temp[20] = {0};

    switch(cmd[0]){
    case 'A'://add node
        // A#(address)#\n
        if(cmd[1] != '#' || cmd[18] != '#'){
            return false;
        }
        break;
    case 'R'://remove node
    case 'D'://node data request
    case 'C'://check reference
        pos1 = charPosition('#', (char*) cmd);
        pos2 = charPosition('#', (char*) &cmd[2]) + 2;
        if(pos1 != 1 || pos2 > 4 || pos2 < 3)
            return false;
        memcpy(temp, &cmd[pos1 + 1], pos2 - pos1 - 1);
        num = atoi((char*) temp);
        if(num > MAX_NODES)
            return false;
        break;
    case 'L'://get local temp
    case 'f'://get all refs
        if(cmd[1] != '#' || cmd[2] != '#')
            return false;
        break;
    case 't'://set or get time
        pos1 = charPosition('#', (char*) cmd);
        pos2 = charPosition('#', (char*) &cmd[2]) + 2;
        if(pos2 == 3){
            if(cmd[2] != 'G')
                return false;
            else{
                return true;
            }
        }
        memcpy(temp, &cmd[pos1 + 1], pos2 - pos1 - 1);
        num = atoi((char*) temp);
        if(num < 1536607412)//ensure time is greater than Monday, September 10, 2018 7:23:32 PM
            return false;
        break;
    case 'G'://garage control
        if(cmd[1] != '#' || cmd[2] != 'C' || cmd[3] != '#')
            return false;
        pos1 = 3;
        pos2 = charPosition('#', (char*) &cmd[4]) + 4;
        memcpy(temp, &cmd[pos1 + 1], pos2 - pos1 - 1);
        num = atoi((char*) temp);
        if(num > MAX_NODES)
            return false;
        pos1 = pos2;
        pos2 = charPosition('#', (char*) &cmd[pos1 + 1]) + pos1 + 1;
        memcpy(temp, &cmd[pos1 + 1], pos2 - pos1 - 1);
        if(strcmp((char*) temp, "CLOSE") != 0 && strcmp((char*) temp, "OPEN") != 0)
            return false;
        break;
    default:
        return false;
    }
    return true;
}

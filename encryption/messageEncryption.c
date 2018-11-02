/*
 * messageEncryption.c
 *
 *  Created on: Jun 4, 2018
 *      Author: Dillon
 */

#include "encryption/messageEncryption.h"
 /* Standard C Libraries */
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

/* For System printf */
#include <xdc/runtime/System.h>

/* Board Header file */
#include "Board.h"

NVS_Handle encrypt_nvsHandle;

CryptoCC26XX_Handle      encryptH;

uint8_t key[16] = ENCRYPT_KEY0;
int keyIndex;

uint32_t nonce[2];

void messageEncrypt_Init(uint8_t *ieeeAddr)
{
    CryptoCC26XX_init();
    // Attempt to open CryptoCC26XX.
    encryptH = CryptoCC26XX_open(Board_CRYPTO0, false, NULL);
    if (!encryptH) {
        System_printf("CryptoCC26XX did not open");
    }

    /*allocate key*/
    keyIndex = CryptoCC26XX_allocateKey(encryptH, CRYPTOCC26XX_KEY_ANY, (uint32_t*)key);

    //storing random number for rand() seed
    NVS_Params nvsParams;

    //initialize nvs for internal flash storage
    NVS_init();

    NVS_Params_init(&nvsParams);
    encrypt_nvsHandle = NVS_open(Board_NVSINTERNAL, &nvsParams);
    uint32_t fbuff[2] = {0};
    NVS_read(encrypt_nvsHandle, 0, (void *) fbuff, sizeof(fbuff));
    if(fbuff[0] != 0xAAAA){
        //use part of IEEE address for initial seed
        fbuff[1] = ((uint32_t) ieeeAddr[0] << 24) | ((uint32_t) ieeeAddr[2] << 16) |
                ((uint32_t) ieeeAddr[3] << 8) | ((uint32_t) ieeeAddr[1]);
        fbuff[0] = 0xAAAA;
    }
    srand(fbuff[1]);
    fbuff[1] = rand();
    NVS_write(encrypt_nvsHandle, 0, (void *) fbuff, sizeof(fbuff),
                NVS_WRITE_ERASE | NVS_WRITE_POST_VERIFY);
    NVS_close(encrypt_nvsHandle);
}

/* generate 12 byte random nonce */
void generateNonce(uint32_t nonceDestination[])
{
    nonceDestination[0] = rand();
    nonceDestination[1] = rand();
    nonceDestination[2] = rand();
}

int32_t encryptMessage(uint8_t *messageBuffer, uint8_t msgLength)
{
    uint8_t plainTextLength     = msgLength;
    uint8_t cipherTextLength    = MAC_LENGTH + plainTextLength;
    uint8_t header[AAD_LENGTH]  = {plainTextLength, cipherTextLength, MAC_LENGTH, NONCE_LENGTH};
    CryptoCC26XX_AESCCM_Transaction trans;

    generateNonce(nonce);//stored globally for payload

    CryptoCC26XX_loadKey(encryptH, keyIndex, (uint32_t*)key);

    /* Initialize encryption transaction */
    CryptoCC26XX_Transac_init((CryptoCC26XX_Transaction *)&trans, CRYPTOCC26XX_OP_AES_CCM_ENCRYPT);

    trans.keyIndex      = keyIndex;
    trans.authLength    = MAC_LENGTH;
    trans.nonce         = (char *) nonce;
    trans.header        = (char *) header;
    trans.fieldLength   = 15 - NONCE_LENGTH;
    trans.msgInLength   = plainTextLength;
    trans.headerLength  = AAD_LENGTH;
    trans.msgIn         = (char *)messageBuffer; /* Points to the plaintext */
    trans.msgOut        = (char *)&(messageBuffer[plainTextLength]); /* position of MAC field */

    /* Start the encryption transaction */
    return CryptoCC26XX_transact(encryptH, (CryptoCC26XX_Transaction *)&trans);
}

void addEncryptedPayload(EasyLink_TxPacket *txPacket, uint8_t *payload, uint8_t payloadLen)
{
    uint8_t cryptoPayload[EASYLINK_MAX_DATA_LENGTH];
    memcpy(cryptoPayload, payload, payloadLen);
    volatile int flag = encryptMessage(cryptoPayload, payloadLen);//generate encrypted message



    uint8_t plainTextLength     = payloadLen;
    uint8_t cipherTextLength    = plainTextLength + MAC_LENGTH;

    /* Fill header with length parameters */
    uint8_t header[AAD_LENGTH]   = {plainTextLength, cipherTextLength, MAC_LENGTH, NONCE_LENGTH};

    memcpy(txPacket->payload, header, AAD_LENGTH);
    memcpy(&txPacket->payload[AAD_LENGTH], nonce , NONCE_LENGTH);
    memcpy(&txPacket->payload[AAD_LENGTH+NONCE_LENGTH], cryptoPayload, cipherTextLength);
    txPacket->len = AAD_LENGTH + NONCE_LENGTH + cipherTextLength;
}

/* decrypt message */
int8_t decryptMessage(uint8_t *msgBuffer , uint8_t *msgLen)
{
    uint8_t buff[EASYLINK_MAX_DATA_LENGTH];
    int32_t status ;
    CryptoCC26XX_AESCCM_Transaction trans;
    uint8_t macBuffer[MAC_LENGTH];
    uint8_t header[AAD_LENGTH];
    uint8_t plainTextLength;
    uint8_t cipherTextLength;
    uint8_t nonceLength;
    uint8_t authLength;

    uint8_t rawMsgLen = *msgLen;

    /* unload package in correct format */
    memcpy(header, msgBuffer, AAD_LENGTH);
    plainTextLength  = header[0];
    cipherTextLength = header[1];
    authLength       = header[2];
    nonceLength      = header[3];
    *msgLen          = plainTextLength;

    CryptoCC26XX_loadKey(encryptH, keyIndex, (uint32_t*)key);

    /* initialize transaction */
    CryptoCC26XX_Transac_init((CryptoCC26XX_Transaction*)&trans, CRYPTOCC26XX_OP_AES_CCM_DECRYPT);

    trans.keyIndex      = keyIndex;
    trans.authLength    = authLength;
    trans.nonce         = (char *)&msgBuffer[AAD_LENGTH]; /* Points to the nonce */
    trans.header        = (char *) header;
    trans.fieldLength   = 15 - nonceLength; /* 15 = fieldlength + nonceLength condition must be true */
    trans.msgInLength   = cipherTextLength;
    trans.headerLength  = AAD_LENGTH;
    trans.msgIn         = (char *)&msgBuffer[AAD_LENGTH + nonceLength]; /* Points to the cipherText */
    trans.msgOut        = (char *)macBuffer;  /* Points to the MAC field */

    //ensure received data is encrypted correctly
    //txPacket->len = AAD_LENGTH + NONCE_LENGTH + cipherTextLength;
    if(plainTextLength != (cipherTextLength - MAC_LENGTH) ||
       rawMsgLen != (AAD_LENGTH + NONCE_LENGTH + cipherTextLength) ||
       trans.fieldLength != (15 - NONCE_LENGTH)){
        return CRYPTOCC26XX_STATUS_ERROR;
    }

    /* Start the decryption transaction */
    status = CryptoCC26XX_transact(encryptH, (CryptoCC26XX_Transaction *)&trans);
    if(status != CRYPTOCC26XX_STATUS_SUCCESS) {
        /* Transaction did not succeed */
        return CRYPTOCC26XX_STATUS_ERROR;
    }

    /* Store the decrypted message in a buffer */
    memcpy(buff, &msgBuffer[AAD_LENGTH + nonceLength], plainTextLength);
    memcpy(msgBuffer, buff, plainTextLength);//copy decrypted string to buffer

    return CRYPTOCC26XX_STATUS_SUCCESS;
}








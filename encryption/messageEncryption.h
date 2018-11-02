/*
 * messageEncrypt.h
 *
 *  Created on: Jun 4, 2018
 *      Author: Dillon
 */

#ifndef MESSAGEENCRYPT_H_
#define MESSAGEENCRYPT_H_

#include <ti/drivers/crypto/CryptoCC26XX.h>
#include <ti/drivers/NVS.h>

/* EasyLink API Header files */
#include "easylink/EasyLink.h"

/* Keys, generated using keyGenerator.xlsm */
#include "keys.h"

#define MAC_LENGTH      4
#define AAD_LENGTH      4
#define NONCE_LENGTH    12

/* defined in keys.h
#define ENCRYPT_KEY0 {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
*/

extern NVS_Handle encrypt_nvsHandle;

/*Functions*/
void messageEncrypt_Init(uint8_t *ieeeAddr);
void generateNonce(uint32_t nonceDestination[]);
int32_t encryptMessage(uint8_t *messageBuffer, uint8_t msgLength);
void addEncryptedPayload(EasyLink_TxPacket *txPacket, uint8_t *payload, uint8_t payloadLen);
int8_t decryptMessage(uint8_t *msgBuffer , uint8_t *msgLen);


#endif /* MESSAGEENCRYPT_H_ */


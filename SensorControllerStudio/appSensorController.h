/*
 * appSensorController.h
 *
 *  Created on: Jul 3, 2018
 *      Author: Dillon
 */

#ifndef HUBAPPLICATION_APPSENSORCONTROLLER_H_
#define HUBAPPLICATION_APPSENSORCONTROLLER_H_

/* Sensor Controller Studios file */
#include "SensorControllerStudio/scif.h"
#define BV(x)    (1 << (x))

extern char appSCdataStr[];

void scInit(void);
void processSensCntrlAlert(void);
/*Sensor Controller Callbacks*/
void scCtrlReadyCallback(void);
void scTaskAlertCallback(void);

#endif /* HUBAPPLICATION_APPSENSORCONTROLLER_H_ */

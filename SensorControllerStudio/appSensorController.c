/*
 * appSensorController.c
 *
 *  Created on: Jul 3, 2018
 *      Author: Dillon
 */

#include "appSensorCOntroller.h"
#include "HubTasks/HubData.h"

// SWI Task Alert
Swi_Struct swiTaskAlert;
Swi_Handle hSwiTaskAlert;

char appSCdataStr[]="L#___.____,___.____#____________#\n";

/*Sensor Controller Callbacks*/
void scCtrlReadyCallback(void)
{

} // scCtrlReadyCallback

void scTaskAlertCallback(void)
{
    // Post a SWI process
    Swi_post(hSwiTaskAlert);
}

void scPostEvntFxn(UArg a0, UArg a1)
{
    processSensCntrlAlert();
}

void processSensCntrlAlert(void)
{
    // Clear the ALERT interrupt source
    scifClearAlertIntSource();

    /*
    * tempValid & 0x000F = 0x0001 if initialization for starting temp conversion failed
    * tempValid & 0x00F0 = 0x0010 if temperature conversion failed
    * tempValid & 0x0F00 = 0x0100 if initialization for reading scratchpad failed
    * See Sensor Studio Execution code for details
    */
    uint16_t temperValid = scifTaskData.temperatureSensor.output.tempValid;

    if(temperValid & 0x0FFF)
    {//temperature sensor com failed
        System_printf("\nTemperature invalid\ntempValid: %04x", temperValid);
        System_flush();
    }
    else
    {
        //raw temperature data from sensor
        uint16_t temperC = scifTaskData.temperatureSensor.output.tempValue;
        uint16_t temperCfrac;
        float temperatureC, temperatureF;
        bool tneg = false;

        if(temperC & 0x8000)//handle negative temperature
        {
            //convert to positive (2s compliment)
            temperC = ~temperC;
            temperC++;
            tneg = true;
        }

        temperCfrac = temperC & 0x000F;//last 4 bits are fractional
        temperC >>= 4;//shift out fractional bits

        temperatureC = (float)temperC + ((float)temperCfrac*0.0625);

        if(tneg)//negate float temperature
            temperatureC *= -1;

        temperatureF = (temperatureC*9.0/5.0) + 32;//convert C to F

        //format temp with current time in string
        System_sprintf(appSCdataStr,"L#%f,%f#%d#\n", temperatureC, temperatureF, Seconds_get());

        //System_printf(temperatureS);
        //System_flush();

        //UART_write(uart, temperatureS, strlen(temperatureS));

    }
    // Acknowledge the ALERT event
    scifAckAlertEvents();
    //Event_post(event, TEMP_CONV_EVT);
} // processTaskAlert

void scInit(void)
{

    // SWI Initialization
    Swi_Params swiParams;
    Swi_Params_init(&swiParams);
    swiParams.priority = 1;
    Swi_construct(&swiTaskAlert, scPostEvntFxn, &swiParams, NULL);
    hSwiTaskAlert = Swi_handle(&swiTaskAlert);

    // Initialize the Sensor Controller
    scifOsalInit();
    scifOsalRegisterCtrlReadyCallback(scCtrlReadyCallback);
    scifOsalRegisterTaskAlertCallback(scTaskAlertCallback);
    scifInit(&scifDriverSetup);

    /*
     * - Bits 31:16 = seconds
     * - Bits 15:0 = 1/65536 of a second
     */

    // Set the Sensor Controller task tick interval to 1 second
    scifStartRtcTicksNow(0x00010000);

    // Start Sensor Controller task
    scifStartTasksNbl(BV(SCIF_TEMPERATURE_SENSOR_TASK_ID));
}

/*
 * project_settings.c
 *
 *  Created on: Apr 9, 2026
 *      Author: bahmdan
 */

#include "project_settings.h"

static projectSettings_t projectSettings;

void defaultValue_projectSettings(void)
{
 projectSettings.baudrate = 9600;
 projectSettings.tempSlaveAddress = 1;
 projectSettings.tempRegisterAddress = 0x0000;
 projectSettings.tempThreshold = 30.0f;
 projectSettings.timeOut = 1000;

 projectSettings.addrIP[0] = 192;
 projectSettings.addrIP[1] = 168;
 projectSettings.addrIP[2] = 1;
 projectSettings.addrIP[3] = 199;

 projectSettings.addrSubMask[0] = 255;
 projectSettings.addrSubMask[1] = 255;
 projectSettings.addrSubMask[2] = 255;
 projectSettings.addrSubMask[3] = 0;

 projectSettings.addrGateway[0] = 192;
 projectSettings.addrGateway[1] = 168;
 projectSettings.addrGateway[2] = 1;
 projectSettings.addrGateway[3] = 1;



}

projectSettings_t* get_projectSettings(void)
{
   return &projectSettings;
}

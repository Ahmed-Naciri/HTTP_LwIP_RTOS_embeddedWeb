/*
 * project_settings.h
 *
 *  Created on: Apr 9, 2026
 *      Author: bahmdan
 */

#ifndef INC_PROJECT_SETTINGS_H_
#define INC_PROJECT_SETTINGS_H_

#include "main.h"

typedef struct
{
  uint32_t baudrate;
  uint8_t tempSlaveAddress ;
  uint16_t tempRegisterAddress;
  float tempThreshold;
  uint16_t timeOut;

  uint8_t addrIP[4];
  uint8_t addrSubMask[4];
  uint8_t addrGateway[4];



}projectSettings_t;

void ProjectSettings_defautValue(void);
projectSettings_t* projectSettings_get(void);




#endif /* INC_PROJECT_SETTINGS_H_ */

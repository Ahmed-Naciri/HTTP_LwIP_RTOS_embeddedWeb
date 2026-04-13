/*
 * application_temperature_monitor.h
 *
 *  Created on: Apr 10, 2026
 *      Author: bahmdan
 */

#ifndef INC_APPLICATION_TEMPERATURE_MONITOR_H_
#define INC_APPLICATION_TEMPERATURE_MONITOR_H_

#include "main.h"
typedef struct
{
   uint16_t rawTemp;
   float tempCelsius;
   uint8_t communicationOK;
   uint8_t alarmeActive;

}tempMonitorData_t;

void temperature_monitor_init(void);
void temperature_monitor_task(void);
tempMonitorData_t* temperature_monitor_getData(void);

#endif /* INC_APPLICATION_TEMPERATURE_MONITOR_H_ */

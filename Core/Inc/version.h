/*
 * version.h
 *
 * Created on: Nov 28, 2025
 * Author: R9OFG.RU
 */

#ifndef INC_VERSION_H_
#define INC_VERSION_H_

#include "main.h"

//Формат версии: 0xVVRR
//VV = Major version, RR = Minor version
//Пример: 0x0102 → v1.2

#define FIRMWARE_VERSION	0x0102  //v1.2

//Внешняя константа для использования в коде
extern const uint16_t firmware_version;

#endif /* INC_VERSION_H_ */

/*
 * gps.h
 *
 * Created on: Nov 29, 2025
 * Author: R9OFG.RU
 */

#ifndef INC_GPS_H_
#define INC_GPS_H_

#include "main.h"
#include "vcp.h"
#include "math.h"

#define GPS_BUF_SIZE 128

//Структура GPS-данных
typedef struct {
    bool valid;               //Валидность
    uint8_t hour, min, sec;   //Только время UTC
    float latitude;           //Десятичные градусы
    float longitude;		  //---
    char locator[7];          //QTH, например "KO85ut"
} gps_data_t;

extern gps_data_t g_gps;
extern volatile bool flag_gps_ready;
extern char gps_line_buf[GPS_BUF_SIZE];

void GPS_Init(void);
void GPS_Feed_Byte(uint8_t c);
bool GPS_Has_Fix(void);
void GPS_Parse_Line(void);

#endif /* INC_GPS_H_ */

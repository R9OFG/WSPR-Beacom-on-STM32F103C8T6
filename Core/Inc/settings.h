/*
 * settings.h
 *
 * Created on: Nov 28, 2025
 * Author: R9OFG.RU
 */

#ifndef INC_SETTINGS_H_
#define INC_SETTINGS_H_

#include "main.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "version.h"  //Тут версия прошивки

//Диапазоны
#define BAND_160M (1U << 0)  //1.8366 MHz
#define BAND_80M  (1U << 1)  //3.5686 MHz
#define BAND_40M  (1U << 2)  //7.0386 MHz
#define BAND_30M  (1U << 3)  //10.1387 MHz
#define BAND_20M  (1U << 4)  //14.0956 MHz
#define BAND_17M  (1U << 5)  //18.1046 MHz
#define BAND_15M  (1U << 6)  //21.0946 MHz
#define BAND_12M  (1U << 7)  //24.9246 MHz
#define BAND_10M  (1U << 8)  //28.1246 MHz
#define BAND_MASK_MAX 0x1FFU

#define POWER_LEVELS_COUNT 12						//Макс. 12
#define CALLSIGN_LEN		6						//Макс. 6 символов
#define GPS_LINE_HEADER_LEN 6						//Макс. 6 символов
#define SETTINGS_FLASH_ADDR	((uint32_t)0x0800FC00)	//Flash адрес (последняя страница 64KB Flash)
#define SETTINGS_MAGIC		0x57535052U				//Идентификатор для FLASH: 'WSPR'

typedef struct {
    uint32_t magic;              					//'WSPR' — сигнатура
    uint16_t version;            					//Версия прошивки на момент SAVE (из FIRMWARE_VERSION)
    char callsign[CALLSIGN_LEN + 1];				//null-terminated
    uint16_t band_mask;          					//Битовая маска диапазонов
    uint8_t power_index;         					//Индекс мощности
    uint8_t reserved1;           					//Резерв-1, выравнивание
    uint32_t xtal_freq_hz;       					//Частота опорного кварцевого резонатора для SI5351
    char gps_line_header[7];						//Заголовок строки GPS: например - $GPRMC
    char ext_pwr[5];								//Управляющий сигнал на пине PA8
    char ext_bpf[5];								//Управляющий сигнал на пинах ABCD (PB12/PB13/PB14/PB15)
    uint32_t reserved2;          					//Резерв-2
} __attribute__((packed)) wspr_settings_t;

#define SETTINGS_FLASH_ADDR	((uint32_t)0x0800FC00)	//Flash адрес (последняя страница 64KB Flash)
#define SETTINGS_MAGIC		0x57535052U				//Идентификатор для FLASH: 'WSPR'

extern const int8_t power_dbm_table[];
extern const uint16_t power_mw_table[POWER_LEVELS_COUNT];

uint16_t bands_mask_to_str(uint16_t mask, char *buf, uint16_t buf_size);
uint16_t bands_str_to_mask(const char *str, bool *ok);
int SETTINGS_Read_from_FLASH(wspr_settings_t *s);
int SETTINGS_Write_to_FLASH(const wspr_settings_t *s);
void SETTINGS_Set_Default(wspr_settings_t *s);

#endif /* INC_SETTINGS_H_ */

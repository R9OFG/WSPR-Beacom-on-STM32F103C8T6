/*
 * settings.c
 *
 * Created on: Nov 28, 2025
 * Author: R9OFG.RU
 */

#include "settings.h"

//Таблица мощности
const int8_t power_dbm_table[POWER_LEVELS_COUNT] = {
    0, 3, 7, 10, 13, 17, 20, 23, 27, 30, 33, 37
};
/*
 * Таблица: dBm → милливатты (округлено)
 * Индекс совпадает с power_dbm_table
 */

const uint16_t power_mw_table[POWER_LEVELS_COUNT] = {
    1,    // 0 dBm = 1 mW
    2,    // 3 dBm ≈ 2 mW
    5,    // 7 dBm ≈ 5 mW
    10,   // 10 dBm = 10 mW
    20,   // 13 dBm ≈ 20 mW
    50,   // 17 dBm ≈ 50 mW
    100,  // 20 dBm = 100 mW
    200,  // 23 dBm ≈ 200 mW
    500,  // 27 dBm ≈ 500 mW
    1000, // 30 dBm = 1000 mW = 1 W
    2000, // 33 dBm ≈ 2000 mW = 2 W
    5000  // 37 dBm ≈ 5000 mW = 5 W
};

//Метки диапазонов (в метрах)
static const char* band_labels[] = {
    "160", "80", "40", "30", "20", "17", "15", "12", "10"
};

//Вспомогательная: маска → строка "20,15,10"
uint16_t bands_mask_to_str(uint16_t mask, char *buf, uint16_t buf_size)
{
    if (buf == NULL || buf_size == 0) return 0;

    buf[0] = '\0';
    char *p = buf;
    size_t remain = buf_size;
    bool first = true;

    for (int i = 0; i < 9; i++) {
        if (mask & (1U << i)) {
            int n;
            if (first) {
                n = snprintf(p, remain, "%s", band_labels[i]);
                first = false;
            } else {
                n = snprintf(p, remain, ",%s", band_labels[i]);
            }
            if (n < 0 || (size_t)n >= remain) break;
            p += n;
            remain -= n;
        }
    }

    return (uint16_t)(p - buf);
}

//Вспомогательная: строка "20,15,10" → маска
uint16_t bands_str_to_mask(const char *str, bool *ok)
{
    if (ok) *ok = false;
    if (str == NULL) return 0;

    uint16_t mask = 0;
    int valid_count = 0;
    int total_count = 0;

    char buf[64];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *token = strtok(buf, ", ");
    while (token) {
        total_count++;
        int val = atoi(token);
        bool found = false;
        switch (val) {
            case 160: mask |= BAND_160M; found = true; break;
            case 80:  mask |= BAND_80M;  found = true; break;
            case 40:  mask |= BAND_40M;  found = true; break;
            case 30:  mask |= BAND_30M;  found = true; break;
            case 20:  mask |= BAND_20M;  found = true; break;
            case 17:  mask |= BAND_17M;  found = true; break;
            case 15:  mask |= BAND_15M;  found = true; break;
            case 12:  mask |= BAND_12M;  found = true; break;
            case 10:  mask |= BAND_10M;  found = true; break;
            default: found = false;
        }
        if (found) {
            valid_count++;
        } else {
            // Невалидный диапазон — выходим сразу
            if (ok) *ok = false;
            return 0;
        }
        token = strtok(NULL, ", ");
    }

    if (ok) *ok = (total_count > 0 && valid_count == total_count);
    return mask;
}

//Загрузка из Flash
int SETTINGS_Read_from_FLASH(wspr_settings_t *s)
{
    const wspr_settings_t *flash = (const wspr_settings_t *)SETTINGS_FLASH_ADDR;

    if (flash->magic != SETTINGS_MAGIC)
    {
        return -1; //Не инициализировано
    }

    memcpy(s, flash, sizeof(wspr_settings_t));
    return 0;
}

//Сохранение во Flash
int SETTINGS_Write_to_FLASH(const wspr_settings_t *s)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0;

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = SETTINGS_FLASH_ADDR;
    erase.NbPages = 1;

    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return -1;
    }

    const uint16_t *src = (const uint16_t *)s;
    uint32_t addr = SETTINGS_FLASH_ADDR;
    uint32_t words = (sizeof(wspr_settings_t) + 1) / 2; //16-битные слова

    for (uint32_t i = 0; i < words; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i * 2, (uint64_t)src[i]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return -1;
        }
    }

    HAL_FLASH_Lock();
    return 0;
}

//Настройки по умолчанию
void SETTINGS_Set_Default(wspr_settings_t *s)
{
    s->magic = SETTINGS_MAGIC;									//Идентификатор для FLESH
    s->version = FIRMWARE_VERSION;								//Текущая версия прошивки

    strncpy(s->callsign, "R9OFG", CALLSIGN_LEN);				//Позывной
    s->callsign[CALLSIGN_LEN] = '\0';							//---

    s->band_mask = BAND_20M;									//Только 20m
    s->power_index = 3;											//10 dBm
    s->xtal_freq_hz = 25000000;					 				//25 MHz

    strncpy(s->gps_line_header, "$GPRMC", GPS_LINE_HEADER_LEN);	//Позывной
    s->gps_line_header[GPS_LINE_HEADER_LEN] = '\0';				//---

    strcpy(s->ext_pwr, "HIGH");									//HIGH при передаче
    strcpy(s->ext_bpf, "LOW");									//LOW при коде 0000

    s->reserved1 = 0;
    s->reserved2 = 0;
}

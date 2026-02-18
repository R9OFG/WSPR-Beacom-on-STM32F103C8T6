/*
 * ext.c
 *
 * Created on: Dec 1, 2025
 * Author: R9OFG.RU
 */

#include "ext.h"

/* Управляющий сигнал внешнего усилителя мощности, пин PA8*/
void EXT_PWR(bool cmd)
{
    if (g_settings.ext_pwr[0] == 'H') //"HIGH"
    {
        if (cmd)
        {
            EXT_PWR_PORT->BSRR = EXT_PWR_PIN; //SET
        }
        else
        {
            EXT_PWR_PORT->BSRR = (uint32_t)EXT_PWR_PIN << 16; //RESET
        }
    }
    else if (g_settings.ext_pwr[0] == 'L') //"LOW"
    {
        if (cmd)
        {
            EXT_PWR_PORT->BSRR = (uint32_t)EXT_PWR_PIN << 16; //RESET
        }
        else
        {
            EXT_PWR_PORT->BSRR = EXT_PWR_PIN; //SET
        }
    }
}

/* Управление внешними ФНЧ на 5 поддиапазонов через SN74LS145D */
void EXT_BPF(uint32_t freq)
{
    uint8_t code = 0;
    if (freq <= 2499999)
    {
        code = 0;   // 0000
    } else if (freq <= 4499999)
    {
        code = 8;   // 1000 → A
    } else if (freq <= 7999999)
    {
        code = 4;   // 0100 → B
    } else if (freq <= 15999999)
    {
        code = 12;  // 1100 → A+B
    } else if (freq <= 30000000)
    {
        code = 2;   // 0010 → C
    }

    //Инвертируем код, если режим LOW
    if (strcmp(g_settings.ext_bpf, "LOW") == 0)
    {
        code = (~code) & 0x0F; //Инвертируем только 4 младших бита
    }

    //Формируем маску пинов (всегда как в HIGH)
    uint16_t pins = 0;

    if (code & 8) pins |= EXT_BPF_A_PIN; //A
    if (code & 4) pins |= EXT_BPF_B_PIN; //B
    if (code & 2) pins |= EXT_BPF_C_PIN; //C
    if (code & 1) pins |= EXT_BPF_D_PIN; //D

    const uint16_t MASK = EXT_BPF_A_PIN | EXT_BPF_B_PIN | EXT_BPF_C_PIN | EXT_BPF_D_PIN;

    //Всегда устанавливаем как в HIGH-режиме
    //Cбросить всё в 0
    EXT_BPF_PORT->BSRR = (uint32_t)MASK << 16;
    //Установить нужные в 1
    EXT_BPF_PORT->BSRR = pins;
}

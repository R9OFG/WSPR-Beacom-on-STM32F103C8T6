/*
 * si5351a.h
 *
 * Created:		11.03.2025
 * Author:		HTTPS://R9OFG.RU
 *
 * Release:		1.0
 *
 * Modified:	01.12.2025
 *
 */

#ifndef SI5351A_H
#define SI5351A_H

#include "main.h"
#include "stdbool.h"

#define SI_I2C_ADDR				0x60			//Адрес на шине I2C

#define SI_CLKX_DISABLE			3				//Глобальное управление выходами
#define SI_CLK0_CTRL			16				//Параметры CLK0
#define SI_CLK1_CTRL			17				//Параметры CLK1
#define SI_CLK2_CTRL			18				//Параметры CLK2
#define SI_SYNTH_PLL_A			26				//Параметры PLLA
#define SI_SYNTH_PLL_B			34				//Параметры PLLB
#define SI_SYNTH_MS_0			42				//Параметры Multisynth0
#define SI_SYNTH_MS_1			50				//Параметры Multisynth1
#define SI_SYNTH_MS_2			58				//Параметры Multisynth2

#define SI_R_DIV_1				0x00			//R-делители - 1
#define SI_R_DIV_2				0x10			//2
#define SI_R_DIV_4				0x20			//4
#define SI_R_DIV_8				0x30			//8
#define SI_R_DIV_16				0x40			//16
#define SI_R_DIV_32				0x50			//32
#define SI_R_DIV_64				0x60			//64
#define SI_R_DIV_128			0x70			//128

#define SI_DRV_2MA				0x4C			//Ток драйвера 2мА
#define SI_DRV_4MA				0x4D			//Ток драйвера 4мА
#define SI_DRV_6MA				0x4E			//Ток драйвера 6мА
#define SI_DRV_8MA				0x4F			//Ток драйвера 8мА

#define SI_CRYSTAL_LOAD			183				//Внутренние нагрузочные конденсаторы
#define SI_CRYSTAL_LOAD_6PF		0x40			//6pF
#define SI_CRYSTAL_LOAD_8PF		0x80			//8Pf
#define SI_CRYSTAL_LOAD_10PF	0xC0			//10pF

#define SI_CLKX_OFF				0x80			//Выключение выхода
#define SI_XTL_SRC_PLL			15				//Источник тактирования PLL
#define SI_XTL_SRC_PLL_AB		0x00			//Кварцевый резонатор для обоих PLL
#define SI_CLK_SRC_PLL_A		0x00			//PLLA для тактирования CLKx
#define SI_CLK_SRC_PLL_B		0x20			//PLLB для тактирования CLKx
#define SI_PLL_RST				177				//Сброс PLL
#define SI_PLL_ALL_RST			0xA0			//Сброс обоих PLL
#define SI_PLL_A_RST			0x20			//Сброс PLLA
#define SI_PLL_B_RST			0x80			//Сброс PLLB
#define SI_MULTIPLIER			1048575			//Множитель
#define SI_PLL_VCO_MIN			600000000		//Минимальная частота PLL, Hz
#define SI_PLL_VCO_MAX			900000000		//Максимальная частота PLL, Hz
#define SI_PLL_BOUNDS_20		20000000		//Первая граница для автовыбора частоты PLL
#define SI_PLL_BOUNDS_80		80000000		//Вторая граница для автовыбора частоты PLL

#define SI_XTALL_FREQ_DEF		25000000		//Частота кварцевого резонатора по умолчанию
#define SI_CLK_DRV_DEF			SI_DRV_8MA		//Ток драйвера по умолчанию
#define SI_FREQ_PLLA_DEF		SI_PLL_VCO_MIN	//Частота PLLA по умолчанию
#define SI_FREQ_PLLB_DEF		SI_PLL_VCO_MIN	//Частота PLLB по умолчанию

extern uint8_t SiCLKDrv_Array[4];
extern bool flagSI5351A;
extern int8_t currentSiDrvIdx;
extern uint32_t shiftFreq;

void SI5351A_OutOff(uint8_t clk);
void SI5351A_SetDrvClk(uint8_t clk, uint8_t drv);
void SI5351_Init(void);
void SI5351A_SetFreq(uint8_t clk, uint32_t siXtallFeq, uint32_t freq);

#endif /* SI5351A_H */

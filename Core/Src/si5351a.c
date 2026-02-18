/*
 * si5351a.c
 *
 * Created:		11.03.2025
 * Author:		HTTPS://R9OFG.RU
 *
 * Release:		1.0
 *
 * Modified:	01.12.2025
 *
 */

#include "si5351a.h"

extern I2C_HandleTypeDef hi2c1;

uint8_t SiCLKDrv_Array[4] 	= {SI_DRV_2MA,			//Массив допустимых значений тока драйвера (в порядке возрастания)
							   SI_DRV_4MA,
							   SI_DRV_6MA,
							   SI_DRV_8MA};

bool flagSI5351A			= false;				//Флаг наличия устройства на шине I2C
int8_t currentSiDrvIdx		= 3;					//Текущий индекс значения тока драйвера SI5351

/*
 * Записываем 8-битное значение регистра в SI5351A по шине I2C
 * с защитой от зависания и перезапуском I2C
 */
HAL_StatusTypeDef SI5351A_WriteData(uint8_t reg, uint8_t value)
{
    HAL_StatusTypeDef status;
    uint8_t retries = 3;

    while (retries--)
    {
        status = HAL_I2C_IsDeviceReady(&hi2c1, (SI_I2C_ADDR << 1), 3, 10);
        if (status == HAL_OK)
        {
            status = HAL_I2C_Mem_Write(&hi2c1, SI_I2C_ADDR<<1,
                                       reg, I2C_MEMADD_SIZE_8BIT,
                                       &value, 1, 10);
            if (status == HAL_OK) return HAL_OK;
        }

        //Если ошибка, перезапускаем I2C
        __HAL_RCC_I2C2_FORCE_RESET();
        __HAL_RCC_I2C2_RELEASE_RESET();
        HAL_Delay(1);
    }

    return status; //Если все попытки неудачны
}

/*
 * Настройки для PLL_A/B с mult, num и denom
 * mult в диапазоне 15..90
 * num в диапазоне 0..1,048,575 (0xFFFFF)
 * denom в диапазоне 0..1,048,575 (0xFFFFF)
 */
void SI5351A_SetupPLL(uint8_t pll, uint8_t mult, uint32_t num, uint32_t denom)
{
	//Проверка диапазона mult
	if(mult < 15) mult = 15;
	if(mult > 90) mult = 90;

	uint32_t frac = (uint32_t)((((uint64_t)num) << 7) / denom); //Замена floor(128*num/denom)

	uint32_t P1 = 128 * mult + frac - 512;
	uint32_t P2 = ((uint64_t)num << 7) - (uint64_t)denom * frac;
	uint32_t P3 = denom;

	//Запись в регистры SI5351A
	SI5351A_WriteData(pll + 0, (P3 & 0x0000FF00) >> 8);
	SI5351A_WriteData(pll + 1, (P3 & 0x000000FF));
	SI5351A_WriteData(pll + 2, (P1 & 0x00030000) >> 16);
	SI5351A_WriteData(pll + 3, (P1 & 0x0000FF00) >> 8);
	SI5351A_WriteData(pll + 4, (P1 & 0x000000FF));
	SI5351A_WriteData(pll + 5, ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
	SI5351A_WriteData(pll + 6, (P2 & 0x0000FF00) >> 8);
	SI5351A_WriteData(pll + 7, (P2 & 0x000000FF));
}

/*
 * Установка MultiSynth с целочисленным делителем и R-делителем
 * R-делитель - это битовое значение, которое помещается в соответствующий регистр, см. #define в si5351a.h
 */
void SI5351A_SetupMultisynth(uint8_t synth, uint32_t divider, uint8_t rDiv)
{
	uint32_t P1 = 128 * divider - 512;
	uint32_t P2 = 0;
	uint32_t P3 = 1;

	//Запись в регистры SI5351A
	SI5351A_WriteData(synth + 0, (P3 & 0x0000FF00) >> 8);
	SI5351A_WriteData(synth + 1, (P3 & 0x000000FF));
	SI5351A_WriteData(synth + 2, ((P1 & 0x00030000) >> 16) | rDiv);
	SI5351A_WriteData(synth + 3, (P1 & 0x0000FF00) >> 8);
	SI5351A_WriteData(synth + 4, (P1 & 0x000000FF));
	SI5351A_WriteData(synth + 5, ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
	SI5351A_WriteData(synth + 6, (P2 & 0x0000FF00) >> 8);
	SI5351A_WriteData(synth + 7, (P2 & 0x000000FF));
}

/*
 * Выключение указанного выхода, параметр - выход CLK0/CLK1/CLK2
 */
void SI5351A_OutOff(uint8_t clk)
{
	SI5351A_WriteData(clk, SI_CLKX_OFF);
}

/*
 * Включение указанного выхода, параметры - выход CLK0/CLK1/CLK2 и выбор силы тока драйвера и PLL_A/B
 */
void SI5351A_SetDrvClk(uint8_t clk, uint8_t drv)
{
	if (clk == SI_CLK0_CTRL) {SI5351A_WriteData(clk, drv | SI_CLK_SRC_PLL_A);}		//CLK0 от PLLA
	else if (clk == SI_CLK2_CTRL) {SI5351A_WriteData(clk, drv | SI_CLK_SRC_PLL_B);}	//CLK2 от PLLB
}

/*
 * Сброс указанного PLL_A/B, параметр - PLL_A/B
 */
void SI5351A_ResetPLLx(uint8_t pll)
{
	if (pll == SI_SYNTH_PLL_A) {SI5351A_WriteData(SI_PLL_RST, SI_PLL_A_RST);}		//Сброс PLLA
	else if (pll == SI_SYNTH_PLL_B) {SI5351A_WriteData(SI_PLL_RST, SI_PLL_B_RST);}	//Сброс PLLB
}

/*
 * Инициализация генератора
 */
void SI5351_Init(void)
{
	if (!flagSI5351A) return; //Если устройство не найдено на шине

	/*
	 * Запрещаем все выходы, значения:
	 * 0xFF - все запрещены
	 * 0xFE - разрешен только CLK0
	 * 0xFC - разрешены только CLK0/CLK1
	 * 0xF8 - разрешены только CLK0/CLK1/CLK2
	 */
	SI5351A_WriteData(SI_CLKX_DISABLE, 0xFF);

	//Отключаем все три выхода
	SI5351A_OutOff(SI_CLK0_CTRL);
	SI5351A_OutOff(SI_CLK1_CTRL);
	SI5351A_OutOff(SI_CLK2_CTRL);

	//Источник опорной частоты — кварц для обеих PLL_A/B, для SI5351A (10-MSOP) не актуально
	SI5351A_WriteData(SI_XTL_SRC_PLL, SI_XTL_SRC_PLL_AB);

	/*
	 * Нагрузка кварцевого резонатора внутренними конденсаторами,
	 * чем точнее выбрана суммарная емкость внутренних и внешних конденсаторов,
	 * тем точнее генерация частоты, либо корректируем выходную частоту,
	 * значением кварцевого резонатора - SI_XTALL_FREQ_DEF
	 */
	SI5351A_WriteData(SI_CRYSTAL_LOAD, SI_CRYSTAL_LOAD_10PF);

	//Сбрасываем оба PLL
	SI5351A_WriteData(SI_PLL_RST, SI_PLL_ALL_RST);

	//Разрешаем выход CLK0
	SI5351A_WriteData(SI_CLKX_DISABLE, 0xFE);
}

/*
 * Генерация заданной частоты,
 * задаваемая частота должна находится в диапазоне от 1 МГц до 150 МГц
 */
void SI5351A_SetFreq(uint8_t clk, uint32_t siXtallFeq, uint32_t freq)
{
	if (!flagSI5351A) return; //Если устройство не найдено на шине

	uint32_t l;
	float    f;
	uint8_t  mult = 0;
	uint32_t num = 0;
	uint32_t denom;
	uint32_t divider;
	uint32_t pllFreq;

	//Автовыбор оптимальной частоты PLL
	if (freq >= SI_PLL_BOUNDS_80)
	{
		//80 МГц и выше, SI5351A генерирует ВЧ
		pllFreq = SI_PLL_VCO_MIN;	//Минимизируем divider, лучше стабильность
	}
	else if (freq >= SI_PLL_BOUNDS_20)
	{
		//20–80 МГц, максимизируем чистоту PLL, ниже фазовый шум
		pllFreq = SI_PLL_VCO_MAX;
	}
	else
	{
		//Ниже 20 МГц, большой divider
		pllFreq = SI_PLL_VCO_MIN;	//Уменьшаем divider, лучше точность и стабильность
	}

	//Ррасчет коэффициента деления
	divider = pllFreq / freq;

	//Целочисленное деление
	if (divider % 2) divider--;

	//Расчет частоты PLL
	pllFreq = divider * freq;

	/*
	 * Расчет множителя для получения заданной pllFreq
	 * состоит из трех частей:
	 * 1: mult - целое число, которое должно быть в диапазоне 15..90;
	 * 2: num и denom - дробные части, числитель и знаменатель,
	 * каждый из них 20 bits (диапазон 0..1048575);
	 * 3: фактический множитель mult + num / denom,
	 * для простоты знаменатель установлен на максимум = 1048575
	 */

	mult = pllFreq / siXtallFeq;
	l = pllFreq % siXtallFeq;
	f = l;
	f *= SI_MULTIPLIER;
	f /= siXtallFeq;
	num = f;
	denom = SI_MULTIPLIER;

	//Настройка PLL-A/B с рассчитанным коэффициентом умножения
	if (clk == SI_CLK0_CTRL) {SI5351A_SetupPLL(SI_SYNTH_PLL_A, mult, num, denom); SI5351A_ResetPLLx(SI_SYNTH_PLL_A);}
	else if (clk == SI_CLK2_CTRL) {SI5351A_SetupPLL(SI_SYNTH_PLL_B, mult, num, denom); SI5351A_ResetPLLx(SI_SYNTH_PLL_B);}

	/*
	 * Установка делителя на выбранном MultiSynth с рассчитанным делителем,
	 * последний этап деления R может делиться на степень 2, начиная с 1..128
	 * представлен константами SI_R_DIV1....SI_R_DIV128,
	 * если необходимо вывести частоты ниже 1 МГц, вы должны использовать SI_R_DIV128
	 */

	//Устанавка заданной частоты на заданном выходе
	if (clk == SI_CLK0_CTRL) {SI5351A_SetupMultisynth(SI_SYNTH_MS_0, divider, SI_R_DIV_1);}
	else if (clk == SI_CLK2_CTRL) {SI5351A_SetupMultisynth(SI_SYNTH_MS_2, divider, SI_R_DIV_1);}

	//Сброс текщего PLL, при значительной смене частоты, например при переключении диапазонов
	/*if (ChangeBand)
	{
		if (clk == SI_CLK0_CTRL) {SI5351A_ResetPLLx(SI_SYNTH_PLL_A);}
		else if (clk == SI_CLK2_CTRL) {SI5351A_ResetPLLx(SI_SYNTH_PLL_B);}
	}*/
}

/*
 * i2c.c
 *
 * Created:		11.03.2025
 * Author:		HTTPS://R9OFG.RU
 *
 * Release:		1.0
 *
 * Modified:	01.12.2025
 *
 */

#include "i2c.h"
#include "led_pcb.h"

extern I2C_HandleTypeDef hi2c1;

//Функция сканирования шины I2C
void I2C_Scan(void)
{
    uint8_t i;
    uint8_t y = 1;
    HAL_StatusTypeDef result;
    uint8_t deviceCount = 0;

    //Сбрасываем флаг
    flagSI5351A = false;

    //Проверяем все возможные адреса (0x01 - 0x7F)
    for (i = 1; i < 128; i++)
    {
        //Попытка отправить сигнал по адресу
        result = HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(i << 1), 1, 100);

        if (result == HAL_OK)
        {
            //Проверяем найдено ли устройство SI5351A
			if(i == SI_I2C_ADDR)
			{
				//Устройство найдено, поднимаем флаг
				flagSI5351A = true;
			}

            deviceCount++;
            y++;
        }
    }
}

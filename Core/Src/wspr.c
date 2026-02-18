/*
 * wspr.c
 *
 * Created on: Nov 29, 2025
 * Author: R9OFG.RU
 */

#include "wspr.h"

//Тестовое кодированное WSPR сообщение "K1ABC FN42 37" в бит последовательности для 4-FSK
/*static const uint8_t test_sequence[162] = {
	3,3,0,0,2,0,0,0,1,0,2,0,1,3,1,2,2,2,1,0,0,3,2,3,1,3,3,2,2,0,
	2,0,0,0,3,2,0,1,2,3,2,2,0,0,2,2,3,2,1,1,0,2,3,3,2,1,0,2,2,1,
	3,2,1,2,2,2,0,3,3,0,3,0,3,0,1,2,1,0,2,1,2,0,3,2,1,3,2,0,0,3,
	3,2,3,0,3,2,2,0,3,0,2,0,2,0,1,0,2,3,0,2,1,1,1,2,3,3,0,2,3,1,
	2,1,2,2,2,1,3,3,2,0,0,0,0,1,0,3,2,0,1,3,2,2,2,2,2,0,2,3,3,2,
	3,2,3,3,2,0,0,3,1,2,2,2
};*/

const char sym_calsign[36] = {
    '0','1','2','3','4','5','6','7','8','9',
    'A','B','C','D','E','F','G','H','I','J',
    'K','L','M','N','O','P','Q','R','S','T',
    'U','V','W','X','Y','Z'
};

const char sym_qth_locator[18] = {
    'A','B','C','D','E','F','G','H','I','J','K',
    'L','M','N','O','P','Q','R'
};

const uint32_t poly[2] = {0xF2D05351, 0xE4613C47};

const uint8_t syn_vector[162] = {
    1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,1,0,0,1,0,1,1,1,1,0,0,0,
    0,0,0,0,1,0,0,1,0,1,0,0,0,0,0,0,1,0,1,1,0,0,1,1,0,1,0,0,0,1,
    1,0,1,0,0,0,0,1,1,0,1,0,1,0,1,0,1,0,0,1,0,0,1,0,1,1,0,0,0,1,
    1,0,1,0,1,0,0,0,1,0,0,0,0,0,1,0,0,1,0,0,1,1,1,0,1,1,0,0,1,1,
    0,1,0,0,0,1,1,1,0,0,0,0,0,1,0,1,0,0,1,1,0,0,0,0,0,0,0,1,1,0,
    1,0,1,1,0,0,0,1,1,0,0,0
};

const uint32_t wspr_frequencies_hz[9] = {
    1836600,   //1.8366 MHz
    3568600,   //3.5686 MHz
    7038600,   //7.0386 MHz
    10138700,  //10.1387 MHz
    14095600,  //14.0956 MHz
    18104600,  //18.1046 MHz
    21094600,  //21.0946 MHz
    24924600,  //24.9246 MHz
    28124600   //28.1246 MHz
};

wspr_state_t wspr_state = WSPR_STATE_IDLE;

volatile bool flag_wspr_on = false;				//Флаг рабочего режима
volatile uint16_t freq_offset = 1500;			//Частота смещения в плюс от базовой частоты диапазона
static uint8_t wspr_symbols[162];				//162 символа WSPR (символы канала)
static bool flag_wspr_message_ready = false;	//Флаг готовности WSPR сообщения
static int current_band = 0;					//Текущий диапазон для WSPR передачи
static uint32_t rand_state = 0;					//Для генератора псевдослучайных чисел

//Вспомогательная функция: генератор псевдослучайных чисел
static uint16_t is_random(void)
{
    if (rand_state == 0)
    {
        rand_state = HAL_GetTick() + 1; //Инициализация
    }
    rand_state = rand_state * 1664525UL + 1013904223UL;
    return (uint16_t)(rand_state >> 16);
}

//Вспомогательная функция: возвращает случайное смещение в диапазоне 1400...1600
uint16_t get_random_offset(void)
{
    return 1400 + (is_random() % 201); //201 значений в диапазоне 1400..1600
}

//Вспомогательная функция: проверяет условие, что до цифры в позывном — 1 символ?
bool is_prefix_1_char(const char *callsign)
{
	if (!callsign) return false;
	int len = strlen(callsign);
	if (len < 3) return false;

	//Ищем первую цифру
	for (int i = 0; i < len; i++)
	{
		if (callsign[i] >= '0' && callsign[i] <= '9')
		{
			return (i == 1); //Цифра на позиции 1 → до неё 1 символ
		}
	}
	return false; //Цифры нет → обычный позывной
}

//Вспомогательная функция: получить название диапазона
const char* get_band_name(int band)
{
    switch (band) {
        case 0: return "160m";
        case 1: return "80m";
        case 2: return "40m";
        case 3: return "30m";
        case 4: return "20m";
        case 5: return "17m";
        case 6: return "15m";
        case 7: return "12m";
        case 8: return "10m";
        default: return "???";
    }
}

//Вспомогательная функция: найти следующий активный диапазон
int find_next_active_band(int start)
{
    for (int i = 0; i < 9; i++)
    {
        int band = (start + 1 + i) % 9;
        if (g_settings.band_mask & (1U << band))
        {
            return band;
        }
    }
    return -1;
}

//Отправка строки в VCP
static void WSPR_VCP_Send_Str(const char *str)
{
    if (!str) return;
    uint16_t len = strlen(str);
    if (len == 0) return;
    CDC_Transmit_FS((uint8_t*)str, len);
}

//Отправка линии в VCP
static void WSPR_VCP_Send_Line(const char *str)
{
    WSPR_VCP_Send_Str(str);
    WSPR_VCP_Send_Str("\r\n");
}

//Генерация WSPR сообщения
void WSPR_Generate_Message(const char *callsign, const char *locator, int8_t power_dbm)
{
	//Массив бит упакованных N и M
	char Bit_Packed[11];

	//Массивы для сверточного кодирования
	char Conv_Encoded[176];
	long Poly[2] = {0xF2D05351, 0xE4613C47};

	//Массив для чередования
	char Data_Symbols[162];

	//Переменные
	int N, M, i, loc1, loc2, p, r, k ,j, l;
	char Even;
	long n2, n3;

	/*
	 * 1.Кодирование позывного в N
	 * n1:= ch1;
	 * n2:= n1 * 36 + ch2;
	 * n3:= n2 * 10 + ch3;
	 * n4:= 27 * n3 + (ch4 - 10);
	 * n5:= 27 * n4 + (ch5 - 10);
	 * n6:= 27 * n5 + (ch6 - 10);
	 */

	/*
	 * Если до цифры в позывном 1 символ то n1 = 36 иначе n1 = порядковый номер символа
	 * из массива sym_calsign
	 */
	if (is_prefix_1_char(callsign))
	{
		//n1
		N = 36;
		//n2
		i = 10;
		while (callsign[0] != sym_calsign[i]) i++;
		N = N * 36 + i;
		//n3
		i = 0;
		while (callsign[1] != sym_calsign[i]) i++;
		N = N * 10 + i;
		//n4
		i = 10;
		while (callsign[2] != sym_calsign[i]) i++;
		N = 27 * N + i - 10;
		//n5
		i = 10;
		while (callsign[3] != sym_calsign[i]) i++;
		N = 27 * N + i - 10;
		//n6
		if (strlen(callsign) == 5)
		{
			i = 10;
			while (callsign[4] != sym_calsign[i]) i++;
			N = 27 * N + i - 10;
		}
		else
			N = 27 * N + 36 - 10;
	}
	else
	{
		//n1
		i = 10;
		while (callsign[0] != sym_calsign[i]) i++;
		N = i;
		//n2
		i = 10;
		while (callsign[1] != sym_calsign[i]) i++;
		N = N * 36 + i;
		//n3
		i = 0;
		while (callsign[2] != sym_calsign[i]) i++;
		N = N * 10 + i;
		//n4
		i = 10;
		while (callsign[3] != sym_calsign[i]) i++;
		N = 27 * N + i - 10;
		//n5
		i = 10;
		while (callsign[4] != sym_calsign[i]) i++;
		N = 27 * N + i - 10;
		//n6
		if (strlen(callsign) == 6)
		{
			i = 10;
			while (callsign[5] != sym_calsign[i]) i++;
			N = 27 * N + i - 10;
		}
		else
			N = 27 * N + 36 - 10;
	}

	/*
	 * 2.Кодирование QTH локатора в m1
	 * m1 = (179-10*[loc1]-[loc3])*180+10*[loc2]+[loc4]
	 * loc1 и loc2 порядковый номер символа из массива sym_qth_locator
	 * loc3 и loc4 непосредственно указанная цифра в локаторе
	 */

	//loc1
	i = 0;
	while (locator[0] != sym_qth_locator[i]) {i++;}
	loc1 = i;

	//loc2
	i = 0;
	while (locator[1] != sym_qth_locator[i]) {i++;}
	loc2 = i;

	M = (179 - 10 * loc1 - (locator[2] - '0')) * 180 + 10 * loc2 + (locator[3] - '0');

	/*
	 * 3.Кодирование уровня выходной мощности в M
	 * M = m1 * 128 + [PwrValue] + 64
	 * где PwrValue значение уровня выходной мощности в dBm
	 */

	switch (power_dbm)
	{
		case 0:  M = M * 128 + 0  + 64; break;
		case 3:  M = M * 128 + 3  + 64; break;
		case 7:  M = M * 128 + 7  + 64; break;
		case 10: M = M * 128 + 10 + 64; break;
		case 13: M = M * 128 + 13 + 64; break;
		case 17: M = M * 128 + 17 + 64; break;
		case 20: M = M * 128 + 20 + 64; break;
		case 23: M = M * 128 + 23 + 64; break;
		case 27: M = M * 128 + 27 + 64; break;
		case 30: M = M * 128 + 30 + 64; break;
		case 33: M = M * 128 + 33 + 64; break;
		case 37: M = M * 128 + 37 + 64; break;
	};

	/*
	 * 4.Бит упаковка N и M в массив Bit_Packed
	 * заполнение массива Bit_Packed путем сдвига битов
	 */
	Bit_Packed[0]  = (N >> 20);
	Bit_Packed[1]  = (N >> 12);
	Bit_Packed[2]  = (N >> 4);
	//В 4 байте размещаем остаток информации от N и начало информации от M
	Bit_Packed[3]  = (((N & 15) << 4) | ((M >> 18) & 15));
	Bit_Packed[4]  = (M >> 10);
	Bit_Packed[5]  = (M >> 2);
	Bit_Packed[6]  = ((M & 3) << 6);
	//Следуюшщие 4 байта пустые
	Bit_Packed[7]  = 0;
	Bit_Packed[8]  = 0;
	Bit_Packed[9]  = 0;
	Bit_Packed[10] = 0;

	/*
	 * 5.Сверточное кодирование (Convolutional Encoding)
	 * WSPR сообщения для K=32, r=1/2 кода
	 * бит упакованные N & M = 259047992 & 2896997 = $F70C238B0D1940
	 */

	n2 = 0;
	k = 0;
	for (j = 0; j < sizeof(Bit_Packed); j++)
	{
		for (i = 7; i >= 0; i--)
		{
			n2 = ((n2 << 1) | ((Bit_Packed[j] >> i) & (long)1));
			for (p = 0; p < 2; p++)
			{
				n3 = n2 & Poly[p];
				Even = 0;
				for (r = 0; r < 32; r++)
				{
					if (((n3 >> r) & 1) != 0)
						Even = (1 - Even);
				};
				Conv_Encoded[k] = Even;
				k++;
			};
		};
	};

	/*
	 * 6.Чередование (Interleaving)
	 */

	p = 0;
	while (p < 162)
	{
		for (k = 0; k <= 255; k++)
		{
			i = k;
			j = 0;
			for (l = 7; l >= 0; l--)
			{
				j = j | (i & 0x01) << l;
				i = i >> 1;
			}
			if (j < 162)
				Data_Symbols[j] = Conv_Encoded[p++];
		}
	}

	/*
	 * 7.Слияние с вектором синхронизации
	 */

	for (i = 0; i < 162; i++) {wspr_symbols[i] = (syn_vector[i] + 2 * Data_Symbols[i]);}

	//WSPR сообщение готово!
	flag_wspr_message_ready = true;
}

/* Задача WSPR */
void WSPR_Task(void)
{
    switch (wspr_state)
    {
        case WSPR_STATE_IDLE:
            //Найти первый активный диапазон
            current_band = -1;
            for (int i = 0; i < 9; i++)
            {
                if (g_settings.band_mask & (1U << i))
                {
                    current_band = i;
                    break;
                }
            }
            if (current_band >= 0)
            {
                wspr_state = WSPR_STATE_WAIT_NEXT;
            }
            else
            {
            	//Сброс времени
				g_gps.hour = g_gps.min = g_gps.sec = 61;
				//Отключаем внешний PA
				EXT_PWR(false);
				//Отключаем CLK0 SI5351
				SI5351A_OutOff(SI_CLK0_CTRL);
				//Включаем верхний ФНЧ/ДПФ в соответствии с настройками
				EXT_BPF(30000000);
				//Отключаем индикатор передачи
				LED_OFF();
                flag_wspr_on = false;
                wspr_state = WSPR_STATE_IDLE;
                return;
            }
            break;

        case WSPR_STATE_WAIT_NEXT:
            if (GPS_Has_Fix() && (g_gps.min % 2 == 0) && (g_gps.sec <= 2))
            {
                //Генерируем сообщение с актуальным локатором
                WSPR_Generate_Message(
                    g_settings.callsign,
                    g_gps.locator,
                    power_dbm_table[g_settings.power_index]
                );

                //Определяем стартовую частоту диапазона
                uint32_t base_freq = wspr_frequencies_hz[current_band];
                //Определяем смещение в окне 1400...1600
                freq_offset = get_random_offset();
                //Включаем необходимый ФНЧ/ДПФ
                EXT_BPF(base_freq);
                //Устанавливаем выбранную частоту
                SI5351A_SetFreq(SI_CLK0_CTRL, g_settings.xtal_freq_hz, base_freq + freq_offset);
                //Включаем CLK0 SI5351
                SI5351A_SetDrvClk(SI_CLK0_CTRL, SiCLKDrv_Array[currentSiDrvIdx]);
                //Включаем внешний PA
                EXT_PWR(true);

                //Меняем статус
                wspr_state = WSPR_STATE_TRANSMITTING;
            }
            break;

        case WSPR_STATE_TRANSMITTING:
            if (current_band >= 0 && current_band < 9)
            {
                const char* band_name = get_band_name(current_band);

                //СТАРТ
                {
                	//Отправляем информацию в VCP
                    uint8_t h = g_gps.hour, m = g_gps.min, s = g_gps.sec;
                    char log_buf[80];
                    snprintf(log_buf, sizeof(log_buf),
                        "WSPR TX: %02d:%02d:%02d | %s | START\r\n",
                        h, m, s, band_name
                    );
                    WSPR_VCP_Send_Line(log_buf);
                    //Включаем индикатор передачи
                	LED_ON();
                }

                //ПЕРЕДАЧА
                for (int i = 0; i < 162; i++)
                {
                	//Перебор тестовой последовательности
					//uint32_t tone_freq = wspr_frequencies_hz[current_band] + freq_offset + (test_sequence[i] * 2);

					/*
					 * Перебор сгенерированной последовательности - рабочий вариант.
					 * По спецификации расстояние между тонами 1,48Гц, но SI5351 принимает значение частоты как целое число в Гц
					 * потому округлил до 2, т.е. последовательность у нас 0 или 1 или 2 или 3, значит
					 * смещаем на 0*2 или 1*2 или 2*2 или 3*2, максимальное смещение = 6, укладываемся в спецификацию!
					 */
                    uint32_t tone_freq = wspr_frequencies_hz[current_band] + freq_offset + (wspr_symbols[i] * 2);
                    SI5351A_SetFreq(SI_CLK0_CTRL, g_settings.xtal_freq_hz, tone_freq);

                    //Задержка между символами
                    HAL_Delay(683);
                }

                //КОНЕЦ
                {
                	//Сброс времени
                    g_gps.hour = g_gps.min = g_gps.sec = 61;
                    //Отключаем внешний PA
                    EXT_PWR(false);
                    //Отключаем CLK0 SI5351
                    SI5351A_OutOff(SI_CLK0_CTRL);
                    //Включаем верхний ФНЧ/ДПФ в соответствии с настройками
                    EXT_BPF(30000000);
                    //Отключаем индикатор передачи
                    LED_OFF();
                    //ОТправляем сообщение в VCP
                    char log_buf[64];
                    snprintf(log_buf, sizeof(log_buf), "WSPR TX: %s | END\r\n", band_name);
                    WSPR_VCP_Send_Line(log_buf);
                }

                //Переходим к следующему активному диапазону (по кругу)
                int next_band = -1;
                for (int i = 0; i < 9; i++)
                {
                    int band = (current_band + 1 + i) % 9;
                    if (g_settings.band_mask & (1U << band))
                    {
                        next_band = band;
                        break;
                    }
                }

                if (next_band >= 0)
                {
                    current_band = next_band;
                    //Ждём следующую чётную минуту
                    wspr_state = WSPR_STATE_WAIT_NEXT;
                }
                else
                {
                	//Сброс времени
					g_gps.hour = g_gps.min = g_gps.sec = 61;
					//Отключаем внешний PA
					EXT_PWR(false);
					//Отключаем CLK0 SI5351
					SI5351A_OutOff(SI_CLK0_CTRL);
					//Включаем верхний ФНЧ/ДПФ в соответствии с настройками
					EXT_BPF(30000000);
					//Отключаем индикатор передачи
					LED_OFF();
                    flag_wspr_on = false;
                    wspr_state = WSPR_STATE_IDLE;
                    return;
                }
            }
            break;
    }
}

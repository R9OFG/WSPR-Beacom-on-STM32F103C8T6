/*
 * vcp.c
 *
 * Created on: Nov 28, 2025
 * Author: R9OFG.RU
 */

#include "vcp.h"

wspr_settings_t g_settings;				//Указатель на структуру настроек

char str_rx[APP_RX_DATA_SIZE];			//Строка принятая через CDC
char rx_buf[APP_RX_DATA_SIZE];			//Временный буффер
char str_tx[APP_TX_DATA_SIZE];			//Строка для передачи через CDC

volatile uint32_t gen_freq = 0;			//Заданная частота тестовой генерации на CLK0 SI5351
volatile uint16_t rx_idx = 0;			//Индекс принятых данных
volatile bool flag_test_gen = false;	//Флаг - тестовая генерация сигнала
volatile bool flag_vcp_rx = false;		//Флаг - данные получены
volatile bool flag_vcp_tx = false;		//Флаг - данные готовы для отправки
volatile bool flag_gps_echo = false;	//Флаг пересылки данных GPS в VCP
volatile bool flag_si_generate = false;	//Флаг генерации сигнала на CLK0 SI5351

/* Вспомогательная: проверка валидности позывного */
bool is_valid_callsign(const char *cs)
{
    if (cs == NULL) return false;
    int len = strlen(cs);
    if (len < 3 || len > 6) return false;

    //Проверка: только A-Z и 0-9
    for (int i = 0; i < len; i++)
    {
        if (!isalpha((unsigned char)cs[i]) && !isdigit((unsigned char)cs[i]))
        {
            return false;
        }
    }

    //Первый и последний — буквы
    if (!isalpha((unsigned char)cs[0]) || !isalpha((unsigned char)cs[len-1]))
    {
        return false;
    }

    //Подсчёт цифр и проверка их расположения
    int digit_count = 0;
    int digit_start = -1;
    int digit_end = -1;

    for (int i = 0; i < len; i++)
    {
        if (isdigit((unsigned char)cs[i]))
        {
            digit_count++;
            if (digit_start == -1) digit_start = i;
            digit_end = i;
        }
    }

    //Должно быть 1 или 2 цифры
    if (digit_count == 0 || digit_count > 2)
    {
        return false;
    }

    //Цифры должны идти подряд
    if (digit_end - digit_start + 1 != digit_count)
    {
        return false;
    }

    //Цифры не в начале и не в конце (уже проверено, что первый и последний — буквы)
    //Но дополнительно: цифры не должны начинаться с позиции 0 или заканчиваться на len-1
    if (digit_start == 0 || digit_end == len - 1)
    {
        return false;
    }

    return true;
}

/* Обработчики команд через VCP */

void VCP_CMD_HELP(void)
{
	if (flag_gps_echo) {flag_gps_echo = false;}
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);

    VCP_Fill_Str_TX("User commands:\r\n"
                    "  HELP\r\n"
    				"  STATUS\r\n"
                    "  CONFIG\r\n"
                    "  CALLSIGN=AB1CD\r\n"
                    "  BANDS=160,80,40,30,20,17,15,12,10\r\n"
                    "  POWER=0/3/7/10/13/17/20/23/27/30/33/37\r\n"
                    "  XTALL=25000000\r\n"
                    "  GPS_LINE_HEADER=$GPRMC\r\n"
    				"  EXT_PWR=HIGH/LOW\r\n"
    				"  EXT_BPF=HIGH/LOW\r\n"
    				"  GEN_ON=15000000, freq in Hz, range 1...30MHz\r\n"
    		        "  GEN_OFF\r\n"
    				"  GPS_START\r\n"
    		        "  GPS_STOP\r\n"
    				"  WSPR_START\r\n"
    		        "  WSPR_STOP\r\n"
                    "  WRITE\r\n\r\n"
                    "  More info on https://r9ofg.ru\r\n");

    flag_vcp_tx = true;
    flag_wspr_on = false;
}

void VCP_CMD_STATUS(void)
{
    if (flag_gps_echo) {flag_gps_echo = false;}
    SI5351A_OutOff(SI_CLK0_CTRL);
    EXT_PWR(false);
    flag_test_gen = false;

    char buf[128];
    const char* wspr_state_str;

    //Отображаем актуальное состояние
    switch (wspr_state)
    {
        case WSPR_STATE_IDLE:
            wspr_state_str = "IDLE";
            break;
        case WSPR_STATE_WAIT_NEXT:
            wspr_state_str = "WAITING";
            break;
        case WSPR_STATE_TRANSMITTING:
            wspr_state_str = "ON AIR";
            break;
        default:
            wspr_state_str = "UNKNOWN";
    }

    const char* tx_mode = flag_wspr_on ? "ON" : "OFF";

    if (GPS_Has_Fix())
    {
        snprintf(buf, sizeof(buf),
            "STATUS: %s | GPS: UTC time: %02d:%02d:%02d QTH: %s | WSPR BEACON MODE: %s\r\n",
            wspr_state_str,
            g_gps.hour, g_gps.min, g_gps.sec,
            g_gps.locator, tx_mode
        );
    }
    else
    {
        snprintf(buf, sizeof(buf),
            "STATUS: %s | GPS: NO FIX | WSPR BEACON MODE: %s\r\n",
            wspr_state_str, tx_mode
        );
    }

    VCP_Fill_Str_TX(buf);
    flag_vcp_tx = true;
}

void VCP_CMD_CONFIG(void)
{
	if (flag_gps_echo) {flag_gps_echo = false;}
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);
	flag_test_gen = false;

    uint8_t major = (g_settings.version >> 8) & 0xFF;
    uint8_t minor = g_settings.version & 0xFF;

    int8_t dbm = power_dbm_table[g_settings.power_index];
    uint16_t mw = power_mw_table[g_settings.power_index];

    char buf[256];
    char bands_str[64];
    bands_mask_to_str(g_settings.band_mask, bands_str, sizeof(bands_str));

    int len = snprintf(buf, sizeof(buf),
        "WSPR-BEACON-4 by R9OFG firmware: %d.%d\r\n\r\n"
    	"  CALLSIGN=%s\r\n"
        "  BANDS=%s\r\n"
        "  POWER=%d dBm (%u mW)\r\n"
        "  XTALL=%lu Hz\r\n"
        "  GPS_LINE_HEADER=%s\r\n"
        "  EXT_PWR=%s\r\n"
        "  EXT_BPF=%s\r\n",
        major, minor,
        g_settings.callsign,
        bands_str,
        dbm, mw,
        g_settings.xtal_freq_hz,
        g_settings.gps_line_header,
        g_settings.ext_pwr,
        g_settings.ext_bpf
    );

    if (len > 0 && len < (int)sizeof(buf))
    {
        VCP_Fill_Str_TX(buf);
    }
    else
    {
        VCP_Fill_Str_TX("ERROR READ CONFIG!");
    }

    flag_vcp_tx = true;
    flag_wspr_on = false;
}

void VCP_CMD_CALLSIGN(const char *val)
{
	if (flag_gps_echo) {flag_gps_echo = false;}
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);
	flag_test_gen = false;

	if (is_valid_callsign(val))
	{
		strncpy(g_settings.callsign, val, CALLSIGN_LEN);
		g_settings.callsign[CALLSIGN_LEN] = '\0';
		VCP_Fill_Str_TX("CALLSIGN OK!\r\n");
	}
	else
	{
		VCP_Fill_Str_TX("ERROR: invalid callsign (3-6 chars, A-Z/0-9, 1-2 digits in middle, no /-_.)!\r\n");
	}

	flag_vcp_tx = true;
	flag_wspr_on = false;
}

void VCP_CMD_BANDS(const char *val)
{
	if (flag_gps_echo) {flag_gps_echo = false;}
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);
	flag_test_gen = false;

	if (val == NULL || strlen(val) == 0)
	{
		VCP_Fill_Str_TX("ERROR: empty bands list!\r\n");

		flag_vcp_tx = true;
		flag_wspr_on = false;

		return;
	}

	bool ok;
	uint16_t mask = bands_str_to_mask(val, &ok);
	if (!ok || mask == 0)
	{
		VCP_Fill_Str_TX("ERROR: invalid bands (use 160,80,40,30,20,17,15,12,10)!\r\n");
	}
	else
	{
		g_settings.band_mask = mask;
		VCP_Fill_Str_TX("BANDS OK!\r\n");
	}

	flag_vcp_tx = true;
	flag_wspr_on = false;
}

void VCP_CMD_POWER(const char *val)
{
	if (flag_gps_echo) {flag_gps_echo = false;}
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);
	flag_test_gen = false;

	if (val == NULL || strlen(val) == 0)
	{
		VCP_Fill_Str_TX("ERROR: empty power value!\r\n");

		flag_vcp_tx = true;
		flag_wspr_on = false;

		return;
	}

	int8_t dbm = (int8_t)atoi(val);
	bool ok = false;
	for (int i = 0; i < POWER_LEVELS_COUNT; i++)
	{
		if (dbm == power_dbm_table[i])
		{
			g_settings.power_index = (uint8_t)i;
			ok = true;
			break;
		}
	}
	if (ok)
	{
		VCP_Fill_Str_TX("POWER OK!\r\n");
	}
	else
	{
		VCP_Fill_Str_TX("ERROR: invalid power (use 0/3/7/10/13/17/20/23/27/30/33/37)!\r\n");
	}

	flag_vcp_tx = true;
	flag_wspr_on = false;
}

void VCP_CMD_XTALL(const char *val)
{
	if (flag_gps_echo) {flag_gps_echo = false;}

	uint32_t freq = strtoul(val, NULL, 10);
	if (freq >= 10000000U && freq <= 28000000U)
	{
		g_settings.xtal_freq_hz = freq;
		VCP_Fill_Str_TX("XTALL OK!\r\n");

		if (flag_si_generate)
		{
			//Устанавливаем заданную частоту выхода CLK0 SI5351
			SI5351A_SetFreq(SI_CLK0_CTRL, g_settings.xtal_freq_hz, gen_freq);
		}
	}
	else
	{
		VCP_Fill_Str_TX("ERROR: XTALL 10000000...28000000 Hz\r\n");
	}

	flag_vcp_tx = true;
	flag_wspr_on = false;
}

void VCP_CMD_GPS_LINE_HEADER(const char *val)
{
	if (flag_gps_echo) {flag_gps_echo = false;}
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);
	flag_test_gen = false;

    if (val == NULL || strlen(val) == 0)
    {
        VCP_Fill_Str_TX("ERROR: empty gps line header!\r\n");

        flag_vcp_tx = true;
        flag_wspr_on = false;

        return;
    }

    //Проверка: строка должна начинаться с '$' и быть 4-7 символов
    if (val[0] != '$' || strlen(val) < 4 || strlen(val) > 7)
    {
        VCP_Fill_Str_TX("ERROR: invalid gps line header (e.g. $GPRMC)!\r\n");

        flag_vcp_tx = true;
        flag_wspr_on = false;

        return;
    }

    //Сохраняем в настройки
    strncpy(g_settings.gps_line_header, val, 6);
    g_settings.gps_line_header[6] = '\0';

    VCP_Fill_Str_TX("GPS LINE HEADER OK!\r\n");

    flag_vcp_tx = true;
    flag_wspr_on = false;
}

void VCP_CMD_EXT_PWR(const char *val)
{
	if (flag_gps_echo) {flag_gps_echo = false;}
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);
	flag_test_gen = false;

    if (strcmp(val, "HIGH") == 0 || strcmp(val, "LOW") == 0)
    {
        strcpy(g_settings.ext_pwr, val);
        VCP_Fill_Str_TX("EXT PWR OK!\r\n");
    }
    else
    {
        VCP_Fill_Str_TX("ERROR: use EXT_PWR=HIGH or EXT_PWR=LOW!\r\n");
    }

    flag_vcp_tx = true;
    flag_wspr_on = false;

    //Сбрасываем пин управления внешним УМ в соответствии с настройками
    EXT_PWR(false);
}

void VCP_CMD_EXT_BPF(const char *val)
{
	if (flag_gps_echo) {flag_gps_echo = false;}
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);
	flag_test_gen = false;

    if (strcmp(val, "HIGH") == 0 || strcmp(val, "LOW") == 0)
    {
        strcpy(g_settings.ext_bpf, val);
        VCP_Fill_Str_TX("EXT BPF OK!\r\n");
    } else {
        VCP_Fill_Str_TX("ERROR: use EXT_BPF=HIGH or EXT_BPF=LOW!\r\n");
    }

    flag_vcp_tx = true;
    flag_wspr_on = false;

    //Включаем верхний ФНЧ/ДПФ в соответствии с настройками
    EXT_BPF(30000000);
}

void VCP_CMD_GEN_ON(const char *val)
{
	if (flag_gps_echo) {flag_gps_echo = false;}

	if (!flagSI5351A)
	{
		VCP_Fill_Str_TX("ERROR: no SI5351!\r\n");

		flag_vcp_tx = true;
		flag_wspr_on = false;

		return;
	}

    if (val == NULL)
    {
        VCP_Fill_Str_TX("ERROR: missing frequency!\r\n");

        flag_vcp_tx = true;
        flag_wspr_on = false;

        return;
    }

    uint32_t freq = strtoul(val, NULL, 10);
    if (freq == 0)
    {
        VCP_Fill_Str_TX("ERROR: invalid frequency!\r\n");

        flag_vcp_tx = true;
        flag_wspr_on = false;

        return;
    }

    //Проверка диапазона
    if (freq < 1000000 || freq > 30000000)  //Диапазон 1...30МГц
    {
        VCP_Fill_Str_TX("ERROR: frequency out of range (1...30MHz)!\r\n");

        flag_vcp_tx = true;
        flag_wspr_on = false;

        return;
    }

    //Включаем необходимый ФНЧ
    EXT_BPF(freq);
    //Устанавливаем заданную частоту выхода CLK0 SI5351
	SI5351A_SetFreq(SI_CLK0_CTRL, g_settings.xtal_freq_hz, freq);
	//Включаем выход CLK0 с заданным током драйвера
	SI5351A_SetDrvClk(SI_CLK0_CTRL, SiCLKDrv_Array[currentSiDrvIdx]);
	//Включаем внешний PA в соответствии с настройками
	EXT_PWR(true);

	gen_freq = freq;
	flag_si_generate = true;

    char buf[64];
    snprintf(buf, sizeof(buf), "GENERATE ON: %lu Hz\r\n", freq);
    VCP_Fill_Str_TX(buf);

    flag_vcp_tx = true;
    flag_wspr_on = false;
    flag_test_gen = true;
}

void VCP_CMD_GEN_OFF(void)
{
	if (flag_gps_echo) {flag_gps_echo = false;}
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);

	flag_si_generate = false;

    VCP_Fill_Str_TX("GENERATE OFF!\r\n");

    flag_test_gen = false;
    flag_vcp_tx = true;
    flag_wspr_on = false;

    //Сбрасываем пин управления внешним УМ в соответствии с настройками
    EXT_PWR(false);

    //Включаем верхний ФНЧ/ДПФ в соответствии с настройками
    EXT_BPF(30000000);
}

void VCP_CMD_GPS_START(void)
{
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);
	flag_test_gen = false;

	flag_gps_echo = true;
    VCP_Fill_Str_TX("GPS START OK!\r\n");

    flag_vcp_tx = true;
    flag_wspr_on = false;
}

void VCP_CMD_GPS_STOP(void)
{
	if (flag_gps_echo) {flag_gps_echo = false;}
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);
	flag_test_gen = false;

    VCP_Fill_Str_TX("GPS STOP OK!\r\n");

    flag_vcp_tx = true;
    flag_wspr_on = false;
}

void VCP_CMD_WSPR_START(void)
{
    if (flag_gps_echo) {flag_gps_echo = false;}
    SI5351A_OutOff(SI_CLK0_CTRL);
    EXT_PWR(false);
    flag_test_gen = false;

	flag_wspr_on = true;

    VCP_Fill_Str_TX("WSPR MODE ON!\r\n");

    flag_vcp_tx = true;
}

void VCP_CMD_WSPR_STOP(void)
{
	if (flag_gps_echo) {flag_gps_echo = false;}

    flag_wspr_on = false;
    SI5351A_OutOff(SI_CLK0_CTRL);
    EXT_PWR(false);
    flag_test_gen = false;

    VCP_Fill_Str_TX("WSPR MODE OFF!\r\n");

    flag_vcp_tx = true;

    //Сбрасываем пин управления внешним УМ в соответствии с настройками
    EXT_PWR(false);

    //Включаем верхний ФНЧ/ДПФ в соответствии с настройками
    EXT_BPF(30000000);
}

void VCP_CMD_WRITE(void)
{
	if (flag_gps_echo) {flag_gps_echo = false;}
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);
	flag_test_gen = false;

	g_settings.version = FIRMWARE_VERSION;
	if (SETTINGS_Write_to_FLASH(&g_settings) == 0)
	{
		VCP_Fill_Str_TX("FLASH WRITE OK!\r\n");
	}
	else
	{
		VCP_Fill_Str_TX("ERROR: flash write failed!\r\n");
	}

	flag_vcp_tx = true;
	flag_wspr_on = false;
}

void VCP_CMD_UNKNOWN(void)
{
	if (flag_gps_echo) {flag_gps_echo = false;}
	SI5351A_OutOff(SI_CLK0_CTRL);
	EXT_PWR(false);
	flag_test_gen = false;

    VCP_Fill_Str_TX("ERROR: unknown command!\r\n");

    flag_vcp_tx = true;
    flag_wspr_on = false;
}

/* END Обработчики команд через VCP */

/* Инициализация VCP */
void VCP_Init(void)
{
	//Читаем настройки из FLASH
	if (SETTINGS_Read_from_FLASH(&g_settings) != 0)
	{
		//Чтение с ошибкой, устанавливаем/сохраняем по умолчанию
		SETTINGS_Set_Default(&g_settings);
		SETTINGS_Write_to_FLASH(&g_settings);
	}

	//Сравниваем версии
	if (g_settings.version != FIRMWARE_VERSION)
	{
		//Сохраняем если не совпадают
		SETTINGS_Write_to_FLASH(&g_settings);
	}

	//Сбрасываем пин управления внешним УМ в соответствии с натройками
	EXT_PWR(false);

	//Включаем верхний ФНЧ/ДПФ
	EXT_BPF(30000000);
}

/* Обработка принятой строки */
void VCP_Parse_Str_RX(void)
{
    flag_vcp_rx = false;

    if (strcmp(str_rx, "HELP") == 0)
	{
		VCP_CMD_HELP();
	}
    else if (strcmp(str_rx, "STATUS") == 0)
	{
		VCP_CMD_STATUS();
	}
    else if (strcmp(str_rx, "CONFIG") == 0)
	{
		VCP_CMD_CONFIG();
	}
    else if (strncmp(str_rx, "CALLSIGN=", 9) == 0)
    {
        VCP_CMD_CALLSIGN(str_rx + 9);
    }
    else if (strncmp(str_rx, "BANDS=", 6) == 0)
    {
        VCP_CMD_BANDS(str_rx + 6);
    }
    else if (strncmp(str_rx, "POWER=", 6) == 0)
    {
        VCP_CMD_POWER(str_rx + 6);
    }
    else if (strncmp(str_rx, "XTALL=", 6) == 0)
    {
        VCP_CMD_XTALL(str_rx + 6);
    }
    else if (strncmp(str_rx, "GPS_LINE_HEADER=", 16) == 0)
    {
        VCP_CMD_GPS_LINE_HEADER(str_rx + 16);
    }
    else if (strncmp(str_rx, "EXT_PWR=", 8) == 0)
    {
        VCP_CMD_EXT_PWR(str_rx + 8);
    }
    else if (strncmp(str_rx, "EXT_BPF=", 8) == 0)
    {
        VCP_CMD_EXT_BPF(str_rx + 8);
    }
    else if (strncmp(str_rx, "GEN_ON=", 7) == 0)
    {
        VCP_CMD_GEN_ON(str_rx + 7);
    }
    else if (strcmp(str_rx, "GEN_OFF") == 0)
    {
        VCP_CMD_GEN_OFF();
    }
    else if (strcmp(str_rx, "GPS_START") == 0)
	{
		VCP_CMD_GPS_START();
	}
	else if (strcmp(str_rx, "GPS_STOP") == 0)
	{
		VCP_CMD_GPS_STOP();
	}
    else if (strcmp(str_rx, "WSPR_START") == 0)
    {
        VCP_CMD_WSPR_START();
    }
    else if (strcmp(str_rx, "WSPR_STOP") == 0)
    {
        VCP_CMD_WSPR_STOP();
    }
    else if (strcmp(str_rx, "WRITE") == 0)
    {
        VCP_CMD_WRITE();
    }
    else
    {
        VCP_CMD_UNKNOWN();
    }
}

/* Заполнение строки для передачи */
uint16_t VCP_Fill_Str_TX(const char *src)
{
    if (src == NULL)
    {
        str_tx[0] = '\0';
        return 0;
    }

    size_t len = strlen(src);
    if (len > APP_TX_DATA_SIZE - 4)
    {
    	//1 байт на \r + 1 на \n в начале + 2 в конце
        len = APP_TX_DATA_SIZE - 4;
    }

    //Начинаем с \r\n
    str_tx[0] = '\r';
    str_tx[1] = '\n';

    //Копируем строку
    memcpy(&str_tx[2], src, len);

    //Завершаем \r\n
    str_tx[2 + len]     = '\r';
    str_tx[2 + len + 1] = '\n';
    str_tx[2 + len + 2] = '\0';

    return (uint16_t)(len + 4); //\r\n + строка + \r\n
}

/* Отправка строки */
void VCP_Transmit(void)
{
	flag_vcp_tx = false;

	CDC_Transmit_FS((uint8_t*)str_tx, strlen(str_tx));
}

/*
 * gps.c
 *
 * Created on: Nov 29, 2025
 * Author: User
 */

#include "gps.h"

gps_data_t g_gps = {0};

static char gps_rx_buffer[GPS_BUF_SIZE];
static uint8_t gps_idx = 0;

volatile bool flag_gps_ready = false;	//Флаг готовности GPS строки к парсингу
char gps_line_buf[GPS_BUF_SIZE]; 		//Буфер для завершённой строки GPS
static uint8_t gps_line_idx = 0;

//Вспомогательная: парсинг времени из строки "123519.00"
static bool parse_time(const char *s, uint8_t *h, uint8_t *m, uint8_t *s_sec)
{
    if (strlen(s) < 6) return false;
    char hh[3] = {s[0], s[1], '\0'};
    char mm[3] = {s[2], s[3], '\0'};
    char ss[3] = {s[4], s[5], '\0'};
    *h = (uint8_t)atoi(hh);
    *m = (uint8_t)atoi(mm);
    *s_sec = (uint8_t)atoi(ss);
    return true;
}

//Вспомогательная: конвертация NMEA-формата "4807.038" в десятичные градусы
static float nmea_to_decimal(const char *nmea, char dir)
{
    if (strlen(nmea) < 4) return 0.0f;
    // Найдём точку
    char *dot = strchr(nmea, '.');
    if (!dot) return 0.0f;
    int dot_pos = dot - nmea;
    if (dot_pos < 2) return 0.0f;

    // Минуты = всё после первых 2/3 цифр
    char degrees_str[4] = {0};
    strncpy(degrees_str, nmea, dot_pos - 2);
    float degrees = (float)atoi(degrees_str);
    float minutes = atof(nmea + (dot_pos - 2));
    float decimal = degrees + minutes / 60.0f;
    if (dir == 'S' || dir == 'W') decimal = -decimal;
    return decimal;
}

//Вспомогательная: конвертация координат в Maidenhead локатор
static void latlon_to_maidenhead(float lat, float lon, char *locator)
{
    lon += 180.0f;
    lat += 90.0f;

    locator[0] = 'A' + (int)(lon / 20);
    locator[1] = 'A' + (int)(lat / 10);

    lon = fmodf(lon, 20.0f);
    lat = fmodf(lat, 10.0f);
    locator[2] = '0' + (int)(lon / 2);
    locator[3] = '0' + (int)(lat / 1);

    lon = fmodf(lon, 2.0f);
    lat = fmodf(lat, 1.0f);
    locator[4] = 'A' + (int)(lon / (2.0f / 24.0f));
    locator[5] = 'A' + (int)(lat / (1.0f / 24.0f));

    locator[6] = '\0';
}

//Парсинг строки NMEA
static void GPS_Parse_NMEA(const char *sentence)
{
	//Парсим строку, заданную в настройках
	if (strncmp(sentence, g_settings.gps_line_header, strlen(g_settings.gps_line_header)) != 0)
	{
		return;
	}

	char *fields[15];
	int field_count = 0;
	char buf[GPS_BUF_SIZE];
	strncpy(buf, sentence, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	char *token = strtok(buf, ",");
	while (token && field_count < 15)
	{
		fields[field_count++] = token;
		token = strtok(NULL, ",");
	}

	//Проверка: строка не пустая
	if (field_count < 7)
	{
		g_gps.valid = false;
		return;
	}

	//Парсим по известным типам строк

	if (strncmp(g_settings.gps_line_header, "$GPRMC", 6) == 0)
	{
		//$GPRMC: время в поле 1, координаты в 3(N),4,5(E/W), статус в 2
		if (strcmp(fields[2], "A") != 0)
		{
			g_gps.valid = false;
			return;
		}
		if (!parse_time(fields[1], &g_gps.hour, &g_gps.min, &g_gps.sec))
		{
			g_gps.valid = false;
			return;
		}
		g_gps.latitude = nmea_to_decimal(fields[3], fields[4][0]);
		g_gps.longitude = nmea_to_decimal(fields[5], fields[6][0]);

	}
	else if (strncmp(g_settings.gps_line_header, "$GPGGA", 6) == 0 || strncmp(g_settings.gps_line_header, "$GNGGA", 6) == 0)
	{
		//$GPGGA или $GNGGA: время в поле 1, координаты в 2(N/S),3,4(E/W), качество в 6
		if (atoi(fields[6]) == 0)
		{
			//0 = invalid
			g_gps.valid = false;
			return;
		}
		if (!parse_time(fields[1], &g_gps.hour, &g_gps.min, &g_gps.sec))
		{
			g_gps.valid = false;
			return;
		}
		g_gps.latitude = nmea_to_decimal(fields[2], fields[3][0]);
		g_gps.longitude = nmea_to_decimal(fields[4], fields[5][0]);

	}
	else
	{
		//Неизвестный тип строки — не парсим
		g_gps.valid = false;
		return;
	}

	//Вычисляем QTH-локатор
	latlon_to_maidenhead(g_gps.latitude, g_gps.longitude, g_gps.locator);
	g_gps.valid = true;
}

//Инициализация
void GPS_Init(void)
{
	gps_idx = 0;
	//Сброс валидности
	g_gps.valid = false;
	//Сброс времени
	g_gps.hour = g_gps.min = g_gps.sec = 61;
	//Сброс координат и QTH локатора
	g_gps.latitude = g_gps.longitude = 0.0f;
	g_gps.locator[0] = '\0';
}

//Вызывать из USART1_IRQHandler
void GPS_Feed_Byte(uint8_t c)
{
	if (c == '\n')
	{
		if (gps_line_idx > 0)
		{
			//Копируем строку в буфер готовой строки
			if (gps_line_idx >= GPS_BUF_SIZE)
			{
				gps_line_idx = GPS_BUF_SIZE - 1;
			}
			memcpy(gps_line_buf, &gps_rx_buffer[0], gps_line_idx);
			gps_line_buf[gps_line_idx] = '\0';
			flag_gps_ready = true;  //Поднимаем флаг для main()
			gps_line_idx = 0;
		}
	}
	else if (gps_line_idx < GPS_BUF_SIZE - 1)
	{
	    gps_rx_buffer[gps_line_idx++] = c;
	}
}

//Проверка наличия фикса
bool GPS_Has_Fix(void)
{
    return g_gps.valid;
}

//Парсинг для вызова из main()
void GPS_Parse_Line(void)
{
    if (!flag_gps_ready) return;
    flag_gps_ready = false;

    GPS_Parse_NMEA(gps_line_buf);  //Теперь сам парсинг
}

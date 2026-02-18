/*
 * wspr.h
 *
 * Created on: Nov 29, 2025
 * Author: R9OFG.RU
 */

#ifndef INC_WSPR_H_
#define INC_WSPR_H_

#include "main.h"
#include "led_pcb.h"
#include "stdbool.h"
#include "vcp.h"
#include "si5351a.h"

//Структура состояния
typedef enum {
    WSPR_STATE_IDLE,			//Ожидание
    WSPR_STATE_TRANSMITTING,	//Передача
    WSPR_STATE_WAIT_NEXT		//Ожидание следующей чётной минуты
} wspr_state_t;

extern wspr_state_t wspr_state;
extern volatile bool flag_wspr_on;
extern const uint32_t wspr_frequencies_hz[9];

void WSPR_Task(void);

#endif /* INC_WSPR_H_ */

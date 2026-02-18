/*
 * ext.h
 *
 * Created on: Dec 1, 2025
 * Author: R9OFG.RU
 */

#ifndef INC_EXT_H_
#define INC_EXT_H_

#include "main.h"
#include "stdbool.h"
#include "vcp.h"
#include "settings.h"

#define EXT_PWR_PIN		GPIO_PIN_8
#define EXT_PWR_PORT	GPIOA

#define EXT_BPF_A_PIN GPIO_PIN_12
#define EXT_BPF_B_PIN GPIO_PIN_13
#define EXT_BPF_C_PIN GPIO_PIN_14
#define EXT_BPF_D_PIN GPIO_PIN_15
#define EXT_BPF_PORT  GPIOB

void EXT_PWR(bool cmd);
void EXT_BPF(uint32_t freq);

#endif /* INC_EXT_H_ */

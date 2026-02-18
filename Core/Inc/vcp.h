/*
 * vcp.h
 *
 * Created on: Nov 28, 2025
 * Author: R9OFG.RU
 */

#ifndef INC_VCP_H_
#define INC_VCP_H_

#include "main.h"
#include "ctype.h"
#include "stdbool.h"
#include "usbd_cdc_if.h"
#include "settings.h"
#include "gps.h"
#include "si5351a.h"
#include "wspr.h"
#include "ext.h"

extern wspr_settings_t g_settings;
extern char str_rx[APP_RX_DATA_SIZE];
extern char rx_buf[APP_RX_DATA_SIZE];
extern volatile uint16_t rx_idx;
extern volatile bool flag_test_gen;
extern volatile bool flag_vcp_rx;
extern volatile bool flag_vcp_tx;
extern volatile bool flag_gps_echo;

void VCP_Init(void);
void VCP_Parse_Str_RX(void);
uint16_t VCP_Fill_Str_TX(const char *src);
void VCP_Transmit(void);

#endif /* INC_VCP_H_ */

/*
 * spx.h
 *
 *  Created on: 8 dic. 2018
 *      Author: pablo
 */

#ifndef SRC_SPX_H_
#define SRC_SPX_H_

//------------------------------------------------------------------------------------
// INCLUDES
//------------------------------------------------------------------------------------
#include <avr/io.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <compat/deprecated.h>
#include <avr/pgmspace.h>
#include <stdarg.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <avr/sleep.h>
#include <ctype.h>
#include "avr_compiler.h"
#include "clksys_driver.h"
#include <inttypes.h>

#include "TC_driver.h"
#include "pmic_driver.h"
#include "wdt_driver.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "list.h"
#include "croutine.h"
#include "semphr.h"
#include "timers.h"
#include "limits.h"

#include "frtos-io.h"
#include "FRTOS-CMD.h"

#include "PROTOP_libs/l_ainputs.h"
#include "PROTOP_libs/l_doutputs.h"
#include "PROTOP_libs/l_drv8814.h"
#include "PROTOP_libs/l_eeprom.h"
#include "PROTOP_libs/l_i2c.h"
#include "PROTOP_libs/l_ina3221.h"
#include "PROTOP_libs/l_iopines.h"
#include "PROTOP_libs/l_iopines.h"
#include "PROTOP_libs/l_nvm/l_nvm.h"
#include "PROTOP_libs/l_printf.h"
#include "PROTOP_libs/l_rtc79410.h"

//------------------------------------------------------------------------------------
// DEFINES
//------------------------------------------------------------------------------------
#define SPX_FW_REV "0.0.7"
#define SPX_FW_DATE "@ 20190619"

#define SPX_HW_MODELO "protoPilotos HW:xmega256A3B R1.1"
#define SPX_FTROS_VERSION "FW:FRTOS10 TICKLESS"

//#define F_CPU (32000000UL)

//#define SYSMAINCLK 2
//#define SYSMAINCLK 8
#define SYSMAINCLK 32
//
#define MAX_ANALOG_CHANNELS		2

#define CHAR32	32
#define CHAR64	64
#define CHAR128	128
#define CHAR256	256

#define tkCtl_STACK_SIZE		512
#define tkCmd_STACK_SIZE		512
#define tkData_STACK_SIZE		512
#define tkRegular_STACK_SIZE		512

#define tkCtl_TASK_PRIORITY	 		( tskIDLE_PRIORITY + 1 )
#define tkCmd_TASK_PRIORITY	 		( tskIDLE_PRIORITY + 1 )
#define tkData_TASK_PRIORITY	 	( tskIDLE_PRIORITY + 1 )
#define tkRegular_TASK_PRIORITY	 	( tskIDLE_PRIORITY + 1 )

TaskHandle_t xHandle_idle, xHandle_tkCtl, xHandle_tkCmd, xHandle_tkData, xHandle_tkRegular;

bool startTask;

void tkCtl(void * pvParameters);
void tkCmd(void * pvParameters);
void tkCounter(void * pvParameters);
void tkData(void * pvParameters);
void tkRegular(void * pvParameters);

typedef enum { OPEN = 0, CLOSE } t_valve_status;

typedef struct {
	// Variables de trabajo.
	// Configuracion de Canales analogicos
	uint8_t ain_imin[MAX_ANALOG_CHANNELS];	// Coeficientes de conversion de I->magnitud (presion)
	uint8_t ain_imax[MAX_ANALOG_CHANNELS];
	float ain_mmin[MAX_ANALOG_CHANNELS];
	float ain_mmax[MAX_ANALOG_CHANNELS];

	uint16_t timerPoll;
	// El checksum DEBE ser el ultimo byte del systemVars !!!!
	uint8_t checksum;

	float pout_ref;
	int tipo_valvula_reguladora;
	float p_margen;

	// Estado de las valvulas
	t_valve_status status_valve_A;
	t_valve_status status_valve_B;

	bool regular;
	bool monitor;

} systemVarsType;

systemVarsType systemVars;

// UTILS
void initMCU(void);
void u_configure_systemMainClock(void);
void u_configure_RTC32(void);
void u_config_timerpoll ( char *s_timerpoll );
bool u_config_analog_channel( uint8_t channel,char *s_imin,char *s_imax,char *s_mmin,char *s_mmax );
void u_load_defaults(void);
uint8_t u_save_params_in_NVMEE(void);
bool u_load_params_from_NVMEE(void);
float u_read_analog_channel ( uint8_t io_channel );
float u_readAin(uint8_t an_id);

float readAin(uint8_t an_id);

void vopen ( char valve_id );
void vclose ( char valve_id );
void vpulse( char valve_id, float pulse_width_s );

#endif /* SRC_SPX_H_ */

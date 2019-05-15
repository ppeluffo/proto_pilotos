/*
 * protop_tkData.c
 *
 *  Created on: 3 mar. 2019
 *      Author: pablo
 */


#include "protop.h"

static void pv_data_init(void);

float presion_alta,presion_baja;

//------------------------------------------------------------------------------------

void tkData(void * pvParameters)
{

( void ) pvParameters;

uint32_t waiting_ticks;
TickType_t xLastWakeTime;

	// Espero la notificacion para arrancar
	while ( !startTask )
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );

	xprintf_P( PSTR("starting tkData..\r\n\0"));

	pv_data_init();

   // Initialise the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();

    // Al arrancar poleo a los 10s
    waiting_ticks = (uint32_t)(10) * 1000 / portTICK_RATE_MS;

	// loop
	for( ;; )
	{
		// Espero. Da el tiempo necesario para entrar en tickless.
		vTaskDelayUntil( &xLastWakeTime, waiting_ticks );

		if ( systemVars.timerPoll > 0 )  {
			// Leo analog.
			presion_alta = u_read_analog_channel (0 );
			presion_baja = u_read_analog_channel (1 );

			// Muestro en pantalla.
			xprintf_P(PSTR("AN0=%.02f, AN1=%.02f, CNT0=%d, CNT1=%d\r\n\0"), presion_alta, presion_baja , u_readCounter(0), u_readCounter(1) );

			// Calculo el tiempo para una nueva espera
			waiting_ticks = (uint32_t)(systemVars.timerPoll) * 1000 / portTICK_RATE_MS;
		} else {
			waiting_ticks = 5000;
		}

	}

}
//------------------------------------------------------------------------------------
// FUNCIONES PUBLICAS
//------------------------------------------------------------------------------------
// FUNCIONES PRIVADAS
//------------------------------------------------------------------------------------
static void pv_data_init(void)
{
	// Inicializo los INA con los que mido las entradas analogicas.
	AINPUTS_init( 0 );

	AINPUTS_prender_12V();
	INA_config(0, CONF_INAS_AVG128 );
	INA_config(1, CONF_INAS_AVG128 );

	vclose( 'B' );
	vclose( 'A' );

}
//------------------------------------------------------------------------------------

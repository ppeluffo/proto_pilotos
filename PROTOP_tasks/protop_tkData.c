/*
 * protop_tkData.c
 *
 *  Created on: 3 mar. 2019
 *      Author: pablo
 */


#include "protop.h"

static void pv_data_init(void);
void u_read_analog_channel ( uint8_t io_board, uint8_t io_channel, uint16_t *raw_val, float *mag_val );

//------------------------------------------------------------------------------------
uint16_t raw_val[2];
float an_val[2];
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

		// Leo analog.
		u_read_analog_channel (0, 0, &raw_val[0], &an_val[0] );
		u_read_analog_channel (0, 1, &raw_val[1], &an_val[1] );

		// Muestro en pantalla.
		xprintf_P(PSTR("AN0=%.02f, AN1=%.02f\r\n\0"),an_val[0],an_val[1] );

		// Calculo el tiempo para una nueva espera
		waiting_ticks = (uint32_t)(systemVars.timerPoll) * 1000 / portTICK_RATE_MS;

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

}
//------------------------------------------------------------------------------------
void u_read_analog_channel ( uint8_t io_board, uint8_t io_channel, uint16_t *raw_val, float *mag_val )
{
	// Lee un canal analogico y devuelve el valor convertido a la magnitud configurada.
	// Es publico porque se utiliza tanto desde el modo comando como desde el modulo de poleo de las entradas.
	// Hay que corregir la correspondencia entre el canal leido del INA y el canal fisico del datalogger
	// io_channel. Esto lo hacemos en AINPUTS_read_ina.

uint16_t an_raw_val;
float an_mag_val;
float I,M,P;
uint16_t D;


//	xprintf_P(PSTR("IOBOARD=[%d], CH=[%d] "),io_board,io_channel );

	an_raw_val = AINPUTS_read_ina(io_board, io_channel );
	*raw_val = an_raw_val;

//	xprintf_P(PSTR("RAW_VAL=[%d] "),an_raw_val );

	// Convierto el raw_value a la magnitud
	// Calculo la corriente medida en el canal
	I = (float)( an_raw_val) * 20 ;

//	xprintf_P(PSTR("I=[%.02f] "),I );

	// Calculo la magnitud
	P = 0;
	M = 0;
	D = systemVars.ain_imax[io_channel] - systemVars.ain_imin[io_channel];

//	xprintf_P(PSTR("D=[%.02f] "),D );

	an_mag_val = 0.0;
	if ( D != 0 ) {
		// Pendiente
		P = (float) ( systemVars.ain_mmax[io_channel]  -  systemVars.ain_mmin[io_channel] ) / D;
		// Magnitud
		M = (float) (systemVars.ain_mmin[io_channel] + ( I - systemVars.ain_imin[io_channel] ) * P);

		an_mag_val = M;

//		xprintf_P(PSTR("P=[%.02f],M=[%.02f]  "),P,M );

	} else {
		// Error: denominador = 0.
		an_mag_val = -999.0;
	}

	*mag_val = an_mag_val;

//	xprintf_P(PSTR("AN_MAG_VAL=[%.02f]\r\n"),an_mag_val );

}
//------------------------------------------------------------------------------------
float readAin(uint8_t an_id)
{
	return (an_val[an_id] );
}
//------------------------------------------------------------------------------------



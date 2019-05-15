/*
 * protop_tkRegular.c
 *
 *  Created on: 2 may. 2019
 *      Author: pablo
 *
 *  Modificaciones:
 *  - Disminuir tiempo de pwr_on_valvulas
 *
 */

#include "protop.h"

#define TIME_PWR_ON_VALVES 10

static void pv_regular_init(void);

uint16_t raw_val[2];
float presion_alta, presion_baja;
float delta_P;
float pulseW;

static float pv_calcular_pulse_width( float delta_P, float tipo_valvula_reguladora );

#define MAX_ROW 11

float Table_PW[MAX_ROW][2] = {
		{ 0.050, 0.01 },	// 0
		{ 0.100, 0.1 },	// 1
		{ 0.200, 0.2 },	// 2
		{ 0.300, 0.3 },	// 3
		{ 0.400, 0.4 },	// 4
		{ 0.500, 0.5 },	// 5
		{ 0.600, 0.6 },	// 6
		{ 0.700, 0.7 },	// 7
		{ 0.800, 0.8 },	// 8
		{ 0.900, 0.9 },	// 9
		{ 1.000, 1.0 },	// 10
};

//------------------------------------------------------------------------------------
void tkRegular(void * pvParameters)
{

( void ) pvParameters;

uint32_t waiting_ticks;

	// Espero la notificacion para arrancar
	while ( !startTask )
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );

	xprintf_P( PSTR("starting tkRegular..\r\n\0"));

	pv_regular_init();


	// loop
	for( ;; )
	{

		// Espero un ciclo
	    waiting_ticks = (uint32_t)(systemVars.timerPoll) * 1000 / portTICK_RATE_MS;
	    vTaskDelay( waiting_ticks );

		// Leo presiones y caudales
		presion_alta = u_read_analog_channel (0);
		presion_baja = u_read_analog_channel (1);

		if ( systemVars.monitor == true ) {
			xprintf_P(PSTR("Mon: AN0=%.02f, AN1=%.02f, CNT0=%d, CNT1=%d\r\n\0"), presion_alta, presion_baja, u_readCounter(0), u_readCounter(1) );
		}

		// Regulo ?
		if ( systemVars.regular == true ) {

			// Calculos
			// Diferencia de presion
			delta_P = fabs( systemVars.pout_ref - presion_baja );
			if ( delta_P < ( systemVars.p_band)  ) {
				xprintf_P(PSTR("Reg(in-band): deltaP = %.02f (%.02f - %.02f)\r\n\0"), delta_P, systemVars.pout_ref, presion_baja );
				continue;
			}
			// Ancho de pulso a aplicar
			pulseW = pv_calcular_pulse_width( delta_P, systemVars.tipo_valvula_reguladora );
			xprintf_P(PSTR("Reg(out-band): deltaP = %.02f (%.02f - %.02f), pulse_width = %.02f\r\n\0"), delta_P, systemVars.pout_ref, presion_baja, pulseW );

			// Aplico correcciones.
			if ( presion_baja < systemVars.pout_ref ) {
				// Debo subir la presion
				vpulse( 'B', pulseW );
			} else if ( presion_baja > systemVars.pout_ref ) {
				// Debo bajar la presion
				vpulse( 'A', pulseW );
			}
		}
	}

}
//------------------------------------------------------------------------------------
// FUNCIONES PUBLICAS
//------------------------------------------------------------------------------------
// FUNCIONES PRIVADAS
//------------------------------------------------------------------------------------
static void pv_regular_init(void)
{
	// Inicializo los INA con los que mido las entradas analogicas.
	AINPUTS_init( 0 );

	AINPUTS_prender_12V();
	INA_config(0, CONF_INAS_AVG128 );
	INA_config(1, CONF_INAS_AVG128 );

}
//------------------------------------------------------------------------------------
static float pv_calcular_pulse_width( float delta_P, float tipo_valvula_reguladora )
{

int row;

	for ( row = 0; row < MAX_ROW; row++ ) {
		if ( delta_P <= Table_PW[row][0] ) {
			return( Table_PW[row][1]);
		}
	}

	return( Table_PW[ MAX_ROW -1 ][1]);

/*
	if ( delta_P > 0.7 ) { return( 1 ); };
	//  Si la diferencia de presion es de mas de 300 gr. aplico un pulso de 5s
	if ( delta_P > 0.3 ) { return( 0.5 ); };
	//  Si la diferencia de presion es entre 200 y 300gr, el pulso es de 1s
	if ( delta_P > 0.2 ) { return( 0.1 ); };
	//  Si la diferencia de presion es entre 100 y 200gr, el pulso es de 0.5s
	if ( delta_P > 0.1 ) { return( 0.05 ); };
	// Otros casos aplico 0.1s
	return(0.01);
*/
}
//------------------------------------------------------------------------------------
// FUNCIONES PUBLICAS
//------------------------------------------------------------------------------------
void vopen ( char valve_id )
{
	// Proporciono corriente.
	DRV8814_power_on();
	// Espero 10s que se carguen los condensasores
	vTaskDelay( ( TickType_t)( TIME_PWR_ON_VALVES / portTICK_RATE_MS ) );

	xprintf_P( PSTR("VALVE OPEN %c\r\n\0"), valve_id );

	DRV8814_vopen( valve_id, 100);

	switch (valve_id) {
	case 'A':
		systemVars.status_valve_A = OPEN;
		break;
	case 'B':
		systemVars.status_valve_B = OPEN;
	}

	DRV8814_power_off();


}
//------------------------------------------------------------------------------------
void vclose ( char valve_id )
{
	// Proporciono corriente.
	DRV8814_power_on();
	// Espero 10s que se carguen los condensasores
	vTaskDelay( ( TickType_t)( TIME_PWR_ON_VALVES / portTICK_RATE_MS ) );

	xprintf_P( PSTR("VALVE CLOSE %c\r\n\0"), valve_id );

	DRV8814_vclose( valve_id, 100);

	switch (valve_id) {
	case 'A':
		systemVars.status_valve_A = CLOSE;
		break;
	case 'B':
		systemVars.status_valve_B = CLOSE;
	}
	DRV8814_power_off();



}
//------------------------------------------------------------------------------------
void vpulse( char valve_id, float pulse_width_ms )
{
	// Genera un pulso de apertura / cierre en la valvula valve_id
	// de duracion 	pulse_width_ms

	// Proporciono corriente.
	DRV8814_power_on();
	// Espero 10s que se carguen los condensasores
	vTaskDelay( ( TickType_t)( TIME_PWR_ON_VALVES / portTICK_RATE_MS ) );

	DRV8814_vopen( toupper(valve_id), 100);
	xprintf_P( PSTR("Vopen %c\r\n\0"), valve_id );
	switch (valve_id) {
	case 'A':
		systemVars.status_valve_A = OPEN;
		break;
	case 'B':
		systemVars.status_valve_B = OPEN;
	}

	vTaskDelay( ( TickType_t)( pulse_width_ms * 1000 / portTICK_RATE_MS ) );

	DRV8814_vclose( toupper(valve_id), 100);
	xprintf_P( PSTR("Vclose %c\r\n\0"), valve_id );
	switch (valve_id) {
	case 'A':
		systemVars.status_valve_A = CLOSE;
		break;
	case 'B':
		systemVars.status_valve_B = CLOSE;
	}

	DRV8814_power_off();

}
//------------------------------------------------------------------------------------
void cpulse( char valve_id, int pulse_counter )
{

char valve = toupper(valve_id);
int contador, contador_actual;

	// Proporciono corriente.
	DRV8814_power_on();
	// Espero 10s que se carguen los condensasores
	vTaskDelay( ( TickType_t)( TIME_PWR_ON_VALVES / portTICK_RATE_MS ) );

	// Abro la valvula indicada
	DRV8814_vopen( valve, 100);
	xprintf_P( PSTR("Vopen %c\r\n\0"), valve );
	switch (valve_id) {
	case 'A':
		systemVars.status_valve_A = OPEN;
		break;
	case 'B':
		systemVars.status_valve_B = OPEN;
	}

	// Espero hasta que el contador correspondiente llegue al valor dado
	contador = 0;
	contador_actual = 0;
	while(1) {
		// Espero
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );
		// Leo el contador correspondiente
		switch(valve) {
		case 'A':
			contador_actual = u_readCounter(0);
			break;
		case 'B':
			contador_actual = u_readCounter(1);
			break;
		}

		// Veo si estoy contando...
		if ( contador_actual > contador ) {
			// Esta contando. OK
			contador = contador_actual;
		} else {
			// No esta contando. Salgo
			xprintf_P( PSTR("Conteo detenido\r\n\0"));
			goto EXIT;
		}

		// Veo si llegue al final de la cuenta...
		if ( contador >= pulse_counter ) {
			goto EXIT;
		}
	}

EXIT:
	// Cierro
	DRV8814_vclose( valve , 100);
	xprintf_P( PSTR("Vclose %c ( counter=%d) \r\n\0"), valve, contador );
	switch (valve_id) {
	case 'A':
		systemVars.status_valve_A = CLOSE;
		break;
	case 'B':
		systemVars.status_valve_B = CLOSE;
	}

	DRV8814_power_off();


}
//------------------------------------------------------------------------------------


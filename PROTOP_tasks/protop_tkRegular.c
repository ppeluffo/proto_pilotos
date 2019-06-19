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
static bool pBaja_estable( float presion_baja );

#define P_AVG_DIFF	0.05
#define MAX_PRES_COUNT	3
static float pStack[MAX_PRES_COUNT];

#define MAX_ROW 11

float Table_PW[MAX_ROW][2] = {
		{ 0.1, 0.3 },	// 0
		{ 0.100, 0.5 },	// 1
		{ 0.200, 1 },	// 2
		{ 0.300, 2 },	// 3
		{ 0.400, 2 },	// 4
		{ 0.500, 4 },	// 5
		{ 0.600, 4 },	// 6
		{ 0.700, 5  },	// 7
		{ 0.800, 6 },	// 8
		{ 0.900, 9 },	// 9
		{ 1.000, 10 },	// 10
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

	// Al iniciar vacio la cabeza del piloto y llevo la presion al minimo
	vclose('B');
	vopen('A');
	vTaskDelay( ( TickType_t)( 10000 / portTICK_RATE_MS ) );
	vclose('A');

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
			RTC_logprint();
			xprintf_P(PSTR("Mon: AN0=%.02f, AN1=%.02f, CNT0=%d, CNT1=%d\r\n\0"), presion_alta, presion_baja, u_readCounter(0), u_readCounter(1) );
		}

		u_clearCounter(0);
		u_clearCounter(1);

		// Espero al menos 3 valores de pB iguales, que la presion este estable
		if ( ! pBaja_estable(presion_baja) )
			continue;

		// La presion esta estable !!
		// Regulo ?
		if ( systemVars.regular == true ) {

			// Calculos
			// Diferencia de presion
			delta_P = fabs( systemVars.pout_ref - presion_baja );

			if ( delta_P < ( systemVars.p_band / 2 )  ) {
				// La presion esta estable. No debo regular
				RTC_logprint();
				xprintf_P(PSTR("Reg(in-band): deltaP = %.02f (%.02f - %.02f)\r\n\0"), delta_P, systemVars.pout_ref, presion_baja );
				// Espero 1 minuto
				waiting_ticks = (uint32_t)( 60000 / portTICK_RATE_MS );
				continue;
			}

			// Ancho de pulso a aplicar
			pulseW = pv_calcular_pulse_width( delta_P, systemVars.tipo_valvula_reguladora );
			RTC_logprint();
			xprintf_P(PSTR("Reg(out-band): deltaP = %.02f (%.02f - %.02f), pulse_width = %.02f\r\n\0"), delta_P, systemVars.pout_ref, presion_baja, pulseW );

			// Aplico correcciones.
			if ( presion_baja < systemVars.pout_ref ) {
				// Debo subir la presion
				//vpulse( 'B', pulseW );	// Accion directa
				vpulse( 'A', pulseW );	// Piloto
			} else if ( presion_baja > systemVars.pout_ref ) {
				// Debo bajar la presion
				//vpulse( 'A', pulseW );		// Accion directa
				vpulse( 'B', pulseW );		// Piloto
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

	// Como el piloto tiene limites en 1K y 2K, la presion fluctua entre estos valores.
	// Esto hace que el movimiento deba ser muy tenue.

float pw = 0.1;

/*
	if ( delta_P > 0.2 ) {
		pw = 0.1;
	} else if (delta_P > 0.15 ) {
		pw = 0.1;
	} else if ( delta_P > 0.1 ) {
		pw = 0.05;
	} else {
		pw = 0.01;
	}
*/
	return( pw);

}
//------------------------------------------------------------------------------------
float pv_calcular_pulse_width_01( float delta_P, float tipo_valvula_reguladora )
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
static bool pBaja_estable ( float presion_baja )
{
	// Manejo un stack circular local donde guardo la presion_baja.
	// Luego hago un promedio y si la diferencia es menor a 0.05 de la presion_actual
	// doy como estable la medida.

static uint8_t sPtr = 0;
float pAvg;
uint8_t i;
float dif_pres;
bool retS = false;

	// Guardo la presion en el stack y avanzo en modo circular
	pStack[sPtr++] = presion_baja;
	if (sPtr == MAX_PRES_COUNT ) {
		sPtr = 0;
	}

	// Promedio
	pAvg = 0;
	for (i = 0; i < MAX_PRES_COUNT; i++ ) {
		pAvg += pStack[i];
	}
	pAvg /= MAX_PRES_COUNT;

	dif_pres = abs ( pAvg - presion_baja);
	if ( dif_pres <= P_AVG_DIFF ) {
		retS = true;
		RTC_logprint();
		xprintf_P( PSTR("pAVG = %.02f, diff = %.02f ESTABLE\r\n\0"), pAvg, dif_pres );
	} else {
		retS = false;
		RTC_logprint();
		xprintf_P( PSTR("pAVG = %.02f, diff = %.02f IN-ESTABLE\r\n\0"), pAvg, dif_pres );
	}

	//xprintf_P( PSTR("pAVG = %.02f, diff = %.02f\r\n\0"), pAvg, dif_pres );

	return(retS);
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

	RTC_logprint();
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

	RTC_logprint();
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
void vpulse( char valve_id, float pulse_width_s )
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

	vTaskDelay( ( TickType_t)( pulse_width_s * 1000 / portTICK_RATE_MS ) );

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


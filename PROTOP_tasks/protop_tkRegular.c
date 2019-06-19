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
static float pv_calcular_pulse_width( float delta_P, float tipo_valvula_reguladora );
static bool pBaja_estable( float presion_baja );
static void espera_progresiva(bool action );

float presion_alta, presion_baja;
float delta_P;
float pulseW;

#define P_AVG_DIFF	0.05
#define MAX_PRES_COUNT	3
static float pStack[MAX_PRES_COUNT];

#define MAX_WAIT_LOOPS	5

//------------------------------------------------------------------------------------
void tkRegular(void * pvParameters)
{
/* Consideraciones generales:
 * 1-La P.alta influye en la velocidad de carga de la cabeza del piloto por lo tanto la duracion
 *   del pulso en subida de presion debe depender de pA. ( Falta )
 * 2-Tenemos 3 tipos de valvulas reguladoras ( grandes/medias/chicas ). Estas tiene diferente tamaño
 *   de camara por lo tanto el tiempo de llenado es diferente para c/tipo.
 *   Esto influye en el timerpoll por lo tanto al considerarlo hay que tener en cuenta la valvula.
 *   H) Grande: 1min, Media: 30s, Chica: 15s.
 * 3- Debemos poner una valvula de aguja a la entrada y salida de agua del cabezal del piloto.
 *   Inicialmente las ponemos al 50% para que de mas margen de regulacion a las electrovalvulas.
 * 4- Incorporamos una espera progresiva.
 *    Una vez en el rango de regulacion, aumentamos el tiempo de poleos hasta un maximo de 5 minutos
 *    para no consumir energia.
 * 5- Convergencia: ( Falta )
 *    Ponemos una forma de controlar cuantos pasos maximos usar para corregir la presion.
 *
 */
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

		// Leo las presiones hasta que sean estable
		while(1) {

			waiting_ticks = (uint32_t)(systemVars.timerPoll) * 1000 / portTICK_RATE_MS;
			vTaskDelay( waiting_ticks );

			// Leo presiones y caudales
			presion_alta = u_read_analog_channel (0);
			presion_baja = u_read_analog_channel (1);

			if ( systemVars.monitor == true ) {
				xprintf_P(PSTR("\r\n\0"));
				RTC_logprint();
				xprintf_P(PSTR("Mon: P.alta=%.02f, P.baja=%.02f\r\n\0"), presion_alta, presion_baja );
			}

			// Espero al menos 3 valores de pB iguales, que la presion este estable
			if ( pBaja_estable(presion_baja) )
				break;

		}

		// La presion esta estable !!
		// Regulo ?
		if ( systemVars.regular == true ) {

			// Calculos
			// Diferencia de presion
			delta_P = fabs( systemVars.pout_ref - presion_baja );

			if ( delta_P < systemVars.p_margen  ) {
				// No debo regular
				RTC_logprint();
				xprintf_P(PSTR("Reg(in-band): deltaP(%.02f) = %.02f (%.02f - %.02f)\r\n\0"),systemVars.p_margen, delta_P, systemVars.pout_ref, presion_baja );
				// Espero progresivamente de 1 a 5 minutos
				espera_progresiva(true);
				continue;

			} else {
				// Si debo regular
				pulseW = pv_calcular_pulse_width( delta_P, systemVars.tipo_valvula_reguladora );
				RTC_logprint();
				xprintf_P(PSTR("Reg(out-band): deltaP(%.02f) = %.02f (%.02f - %.02f), pulse_width = %.02f\r\n\0"), systemVars.p_margen, delta_P, systemVars.pout_ref, presion_baja, pulseW );

				// Aplico correcciones.
				if ( presion_baja < systemVars.pout_ref ) {
					// Debo subir la presion
					vpulse( 'A', pulseW );
				} else if ( presion_baja > systemVars.pout_ref ) {
					// Debo bajar la presion
						vpulse( 'B', pulseW );
				}
				// Reseteo la espera progresiva
				espera_progresiva(false);
			}

		} else {
			vTaskDelay( 60000 / portTICK_RATE_MS );
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

	// Al iniciar vacio la cabeza del piloto y llevo la presion al minimo
/*
	vclose('B');
	vopen('A');
	vTaskDelay( ( TickType_t)( 10000 / portTICK_RATE_MS ) );
	vclose('A');
*/
	vclose('A');
	vopen('B');
	vTaskDelay( ( TickType_t)( 20000 / portTICK_RATE_MS ) );
	vclose('B');
}
//------------------------------------------------------------------------------------
static float pv_calcular_pulse_width( float delta_P, float tipo_valvula_reguladora )
{

	// Como el piloto tiene limites en 1K y 2K, la presion fluctua entre estos valores.
	// Esto hace que el movimiento deba ser muy tenue.

float pw = 0.1;


	if ( delta_P > 0.2 ) {
		pw = 0.1;
	} else if (delta_P > 0.15 ) {
		pw = 0.7;
	} else if ( delta_P > 0.1 ) {
		pw = 0.05;
	} else {
		pw = 0.01;
	}

	return( pw);

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

	dif_pres = fabs ( pAvg - presion_baja);
	if ( dif_pres <= P_AVG_DIFF ) {
		retS = true;
	} else {
		retS = false;
	}

	RTC_logprint();
	xprintf_P( PSTR("pAVG=%.02f, diff=%.02f : { \0"), pAvg, dif_pres );
	for (i = 0; i < MAX_PRES_COUNT; i++ ) {
		xprintf_P( PSTR("p[%d]=%.02f \0"), i, pStack[i] );
	}
	if ( retS ) {
		xprintf_P( PSTR("} ESTABLE\r\n\0"));
	} else {
		xprintf_P( PSTR("} IN-ESTABLE\r\n\0"));
	}

	return(retS);
}
//------------------------------------------------------------------------------------
static void espera_progresiva(bool action )
{

static uint8_t wait_loops = 0;
uint8_t i;

	if ( action == true ) {
		if ( wait_loops++ > MAX_WAIT_LOOPS ) {
			wait_loops = MAX_WAIT_LOOPS;
		}

	} else {
		wait_loops = 0;
		//return;
	}

	// Espera
	RTC_logprint();
	xprintf_P( PSTR("Espera progresiva(%d)...\0"), wait_loops );
	for (i=0; i< wait_loops; i++) {
		vTaskDelay( 60000 / portTICK_RATE_MS );
	}
	xprintf_P( PSTR("End\r\n\0"), wait_loops );
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
	RTC_logprint();
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

	RTC_logprint();
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

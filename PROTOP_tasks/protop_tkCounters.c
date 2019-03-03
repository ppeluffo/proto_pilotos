/*
 * spx_counters.c
 *
 * Tarea de contadores:
 * Los contadores de pulsos estan conectados a los flujimetros de entrada y salida
 * de agua de la cabeza del piston.
 * Cuentan por interrupcion y muestran el conteo.
 *
 *
 */

#include "protop.h"

#define MAX_COUNTER_CHANNELS 2

static float counters[MAX_COUNTER_CHANNELS];
static void pv_tkCounter_init(void);

//------------------------------------------------------------------------------------
void tkCounter(void * pvParameters)
{

( void ) pvParameters;
uint32_t ulNotificationValue;
const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 10000 );

	// Espero la notificacion para arrancar
	while ( !startTask )
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );

	pv_tkCounter_init();

	xprintf_P( PSTR("starting tkCounter..\r\n\0"));

	// loop
	for( ;; )
	{

		// Cuando la interrupcion detecta un flanco, solo envia una notificacion
		// Espero que me avisen. Si no me avisaron en 10s salgo y repito el ciclo.
		// Esto es lo que me permite entrar en tickless.
		ulNotificationValue = ulTaskNotifyTake( pdTRUE, xMaxBlockTime );

		if( ulNotificationValue != 0 ) {
			// Fui notificado: llego algun flanco que determino.

			if ( cTask_wakeup_for_C0() ) {
				counters[0]++;
				reset_wakeup_for_C0();
			}

			if ( cTask_wakeup_for_C1() ) {
				counters[1]++;
				reset_wakeup_for_C1();
			}

			//xprintf_P( PSTR("COUNTERS: C0=%d,C1=%d\r\n\0"),(uint16_t) counters[0], (uint16_t) counters[1]);

			// Espero el tiempo de debounced
			vTaskDelay( ( TickType_t)( 10 / portTICK_RATE_MS ) );

			CNT_clr_CLRD();		// Borro el latch llevandolo a 0.
			CNT_set_CLRD();		// Lo dejo en reposo en 1

		}

	}
}
//------------------------------------------------------------------------------------
static void pv_tkCounter_init(void)
{
	// Configuracion inicial de la tarea

uint8_t i;

	COUNTERS_init( xHandle_tkCounter );

	for ( i = 0; i < MAX_COUNTER_CHANNELS; i++) {
		counters[ i ] = 0;
	}

}
//------------------------------------------------------------------------------------
// FUNCIONES PUBLICAS
//------------------------------------------------------------------------------------
void clearCounter(uint8_t counter_id)
{
	counters[counter_id] = 0;

}
//------------------------------------------------------------------------------------
uint16_t readCounter(uint8_t counter_id)
{
	return ((uint16_t) counters[counter_id] );
}
//------------------------------------------------------------------------------------


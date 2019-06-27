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

#define MODO_REGULACION ( systemVars.regular == true )
#define P_STACK_SIZE	3
#define TIME_PWR_ON_VALVES 10
#define P_AVG_DIFF	0.1
#define MAX_WAIT_LOOPS	5
#define MAX_TIMES_FOR_STABILITY	5

float pres_baja_data[ P_STACK_SIZE ];

typedef struct {
	float *data;
	uint8_t ptr;
	uint8_t size;
} stack_t;

stack_t pres_baja_s;

static void pv_regular_init(void);
static void pv_init_stack( stack_t *d_stack, float *data, const uint8_t size );
static void pv_push_stack( stack_t *d_stack, float data);
static float pv_get_stack_avg( stack_t *d_stack );
static void pv_print_stack( stack_t *d_stack );
static void pv_flush_stack (stack_t *d_stack );
static void pv_leer_presiones_estables( t_valvula_reguladora tipo_valvula_regualdora   );
static void regular_presion( void );
static float pv_calcular_pulse_width( float delta_P, float presion_alta );
static void espera_progresiva(bool action );

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
 * 3-Debemos poner una valvula de aguja a la entrada y salida de agua del cabezal del piloto.
 *   Inicialmente las ponemos al 50% para que de mas margen de regulacion a las electrovalvulas.
 * 4-Incorporamos una espera progresiva.
 *   Una vez en el rango de regulacion, aumentamos el tiempo de poleos hasta un maximo de 5 minutos
 *   para no consumir energia.
 * 5-Convergencia: ( Falta )
 *   Ponemos una forma de controlar cuantos pasos maximos usar para corregir la presion.
 * 6-Variacion de carga.
 *   Esto genera que la presion de baja oscile por lo tanto debemos tener un promedio estable y este
 *   usarlo como presion de baja y no el valor instantaneo. ( Falta )
 *
 *  21/06/2019 13:41:51: Mon: P.alta=-2.50, P.baja=1.12 pasamos a 1.54 !!!!
 */


( void ) pvParameters;


	// Espero la notificacion para arrancar
	while ( !startTask )
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );

	vTaskDelay( ( TickType_t)( 500 / portTICK_RATE_MS ) );

	xprintf_P( PSTR("starting tkRegular..\r\n\0"));

	pv_regular_init();
	pv_init_stack( &pres_baja_s, &pres_baja_data[0] , P_STACK_SIZE );

	// loop
	for( ;; )
	{

		// Espero leyendo las presiones hasta que tenga valores estables.
		pv_leer_presiones_estables( systemVars.tipo_valvula_reguladora );

		// La presion esta estable !!
		// Regulo ?
		if ( MODO_REGULACION ) {
			regular_presion();
		} else {
			vTaskDelay( 60000 / portTICK_RATE_MS );
		}
	}

}
//------------------------------------------------------------------------------------
// FUNCIONES PRIVADAS
//------------------------------------------------------------------------------------
static void pv_init_stack( stack_t *d_stack, float *data, const uint8_t size )
{

	// Asigno la estructura de datos al stack
	d_stack->data = data;
	// Asigno el tamaño
	d_stack->size = size;

	// Inicializo en 0 los datos
	for ( d_stack->ptr = 0; d_stack->ptr < d_stack->size; d_stack->ptr++ )
		d_stack->data[d_stack->ptr] = 0.0;

	// Inicializo el puntero.
	d_stack->ptr = 0;

}
//------------------------------------------------------------------------------------
static void pv_push_stack( stack_t *d_stack, float data)
{
	d_stack->data[d_stack->ptr++] = data;
	if ( d_stack->ptr == d_stack->size )
		d_stack->ptr = 0;

}
//------------------------------------------------------------------------------------
static float pv_get_stack_avg( stack_t *d_stack )
{

uint8_t i;
float avg;

	avg = 0.0;
	for ( i = 0; i < d_stack->size; i++ )
		avg += d_stack->data[i];

	avg /= d_stack->size;

	return(avg);
}
//------------------------------------------------------------------------------------
static void pv_print_stack( stack_t *d_stack )
{
uint8_t i;

	xprintf_P(PSTR("{ "));
	for ( i = 0; i < d_stack->size; i++ )
		xprintf_P(PSTR("p[%d]=%.02f "),i,d_stack->data[i]);
	xprintf_P(PSTR("}\r\n\0"));
}
//------------------------------------------------------------------------------------
static void pv_flush_stack (stack_t *d_stack )
{
	// Inicializo en 0 los datos
	for ( d_stack->ptr = 0; d_stack->ptr < d_stack->size; d_stack->ptr++ )
		d_stack->data[d_stack->ptr] = 0.0;

	// Inicializo el puntero.
	d_stack->ptr = 0;
}
//------------------------------------------------------------------------------------
static void pv_leer_presiones_estables( t_valvula_reguladora tipo_valvula_reguladora  )
{
	// Leo las presiones hasta que sean estable.
	// Criterio:
	// Mido 3 presiones y tomo como presion estable el promedio.

uint32_t waiting_ticks;
int8_t counts;

	counts = P_STACK_SIZE;

	pv_flush_stack( &pres_baja_s );

	while( counts-- > 0 ) {

		// Espero
		switch ( tipo_valvula_reguladora ) {
		case VR_CHICA:
			waiting_ticks = ( 15000 / portTICK_RATE_MS );
			break;
		case VR_MEDIA:
			waiting_ticks = ( 30000 / portTICK_RATE_MS );
			break;
		case VR_GRANDE:
			waiting_ticks = ( 45000 / portTICK_RATE_MS );
			break;
		default:
			waiting_ticks = ( 30000 / portTICK_RATE_MS );
			break;
		}

		//waiting_ticks = (uint32_t)(systemVars.timerPoll) * 1000 / portTICK_RATE_MS;
		vTaskDelay( waiting_ticks );

		// Leo presiones
		localVars.pA = u_read_analog_channel (0);
		localVars.pB = u_read_analog_channel (1);

		pv_push_stack( &pres_baja_s, localVars.pB );

		if ( systemVars.monitor == true ) {
			xprintf_P(PSTR("%s: Mon: P.alta=%.02f, P.baja=%.02f\r\n\0"), RTC_logprint(), localVars.pA, localVars.pB );
		}

	}

	xprintf_P( PSTR("%s: Mon: \0"), RTC_logprint());
	pv_print_stack( &pres_baja_s );

	localVars.pB_avg = pv_get_stack_avg( &pres_baja_s );
	xprintf_P( PSTR("%s: Mon: P.baja avg = %0.02f\r\n\0"), RTC_logprint(), localVars.pB_avg );

}
//------------------------------------------------------------------------------------
static void pv_regular_init(void)
{
	// Inicializo los INA con los que mido las entradas analogicas.
	AINPUTS_init( 0 );

	AINPUTS_prender_12V();
	INA_config(0, CONF_INAS_AVG128 );
	INA_config(1, CONF_INAS_AVG128 );

	set_lineal_zone( BAJA );

	localVars.VA_cnt = 0;
	localVars.VB_cnt = 0;

	pv_flush_stack(&pres_baja_s );
}
//------------------------------------------------------------------------------------
static float pv_calcular_pulse_width( float delta_P, float presion_alta )
{


	// De las graficas vemos que en la zona lineal, un pulso de 0.1 corresponde aprox. a 90gr.

float pw = 0.1;
float dP;

	dP = fabs(delta_P);

	if ( dP > 0.2 ) {
		pw = 0.2;
	} else if (dP > 0.15 ) {
		pw = 0.1;
	} else if ( dP > 0.1 ) {
		pw = 0.05;
	} else {
		pw = 0.01;
	}

	return( pw);

}
//------------------------------------------------------------------------------------
static void regular_presion( void )
{

float delta_P;
float pulseW;

	// Calculos
	// Diferencia de presion. Puede ser positiva o negativa
	delta_P = ( systemVars.pout_ref - localVars.pB_avg );

	if ( fabs(delta_P) <= systemVars.p_margen  ) {
		// No debo regular
		xprintf_P(PSTR("%s: Reg(in-band): deltaP(%.02f) = %.02f (%.02f - %.02f)\r\n\0"), RTC_logprint(), systemVars.p_margen, delta_P, systemVars.pout_ref, localVars.pB_avg );
		localVars.VA_cnt = 0;
		localVars.VB_cnt = 0;
		// Espero progresivamente de 1 a 5 minutos
		espera_progresiva(true);

	} else {
		// Si debo regular
		pulseW = pv_calcular_pulse_width( delta_P, localVars.pA );
		xprintf_P(PSTR("%s: Reg(out-band): deltaP(%.02f) = %.02f (%.02f - %.02f), pulse_width = %.02f\r\n\0"), RTC_logprint(), systemVars.p_margen, delta_P, systemVars.pout_ref, localVars.pB_avg, pulseW );

		// Aplico correcciones.
		if ( systemVars.pout_ref > localVars.pB_avg ) {
			// Delta > 0
			// Debo subir la presion ( llenar el piston )
			localVars.VA_cnt++;
			vpulse( 'A', pulseW );
		} else if ( systemVars.pout_ref < localVars.pB_avg ) {
			// Delta  < 0
			// Debo bajar la presion ( vaciar el piston )
			vpulse( 'B', pulseW );
			localVars.VB_cnt++;
		}

		// Reseteo la espera progresiva
		xprintf_P(PSTR("%s: Reg Pulsos: total=%d, VA=%d, VB=%d\r\n\0"), RTC_logprint(), (localVars.VA_cnt + localVars.VB_cnt), localVars.VA_cnt, localVars.VB_cnt );
		espera_progresiva(false);
	}

}
//------------------------------------------------------------------------------------
static void espera_progresiva(bool action )
{

static uint8_t wait_loops = 0;
uint16_t i;

	if ( action == true ) {
		if ( wait_loops++ > MAX_WAIT_LOOPS ) {
			wait_loops = MAX_WAIT_LOOPS;
		}

	} else {
		wait_loops = 0;
		//return;
	}

	// Espera
	xprintf_P( PSTR("%s: Espera progresiva(%d)\0"), RTC_logprint(), ( wait_loops * 60 ));
	for (i = 1; i<= ( wait_loops * 6 ); i++) {

		// c/10s muestro un tick.
		vTaskDelay( 10000 / portTICK_RATE_MS );
		if ( i % 6 == 0 ) {
			xprintf_P( PSTR("|"));	// Minutos
		} else {
			xprintf_P( PSTR("."));	// 10 secs.
		}

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

	xprintf_P( PSTR("%s: VALVE OPEN %c\r\n\0"), RTC_logprint(), valve_id );

	DRV8814_vopen( valve_id, 100);

	switch (valve_id) {
	case 'A':
		localVars.VA_status = OPEN;
		break;
	case 'B':
		localVars.VB_status = OPEN;
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

	xprintf_P( PSTR("%s: VALVE CLOSE %c\r\n\0"), RTC_logprint(), valve_id );

	DRV8814_vclose( valve_id, 100);

	switch (valve_id) {
	case 'A':
		localVars.VA_status = CLOSE;
		break;
	case 'B':
		localVars.VB_status = CLOSE;
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
	xprintf_P( PSTR("%s: Vopen %c\r\n\0"), RTC_logprint(), valve_id );

	switch (valve_id) {
	case 'A':
		localVars.VA_status = OPEN;
		break;
	case 'B':
		localVars.VB_status = OPEN;
	}

	vTaskDelay( ( TickType_t)( pulse_width_s * 1000 / portTICK_RATE_MS ) );

	DRV8814_vclose( toupper(valve_id), 100);

	xprintf_P( PSTR("%s: Vclose %c\r\n\0"), RTC_logprint(), valve_id );
	switch (valve_id) {
	case 'A':
		localVars.VA_status = CLOSE;
		break;
	case 'B':
		localVars.VB_status = CLOSE;
	}

	DRV8814_power_off();

}
//------------------------------------------------------------------------------------
void set_lineal_zone( t_init_limit zona )
{
	// Lleva el piloto a una zona de trabajo lineal.
	// Lo llena para llevar la presion al maximo y luego abre la valvula de escape por
	// 1.5s para llevarlo a unos 1.6K.

	xprintf_P( PSTR("%s: Set lineal_zone..\r\n\0"),RTC_logprint() );

	if ( zona == ALTA ) {
		// Al iniciar lleno la cabeza del piloto y llevo la presion al maximo
		vclose('B');
		vopen('A');
		vTaskDelay( ( TickType_t)( 10000 / portTICK_RATE_MS ) );
		vclose('A');
	} else {
		// Al iniciar vacio la cabeza del piloto y llevo la presion al minimo
		vclose('A');
		vopen('B');
		vTaskDelay( ( TickType_t)( 10000 / portTICK_RATE_MS ) );
		vclose('B');
	}

	// Espero
	vTaskDelay( ( TickType_t)( 3000 / portTICK_RATE_MS ) );

	if ( zona == ALTA ) {
		// Llevo la presion a la zona lineal
		vopen('B');
		vTaskDelay( ( TickType_t)( 1500 / portTICK_RATE_MS ) );
		vclose('B');
	} else {
		vopen('A');
		vTaskDelay( ( TickType_t)( 1500 / portTICK_RATE_MS ) );
		vclose('A');
	}

	vTaskDelay( ( TickType_t)( 10000 / portTICK_RATE_MS ) );

}
//------------------------------------------------------------------------------------
// DEPRECATED
/*
float pv_calcular_pulse_width_( float delta_P, float presion_alta )
{

	// Como el piloto tiene limites en 1K y 2K, la presion fluctua entre estos valores.
	// Esto hace que el movimiento deba ser muy tenue.
	//
	// Delta > 0
	// Debo subir la presion llenando el piston
	// Conviene subirla hasta el limite de la banda ya que si hay perdidas se va a bajar sola
	// En el llenado, el dx es proporcional al cuadrado del tiempo mas la presion de entrada
	//
	// Delta < 0
	// Debo bajar la presion vaciando el piston
	// Conviene dejarla arriba de la franja.
	// En vaciado usamos siempre un pulso constante de 0.1ms

	// Notas:
	// El ancho del pulso debe depender de la presion de alta

	// De las graficas vemos que en la zona lineal, un pulso de 0.1 corresponde aprox. a 90gr.
float pw = 0.1;

	// Corrección fina: menos de 50gr
	if ( fabs( delta_P ) < 0.05 ) {
		xprintf_P(PSTR("%s: Pulso corto\r\n\0"), RTC_logprint() );
		return(0.05);
	}

//	pw = 0.1;
//	return(pw);

	// Correccion gruesa: Mas de 50 gr
	// Vaciado con pulso fijo
	if ( delta_P < 0 ) {
		xprintf_P(PSTR("%s: Pulso ancho (bajada)\r\n\0"), RTC_logprint() );
		return(0.1);
	}

	// Llenado. Depende de la presion de alta.

	if ( delta_P > 0.2 ) {
		pw = 0.2;
	} else if (delta_P > 0.15 ) {
		pw = 0.7;
	} else if ( delta_P > 0.1 ) {
		pw = 0.05;
	} else {
		pw = 0.01;
	}

	xprintf_P(PSTR("%s: Pulso ancho (subida)\r\n\0"), RTC_logprint() );
	return( pw);

}
//------------------------------------------------------------------------------------
void pv_leer_presiones_estables_01( t_valvula_reguladora tipo_valvula_reguladora  )
{
	// Leo las presiones hasta que sean estable.
	// Para esto comparo la presion promedio del stack con la ultima medida y veo si estan
	// dentro de una banda.
	// Puede ocurrir que tenga mucha variacion por lo que nunca alcanzaria la condicion por
	// lo tanto espero hasta 5 medidas y salgo con lo que tenga.


uint32_t waiting_ticks;
float  dP;
int8_t counts;

	counts = MAX_TIMES_FOR_STABILITY;

	while( counts-- > 0 ) {

		// Espero
		switch ( tipo_valvula_reguladora ) {
		case VR_CHICA:
			waiting_ticks = ( 15000 / portTICK_RATE_MS );
			break;
		case VR_MEDIA:
			waiting_ticks = ( 30000 / portTICK_RATE_MS );
			break;
		case VR_GRANDE:
			waiting_ticks = ( 45000 / portTICK_RATE_MS );
			break;
		default:
			waiting_ticks = ( 30000 / portTICK_RATE_MS );
			break;
		}

		//waiting_ticks = (uint32_t)(systemVars.timerPoll) * 1000 / portTICK_RATE_MS;
		vTaskDelay( waiting_ticks );

		// Leo presiones
		localVars.pA = u_read_analog_channel (0);
		localVars.pB = u_read_analog_channel (1);

		pv_push_stack( &pres_baja_s, localVars.pB );

		// Espero una presion estable:
		// Esto lo defino como que varie menos de P_AVG_DIFF respecto de presion_baja instantanea
		presion_baja_avg = pv_get_stack_avg( &pres_baja_s );
		dP = fabs ( presion_baja_avg - localVars.pB );

		if ( systemVars.monitor == true ) {
			xprintf_P(PSTR("\r\n\0"));
			xprintf_P(PSTR("%s: Mon: P.alta=%.02f, P.baja=%.02f, pAVG=%.02f, dP=%.02f\r\n\0"), RTC_logprint(), localVars.pA, localVars.pB, presion_baja_avg, dP );
		}

		xprintf_P( PSTR("%s: Mon:\0"), RTC_logprint());
		pv_print_stack( &pres_baja_s );

		if ( dP <= P_AVG_DIFF ) {
			xprintf_P( PSTR(" ESTABLE\r\n\0"));
			return;
		} else {
			xprintf_P( PSTR("} IN-ESTABLE\r\n\0"));
		}

		//xprintf_P( PSTR("\r\n\0"));
	}

}
*/

//------------------------------------------------------------------------------------


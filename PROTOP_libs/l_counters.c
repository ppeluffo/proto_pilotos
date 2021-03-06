/*
 * l_counters.c
 *
 *  Created on: 8 dic. 2018
 *      Author: pablo
 */


#include "../PROTOP_libs/l_counters.h"

BaseType_t xHigherPriorityTaskWokenDigital = false;
TaskHandle_t countersTaskHandle;

//------------------------------------------------------------------------------------
void COUNTERS_init( TaskHandle_t taskHandle )
{
	CNT_config_CLRD();
	CNT_config_CNT0();
	CNT_config_CNT1();

//	PORTA.PIN2CTRL = PORT_OPC_PULLUP_gc | PORT_ISC_RISING_gc;	// Sensa rising edge
	PORTA.PIN2CTRL = PORT_OPC_PULLDOWN_gc | PORT_ISC_RISING_gc;	// Sensa rising edge
	PORTA.INT0MASK = PIN2_bm;
	PORTA.INTCTRL = PORT_INT0LVL0_bm;

//	PORTB.PIN2CTRL = PORT_OPC_PULLUP_gc | PORT_ISC_RISING_gc;	// Sensa rising edge
	PORTB.PIN2CTRL = PORT_OPC_PULLDOWN_gc | PORT_ISC_RISING_gc;	// Sensa rising edge
	PORTB.INT0MASK = PIN2_bm;
	PORTB.INTCTRL = PORT_INT0LVL0_bm;

	CNT_clr_CLRD();	// Borro el latch llevandolo a 0.
	CNT_set_CLRD();	// Lo dejo en reposo en 1

	// https://www.freertos.org/FreeRTOS_Support_Forum_Archive/June_2005/freertos_Get_task_handle_1311096.html
	// The task handle is just a pointer to the TCB of the task - but outside of tasks.c the type is hidden as a void*.

	countersTaskHandle = ( xTaskHandle ) taskHandle;

}
//------------------------------------------------------------------------------------
ISR ( PORTA_INT0_vect )
{
	// Esta ISR se activa cuando el contador D2 (PA2) genera un flaco se subida.
	// Solo avisa a la tarea principal ( que esta dormida ) que se levante y cuente
	// el pulso y haga el debounced.
	// Dado que los ISR de los 2 contadores son los que despiertan a la tarea, debo
	// indicarle de donde proviene el llamado

	//wakeup_for_C0 = true;
	//vTaskNotifyGiveFromISR( countersTaskHandle , &xHigherPriorityTaskWokenDigital );

	counter_C0++;		// Cuento
	CNT_clr_CLRD();		// Borro el latch
	PORTA.INTFLAGS = PORT_INT0IF_bm;	// Borro la interrupcion

}
//------------------------------------------------------------------------------------
ISR( PORTB_INT0_vect )
{
	// Esta ISR se activa cuando el contador D1 (PB2) genera un flaco se subida.
	// Solo avisa a la tarea principal ( que esta dormida ) que se levante y cuente
	// el pulso y haga el debounced.
	// Dado que los ISR de los 2 contadores son los que despiertan a la tarea, debo
	// indicarle de donde proviene el llamado

	//wakeup_for_C1 = true;
	//vTaskNotifyGiveFromISR( countersTaskHandle , &xHigherPriorityTaskWokenDigital );
	counter_C1++;
	CNT_clr_CLRD();
	PORTB.INTFLAGS = PORT_INT0IF_bm;

}
//------------------------------------------------------------------------------------
void COUNTERS_reset( uint8_t counter_id )
{
	if ( counter_id == 0 ) {
		counter_C0 = 0;
		return;
	}

	if ( counter_id == 1 ) {
		counter_C1 = 0;
		return;
	}
}
//------------------------------------------------------------------------------------
uint16_t COUNTERS_read( uint8_t counter_id )
{
	if ( counter_id == 0 ) {
		return( counter_C0);
	}

	if ( counter_id == 1 ) {
		return( counter_C1);
	}
}
//------------------------------------------------------------------------------------


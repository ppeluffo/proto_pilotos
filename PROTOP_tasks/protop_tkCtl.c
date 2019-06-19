/*
 * spx_tkCtl.c
 *
 *  Created on: 4 de oct. de 2017
 *      Author: pablo
 */

#include "protop.h"

//------------------------------------------------------------------------------------
static void pv_ctl_init_system(void);
static void pv_ctl_wink_led(void);
static void pv_ctl_wdg_reset(void);

//------------------------------------------------------------------------------------
void tkCtl(void * pvParameters)
{

( void ) pvParameters;

	vTaskDelay( ( TickType_t)( 500 / portTICK_RATE_MS ) );

	pv_ctl_init_system();

	xprintf_P( PSTR("\r\nstarting tkControl..\r\n\0"));

	for( ;; )
	{
		vTaskDelay( ( TickType_t)( 1000 / portTICK_RATE_MS ) );
		WDT_Reset();
		pv_ctl_wink_led();
		pv_ctl_wdg_reset();

	}
}
//------------------------------------------------------------------------------------
static void pv_ctl_init_system(void)
{


	// Configuro los pines del micro que no se configuran en funciones particulares
	// LEDS:
	IO_config_LED_KA();
	IO_config_LED_COMMS();

	// TERMINAL CTL PIN
	IO_config_TERMCTL_PIN();
	IO_config_TERMCTL_PULLDOWN();

	// BAUD RATE PIN
	IO_config_BAUD_PIN();

	// Luego del posible error del bus I2C espero para que se reponga !!!
	vTaskDelay( ( TickType_t)( 100 ) );

	// Leo los parametros del la EE y si tengo error, cargo por defecto
	if ( ! u_load_params_from_NVMEE() ) {
		u_load_defaults();
		xprintf_P( PSTR("\r\nLoading defaults !!\r\n\0"));
	}

	// Arranco el RTC. Si hay un problema lo inicializo.
	RTC_init();

	DRV8814_init();

	COUNTERS_init( NULL );

	// Habilito a arrancar al resto de las tareas
	startTask = true;

}
//------------------------------------------------------------------------------------
static void pv_ctl_wink_led(void)
{

	// Prendo los leds
	IO_set_LED_KA();

	vTaskDelay( ( TickType_t)( 50 / portTICK_RATE_MS ) );
	//taskYIELD();

	// Apago
	IO_clr_LED_KA();

}
//------------------------------------------------------------------------------------
static void pv_ctl_wdg_reset(void)
{
	WDT_Reset();

}
//------------------------------------------------------------------------------------


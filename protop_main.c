/*
 * main.c
 *
 *  Created on: 8 dic. 2018
 *      Author: pablo
 *
 *  avrdude -v -Pusb -c avrispmkII -p x256a3 -F -e -U flash:w:spxR4.hex
 *  avrdude -v -Pusb -c avrispmkII -p x256a3 -F -e
 *
 *  REFERENCIA: /usr/lib/avr/include/avr/iox256a3b.h
 *
 *  El watchdog debe estar siempre prendido por fuses.
 *  FF 0A FD __ F5 D4
 *
 *  PROGRAMACION FUSES:
 *  /usr/bin/avrdude -px256a3b -cavrispmkII -Pusb -u -Uflash:w:spx.hex:a -Ufuse0:w:0xff:m -Ufuse1:w:0x0:m -Ufuse2:w:0xff:m -Ufuse4:w:0xff:m -Ufuse5:w:0xff:m
 *  /usr/bin/avrdude -px256a3b -cavrispmkII -Pusb -u -Ufuse0:w:0xff:m -Ufuse1:w:0x0:m -Ufuse2:w:0xff:m -Ufuse4:w:0xff:m -Ufuse5:w:0xff:m
 *
 *  Para ver el uso de memoria usamos
 *  avr-nm -n spxR4.elf | more
 *
 *  Version 0.0.1.R1 @ 20190218
 *  - Las funciones de manejo de la NVM son las tomadas del AVR1605 y usadas en spxboot.
 *  - Mejoro las funciones de grabar el DLGID para que pongan un \0 al final del string.
 *  - En reset default el dlgid queda en DEFAULT
 *
 *  Pendiente:
 *  - sim password
 *  - Revisar OUTPUTS ( Consignas )
 *  - nuevo server script
 *	- revisar watchdogs
 *  - timerDial no corre para atras
 *  - Al grabar un firmware detecta un default pero quedan mal los canales.
 *
 */

#include "protop.h"

//------------------------------------------------------------------------------------
int main( void )
{
	// Clock principal del sistema
	u_configure_systemMainClock();
	u_configure_RTC32();

	// Configuramos y habilitamos el watchdog a 8s.
	WDT_EnableAndSetTimeout(  WDT_PER_8KCLK_gc );
	if ( WDT_IsWindowModeEnabled() )
		WDT_DisableWindowMode();


	// Inicializacion de los devices del frtos-io
	if ( BAUD_PIN_115200() ) {
		frtos_open(fdTERM, 115200 );
	} else {
		frtos_open(fdTERM, 9600 );
	}

	frtos_open(fdGPRS, 115200);
	frtos_open(fdI2C, 100 );

	// Creo los semaforos
	xprintf_init();

	startTask = false;

	// Creamos las tareas
	xTaskCreate(tkCtl, "CTL", tkCtl_STACK_SIZE, NULL, tkCtl_TASK_PRIORITY,  &xHandle_tkCtl );
	xTaskCreate(tkCmd, "CMD", tkCmd_STACK_SIZE, NULL, tkCmd_TASK_PRIORITY,  &xHandle_tkCmd);
//	xTaskCreate(tkCounter, "COUNT", tkCounter_STACK_SIZE, NULL, tkCounter_TASK_PRIORITY,  &xHandle_tkCounter);
	xTaskCreate(tkRegular, "REGU", tkRegular_STACK_SIZE, NULL, tkRegular_TASK_PRIORITY,  &xHandle_tkRegular);

	/* Arranco el RTOS. */
	vTaskStartScheduler();

	// En caso de panico, aqui terminamos.
	exit (1);

}
//------------------------------------------------------------------------------------
void vApplicationIdleHook( void )
{
	// Como trabajo en modo tickless no entro mas en modo sleep aqui.
//	if ( sleepFlag == true ) {
//		sleep_mode();
//	}
}
//------------------------------------------------------------------------------------
// Define the function that is called by portSUPPRESS_TICKS_AND_SLEEP().
//------------------------------------------------------------------------------------
void vApplicationStackOverflowHook( TaskHandle_t xTask, signed char *pcTaskName )
{
	// Es invocada si en algun context switch se detecta un stack corrompido !!
	// Cuando el sistema este estable la removemos.
	// En FreeRTOSConfig.h debemos habilitar
	// #define configCHECK_FOR_STACK_OVERFLOW          2

//	xprintf_P( PSTR("PANIC:%s !!\r\n\0"),pcTaskName);

}
//------------------------------------------------------------------------------------

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
//------------------------------------------------------------------------------------

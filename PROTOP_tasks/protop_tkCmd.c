/*
 * sp5K_tkCmd.c
 *
 *  Created on: 27/12/2013
 *      Author: root
 */

#include "protop.h"

//----------------------------------------------------------------------------------------
// FUNCIONES DE USO PRIVADO
//----------------------------------------------------------------------------------------
static void pv_snprintfP_OK(void );
static void pv_snprintfP_ERR(void);
static void pv_cmd_write_valve(void);

//----------------------------------------------------------------------------------------
// FUNCIONES DE CMDMODE
//----------------------------------------------------------------------------------------
static void cmdHelpFunction(void);
static void cmdClearScreen(void);
static void cmdResetFunction(void);
static void cmdWriteFunction(void);
static void cmdReadFunction(void);
static void cmdStatusFunction(void);
static void cmdConfigFunction(void);

RtcTimeType_t rtc;

//------------------------------------------------------------------------------------
void tkCmd(void * pvParameters)
{

uint8_t c;
uint8_t ticks;

( void ) pvParameters;

	// Espero la notificacion para arrancar
	while ( !startTask )
		vTaskDelay( ( TickType_t)( 100 / portTICK_RATE_MS ) );

	vTaskDelay( ( TickType_t)( 500 / portTICK_RATE_MS ) );

	FRTOS_CMD_init();

	// Registro los comandos y los mapeo al cmd string.
	FRTOS_CMD_register( "cls\0", cmdClearScreen );
	FRTOS_CMD_register( "reset\0", cmdResetFunction);
	FRTOS_CMD_register( "write\0", cmdWriteFunction);
	FRTOS_CMD_register( "read\0", cmdReadFunction);
	FRTOS_CMD_register( "help\0", cmdHelpFunction );
	FRTOS_CMD_register( "status\0", cmdStatusFunction );
	FRTOS_CMD_register( "config\0", cmdConfigFunction );

	// Fijo el timeout del READ
	ticks = 5;
	frtos_ioctl( fdTERM,ioctl_SET_TIMEOUT, &ticks );

	xprintf_P( PSTR("starting tkCmd..\r\n\0") );

	//FRTOS_CMD_regtest();
	// loop
	for( ;; )
	{

		c = '\0';	// Lo borro para que luego del un CR no resetee siempre el timer.
		// el read se bloquea 50ms. lo que genera la espera.
		//while ( CMD_read( (char *)&c, 1 ) == 1 ) {
		while ( frtos_read( fdTERM, (char *)&c, 1 ) == 1 ) {
			FRTOS_CMD_process(c);
		}
	}
}
//------------------------------------------------------------------------------------
static void cmdStatusFunction(void)
{

uint8_t channel;

	xprintf_P( PSTR("\r\nSpymovil %s %s %s %s \r\n\0"), SPX_HW_MODELO, SPX_FTROS_VERSION, SPX_FW_REV, SPX_FW_DATE);
	xprintf_P( PSTR("Clock %d Mhz, Tick %d Hz\r\n\0"),SYSMAINCLK, configTICK_RATE_HZ );

	// SIGNATURE ID
	xprintf_P( PSTR("uID=%s\r\n\0"), NVMEE_readID() );

	RTC_read_time();

	// Timerpoll
	xprintf_P( PSTR("timerPoll: %d s\r\n\0"),systemVars.timerPoll );

	// aninputs
	for ( channel = 0; channel < MAX_ANALOG_CHANNELS; channel++) {
		xprintf_P( PSTR("AN%d [%d-%d mA/ %.02f,%.02f]\r\n\0"),channel, systemVars.ain_imin[channel],systemVars.ain_imax[channel],systemVars.ain_mmin[channel],systemVars.ain_mmax[channel] );
	}

	// Medidas
	xprintf_P( PSTR("--------------------\r\n\0"));

	// Analog
	xprintf_P(PSTR("AN0=%.02f, AN1=%.02f\r\n\0"), readAin(0), readAin(1) );

	// Contadores
	xprintf_P(PSTR("CNT0=%d, CNT1=%d\r\n\0"), readCounter(0), readCounter(1) );


}
//-----------------------------------------------------------------------------------
static void cmdResetFunction(void)
{
	FRTOS_CMD_makeArgv();

	// Reset counters
	if (!strcmp_P( strupr(argv[1]), PSTR("COUNTERS\0"))) {
		clearCounter(0);
		clearCounter(1);
		return;
	}

//	xprintf_P( PSTR("ERROR\r\nRESET NOT DEFINED\r\n\0"));

	cmdClearScreen();
	CCPWrite( &RST.CTRL, RST_SWRST_bm );   /* Issue a Software Reset to initilize the CPU */
}
//------------------------------------------------------------------------------------
static void cmdWriteFunction(void)
{

	FRTOS_CMD_makeArgv();

	// RTC
	// write rtc YYMMDDhhmm
	if (!strcmp_P( strupr(argv[1]), PSTR("RTC\0")) ) {
		( RTC_write_time( argv[2]) > 0)?  pv_snprintfP_OK() : 	pv_snprintfP_ERR();
		return;
	}

	// VALVE
	// write valve (enable|disable),(set|reset),(sleep|awake),(ph01|ph10) } {A/B}
	//             (open|close) (A|B) (ms)
	//              power {on|off}
	if (!strcmp_P( strupr(argv[1]), PSTR("VALVE\0")) ) {
		pv_cmd_write_valve();
		return;
	}

	// write vpulse (A|B) (s)
	if (!strcmp_P( strupr(argv[1]), PSTR("VPULSE\0")) ) {

		// Proporciono corriente.
		DRV8814_power_on();
		// Espero 10s que se carguen los condensasores
		vTaskDelay( ( TickType_t)( 5000 / portTICK_RATE_MS ) );

		DRV8814_vopen( toupper(argv[2][0]), 100);
		xprintf_P( PSTR("Vopen..."));
		vTaskDelay( ( TickType_t)( atoi(argv[3]) * 1000 / portTICK_RATE_MS ) );
		DRV8814_vclose( toupper(argv[2][0]), 100);
		xprintf_P( PSTR("Vclose\r\n\0"));
		DRV8814_power_off();
		pv_snprintfP_OK();
		return;
	}

	// CMD NOT FOUND
	xprintf_P( PSTR("ERROR\r\nCMD NOT DEFINED\r\n\0"));
	return;
}
//------------------------------------------------------------------------------------
static void cmdReadFunction(void)
{

	FRTOS_CMD_makeArgv();

	// Contadores
	if (!strcmp_P( strupr(argv[1]), PSTR("COUNTERS\0"))) {
		xprintf_P(PSTR("CNT0=%d, CNT1=%d\r\n\0"), readCounter(0), readCounter(1) );
 		return;
 	}

	// CMD NOT FOUND
	xprintf_P( PSTR("ERROR\r\nCMD NOT DEFINED\r\n\0"));
	return;

}
//------------------------------------------------------------------------------------
static void cmdClearScreen(void)
{
	// ESC [ 2 J
	xprintf_P( PSTR("\x1B[2J\0"));
}
//------------------------------------------------------------------------------------
static void cmdConfigFunction(void)
{

	FRTOS_CMD_makeArgv();

	// DEFAULT
	// config default
	if (!strcmp_P( strupr(argv[1]), PSTR("DEFAULT\0"))) {
		u_load_defaults();
		pv_snprintfP_OK();
		return;
	}

	// SAVE
	// config save
	if (!strcmp_P( strupr(argv[1]), PSTR("SAVE\0"))) {
		u_save_params_in_NVMEE();
		pv_snprintfP_OK();
		return;
	}

	// TIMERPOLL
	// config timerpoll
	if (!strcmp_P( strupr(argv[1]), PSTR("TIMERPOLL\0")) ) {
		u_config_timerpoll( argv[2] );
		pv_snprintfP_OK();
		return;
	}

	// ANALOG
	// config analog {0..n} imin imax mmin mmax
	if (!strcmp_P( strupr(argv[1]), PSTR("ANALOG\0")) ) {
		u_config_analog_channel( atoi(argv[2]), argv[3], argv[4], argv[5], argv[6] ) ? pv_snprintfP_OK() : pv_snprintfP_ERR();
		return;
	}


	pv_snprintfP_ERR();
	return;
}
//------------------------------------------------------------------------------------
static void cmdHelpFunction(void)
{

	FRTOS_CMD_makeArgv();

	// HELP WRITE
	if (!strcmp_P( strupr(argv[1]), PSTR("WRITE\0"))) {
		xprintf_P( PSTR("-write\r\n\0"));
		xprintf_P( PSTR("  rtc YYMMDDhhmm\r\n\0"));
		xprintf_P( PSTR("  valve (enable|disable),(set|reset),(sleep|awake),(ph01|ph10) } {A/B}\r\n\0"));
		xprintf_P( PSTR("        (open|close) (A|B) (ms)\r\n\0"));
		xprintf_P( PSTR("        power {on|off}\r\n\0"));
		xprintf_P( PSTR("  vpulse (A|B) (s)\r\n\0"));
		return;
	}

	if (!strcmp_P( strupr(argv[1]), PSTR("READ\0"))) {
		xprintf_P( PSTR("-read\r\n\0"));
		xprintf_P( PSTR("  counters\r\n\0"));
		return;
	}

	// HELP RESET
	else if (!strcmp_P( strupr(argv[1]), PSTR("RESET\0"))) {
		xprintf_P( PSTR("-reset\r\n\0"));
		xprintf_P( PSTR("   counters\r\n\0"));
		return;
	}

	// HELP CONFIG
	else if (!strcmp_P( strupr(argv[1]), PSTR("CONFIG\0"))) {
		xprintf_P( PSTR("-config\r\n\0"));
		xprintf_P( PSTR("  analog {0..%d} imin imax mmin mmax\r\n\0"),( MAX_ANALOG_CHANNELS - 1 ) );
		xprintf_P( PSTR("  timerpoll {val}\r\n\0"));
		xprintf_P( PSTR("  default\r\n\0"));
		xprintf_P( PSTR("  save\r\n\0"));

	} else {

		// HELP GENERAL
		xprintf_P( PSTR("\r\nSpymovil %s %s %s %s\r\n\0"), SPX_HW_MODELO, SPX_FTROS_VERSION, SPX_FW_REV, SPX_FW_DATE);
		xprintf_P( PSTR("Clock %d Mhz, Tick %d Hz\r\n\0"),SYSMAINCLK, configTICK_RATE_HZ );
		xprintf_P( PSTR("Available commands are:\r\n\0"));
		xprintf_P( PSTR("-cls\r\n\0"));
		xprintf_P( PSTR("-help\r\n\0"));
		xprintf_P( PSTR("-status\r\n\0"));
		xprintf_P( PSTR("-reset...\r\n\0"));
		xprintf_P( PSTR("-write...\r\n\0"));
		xprintf_P( PSTR("-read...\r\n\0"));
		xprintf_P( PSTR("-config...\r\n\0"));

	}

	xprintf_P( PSTR("\r\n\0"));

}
//------------------------------------------------------------------------------------
static void pv_cmd_write_valve(void)
{
	// write valve (enable|disable),(set|reset),(sleep|awake),(ph01|ph10) } {A/B}
	//             (open|close) (A|B) (ms)
	//              power {on|off}

	// write valve enable (A|B)
	if (!strcmp_P( strupr(argv[2]), PSTR("ENABLE\0")) ) {
		DRV8814_enable_pin( toupper(argv[3][0]), 1); pv_snprintfP_OK();
		return;
	}

	// write valve disable (A|B)
	if (!strcmp_P( strupr(argv[2]), PSTR("DISABLE\0")) ) {
		DRV8814_enable_pin( toupper(argv[3][0]), 0); pv_snprintfP_OK();
		return;
	}

	// write valve set
	if (!strcmp_P( strupr(argv[2]), PSTR("SET\0")) ) {
		DRV8814_reset_pin(1); pv_snprintfP_OK();
		return;
	}

	// write valve reset
	if (!strcmp_P( strupr(argv[2]), PSTR("RESET\0")) ) {
		DRV8814_reset_pin(0); pv_snprintfP_OK();
		return;
	}

	// write valve sleep
	if (!strcmp_P( strupr(argv[2]), PSTR("SLEEP\0")) ) {
		DRV8814_sleep_pin(1);  pv_snprintfP_OK();
		return;
	}

	// write valve awake
	if (!strcmp_P( strupr(argv[2]), PSTR("AWAKE\0")) ) {
		DRV8814_sleep_pin(0);  pv_snprintfP_OK();
		return;
	}

	// write valve ph01 (A|B)
	if (!strcmp_P( strupr(argv[2]), PSTR("PH01\0")) ) {
		DRV8814_phase_pin( toupper(argv[3][0]), 1);  pv_snprintfP_OK();
		return;
	}

	// write valve ph10 (A|B)
	if (!strcmp_P( strupr(argv[2]), PSTR("PH10\0")) ) {
		DRV8814_phase_pin( toupper(argv[3][0]), 0);  pv_snprintfP_OK();
		return;
	}

	// write valve power on|off
	if (!strcmp_P( strupr(argv[2]), PSTR("POWER\0")) ) {

		if (!strcmp_P( strupr(argv[3]), PSTR("ON\0")) ) {
			DRV8814_power_on();
			pv_snprintfP_OK();
			return;
		}
		if (!strcmp_P( strupr(argv[3]), PSTR("OFF\0")) ) {
			DRV8814_power_off();
			pv_snprintfP_OK();
			return;
		}
		pv_snprintfP_ERR();
		return;
	}

	//  write valve (open|close) (A|B) (ms)
	if (!strcmp_P( strupr(argv[2]), PSTR("VALVE\0")) ) {

		// Proporciono corriente.
		DRV8814_power_on();
		// Espero 10s que se carguen los condensasores
		vTaskDelay( ( TickType_t)( 5000 / portTICK_RATE_MS ) );

		if (!strcmp_P( strupr(argv[3]), PSTR("OPEN\0")) ) {
			DRV8814_vopen( toupper(argv[4][0]), 100);
			return;
		}
		if (!strcmp_P( strupr(argv[3]), PSTR("CLOSE\0")) ) {
			DRV8814_vclose( toupper(argv[4][0]), 100);
			return;
		}

		DRV8814_power_off();
		return;
	}

	pv_snprintfP_ERR();
	return;

}
//------------------------------------------------------------------------------------

static void pv_snprintfP_OK(void )
{
	xprintf_P( PSTR("ok\r\n\0"));
}
//------------------------------------------------------------------------------------
static void pv_snprintfP_ERR(void)
{
	xprintf_P( PSTR("error\r\n\0"));
}
//------------------------------------------------------------------------------------

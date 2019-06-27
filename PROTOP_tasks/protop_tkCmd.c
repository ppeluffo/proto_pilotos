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

//----------------------------------------------------------------------------------------
// FUNCIONES DE CMDMODE
//----------------------------------------------------------------------------------------
static void cmdHelpFunction(void);
static void cmdClearScreen(void);
static void cmdResetFunction(void);
static void cmdStatusFunction(void);
static void cmdConfigFunction(void);
static void cmdValveFunction(void);

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

	FRTOS_CMD_init();

	// Registro los comandos y los mapeo al cmd string.
	FRTOS_CMD_register( "cls\0", cmdClearScreen );
	FRTOS_CMD_register( "reset\0", cmdResetFunction);
	FRTOS_CMD_register( "help\0", cmdHelpFunction );
	FRTOS_CMD_register( "status\0", cmdStatusFunction );
	FRTOS_CMD_register( "config\0", cmdConfigFunction );
	FRTOS_CMD_register( "valve\0", cmdValveFunction);
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

	// Pout Ref
	xprintf_P( PSTR("Pout reference: %.02f\r\n\0"), systemVars.pout_ref );

	// Tipo Valvula Reguladora
	switch( systemVars.tipo_valvula_reguladora) {
	case VR_CHICA:
		xprintf_P( PSTR("tipo reguladora: CHICA\r\n\0") );
		break;
	case VR_MEDIA:
		xprintf_P( PSTR("tipo reguladora: MEDIA\r\n\0") );
		break;
	case VR_GRANDE:
		xprintf_P( PSTR("tipo reguladora: GRANDE\r\n\0") );
		break;
	}

	// Regulacion
	if ( systemVars.regular == true ) {
		xprintf_P( PSTR("regulacion: on\r\n\0"));
	} else {
		xprintf_P( PSTR("regulacion: off\r\n\0"));
	}

	// P_MARGEN
	xprintf_P( PSTR("margen regulacion: %.02f\r\n\0"), systemVars.p_margen );

	// Monitor
	if ( systemVars.monitor == true ) {
		xprintf_P( PSTR("monitor: on\r\n\0"));
	} else {
		xprintf_P( PSTR("monitor: off\r\n\0"));
	}

	// aninputs
	for ( channel = 0; channel < MAX_ANALOG_CHANNELS; channel++) {
		xprintf_P( PSTR("AN%d [%d-%d mA/ %.02f,%.02f]\r\n\0"),channel, systemVars.ain_imin[channel],systemVars.ain_imax[channel],systemVars.ain_mmin[channel],systemVars.ain_mmax[channel] );
	}

	// Medidas
	xprintf_P( PSTR("--------------------\r\n\0"));

	// Estado de las valvulas
	if ( localVars.VA_status == OPEN ) {
		xprintf_P(PSTR("Valve A: OPEN, \0"));
	} else {
		xprintf_P(PSTR("Valve A: CLOSE, \0"));
	}
	if ( localVars.VB_status == OPEN ) {
		xprintf_P(PSTR("Valve B: OPEN \r\n\0"));
	} else {
		xprintf_P(PSTR("Valve B: CLOSE \r\n\0"));
	}

	// Analog
	xprintf_P(PSTR("P.alta=%.02f, P.baja=%.02f\r\n\0"), u_readAin(0), u_readAin(1) );

}
//-----------------------------------------------------------------------------------
static void cmdResetFunction(void)
{
	FRTOS_CMD_makeArgv();
	cmdClearScreen();
	CCPWrite( &RST.CTRL, RST_SWRST_bm );   /* Issue a Software Reset to initilize the CPU */
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

	// TIPO VALVULA REGULADORA
	// config reguladora (chica,media,grande)
	if (!strcmp_P( strupr(argv[1]), PSTR("REGULADORA\0")) ) {
		if (!strcmp_P( strupr(argv[2]), PSTR("CHICA\0")) ) {
			systemVars.tipo_valvula_reguladora = VR_CHICA;
			pv_snprintfP_OK();
			return;
		}

		if (!strcmp_P( strupr(argv[2]), PSTR("MEDIA\0")) ) {
			systemVars.tipo_valvula_reguladora = VR_MEDIA;
			pv_snprintfP_OK();
			return;
		}

		if (!strcmp_P( strupr(argv[2]), PSTR("GRANDE\0")) ) {
			systemVars.tipo_valvula_reguladora = VR_GRANDE;
			pv_snprintfP_OK();
			return;
		}

		pv_snprintfP_ERR();
		return;

	}

	// REGULAR
	// config regular on/off
	if (!strcmp_P( strupr(argv[1]), PSTR("REGULAR\0")) ) {
		if (!strcmp_P( strupr(argv[2]), PSTR("ON\0")) ) {
			systemVars.regular = true;
			pv_snprintfP_OK();
			return;
		}

		if (!strcmp_P( strupr(argv[2]), PSTR("OFF\0")) ) {
			systemVars.regular = false;
			pv_snprintfP_OK();
			return;
		}
		pv_snprintfP_ERR();
		return;

	}

	// MONITOR
	// config monitor on/off
	if (!strcmp_P( strupr(argv[1]), PSTR("MONITOR\0")) ) {
		if (!strcmp_P( strupr(argv[2]), PSTR("ON\0")) ) {
			systemVars.monitor = true;
			pv_snprintfP_OK();
			return;
		}

		if (!strcmp_P( strupr(argv[2]), PSTR("OFF\0")) ) {
			systemVars.monitor = false;
			pv_snprintfP_OK();
			return;
		}
		pv_snprintfP_ERR();
		return;

	}

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

	// POUT
	// config pout
	if (!strcmp_P( strupr(argv[1]), PSTR("POUT\0")) ) {
		systemVars.pout_ref = atof( argv[2] );
		pv_snprintfP_OK();
		return;
	}

	// PMARGEN
	// config pmargen
	if (!strcmp_P( strupr(argv[1]), PSTR("PMARGEN\0")) ) {
		systemVars.p_margen = atof( argv[2] );
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

	xprintf_P( PSTR("\r\nSpymovil %s %s %s %s\r\n\0"), SPX_HW_MODELO, SPX_FTROS_VERSION, SPX_FW_REV, SPX_FW_DATE);
	xprintf_P( PSTR("Clock %d Mhz, Tick %d Hz\r\n\0"),SYSMAINCLK, configTICK_RATE_HZ );

	xprintf_P( PSTR("Available commands are:\r\n\0"));
	xprintf_P( PSTR("-cls\r\n\0"));
	xprintf_P( PSTR("-help\r\n\0"));
	xprintf_P( PSTR("-status\r\n\0"));

	xprintf_P( PSTR("-config\r\n\0"));
	xprintf_P( PSTR("  analog {0..%d} imin imax mmin mmax\r\n\0"),( MAX_ANALOG_CHANNELS - 1 ) );
	xprintf_P( PSTR("  timerpoll {val}\r\n\0"));
	xprintf_P( PSTR("  default\r\n\0"));
	xprintf_P( PSTR("  regular (ON | OFF)\r\n\0"));
	xprintf_P( PSTR("  monitor (ON | OFF)\r\n\0"));
	xprintf_P( PSTR("  pout {val}, pmargen {val} \r\n\0"));
	xprintf_P( PSTR("  reguladora {chica | media | grande} \r\n\0"));
	xprintf_P( PSTR("  save\r\n\0"));

	xprintf_P( PSTR("-valve\r\n\0"));
	xprintf_P( PSTR("  (enable|disable),(set|reset),(sleep|awake),(ph01|ph10) } {A/B}\r\n\0"));
	xprintf_P( PSTR("  (open|close) (A|B)\r\n\0"));
	xprintf_P( PSTR("  power {on|off}\r\n\0"));
	xprintf_P( PSTR("  vpulse (A|B) (s)\r\n\0"));
	xprintf_P( PSTR("  init\r\n\0"));

	xprintf_P( PSTR("\r\n\0"));

}
//------------------------------------------------------------------------------------
static void cmdValveFunction(void)
{

	// valve (enable|disable),(set|reset),(sleep|awake),(ph01|ph10) } {A/B}
	//       (open|close) (A|B) (ms)
	//       power {on|off}

	FRTOS_CMD_makeArgv();

	// valve enable (A|B)
	if (!strcmp_P( strupr(argv[1]), PSTR("ENABLE\0")) ) {
		DRV8814_enable_pin( toupper(argv[2][0]), 1); pv_snprintfP_OK();
		return;
	}

	// valve disable (A|B)
	if (!strcmp_P( strupr(argv[1]), PSTR("DISABLE\0")) ) {
		DRV8814_enable_pin( toupper(argv[2][0]), 0); pv_snprintfP_OK();
		return;
	}

	// valve set
	if (!strcmp_P( strupr(argv[1]), PSTR("SET\0")) ) {
		DRV8814_reset_pin(1); pv_snprintfP_OK();
		return;
	}

	// valve reset
	if (!strcmp_P( strupr(argv[1]), PSTR("RESET\0")) ) {
		DRV8814_reset_pin(0); pv_snprintfP_OK();
		return;
	}

	// valve sleep
	if (!strcmp_P( strupr(argv[1]), PSTR("SLEEP\0")) ) {
		DRV8814_sleep_pin(1);  pv_snprintfP_OK();
		return;
	}

	// valve awake
	if (!strcmp_P( strupr(argv[1]), PSTR("AWAKE\0")) ) {
		DRV8814_sleep_pin(0);  pv_snprintfP_OK();
		return;
	}

	// valve ph01 (A|B)
	if (!strcmp_P( strupr(argv[2]), PSTR("PH01\0")) ) {
		DRV8814_phase_pin( toupper(argv[3][0]), 1);  pv_snprintfP_OK();
		return;
	}

	// valve ph10 (A|B)
	if (!strcmp_P( strupr(argv[1]), PSTR("PH10\0")) ) {
		DRV8814_phase_pin( toupper(argv[2][0]), 0);  pv_snprintfP_OK();
		return;
	}

	// valve power on|off
	if (!strcmp_P( strupr(argv[1]), PSTR("POWER\0")) ) {

		if (!strcmp_P( strupr(argv[2]), PSTR("ON\0")) ) {
			DRV8814_power_on();
			pv_snprintfP_OK();
			return;
		}
		if (!strcmp_P( strupr(argv[2]), PSTR("OFF\0")) ) {
			DRV8814_power_off();
			pv_snprintfP_OK();
			return;
		}
		pv_snprintfP_ERR();
		return;
	}

	//  valve (open|close) (A|B) (ms)
	if (!strcmp_P( strupr(argv[1]), PSTR("OPEN\0")) ) {
		vopen( toupper(argv[2][0] ) );
		return;
	}

	if (!strcmp_P( strupr(argv[1]), PSTR("CLOSE\0")) ) {
		vclose( toupper(argv[2][0] ) );
		return;
	}

	// vpulse (A|B) (s)
	if (!strcmp_P( strupr(argv[1]), PSTR("VPULSE\0")) ) {
		vpulse( toupper(argv[2][0] ), atof(argv[3]) );
		return;
	}

	// reset
	// Cierra las 2 valvulas
	if (!strcmp_P( strupr(argv[1]), PSTR("INIT\0")) ) {
		// Borramos los contadores antes de c/pulso de valvulas
		// para tener el agua que entro/salio en el pulso
		vclose( 'B' );
		vclose( 'A' );
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

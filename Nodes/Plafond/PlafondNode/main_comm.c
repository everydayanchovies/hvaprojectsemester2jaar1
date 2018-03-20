/*
 * PlafondNode.c
 *
 * Created: 5-3-2018 09:05:29
 * Author : Max
 */ 

#define F_CPU 32000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sfr_defs.h>
#include <stddef.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "nrf24spiXM2.h"
#include "nrf24L01.h"
#include "clock.h"
#include "stream.h"
#include <ctype.h>

#define DEBOUNCE_PERIOD_MS	10		
#define LOCK_PERIOD_MS		200		

#define SENSOR_NAME			"PL"

#define C_PRESSED		"Pr"
#define C_PRINT			"Pf"

#define N_MOVEMENT		"Mv"
#define N_SMOKE			"MS"
#define N_HIGH_CO		"MC"


uint8_t  pipes[][6] = {
	"AVH01",
	"AVH02"
};

uint8_t  packet[32];

void init_pwm(void);
void init_nrf(void);
void init_adc(void);
void read_adcs(uint16_t *res);
int button_pressed(void);
char* concat(const char *s1, const char *s2);
void stuur(char* command_id, char* command_data);

int main(void)
{
    Config32MHzClock_Ext16M();
    init_adc();
    init_pwm();
    init_nrf();

    init_stream(F_CPU);
    PMIC.CTRL |= PMIC_LOLVLEN_bm;           // Low level interrupt
    sei();

    clear_screen();
    
    printf("Sensor %s (Slave)\n", SENSOR_NAME);

    while (1) {
	    if ( button_pressed() ) {
		    printf("Button pressed!\n");
		    
		    stuur(C_PRINT, "Hooi!");
	    }
    }
}

ISR(PORTF_INT0_vect)
{
	uint8_t tx, fail, rx;
	static uint8_t  message_count = 0;

	nrfWhatHappened(&tx, &fail, &rx);
	
	printf("interrupt : tx(%d) fail(%d) rx(%d)\n", tx, fail, rx);
}

void init_pwm(void)
{
	PORTD.DIRCLR   = PIN3_bm;            // input pin switch
	PORTD.PIN3CTRL = PORT_OPC_PULLUP_gc; // enable pull up
}

void init_nrf(void)
{
	nrfspiInit();
	
	nrfBegin();

	nrfSetRetries(NRF_SETUP_ARD_1000US_gc, NRF_SETUP_ARC_8RETRANSMIT_gc);
	nrfSetPALevel(NRF_RF_SETUP_PWR_6DBM_gc);
	nrfSetDataRate(NRF_RF_SETUP_RF_DR_250K_gc);
	nrfSetCRCLength(NRF_CONFIG_CRC_16_gc);
	nrfSetChannel(48);
	nrfSetAutoAck(1);
	nrfEnableDynamicPayloads();

	nrfClearInterruptBits();
	nrfFlushRx();
	nrfFlushTx();

	// Interrupt Pin
	PORTF.INT0MASK |= PIN6_bm;
	PORTF.PIN6CTRL  = PORT_ISC_FALLING_gc;
	PORTF.INTCTRL  |= // (PORTF.INTCTRL & ~PORT_INT0LVL_gm) |
	PORT_INT0LVL_LO_gc ;  // Interrupts Low Level

	// Opening pipes
	nrfOpenWritingPipe(pipes[1]);
	nrfOpenReadingPipe(1,pipes[0]);
	nrfStartListening();
}

int button_pressed(void)
{
	if ( bit_is_clear(PORTD.IN,PIN3_bp) ) {
		_delay_ms(DEBOUNCE_PERIOD_MS);
		while ( bit_is_clear(PORTD.IN,PIN3_bp) ) ;
		return 1;
	}
	return 0;
}

// adds two strings together
char* concat(const char *s1, const char *s2)
{
	char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the null-terminator
	//in real code you would check for errors in malloc here
	strcpy(result, s1);
	strcat(result, s2);
	return result;
}

// sends a command
void stuur(char* command_id, char* command_data){
	char* command = SENSOR_NAME;
	
	command = concat(command, command_id);
	command = concat(command, command_data);
	
	nrfWrite( (uint8_t *) command, strlen(command) );
	
	_delay_ms(LOCK_PERIOD_MS);
}



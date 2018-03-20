/*
* Slave 1.c
*
* Created: 7-2-2018 11:38:04
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

#define C_ON			"On"	//
#define C_PRINT			"Pf"	//
#define C_WINDOWCTRL	"Rc"	// 1=open, 0=shut, 2=slightly open
#define C_LIGHTCTRL		"Lc"	// 0=dim, 255=bright

#define M_SMOKE			"mS"	// {state 0/1}
#define M_CO			"mC"	//
#define M_LIGHT			"mL"	// {brightness 0-255}
#define M_WINDOW		"mW"	// {state 0/1/2}

#define DEBOUNCE_PERIOD_MS	10		//
#define LOCK_PERIOD_MS		200		// bepaalt voor hoelang je de knop niet kan indrukken

#define SENSOR_NAME			"SP"	// bepaalt de naam van de slave

int led_pwm = 1500;

// twee pipes voor read & transmit
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

	clear_screen();		// leegt de scherm op de pc
	
	// overal van er printf staat komt er een zin te schrijnen op de PC. Dit gebruik ik om de xMega te testen
	printf("Sensor %s (Slave)\n", SENSOR_NAME);

	int hasSentSmokeNotification=0;
	int hasSentSmokeNotification2=0;
	int hasSentCONotification=0;
	int hasSentCONotification2=0;

	// oneindig loop (1 = true, dus while(true))
	while (1) {
		if ( bit_is_clear(PORTD.IN,PIN3_bp) || !(PORTD.IN & PIN5_bm) ) {
			PORTD.OUTSET = PIN2_bm;
			
			if(!hasSentSmokeNotification){
				nrfStopListening();
				stuur(M_SMOKE, "1");
				
				hasSentSmokeNotification=1;
				hasSentSmokeNotification2=0;
			}
		}
		else
		{
			PORTD.OUTCLR = PIN2_bm;
			
			if(!hasSentSmokeNotification2){
				nrfStopListening();
				stuur(M_SMOKE, "0");
				
				hasSentSmokeNotification=0;
				hasSentSmokeNotification2=1;
			}
		}
		
		
		if ( bit_is_clear(PORTD.IN,PIN4_bp) ) {
			PORTD.OUTSET = PIN1_bm;
			
			if(!hasSentCONotification){
				nrfStopListening();
				stuur(M_CO, "1");
				
				hasSentCONotification=1;
				hasSentCONotification2=0;
			}
		}
		else
		{
			PORTD.OUTCLR = PIN1_bm;
			
			if(!hasSentCONotification2){
				nrfStopListening();
				stuur(M_CO, "0");
				
				hasSentCONotification=0;
				hasSentCONotification2=1;
			}
		}
		
		_delay_ms(200);
	}
}

ISR(PORTF_INT0_vect)
{
	uint8_t  tx, fail, rx;
	uint8_t  len;

	nrfWhatHappened(&tx, &fail, &rx);
	
	if(rx){
		len =  nrfGetDynamicPayloadSize();
		
		nrfRead( packet, len );
		
		packet[len] = '\0';
		
		char* sensor_id;
		char* command_id;
		char* command_data;
		
		printf("%s: command %s, data %s\n", sensor_id, command_id, command_data);
		
		sensor_id = (char*)malloc(2+1);
		memcpy(sensor_id,packet,2);
		sensor_id[2] = 0;
		
		command_id = (char*)malloc(2+1);
		memcpy(command_id,packet+2,2);
		command_id[2] = 0;
		
		command_data = (char*)malloc(len+1);
		memcpy(command_data,packet+4,len);
		command_data[len] = 0;
		
		if(strcmp(sensor_id, SENSOR_NAME) != 0)return;
		
		printf("%s: command %s, data %s\n", sensor_id, command_id, command_data);
		
		if (strcmp(command_id, C_LIGHTCTRL) == 0)
		{
			int mod = atoi(command_data);
			if(led_pwm>1000&&led_pwm<9000) led_pwm+=100*mod;
			else if(led_pwm<=1000) led_pwm=1005;
			else if(led_pwm>=9000) led_pwm=8995;
			
			TCD0.CCA     = led_pwm;
			printf("%d\n", led_pwm);
		}
		
		_delay_ms(5);
	}
}

void init_pwm(void)
{
	PORTD.DIRCLR   = PIN3_bm;            // input pin switch
	PORTD.PIN3CTRL = PORT_OPC_PULLUP_gc; // enable pull up
	PORTD.DIRCLR   = PIN4_bm;            // input pin switch
	PORTD.PIN4CTRL = PORT_OPC_PULLUP_gc; // enable pull up
	
	// LED
	PORTD.DIRSET = PIN2_bm;
	PORTD.DIRSET = PIN1_bm;
	
	// PWM LEDs
	PORTD.DIRSET = PIN0_bm;

	TCD0.CTRLB   = TC0_CCAEN_bm | TC_WGMODE_SINGLESLOPE_gc;
	TCD0.CTRLA   = TC_CLKSEL_DIV4_gc;
	TCD0.PER     = 9999;
	TCD0.CCA     = led_pwm;
	
	// Smoke sensor
	PORTD.DIRCLR = PIN5_bm;
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

char* concat(const char *s1, const char *s2)
{
	char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the null-terminator
	//in real code you would check for errors in malloc here
	strcpy(result, s1);
	strcat(result, s2);
	return result;
}

void stuur(char* command_id, char* command_data){
	nrfStopListening();
	
	char* command = SENSOR_NAME;
	
	command = concat(command, command_id);
	command = concat(command, command_data);
	
	nrfWrite( (uint8_t *) command, strlen(command) );
	
	_delay_ms(10);
	
	nrfStartListening();
	_delay_ms(5);
}

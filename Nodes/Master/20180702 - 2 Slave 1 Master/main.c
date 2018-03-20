/*
* 20180702 - 2 Slave 1 Master.c
*
* Created: 7-2-2018 11:37:30
* Author : Max
*/

// Dit is de code van de Master, er hoort een code van de 2 Slaves bij. De code van de Master MOET bij de code van de Slaves passen.

#define F_CPU 32000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stddef.h>
#include <util/delay.h>
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

#define LOCK_PERIOD_MS		200

uint8_t  pipes[][6] = {
	"AVH01",
	"AVH02"
};

int  blacklist[6] = {1,1,1,1,1,1};

char     buffer[128];
uint8_t  packet[32];
volatile uint8_t sending = 1;

void init_pwm(void);
void init_nrf(void);
uint8_t getLine(char *s);
void    nrfSendCommand(char *command);
uint8_t nrfReceive(uint8_t *iPacket, uint8_t *iPacketLength);
char* concat(const char *s1, const char *s2);
void stuur(char* sensor_name, char* command_id, char* command_data);

int main(void)
{
	uint16_t length;

	Config32MHzClock_Ext16M();

	init_pwm();
	init_nrf();

	init_stream(F_CPU);
	PMIC.CTRL |= PMIC_LOLVLEN_bm;           // Low level interrupt
	sei();

	clear_screen();
	printf("Master\n");

	nrfStartListening();
	
	int lighting_button_toggle_status = 1;
	int lighting_control_mod = 1;
	
	while (1) {
		if (sending) {
			if(0){//if ( (length = getLine(buffer)) > 0) {
				buffer[length] = '\0';
				
				if (buffer[0]=='S' || buffer[0]=='s')
				{
					int sensor_id_index = buffer[1] - 48;
					blacklist[sensor_id_index]=blacklist[sensor_id_index]*-1;
					printf("Toggled sensor S%d, status: %d\n", sensor_id_index, blacklist[sensor_id_index]);
					nrfSendCommand("Toggled sensor!");
					continue;
				}

				printf("sent: %s\n", buffer);
				nrfSendCommand(buffer);
			}
			
			if(bit_is_clear(PORTD.IN,PIN3_bp)){
				// Lighting adjust button is held down
				
				if(lighting_button_toggle_status==0) _delay_ms(800);
				
				if(lighting_control_mod==1) stuur("SP", C_LIGHTCTRL, "1");
				else stuur("SP", C_LIGHTCTRL, "-1");
				
				_delay_ms(10);
				
				lighting_button_toggle_status=1;
			}
			else if(lighting_button_toggle_status)
			{
				lighting_button_toggle_status=0;
				
				lighting_control_mod=lighting_control_mod*-1;
			}
		}
	}
}

ISR(PORTF_INT0_vect)
{
	uint8_t  tx, fail, rx;
	uint8_t  len;
	uint8_t  ipipe;

	nrfWhatHappened(&tx, &fail, &rx);
	
	if(rx){
		len =  nrfGetDynamicPayloadSize();
		
		nrfRead( packet, len );
		
		packet[len] = '\0';
		
		char* sensor_id;
		char* command_id;
		char* command_data;
		
		int sensor_id_index = packet[1] - 48;
		if(blacklist[sensor_id_index] == -1) return;
		
		sensor_id = (char*)malloc(2+1);
		memcpy(sensor_id,packet,2);
		sensor_id[2] = 0;
		
		command_id = (char*)malloc(2+1);
		memcpy(command_id,packet+2,2);
		command_id[2] = 0;
		
		command_data = (char*)malloc(len+1);
		memcpy(command_data,packet+4,len);
		command_data[len] = 0;
		
		//printf("%s: command %s, data %s\n", sensor_id, command_id, command_data);
		
		if (strcmp(command_id, C_PRINT) == 0)
		{
			printf("%s\n", command_data);
		}
		if (strcmp(command_id, M_SMOKE) == 0)
		{
			int smoke = atoi(command_data);
			
			if(smoke){
				printf("ROOK GEDETECTEERD!\n");
				PORTD.OUTSET = PIN2_bm;
				}else{
				printf("De rook is niet meer aanwezig!\n");
				PORTD.OUTCLR = PIN2_bm;
			}
		}
		if (strcmp(command_id, M_CO) == 0)
		{
			int smoke = atoi(command_data);
			
			if(smoke){
				printf("CO GEHALTE TE HOOG!\n");
				PORTD.OUTSET = PIN1_bm;
				}else{
				printf("CO gehalte weer normaal!\n");
				PORTD.OUTCLR = PIN1_bm;
			}
		}
		
		_delay_ms(5);
	}
	
	if (tx) {
		nrfStopListening();
		_delay_ms(5);
		sending = 1;
	}
}

void init_pwm(void)
{
	PORTD.DIRCLR   = PIN3_bm;            // input pin switch
	PORTD.PIN3CTRL = PORT_OPC_PULLUP_gc; // enable pull up
	
	// LED
	PORTD.DIRSET   = PIN2_bm;
	PORTD.DIRSET   = PIN1_bm;
	
	PORTC.OUTCLR = PIN0_bm;
	PORTF.OUTCLR = PIN1_bm|PIN0_bm;
	PORTC.DIRSET = PIN0_bm;               // PC0 output (blue)
	PORTF.DIRSET = PIN0_bm|PIN1_bm;       // PF0, PF1  output (green red)

	TCC0.CTRLB   = TC0_CCAEN_bm | TC_WGMODE_SINGLESLOPE_gc;
	TCC0.CTRLA   = TC_CLKSEL_DIV8_gc;    // f = FCPU/(N*PER) = 32M/(8*20000) = 200 Hz
	TCC0.PER     = 20000;
	TCC0.CCA     = 0;

	TCF0.CTRLB   = TC0_CCAEN_bm | TC0_CCBEN_bm  | TC_WGMODE_SINGLESLOPE_gc;
	TCF0.CTRLA   = TC_CLKSEL_DIV8_gc;
	TCF0.PER     = 20000;
	TCF0.CCA     = 0;
	TCF0.CCB     = 0;
	
}

void init_nrf(void)
{
	nrfspiInit();
	// Check SPI Connection
	if (nrfVerifySPIConnection())   PORTF.OUTSET = PIN0_bm;
	else                             PORTF.OUTSET = PIN1_bm;

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
	PORTF.INTCTRL   = (PORTF.INTCTRL & ~PORT_INT0LVL_gm) |
	PORT_INT0LVL_LO_gc;

	nrfOpenWritingPipe(pipes[0]);
	nrfOpenReadingPipe(1,pipes[1]);
}

uint8_t getLine(char *s)
{
	uint8_t c;
	uint8_t i = 0;

	while ( (c = getchar()) != '\r') {
		*s = c;
		s++;
		i++;
	}
	*s = '\0';
	
	return i;
}

void nrfSendCommand(char *command)
{
	nrfStopListening();
	nrfWrite( (uint8_t *) command, strlen(command) );
	_delay_ms(5);
	nrfStartListening();
	_delay_ms(5);
}

uint8_t xxxAvailable(uint8_t* pipe_num)
{
	if ( pipe_num ){
		uint8_t status = nrfGetStatus();
		*pipe_num = ( status >> NRF_STATUS_RX_P_NO_gp ) & 0b111;
		printf("BINGO %d\n", *pipe_num);
	}
	return 1;

	return 0;
}


uint8_t nrfReceive(uint8_t *iPacket, uint8_t *iPacketLength)
{
	uint8_t iRXPipe;

	if ( ! xxxAvailable(&iRXPipe) ) {
		return 254;
	}

	*iPacketLength = nrfGetDynamicPayloadSize();

	if (*iPacketLength > 32) return 255;              // Not possible

	nrfReadPayload(iPacket, *iPacketLength);

	nrfFlushRx();
	nrfClearInterruptBits();

	return(iRXPipe);  // Return incoming RX pipe number, 1 = MyAddress,  0 = LANAddress   255=rubish
}

char* concat(const char *s1, const char *s2)
{
	char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the null-terminator
	//in real code you would check for errors in malloc here
	strcpy(result, s1);
	strcat(result, s2);
	return result;
}

void stuur(char* sensor_name, char* command_id, char* command_data){
	nrfStopListening();
	
	char* command = sensor_name;
	command = concat(command, command_id);
	command = concat(command, command_data);
	
	nrfWrite( (uint8_t *) command, strlen(command) );
	
	_delay_ms(10);
	
	
	//_delay_ms(5);
	nrfStartListening();
	_delay_ms(5);
}

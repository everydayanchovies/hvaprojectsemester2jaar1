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

uint8_t  pipes[][6] = {
	"AVH01",
	"AVH02"
};

char     buffer[128];
uint8_t  packet[32];
volatile uint8_t sending = 1;

void init_pwm(void);
void init_nrf(void);
uint8_t getLine(char *s);
void    nrfSendCommand(char *command);
uint8_t nrfReceive(uint8_t *iPacket, uint8_t *iPacketLength);

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
	
	while (1) { }
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
		
		if (strcmp(packet, "S1P") == 0)
		{
			printf("Sensor 1 is gedrukt!\n");
		}
		else if (strcmp(packet, "S2P") == 0)
		{
			printf("Sensor 2 is gedrukt!\n");
		}
		
		_delay_ms(5);
	}
}

void init_pwm(void)
{ }

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

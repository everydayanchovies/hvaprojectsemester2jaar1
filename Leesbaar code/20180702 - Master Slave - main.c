/*!
*  \file    nrf24_test_receive.c
*  \author  Wim Dolman (<a href="mailto:w.e.dolman@hva.nl">w.e.dolman@hva.nl</a>)
*  \date    30-06-2016
*  \version 1.0
*
*  \brief   Test program for receiving data with a Nordic NRF24L01p and a Xmega
*
*  \details The hardware configuration consists of a sender and a receiver.
*           This file contains the program for the receiver.
*           The sender is in file nrf24_test_send.c.
*           The receiver is just a HvA-Xmegaboard with nothing connected.
*           The led is the blue led.
*  \image   html  xmega3u_nrf_ptx_prx.png
*  \image   latex xmega3u_nrf_ptx_prx.eps
*           The receiver gets two bytes from the sender.
*           The first byte received is the least significant byte and the second byte
*           is the most significant byte.
*           This 16-bits value is used to change the duty-cycle of the PWM-signaal
*
*           This test program is based on example in paragraph E.1 from
*           <a href="http://www.dolman-wim.nl/xmega/index.php">'De taal C en de Xmega'</a>
*
*/

// Dit is de code van de Master, er hoort een code van de Slave bij. De code van de Master MOET bij de code van de Slave passen.

#define  F_CPU  2000000UL

#include <avr/io.h>				// (In/Out)
#include <avr/interrupt.h>		// (Interrupt)
#include "nrf24spiXM2.h"		// (RF) Bestand regelt SPI communicatie van de xMega
#include "nrf24L01.h"			// (RF)

// {0x48, 0x76, 0x41, 0x30, 0x31} staat voor HVA01
// H    , V   , A   , 0   , 1
// uint8_t  pipe[5] = {0x48, 0x76, 0x41, 0x30, 0x31};    //  pipe address is "HVA01"

// een alternatief, wat meer leesbaar is:
uint8_t pipe[6] = {"AVH01"};	// pipe address is "AVH01"
// (we schrijven pipe[6] ipv pipe[5] omdat er een extra bit bij een string hoort. "AVH01" (5bits) is in werkelijkheid "AVH010" (6bits)
// de laatste bit wordt genegeerd.

// packet heeft een grotte van 32 bits
uint8_t  packet[32];                                  //  buffer for storing received data

// init = initialize, setup
// pwm = pulse-width modulation
void init_pwm(void);
// rf = radio frequency
void init_nrf(void);

/*			main routine for receiver
*
*           It initializes the nrf24L01+ (RF module van de xMega) and the pwm output (zorgt ervoor dat de led werkt) and enables the interrupt mechanism.
*           It does nothing. The program just waits for an interrupt of the nrf24L01+.
*
*           This routine is code E.4 from 'De taal C en de Xmega' second edition,
*           see http://www.dolman-wim.nl/xmega/book/index.php
*/
int main(void)
{
	init_pwm();
	init_nrf();

	PMIC.CTRL |= PMIC_LOLVLEN_bm;
	sei();

	while (1) {}   // doe niets, wacht voor een interrupt
}

/*			Initializes nrf24L01+ (RF module van de xMega)
*
*           This function is almost the same as the init_nrf() of the sender.
*           The interrupt is enabled. The interrupt pin is pin 6 of port F and
*           responses to a falling edge.
*
*           Only a pipe for reading opened and the radiomodule is set in the
*           receive mode.
*
*           This routine is code E.3 from 'De taal C en de Xmega' second edition,
*           see http://www.dolman-wim.nl/xmega/book/index.php
*/
void init_nrf(void)
{
	nrfspiInit();
	nrfBegin();

	nrfSetRetries(NRF_SETUP_ARD_1000US_gc, NRF_SETUP_ARC_8RETRANSMIT_gc);
	nrfSetPALevel(NRF_RF_SETUP_PWR_6DBM_gc);
	nrfSetDataRate(NRF_RF_SETUP_RF_DR_250K_gc);
	nrfSetCRCLength(NRF_CONFIG_CRC_16_gc);
	// we communiceren over kanaal 50
	nrfSetChannel(50);
	nrfSetAutoAck(1);
	nrfEnableDynamicPayloads();

	nrfClearInterruptBits();
	nrfFlushRx();
	nrfFlushTx();

	// Interrupt
	PORTF.INT0MASK |= PIN6_bm;
	PORTF.PIN6CTRL  = PORT_ISC_FALLING_gc;
	PORTF.INTCTRL   = (PORTF.INTCTRL & ~PORT_INT0LVL_gm) | PORT_INT0LVL_LO_gc;

	// we willen ons pipe (AVH01) gebruiken voor het lezen van gestuurde informatie
	nrfOpenReadingPipe(0, pipe);
	// we beginnen te luisteren
	nrfStartListening();
}

/*			Initializes pin 0 of port C as PWM ouput
*
*           With F_CPU is 2 MHz the frequency will be 200 Hz
*
*           This routine is code E.5 from 'De taal C en de Xmega' second edition,
*           see <a href="http://www.dolman-wim.nl/xmega/book/index.php">Voorbeelden uit 'De taal C en de Xmega'</a>
*  \return void
*/
void init_pwm(void)
{
	PORTC.OUTCLR = PIN0_bm;
	PORTC.DIRSET = PIN0_bm;

	TCC0.CTRLB   = TC0_CCAEN_bm | TC_WGMODE_SINGLESLOPE_gc;
	TCC0.CTRLA   = TC_CLKSEL_DIV1_gc;
	TCC0.PER     = 9999;
	TCC0.CCA     = 0;
}

/*			Interrupt function for receiving data
*
*           If the interrupt is caused by the received data a data packet
*           is read and assigned to CCABUF of timer/counter0 of port C.
*
*           This routine is code E.6 from 'De taal C en de Xmega' second edition,
*           see http://www.dolman-wim.nl/xmega/book/index.php
*/
ISR(PORTF_INT0_vect)
{
	uint8_t  tx_ds, max_rt, rx_dr;

	// Tells you what caused the interrupt, and clears the state of interrupts.
	// The retuend values are not equal to 0 if the interrupt occured.
	nrfWhatHappened(&tx_ds, &max_rt, &rx_dr);

	// if rx_dr is niet 0 (0 = false) dan betekent het dat we informatie gekregen hebben.
	if ( rx_dr ) {
		// we lezen de informatie de we kregen gestuurd
		nrfRead(packet, 2);
		// de waarde die we hebben gestuurd is groter dan twee bits, dus we lezen het bit voor bit
		TCC0.CCABUFL  =  packet[0];       // low byte was als eerst gestuurd (dat hebben we geregeld bij de code van de slave)
		TCC0.CCABUFH  =  packet[1];			// high byte was als tweede gestuurd
	}
}
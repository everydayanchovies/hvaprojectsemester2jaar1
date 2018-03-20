#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stddef.h>

uint8_t readCalibrationByte(uint8_t index) {
  uint8_t result;

  NVM.CMD = NVM_CMD_READ_CALIB_ROW_gc;
  result = pgm_read_byte(index);
  NVM.CMD = NVM_CMD_NO_OPERATION_gc;

  return result;
}

void init_adc(void)
{
  ADCA.CALL = readCalibrationByte( offsetof(NVM_PROD_SIGNATURES_t, ADCACAL0) );
  ADCA.CALH = readCalibrationByte( offsetof(NVM_PROD_SIGNATURES_t, ADCACAL1) );

  PORTA.DIRCLR     = PIN2_bm|PIN1_bm|PIN0_bm;          // pins configured as input
  ADCA.CH2.MUXCTRL = ADC_CH_MUXPOS_PIN2_gc |           // PA1 to + channel 0
                     ADC_CH_MUXNEG_INTGND_MODE3_gc;    // internal ground to - channel 0
  ADCA.CH2.CTRL    = ADC_CH_INPUTMODE_DIFF_gc;         // channel 0 differential
  ADCA.CH1.MUXCTRL = ADC_CH_MUXPOS_PIN1_gc |           // PA1 to + channel 0
                     ADC_CH_MUXNEG_INTGND_MODE3_gc;    // internal ground to - channel 0
  ADCA.CH1.CTRL    = ADC_CH_INPUTMODE_DIFF_gc;         // channel 0 differential
  ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN0_gc |           // PA0 to + channel 0
                     ADC_CH_MUXNEG_INTGND_MODE3_gc;    // internal ground to - channel 0
  ADCA.CH0.CTRL    = ADC_CH_INPUTMODE_DIFF_gc;         // channel 0 differential
  ADCA.REFCTRL     = ADC_REFSEL_INTVCC_gc;             // internal VCC/1.6 reference
  ADCA.CTRLB       = ADC_RESOLUTION_12BIT_gc |         // 12 bit conversion
                     ADC_CONMODE_bm;                   // signed
  ADCA.PRESCALER   = ADC_PRESCALER_DIV16_gc;           // 32MHz/128 is 256 kHz
  ADCA.CTRLA       = ADC_ENABLE_bm;                    // enable adc
}

uint16_t read_adc(void)
{
  ADCA.CH0.CTRL |= ADC_CH_START_bm;                    // start ADC conversion
  while ( !(ADCA.CH0.INTFLAGS & ADC_CH_CHIF_bm) ) ;    // wait until it's ready
  ADCA.CH0.INTFLAGS |= ADC_CH_CHIF_bm;                 // reset interrupt flag

  return ADCA.CH0.RES;                                 // return measured value
}

void read_adcs(uint16_t *res)
{
  ADCA.CH0.CTRL |= ADC_CH_START_bm;                    // start ADC conversions
  ADCA.CH1.CTRL |= ADC_CH_START_bm;
  ADCA.CH2.CTRL |= ADC_CH_START_bm;
  while ( !(ADCA.CH2.INTFLAGS & ADC_CH_CHIF_bm) ) ;    // wait until last is ready
  res[0] = ADCA.CH0.RES;
  res[1] = ADCA.CH1.RES;
  res[2] = ADCA.CH2.RES;

  ADCA.CH2.INTFLAGS |= ADC_CH_CHIF_bm;                 // reset interrupt flag
}


/*!
 *  \file    clock.h
 *  \author  Wim Dolman (<a href="mailto:w.e.dolman@hva.nl">w.e.dolman@hva.nl</a>)
 *  \date    30-06-2016
 *  \version 1.3
 *
 *  \brief   Clock functions for Xmega 
 *
 */

void Config32MHzClock(void);
void AutoCalibration32M(void);
void AutoCalibration2M(void);
void AutoCalibrationTosc32M(void);
void AutoCalibrationTosc2M(void);
void Config32MHzClock_Ext16M(void);
void Config16MHzClock_Ext16M(void);

#define init_clock  Config32MHzClock_Ext16M   //<! Macro to select right clock function

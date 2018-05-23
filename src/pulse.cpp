//   EMONTX3-CONTINUOUS - continuous sampling Arduino firmware
// 
//   Copyright (C) 2018 C. B. Markwardt
//   License: GNU GPL V3
//
// Utility meter pulse counter
// Note: this module was derived from the pulse counter of the 
// standard emonTx firmware.
//

#include <Arduino.h>
#include <Math.h>
#include "cont.h"
#include "cal.h"

volatile uint8_t pulse_count_ticks = 0;  // Pulses measured by ISR
uint32_t pulse_count = 0;                // Total lifetime pulses received
uint32_t last_pulse_count = 0;

const uint8_t pulse_countINT=         1;                              // INT 1 / Dig 3 Terminal Block / RJ45 Pulse counting pin(emonTx V3.4) - (INT0 / Dig2 emonTx V3.2)
const uint8_t pulse_count_pin=        3;                              // INT 1 / Dig 3 Terminal Block / RJ45 Pulse counting pin(emonTx V3.4) - (INT0 / Dig2 emonTx V3.2)
const uint8_t min_pulsewidth = 110;

void pulse_interrupt_handler();

void init_pulse(void)
{
  pulse_count = 0;  
  
  pinMode(pulse_count_pin, INPUT_PULLUP);                     // Set emonTx V3.4 interrupt pulse counting pin as input (Dig 3 / INT1)
  attachInterrupt(pulse_countINT, pulse_interrupt_handler, FALLING);     // Attach pulse counting interrupt pulse counting
}

void record_pulse_count(void)
{
  uint8_t sreg;
  if (pulse_count_ticks) {
    sreg = SREG; cli();
    {
      pulse_count += pulse_count_ticks;
      pulse_count_ticks = 0;
    }
    SREG = sreg;
  }
}

uint32_t t_report_pulse = 0;

void report_pulse_count(void)
{
  uint32_t t = micros();
  if (t_report_pulse == 0 || 
      ((t - t_report_pulse) > REPORT_PULSE_PERIOD) && 
       (pulse_count != last_pulse_count)) {
    push_report_uint32("pulse",pulse_count,0);
    push_report_break();
    t_report_pulse = t;
    last_pulse_count = pulse_count;
  }
}

//-------------------------------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------
// The interrupt routine - runs each time a falling edge of a pulse is detected
//-------------------------------------------------------------------------------------------------------------------------------------------
unsigned long pulsetime=0;                                    // Record time of interrupt pulse

void pulse_interrupt_handler()
{
  if ( (millis() - pulsetime) > min_pulsewidth) {
    pulse_count_ticks++;          //calculate Wh elapsed from time between pulses
  }
  pulsetime=millis();
}

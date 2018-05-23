// 
//
//   EMONTX3-CONTINUOUS - continuous sampling Arduino firmware
// 
//   Copyright (C) 2018 C. B. Markwardt
//   License: GNU GPL V3
//
//   This main innovation of this firmware is the continuous sampling
//   of emonTx data inputs at a high data rate.  Inputs are sampled
//   continuously using the interrupt-driven Atmel free-running mode.
//   Samples are accumulated by a short interrupt handler and processed
//   by a state machine.  The machine has different behaviors as 
//   the system is initialized and more knowledge is gained.
//
//   Summary of the states, in order:
//     STATE_STAB - upon power-up, wait for inputs to stabilize
//   
//     STATE_SCAN - after stabilization, scan for valid sensor inputs
//
//     STATE_ZER1 - when inputs are known, wait for voltage zero-crossing
//
//     STATE_FREQ - measure mains voltage frequency
//
//     STATE_CALF - report mains voltage frequency and calculate calibration factors
//
//     STATE_STAT - after frequency is known, this is the final state which
//                  accumulates statistics about each input
//
//     STATE_CALS - periodically, when enough statistics are accumulated
//                  by STATE_STAT, this state is triggered to report the results
//
//  This firmware rapidly samples all ADC inputs and stores them in a buffer.
//  When large amounts of CPU processing occurs, the buffer can approach full.
//  To prevent buffer overflows, some activities are deferred until idle periods.
//  The buffer state and serial output state are monitored and when the system
//  is relatively idle, more data can be sent.  During extensive testing with all
//  four inputs active, the buffer never overflowed.
//  
//  Other modules:
//     cont.h - configuration of the system; no need to change these values
//     cal.h  - use for calibration of the system
//     adc.cpp - functions used to manage the ADC
//     pulse.cpp - functions used to manage the pulse counter
//     inlineAVR201def.h - high speed math routines for sum-and-multiply
//     report.cpp - functions to store and send data
//     state.cpp - main state machine functions
//  

// Version number of this firmware.  Update after changes
// This version number is reported to emonCMS with tag "ever"
#define VERSIONTAG  1006

// Standard includes
#include <Arduino.h>
#include <Math.h>
#include "cont.h"
#include "cal.h"

//
// Setup 
//   - initialize serial
//   - initialize the ADC for continuous sampling
//   - initialize the pulse counter
void setup() {

  // Initialize serial
  Serial.begin(115200);
  Serial.println("");
  Serial.print("#EMONTX3-continuous=v");Serial.println(VERSIONTAG);
  push_report_int32("ever", VERSIONTAG, 1);
  init_cal();
  push_report_break();

  // Initialize ADC
  init_adc(ADC_PRESCALAR);

  // Initialize pulse counter
  init_pulse();
}

// 
// The Arduino loop() is a state machine that processes ADC readings
// and sends them to their associated state processor.  The state processor
// decides when to proceed to the next state.
//
//
void loop() {
  struct adc_readings_struct reading; // current ADC readings
  static uint8_t state = STATE_STAB;  // initial state is the "stabilization" state

  // Retrieve the next ADC reading, if it is available
  if (get_next_adc_reading(&reading)) {

    // Send this reading to its associated state
    switch(state) {
      case STATE_STAB: state = stabilize_inputs(&reading,STATE_STAB,STATE_SCAN); break;
      case STATE_SCAN: state = scan_inputs(&reading,STATE_SCAN,STATE_ZER1); break;
      case STATE_ZER1: state = zero_crossing(&reading, (N_READINGS+N_VHIST_RING),
                                             STATE_ZER1, STATE_FREQ); break;
      case STATE_FREQ: state = accum_freq(&reading, STATE_FREQ, STATE_STAT); break;
      case STATE_STAT: state = accum_stats(&reading, 1000000, STATE_STAT, STATE_STAT); break;
    }

    // Follow-up states for reporting
    switch(state) {
      case STATE_CALF: state = calc_freq(&reading, STATE_CALF, STATE_STAT); break;
      case STATE_CALS: state = calc_stats(&reading, STATE_CALS, STATE_STAT); break;
    }
  }

  // When the input ADC buffer is quite idle, then stuff more reports into
  // the output serial buffer.  This can block for about about 2 ADC samples.
  if (get_adc_depth() < 4) {
    record_pulse_count();
    if (Serial.availableForWrite() > 20) {
      if (state > STATE_FREQ) report_pulse_count();
      send_report();
    }
  }
}



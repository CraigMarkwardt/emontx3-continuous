//   EMONTX3-CONTINUOUS - continuous sampling Arduino firmware
// 
//   Copyright (C) 2018 C. B. Markwardt
//   License: GNU GPL V3
//
//   ADC management module
//

#include <Arduino.h>
#include "cont.h"

// ADC pin for each input channel.  These are AVR ADC MUX channel numbers
// If you are using different ADC channels to a CPU than the standard emonTx
// channels, then you will modify these numbers.  Up to five channels, with
// the "voltage" signal assumed to be the first channel.
const uint8_t adc_chans[N_ADC_CHAN] = {0, 1, 2, 3, 4};

// Linked list pointers that point forward and backward.  These are indices
// to adc_chans[].  This will be filled in by init_adc().
uint8_t next_adc_chan[N_ADC_CHAN]   = {};
uint8_t prev_adc_chan[N_ADC_CHAN]   = {};

// Current ADC channel number.  These are indices to adc_chans[].  This is an
// internal state variable, do not modify.
volatile uint8_t cur_chan = 1;
// Maximum depth we have gone into the ADC ring buffer.  Used for
// diagnostics (have we overflowed the buffer?)
uint8_t max_adc_depth = 0;

// Buffer or ADC readings; ring buffer filled by the interrupt handler
volatile struct adc_readings_struct adc_readings[N_READINGS];
// ADC offset is zero-point of each ADC input channel
volatile struct adc_readings_struct adc_offset;
// Ring buffer read and write indices
volatile uint8_t adc_write_index = 0;
volatile uint8_t adc_read_index = 0;
// Records ring buffer overflows
volatile uint16_t n_overflow = 0;


// ============================= ADC setup and interrupt reading
// init_adc - initialize the ADC
//   prescalar - Atmel ADC prescalar (see cont.h)
void init_adc(uint8_t prescalar) 
{
  uint8_t adcsra;

  ADCSRA = ADCSRB = 0; // Disable ADC temporarily
  init_adc_chans();

  // For eMonTx3, use AVCC as reference (=REFS0)
  // Select first ADC channel
  ADMUX  = _BV(REFS0) | ((cur_chan-1+N_ADC_CHAN)%N_ADC_CHAN);

  // Init ADC free-run mode; f = ( 16MHz/prescaler ) / 13 cycles/conversion 
  DIDR0 = 0;
  for (uint8_t ich = 0; ich < N_ADC_CHAN; ich++) {
    DIDR0 |= 1 << adc_chans[ich]; // Turn off digital input for ADC pin
  }

  ADCSRB = 0;           // Free run mode, no high MUX bit
  adcsra = _BV(ADEN)  | // ADC enable
           _BV(ADSC)  | // ADC start
           _BV(ADATE) | // Auto trigger
           _BV(ADIE); // Interrupt enable
  switch (prescalar) {
    case 128: 
      adcsra |= _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // 128:1; 125 kHz / 13 =  9615 Hz         
      break;
    case 64:
      adcsra |= _BV(ADPS2) | _BV(ADPS1) ;             // 64:1;  250 kHz / 13 = 19231 Hz
      break;
  }
  ADCSRA = adcsra;

  sei(); // Enable interrupts
}

// Initialize ADC channels to original state
//  init_adc_chans()
void init_adc_chans(void)
{
  uint8_t j;
  // Reset channels that were potentially disabled
  for (j = 0; j<N_ADC_CHAN; j++) {
    next_adc_chan[j] = (j+1)%(N_ADC_CHAN);
    prev_adc_chan[j] = (j+N_ADC_CHAN-1)%(N_ADC_CHAN);
  }
  return;
}

// Disable one ADC input channel
//  disable_adc_chan()
void disable_adc_chan(uint8_t chan)
{
  uint8_t next, prev;
  uint8_t sreg;
  
  // Do not allow disabling channel 0
  if (chan == 0) return;
  // Already disable???
  if (next_adc_chan[chan] == 0xff || prev_adc_chan[chan] == 0xff) return;

  // Heal the linked list
  next = next_adc_chan[chan];
  prev = prev_adc_chan[chan];

  // Disable interrupts while we twizzle these pointers
  sreg = SREG; cli();
  {
    next_adc_chan[prev] = next;
    prev_adc_chan[next] = prev;
  
    // Mark this channel as dead
    next_adc_chan[chan] = prev_adc_chan[chan] = 0xff;
  }
  SREG = sreg; // Restore interrupts

  return;
}

// set_adc_offset() - set the zero-point of ADC channel
//   chan - ADC input number (0-4)
//   offset - zero-point of this channel
void set_adc_offset(uint8_t chan, int16_t offset)
{
  if (chan >= 0 && chan < N_ADC_CHAN && adc_offset.vals[chan] == 0) {
    adc_offset.vals[chan] = offset;
  }
}

// get_adc_depth() - get the current ADC ring buffer depth
//  returns: depth
uint8_t get_adc_depth(void)
{
  uint8_t depth;
  uint8_t sreg;
  
  sreg = SREG; cli();
  { // Inside interrupt disabled block
     depth = (adc_write_index - adc_read_index);
  }
  SREG = sreg; // Restore interrupts
  while (depth > 0x80) depth += N_READINGS;
  return depth;
}

// reset_overflow() - reset ring buffer overflow counter
void reset_overflow(void)
{
  n_overflow = 0;
}

// get_next_adc_reading() - retrieve the next available ADC ring buffer sample
//  reading - ADC reading structure to be filled upon return
//  returns: 0 if no reading is available; 1 if reading returned in *reading
uint8_t get_next_adc_reading(struct adc_readings_struct *reading) 
{
  uint8_t sreg, j;
  volatile struct adc_readings_struct *datap = reading;
  uint8_t depth;

  sreg = SREG; cli();
  { // Inside interrupt disabled block
    uint8_t cr = adc_read_index;
    // Return if no data ready
    if (! adc_readings[cr].set ) {
      SREG = sreg; // Restore interrupts
      return 0;    // Return not ready
    }

    adc_readings[cr].set = 0; // Indicate we've processed this
    datap->t = adc_readings[cr].t;
    for (j=0; j<N_ADC_CHAN; j++)  datap->vals[j] = adc_readings[cr].vals[j];
    // This does not work.  Why?  
    // *datap = adc_readings[cr];

    cr++; if (cr >= N_READINGS) cr = 0; // Advance to next read position
    adc_read_index = cr;
    depth = (adc_write_index - adc_read_index);
  }
  SREG = sreg; // Re-enable interrupts

  while (depth > 0x80) depth += N_READINGS;
  if (depth > max_adc_depth) max_adc_depth = depth;
  // max_adc_depth = depth;
  return 1;
}


// ============================= ADC Interrupt handler
// The handler retrieves the ADC data from the ADC registers and
// saves it in the ring buffer.
ISR(ADC_vect) { // ADC-sampling interrupt
  uint16_t sample = ADCW; // ADC sample (full 10-bit word)
  uint8_t ich = cur_chan; // Ring buffer write pointer
  uint8_t ichr = prev_adc_chan[ich]; // ADC is reporting previous sample
  uint8_t ichn = next_adc_chan[ich]; // ... and we will advance to next sample
  uint8_t cr = adc_write_index; 

  // Record sample, after subtracting offset
  adc_readings[cr].vals[ichr] = sample - adc_offset.vals[ichr];

  // Finish the reading if we have completed the round-robin and next
  // channel will be back to zero.
  if (ichn == 0) {
    // Finalize this reading
    adc_readings[cr].t = micros();
    adc_readings[cr].set = 1;

    // Advance to next reading
    cr ++; if (cr == N_READINGS) cr = 0;

    // Overflow occurred.  We must manually advance the read index and
    // record the overflow
    if (adc_readings[cr].set) {
      adc_read_index = cr+1;
      if (adc_read_index == N_READINGS) adc_read_index = 0;
      n_overflow++; // Check for overflow
    }
    
    // Initialize next reading
    adc_readings[cr].set = 0;
    adc_write_index = cr;
  }

  // Advance ADC pointer to next
  ADMUX = (ADMUX & 0xf0) | (adc_chans[ichn]); // Point to next input channel
  cur_chan = ichn;
}


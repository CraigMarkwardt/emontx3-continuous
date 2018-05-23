//   EMONTX3-CONTINUOUS - continuous sampling Arduino firmware
// 
//   Copyright (C) 2018 C. B. Markwardt
//   License: GNU GPL V3
//
//   State machine functions

#include <Arduino.h>
#include <Math.h>
#include "inlineAVR201def.h"
#include "cont.h"
#include "cal.h"

// Cumulative uptime in seconds
uint32_t uptime = 0;
// Currently accumulated reading stats
struct reading_stats vstats, istats[N_CUR_CHAN];

// Which channels to notice
const uint8_t adc_notice_chan[N_CUR_CHAN] = ADC_NOTICE_CHAN;

// Calibration factors
float VCAL, VCAL2;
const float ical[N_CUR_CHAN] = {ICAL0, ICAL1, ICAL2, ICAL3}; // Current calibration
const float iphcal[N_CUR_CHAN] = {IPH0, IPH1, IPH2, IPH3};   // Phase offset calibration
float cosph[N_CUR_CHAN], sinph[N_CUR_CHAN];                  // Phase cos() and sin() factors

// Running average mean voltage level, and current level
float vavg_ra = 0.0;
float iavg_ra[N_CUR_CHAN] = {0,0,0,0};

// Running counters for statistics accmulation
uint32_t sample_period = 0;
uint32_t vmains_period = 0;
float vmains_fprod = 0.0;

// Start of accumulation time
uint32_t start_time = 0;
uint16_t ncycles = 0;

// =========================================================
// Utility stuff
// Ring buffer for previous voltage measurements
int16_t vhist_ring[N_VHIST_RING];
uint8_t vhist_cur = N_VHIST_RING;
uint8_t vhist_lookback = 0;

// Accumulated energy usage for active and reactive components...
int16_t energy_fracac = 0, energy_fracre = 0;   // .. fractional
int32_t energy_active = 0, energy_reactive = 0; // .. integer

// store_vhist() - store voltage reading
//   val - voltage value to store
void store_vhist(int16_t val)
{
  // Store voltage reading in ring buffer
  vhist_cur = (vhist_cur + 1) % N_VHIST_RING;
  vhist_ring[vhist_cur] = val;
}
// retrieve_vhist() - retrieve voltage reading from history
//   ilookback - look back this many samples
//   returns: voltage value at requested lookback time
int16_t retrieve_vhist(uint8_t ilookback)
{
  uint8_t cur = (vhist_cur + N_VHIST_RING - ilookback);
  while (cur >= N_VHIST_RING) cur -= N_VHIST_RING;
  return vhist_ring[cur];
}
// init_stats() - initialize statistics counters
//   s - statistics counters to initialize
void init_stats(struct reading_stats *s)
{
  uint8_t present = s->present;
  memset(s,0,sizeof(*s));
  s->present = present;
}

// update_uptime() - update current uptime 
//   returns: uptime in seconds
uint32_t update_uptime(void)
{
  uint32_t new_millis = millis();
  static uint32_t old_millis = 0;

  // This is the upper part that counts rollovers
  uint16_t upper = (uptime & 0xffc00000); 
  if (new_millis < old_millis) upper += 0x00400000; // millis() rollover

  uptime = upper | (new_millis >> 10);
  old_millis = new_millis;
  
  return uptime;
}

// Initialize calibration constants
void init_cal(void)
{
  int8_t v240;

  pinMode(DIP_VMAINS, INPUT_PULLUP);
  v240 = digitalRead(DIP_VMAINS);
  if (v240 == LOW) { // Switch is pulled low: 120VAC
    VCAL = VCAL_120VAC*VCAL_ADC;
    push_report_int32("vman", 120, 0);
    
  } else {           // Switch is default pull-up: 240VAC
    VCAL = VCAL_240VAC*VCAL_ADC;
    push_report_int32("vman", 240, 0);
  }
  VCAL2 = VCAL*VCAL;
}


// =========================================================
// STATE_STAB: stabilize the inputs by waiting a certain duration
//   reading - current ADC reading
//   curstate - current state
//   nextstate - default next state
uint8_t stabilize_inputs(struct adc_readings_struct *reading, 
                         uint8_t curstate, uint8_t nextstate)
{
  static uint16_t nreadings = 0;

  // Make sure all ADC channels are enabled
  init_adc_chans();
  nreadings++;
  // AVR ADC reads at 9615 Hz for default ADC prescalar of 128
  // Use desired stabilization duration to compute number of cycles
  if (nreadings < ((uint16_t) (9615 / N_ADC_CHAN) * (128/ADC_PRESCALAR) * (STABILIZE_DURATION/1000000))) return curstate;
  
  Serial.println("#STATE_STAB complete");
  nreadings = 0;
  return nextstate;
}

// =========================================================
// STATE_SCAN: scan for which inputs are present
//   reading - current ADC reading
//   curstate - current state
//   nextstate - default next state
uint8_t scan_inputs(struct adc_readings_struct *reading, 
                    uint8_t curstate, uint8_t nextstate)
{
  static uint16_t nreadings = 0;
  static uint8_t first = 1;
  uint8_t n_cur_chan = 0;
  uint8_t j;

  if (first) {
    init_stats(&vstats);
    for (j=0; j<N_CUR_CHAN; j++) init_stats(&(istats[j]));
    first = 0;
    max_adc_depth = 0;
    start_time = reading->t;
  }
  
  vstats.val = reading->vals[0];
  for (j=0; j<N_CUR_CHAN; j++) istats[j].val = reading->vals[j+1];

  // Compute voltage stats
  if (vstats.present || vstats.val > 0) {
    int16_t val = vstats.val;
    if (vstats.n == 0) vstats.val_min = vstats.val_max = val;
    if (val < vstats.val_min) vstats.val_min = val;
    if (val > vstats.val_max) vstats.val_max = val;
    vstats.val_sum += val;
    vstats.n   += 1;
  }
  // Compute current stats
  for (j = 0; j<N_CUR_CHAN; j++) {
    if (istats[j].present || istats[j].val > 0) {
      int16_t val = istats[j].val;
      if (istats[j].n == 0) istats[j].val_min = istats[j].val_max = val;;
      if (val < istats[j].val_min) istats[j].val_min = val;
      if (val > istats[j].val_max) istats[j].val_max = val;
      istats[j].val_sum += val;
      istats[j].n   += 1;
    }
  }
  nreadings ++;
  if (nreadings < 4000) return curstate;

  Serial.println("#STATE_SCAN complete");
  sample_period = (reading->t - start_time) / nreadings;
  Serial.print("#tsample = ");Serial.println(sample_period);
  
  if (vstats.present || (vstats.n > 0 && vstats.val_sum != 0)) {
    int16_t mean = vstats.val_sum / vstats.n;
    vstats.present = 1;
    set_adc_offset(0, mean);
    vstats.val_sum = 0; vstats.n = 0;
        
    Serial.print("#vstats.val_mean = ");Serial.print(mean);Serial.print(" min/max=");Serial.print(vstats.val_min);Serial.print("/");Serial.println(vstats.val_max);
  } else {
    Serial.println("#ERROR - no voltage input");
    return STATE_STAB; // Return to the signal stabilization phase
  }
  for (j = 0; j<N_CUR_CHAN; j++) {
    if (!adc_notice_chan[j]) continue;
    if (istats[j].present || (istats[j].n > 0 && istats[j].val_sum != 0)) {
      int16_t mean = istats[j].val_sum / istats[j].n;
      n_cur_chan ++;
      istats[j].present = 1;
      set_adc_offset(j+1, mean);
      istats[j].val_sum = 0; istats[j].n = 0;
      Serial.print("#istats[");Serial.print(j);Serial.print("].val_mean = ");Serial.print(mean);Serial.print(" min/max=");Serial.print(istats[j].val_min);Serial.print("/");Serial.println(istats[j].val_max);
    } else {
        // Found no signal on this input channel, do we disable it?
        // The answer is, as we get more rapid-fire ADC readings, we tend to overflow
        // the buffer more quickly.  Any benefit of more rapid sampling is lost
        // because we have to make the ring buffer really big.  And that causes
        // us to run out of RAM.  I tried it. So, we do not disable the channel.
        // disable_adc_chan(j+1);
    }
  }
  if (n_cur_chan == 0) {
    Serial.println("#ERROR - no current inputs enabled");
    return STATE_STAB;  // Return to the signal stabilization phase, look for inputs
  }

  nreadings = 0;  // Initialize to zero in case we come back to this state
  first = 1;
  start_time = 0;
  init_stats(&vstats);
  return nextstate;
}

// =========================================================
// STATE_ZERO: Advance to the next zero crossing, and also queue up the voltage history
//   reading - current ADC reading
//   nclear - number of samples to eat before returning
//   curstate - current state
//   nextstate - default next state
uint8_t zero_crossing(struct adc_readings_struct *reading, uint8_t nclear,
                      uint8_t curstate, uint8_t nextstate)
{
  static uint16_t nreadings = 0;
  static uint32_t old_time;
  
  vstats.oldval = vstats.val;
  vstats.val = reading->vals[0];
  nreadings ++;

  // Store voltage reading in ring buffer
  store_vhist(vstats.val);

  // Clear out the input queue at least nclear items
  if (nreadings > nclear &&
      vstats.oldval < 0 && vstats.val >= 0) {
      Serial.print("#STATE_ZERO - t=");
      Serial.println(reading->t - old_time);
      max_adc_depth = 0;
      old_time = reading->t;
      nreadings = 0;
      reset_overflow(); // Reset the overflow counter so it doesn't bother us
      return nextstate;
  }
  return curstate;
}

// =========================================================
// STATE_FREQ: Measure mains voltage frequency
// Advance to the next zero crossing, and also queue up the cache
//   reading - current ADC reading
//   curstate - current state
//   nextstate - default next state
uint8_t accum_freq(struct adc_readings_struct *reading, 
                   uint8_t curstate, uint8_t nextstate)
{  
  if (start_time == 0) start_time = reading->t; // GLOBAL: start_time
  
  vstats.oldval = vstats.val;
  vstats.val = reading->vals[0];

  // Store voltage reading in ring buffer
  store_vhist(vstats.val);

  // Wait for a zero crossing
  if (! (vstats.oldval < 0 && vstats.val >= 0)) return curstate;
  
  ncycles ++;   // GLOBAL ncycles
  if (ncycles < 120) return curstate; // Wait for at least 120 cycles

  return STATE_CALF; // Advance to calculate info from this accumulation
}

// STATE_CALF: Calculate mains frequency
//   reading - current ADC reading
//   curstate - current state
//   nextstate - default next state
uint8_t calc_freq(struct adc_readings_struct *reading,
                  uint8_t curstate, uint8_t nextstate)
{
  uint8_t j;

  // Determine the mains period (1/frequency)
  vmains_period = (reading->t - start_time) / ncycles; // GLOBAL: vmains_period
  Serial.print("#STATE_FREQ:vmains_period=");
  Serial.println(vmains_period);

  // Compute one quarter of mains period, in units of sample period,
  // the 2*sample_period is for rounding to the nearest sample
  // One quarter of the mains period is used for lookback when computing
  // in-phase and quadrature products.
  vhist_lookback = (vmains_period + 2*sample_period) / (sample_period*4); // GLOBAL: vhist_lookback
  Serial.print("#STATE_FREQ:vmains_quadlookback=");
  Serial.println(vhist_lookback);

  // Compute cos() and sin() phase correction factors.  We use the
  // known sample period to compute the offset between the current
  // sample and the voltage sample, plus any calibration phase offset.
  for (j=0; j<N_CUR_CHAN; j++) {
    float ph = M_PI/180.0*(PHV + iphcal[j]) 
               + (float) 2.0 * M_PI * (j+1) * sample_period / N_ADC_CHAN / vmains_period;
    cosph[j] = cos(ph);
    sinph[j] = sin(ph);
  }

  // Reset global variables for next go round
  ncycles = 0;       // GLOBAL: ncycles
  start_time = 0;    // GLOBAL: start_time
  max_adc_depth = 0; // GLOBAL: max_adc_depth
  return nextstate;
}



// =========================================================
// STATE_STAT: Main state, accumulate statistics
//   reading - current ADC reading
//   tdur - number of microseconds to accumulate
//   curstate - current state
//   nextstate - default next state
uint8_t accum_stats(struct adc_readings_struct *reading, uint32_t tdur,
                      uint8_t curstate, uint8_t nextstate)
{
  int16_t vval, vdel;
  uint8_t j;
  uint8_t zero_crossing;

  // Initialize GLOBAL variables start_time and ncycles
  if (start_time == 0) {
    start_time = reading->t;
    ncycles = 0; 
  }

  // Voltage statistics
  vval = reading->vals[0];
  {
    vstats.oldval = vstats.val;  // Save old value
    vstats.val = vval;           // Save current value

    // Store voltage reading in ring buffer, and retrive lookback
    // value corresponding to ~90 degrees out of phase.
    store_vhist(vval);
    vdel = retrieve_vhist(vhist_lookback);

    // Accumulate...
    vstats.val_sum += vval;  // ... average voltage
    mac16x16_32(vstats.val2_sum,vval,vval); // .. squared voltage
    if (vmains_fprod == 0) {
      mac16x16_32(vstats.proddel_sum,vval,vdel); // .. cross voltage (vnow x vthen)
    }
    //vstats.val2_sum += (int32_t) vval*vval;
    //if (vmains_fprod == 0) {
    //  vstats.proddel_sum += (int32_t) vval*vdel;
    //}
    vstats.n ++;

    // min/max statistics
    if (vval > vstats.val_max) vstats.val_max = vval;
    if (vval < vstats.val_min) vstats.val_min = vval;
  }

  // Compute current stats
  for (j = 0; j<N_CUR_CHAN; j++) {
    int16_t val = reading->vals[j+1];
    if (istats[j].present) {
      // save old value and current value
      istats[j].oldval = istats[j].val;
      istats[j].val = val;

      // Accumulate ... 
      istats[j].val_sum += val;  // ... average current
      //istats[j].val2_sum += (int32_t) val*val;
      //istats[j].prod_sum += (int32_t) val*vval;
      //istats[j].proddel_sum += (int32_t) val*vdel;
      
      mac16x16_32(istats[j].val2_sum,val,val);    // .. squared current
      mac16x16_32(istats[j].prod_sum,val,vval);   // .. current x vnow
      mac16x16_32(istats[j].proddel_sum,val,vdel);// .. current x vthen

      istats[j].n ++;
    }
  }

  // Determine if we are at zero-crossing
  zero_crossing = (vstats.oldval < 0 && vstats.val >= 0);
  // If not, then return immediately
  if (!zero_crossing) return curstate;

  // We are at a zero crossing, so bunch more calculations could be coming
  ncycles++;

  // Wait duration of at least tdur
  if ((reading->t - start_time) < tdur) return curstate;

  // ONLY REACH HERE if we have accumulated the appropriate number of cycles
  // Go onward to compute statistics
  return STATE_CALS;
}

// =========================================================
// STATE_CALS: Calculate statistics
//   reading - current ADC reading
//   curstate - current state
//   nextstate - default next state
uint8_t calc_stats(struct adc_readings_struct *reading,
                   uint8_t curstate, uint8_t nextstate)
{
  static float itot_old = -999;
  static uint32_t t_report_vrms = 0, t_report_pow = 0, t_report_energy = 0;
  float vavg, itot = 0.0;
  uint8_t reported = 0;
  float invwt;
  uint8_t j;
  float accum_time;
  static uint16_t ncycles_freq = 0; // Accumulated data for accurate frequency measurement
  static float accum_freq = 0.0;

  float crest_factor = 1.0, vmains_freq = 0.0;

  invwt = 1.0 / vstats.n;               // For averaging
  accum_time = 1.0e-6*(reading->t - start_time); // [sec] Accumulation duration since start to now
  
  // MAINS VOLTAGE CALCULATIONS
  {

    float vavg2, vrms2, vrms;
    float vcal = invwt * VCAL, vcal2 = vcal * VCAL;

    // Compute running average voltage
    vavg = (float) vstats.val_sum * vcal; // local average
    if (vavg_ra == 0) vavg_ra = vavg;
    vavg_ra = RA_PAST * vavg_ra + RA_CUR * vavg;  // running average

    // Now compute RMS voltage as sqrt(<V^2> - <V>)
    vavg2 = vavg_ra*vavg_ra;
    vrms2 = (float) vstats.val2_sum * vcal2 - vavg2;
    if (vrms2 <= 0) vrms2 = 0; // guard domain error
    vrms = sqrt(vrms2);
    vstats.val_rms = vrms;
    // Compute voltage peak half-amplitude
    vstats.pow_ac = (vstats.val_max-vstats.val_min)*VCAL/2.0;
    // Compute the crest factor = SEMI-AMPLITUDE / RMS = 1.414 for sine wave
    if (vstats.val_rms > 100.0) crest_factor = vstats.pow_ac / vstats.val_rms;
    // Compute the mains frequency.  Actually store accumulated data so that
    // we can compute a more accurate frequency value.
    ncycles_freq += ncycles;
    accum_freq   += accum_time;
    
    if (vmains_fprod == 0) {
      //Serial.print("#n=");Serial.println(vstats.n);
      //Serial.print("#proddel_sum=");Serial.println(vstats.proddel_sum);
      vmains_fprod = (float) vstats.proddel_sum * invwt * VCAL2 - vavg2;
      vmains_fprod /= vrms2;
      //Serial.print("#vmains_fprod=");Serial.println(vmains_fprod,5);
      push_report_float("vdel", vmains_fprod, 4, 0);
      push_report_break();
      reported = 1;
    }


  }

  // Calculations: current transformer power measurements
  itot = 0.0;
  for (j = 0; j<N_CUR_CHAN; j++) {
    if (istats[j].present) {
      float iavg, iavg2, irms2, irms;
      float pre0, pac0, pre1, pac1;
      float p_offset;
      float ical1 = ical[j]*invwt, ical2 = ical1*ical[j], ivcal = ical1*VCAL;
      int32_t old_energy_active = energy_active, old_energy_reactive = energy_reactive;

      // Compute running average current
      iavg = (float) istats[j].val_sum * ical1; // local average
      if (iavg_ra[j] == 0) iavg_ra[j] = iavg;
      iavg_ra[j] = RA_PAST * iavg_ra[j] + RA_CUR * iavg; // running average
      
      iavg2 = iavg_ra[j]*iavg_ra[j];   // iavg^2 = bias in I
      p_offset = vavg_ra * iavg_ra[j]; // iavg*v_avg = bias in P

      // RMS current
      irms2 = (float) istats[j].val2_sum * ical2 - iavg2;
      if (irms2 <= 0) irms2 = 0; // guard domain error
      irms = sqrt(irms2);
      istats[j].val_rms = irms;
      itot += irms;

      // Raw active and reactive power
      pac0 = (float) istats[j].prod_sum    * ivcal - p_offset;
      pre0 = (float) istats[j].proddel_sum * ivcal - p_offset;

      // Correct reactive power for not being perfectly 90 degrees behind active  
      pac1 = pac0;
      pre1 = pre0 - vmains_fprod*pac0;
      
      // Correct for phase offsets, to get final measure active & reactive powers
      istats[j].pow_ac =  cosph[j]*pac1 - sinph[j]*pre1;
      istats[j].pow_re = +sinph[j]*pac1 + cosph[j]*pre1; // + for inductive loads

      // Compute accumulated energy = (time)*(power)
      // Note that this calculation is done in energy units of Watt-sec
      // At the smallest measurable loads of 10-20 Watts, this unit has plenty
      // of resolution.  At the largest measureable loads of 100 Amp per circuit
      // we do not have overflow.  This is 100Amp x 240VAC = 24000 W-hr which
      // is under the 32K overflow limit.
      energy_fracac += ( accum_time * istats[j].pow_ac ); // Energy in Watt-sec
      energy_fracre += ( accum_time * istats[j].pow_re );
      // Any rollovers of 3600 Watt-sec is a Watt-hr
      while (energy_fracac > +3600) { energy_fracac -= 3600; energy_active ++; }
      while (energy_fracac < -3600) { energy_fracac += 3600; energy_active --; }
      while (energy_fracre > +3600) { energy_fracre -= 3600; energy_reactive ++; }
      while (energy_fracre < -3600) { energy_fracre += 3600; energy_reactive --; }
      
      // Detect signed rollover of 32-bit integer
      if ((old_energy_active >  0x70000000 && energy_active < 0) ||
          (old_energy_active < -0x70000000 && energy_active > 0)) energy_active = 0;
      if ((old_energy_reactive >  0x70000000 && energy_reactive < 0) ||
          (old_energy_reactive < -0x70000000 && energy_reactive > 0)) energy_reactive = 0;
    }
  }

  // Decide on which items to report
  uint8_t report_voltage = ( t_report_vrms == 0 || (reading->t - t_report_vrms) > REPORT_VRMS_PERIOD );
  uint8_t report_power = (itot_old == -999  // initial reading
      || fabs(itot - itot_old) > REPORT_POW_ILIMIT  // Current limit changes
      || (reading->t - t_report_pow) > REPORT_POW_PERIOD);

  // Reporting: voltage (and always report voltage with current)
  if (report_voltage || report_power) {
    push_report_float("vrms",vstats.val_rms, 2, 0);
    if (ncycles_freq > 0 && accum_freq > 0) {
      float vmains_freq = ncycles_freq / accum_freq;
      push_report_float("vfrq",vmains_freq, 3, 0);
      ncycles_freq = 0;
      accum_freq   = 0;
    }
    push_report_float("vcrs",crest_factor, 3, 0);
    t_report_vrms = reading->t;
    reported = 1;
  }
  // Reporting: current and power.  Do an update when...
  if (report_power) { // Time limit expires
    char irmnam[5] = "irm0", pacnam[5] = "pac0", prenam[5] = "pre0";
    char pwfnam[5] = "pow0";

    for (j = 0; j<N_CUR_CHAN; j++) {
      if (istats[j].present) {
        float pap, power_factor;
        
        // RMS current
        irmnam[3] = '0'+j; push_report_float(irmnam, istats[j].val_rms, 3, 0);
        // Active and reactive power
        pacnam[3] = '0'+j; push_report_float(pacnam, istats[j].pow_ac, 1, 0); // pac - active power
        prenam[3] = '0'+j; push_report_float(prenam, istats[j].pow_re, 1, 0); // pre - reactive power
        pap = (istats[j].val_rms*vstats.val_rms);
        power_factor = 1.0;
        if (pap > MIN_POWER && pap >= istats[j].pow_ac) power_factor = istats[j].pow_ac / pap;
        pwfnam[3] = '0'+j; push_report_float(pwfnam, power_factor, 4, 0);
        reported = 1;
      }
    }

    t_report_pow = reading->t;
    itot_old = itot;
  }

  // Reporting: total energy usage
  if ( t_report_energy == 0 || (reading->t - t_report_energy) > REPORT_ENERGY_PERIOD) {
    push_report_int32("enac",energy_active, 1);
    push_report_int32("enre",energy_reactive, 1);
    t_report_energy = reading->t;
    reported = 1;
  }

  // Reset the accumulated statistics
  start_time = reading->t;
  ncycles = 0;
  init_stats(&vstats);
  for (j=0; j<N_CUR_CHAN; j++) init_stats(&istats[j]);
  
  // Reporting: If we reported other any other data, also report
  // realtime data processing info
  if (reported) {
    report_pulse_count();
    push_report_int32("adcd", max_adc_depth, 1);
    push_report_int32("novr", n_overflow, 1);
    push_report_uint32("uptm", update_uptime(), 1);
    push_report_break();
    max_adc_depth = 0;      
  }
  return nextstate;
}


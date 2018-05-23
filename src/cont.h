//   EMONTX3-CONTINUOUS - continuous sampling Arduino firmware
// 
//   Copyright (C) 2018 C. B. Markwardt
//   License: GNU GPL V3
//
//   Low-level configuration of the firmware.
//

// ======================================
// DEBUG_CONT: If set, then we are in debugging mode.
// In debugging mode we always get all samples every second.  It also enables
// an additional calibration factor which may allow you to calibrate your
// device more easily in a test environment, before you deploy to the real
// environment (see cal.h)
// #define DEBUG_CONT

// ======================================
// ADC_PRESCALAR: This is the ADC clock prescalar.  For the emonTX which operates
// 16 MHz, the ADC clock is (16 MHz)/(ADC_PRESCALAR).  This affects
// how many samples per mains AC cycle can be retrieved.
//       ADC_PRESCALAR=128 : ADC clock = 125 kHz : 32 samples at 60 Hz : 38.5 samples @ 50 Hz
//       ADC_PRESCALAR=64  : ADC clock = 250 kHz : 64 samples at 60 Hz : 77   samples @ 50 Hz
// The AVR data sheet recommends not setting the ADC clock above 200 kHz,
// so 250 kHz is a bit high.  On the other hand, the data sheet speculates
// that any prescalar value at or below 1 MHz will have a negligible effect.
// Nick Gammon's research shows that an ADC clock of 250 kHz has no effect
// on performance.  The code deals with the fact that the ADC clock is not exactly in 
// sync with the mains AC cycle.
// 
// Therefore, there is really no penalty with going to the faster sampling of
// ADC_PRESCALAR=64 and every benefit.  Go for it.
// #define ADC_PRESCALAR 128  // 32 samples @ 60 Hz : 38.5 samples @ 50 Hz
#define ADC_PRESCALAR 64      // 64 samples @ 60 Hz : 77   samples @ 50 Hz

// Now you may ask, what if I go to even faster prescalars?  The answer is, 
// probably not worth it.  First of all, we need more memory for ring buffer
// samples.  Also, with an ADC clock of 500 kHz or higher, now we are getting
// close to the really-not-recommended clock of 1000 kHz where the signal can
// be distorted.  Also, WARNING, setting faster may lead to overflow in
// stabilize_inputs().

// ======================================
// DIP switch that selects mains voltage
#define DIP_VMAINS 9

// ==============================================
// Reporting frequency.  Normally we report less frequently...
#define SECS (1000000)
#ifndef DEBUG_CONT
#define REPORT_VRMS_PERIOD (10*SECS)    // [us] report voltage every 10 sec
#define REPORT_POW_PERIOD  (30*SECS)    // [us] report power/current every 30 sec
#define REPORT_ENERGY_PERIOD (60*SECS)  // [us] report energy every 60 sec
#define REPORT_PULSE_PERIOD (1*SECS)  // [us] report pulse count every 120 sec
#else
// ... but for debugging purposes, we report every second.
#define REPORT_VRMS_PERIOD (1*SECS)    // [us] 
#define REPORT_POW_PERIOD  (1*SECS)    // [us] 
#define REPORT_ENERGY_PERIOD (1*SECS)  // [us] 
#define REPORT_PULSE_PERIOD (1*SECS)   // [us] 
#endif
#define REPORT_POW_ILIMIT  (1.1)        // [Amp] report power/current when current changes by this much
#define MIN_POWER 30.0                  // [Watt] Minimum power needed to computer power factor
#define STABILIZE_DURATION (10*SECS)    // [us] time to wait for mains voltages to stabilize (10 sec)

// State machine definitions
#define STATE_STAB 0 // Stabilize inputs
#define STATE_SCAN 1 // Scan inputs for signals present
#define STATE_ZER1 2 // Wait for next zero crossing
#define STATE_FREQ 3 // Accumulate input mains frequency data
#define STATE_CALF 4 // Calculate mains frequency
#define STATE_STAT 5 // Accumulate stats
#define STATE_CALS 6 // Calculate statistics

// ADC Input channels to process.  Can't just change this because it appears
// in other places such as adc.cpp.
//   Channel 0 is voltage (required)
//   Channel 1-4 are current transformer (optional)
#define N_ADC_CHAN 5
#define N_CUR_CHAN (N_ADC_CHAN-1)
extern const uint8_t adc_chans[N_ADC_CHAN];
extern volatile uint16_t n_overflow;

// Size of ADC readings ring buffer
// This buffer must be able to accomodate all ADC readings stored by the 
// ISR for the worst case processing load.  This value of 16 can accomodate 
// an ADC prescalar of 64 ("overclocking" the ADC clock).
//   Max usage at prescalar 128 ~  8
//   Max usage at prescalar  64 ~ 12
#define N_READINGS 16
// ADC readings consist of the N_ADC_CHAN values read, plus the time
struct adc_readings_struct {
  int16_t vals[N_ADC_CHAN];
  uint32_t t;
  uint8_t set;
};

// Where we store and accumulate all the interesting readings for each
// ADC input channel.
struct reading_stats {
  uint8_t present;
  uint16_t n;
  int16_t  val, oldval;
  int32_t  wt_sum;
  int32_t  val_sum;
  uint32_t val2_sum;
  int16_t  val_min, val_max;
  int32_t  prod_sum, proddel_sum;
  float val_rms, pow_ac, pow_re;
};
// Accumulated stats for voltage and current channels
extern struct reading_stats vstats, istats[N_CUR_CHAN];

// Size of voltage history ring buffer.
//   For ADC prescalar of 128, this needs to be at least  9 for 60 Hz, 11 for 50 Hz
//   For ADC prescalar of  64, this needs to be at least 18 for 60 Hz, 21 for 50 Hz
#define N_VHIST_RING 24

// Size of ring buffer for data reports.  These are the actual voltage, power current, 
// and metadata reports that go out via Serial.  Maximum number of reports per second
// are Voltage: vrms, vcrs, vfrq
//     Power  : 4x(pac_, pre_, pow_, irm_)
//     Pulse samples: pulse
//     Metadata: _evers, _adcd, _novr, _uptm, <break>
// Total of 26
#define N_REPORT 31
#define VOID_TYPE 0
#define BREAK_TYPE 1
#define FLOAT_TYPE 2
#define INT32_TYPE 3
#define UINT32_TYPE 4
struct report_struct {
  char name[6];
  uint8_t type;
  uint8_t digits;
  union {
    float floatval;
    int32_t int32val;
    uint32_t uint32val;
  } value;
};


// Forward function definitions ======================
// adc.cc
void set_adc_offset(uint8_t chan, int16_t offset);
extern uint8_t get_adc_depth(void);
extern void init_adc(uint8_t prescalar);
extern void init_adc_chans(void);
extern void disable_adc_chan(uint8_t chan);
extern uint8_t get_next_adc_reading(struct adc_readings_struct *data);
extern void reset_overflow(void);

// state.cc
extern void init_cal(void);
extern uint8_t stabilize_inputs(struct adc_readings_struct *,
                                uint8_t curstate, uint8_t nextstate);
extern uint8_t scan_inputs(struct adc_readings_struct *,
                                uint8_t curstate, uint8_t nextstate);
extern uint8_t zero_crossing(struct adc_readings_struct *reading, uint8_t nclear,
                      uint8_t curstate, uint8_t nextstate);
uint8_t accum_freq(struct adc_readings_struct *reading, 
                      uint8_t curstate, uint8_t nextstate);                      
uint8_t calc_freq(struct adc_readings_struct *reading,
                   uint8_t curstate, uint8_t nextstate);
extern uint8_t accum_stats(struct adc_readings_struct *reading, uint32_t tdur,
                      uint8_t curstate, uint8_t nextstate);
uint8_t calc_stats(struct adc_readings_struct *reading,
                   uint8_t curstate, uint8_t nextstate);
                   
// report
extern void push_report_float(const char name[5], float value, uint8_t digits, uint8_t retained);
extern void push_report_int32(const char name[5], int32_t value, uint8_t retained);
extern void push_report_uint32(const char name[5], uint32_t value, uint8_t retained);
extern void push_report_break(void);
extern void send_report(void);
                      
// main
extern uint8_t max_adc_depth;
extern uint32_t sample_period;
extern uint32_t vmains_period;
extern float vmains_fprod;

// pulse
void init_pulse(void);
void record_pulse_count(void);
void report_pulse_count(void);

// ======================================
// If we are not using the external library definition of mac16x16_32 for
// fast multiply+accumulate, we define it here as a slower version
#ifndef mac16x16_32
#define mac16x16_32(a,b,c) (a) += (int32_t) (b)*(c)
#endif
  
                      
// ======================================
// Running average constants.  Since averages are done every second, this
// is equivalent to 1/RA_CUR [seconds] ~ 100 sec time constant.
#define RA_PAST (0.99)
#define RA_CUR  (1.0 - RA_PAST)


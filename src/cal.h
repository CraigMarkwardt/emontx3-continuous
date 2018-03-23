//   EMONTX3-CONTINUOUS - continuous sampling Arduino firmware
// 
//   Copyright (C) 2018 C. B. Markwardt
//   License: GNU GPL V3
//
// Calibration constants
//

// ======================================
// Voltage calibration from ADU to Volts AC
// 13.0 is the default resistor divider ratio in the emonTx3
// 3.3V is the standard Vcc for the emonTx3
// 1024 is the full-range of the emonTx3 ADC
// ~230V or ~120V is the mains AC voltage input
// ~11V is the transformer AC voltage output
// ~1 is correction factor with in-circuit usage
#define VCAL_120VAC (121.8/11.140 * 1.0000)  // 120VAC transformer
#define VCAL_240VAC (230.0/11.116 * 1.0000)  // 240VAC transformer
#define VCAL_ADC (13.0 * 3.3 / 1024.0)      // ADC conversion

// Calibration "fudges" - extra correction terms to apply to all channels
#ifdef DEBUG_CONT
#define ICALFUDGE 1.0         
#else
#define ICALFUDGE (1.0/1.000) 
#endif

// ======================================
// Current calibration from ADU to Ampere
// 2000 is number of turns on SCT-013-000 current transformer sensor
// 3.3V is the standard Vcc for emonTx3
// 1024 is the full-range of the emonTx3 ADC
// 22 is the default burden resistor in the emonTx3
// ~1 is the correction factor with in-circuit usage
#define ICAL (ICALFUDGE*2000.0*3.3/1024.0) // (2000 turns 3.3 Volt range over 1024 ADU) 
#define ICAL0 (ICAL/22 *1.00000)   // [A/ADU] chan 0 in-circuit burden resistor 22 Ohm + cal factor
#define ICAL1 (ICAL/22 *1.00000)   // chan 1
#define ICAL2 (ICAL/22 *1.00000)   // chan 2
#define ICAL3 (ICAL/120*1.00000)   // chan 3 has 120 Ohm burden resistor

// ======================================
// Phase offset of the current [deg] w.r.t. voltage sample
#define PHV  (0.00)
// Phase offset of the current transformer [deg] w.r.t. its own sample, plus
// any variance on a per-channel basis
#define IPH0  (+0.00)  // [deg] for CT input channel 0
#define IPH1  (+0.00)  // [deg] chan 1
#define IPH2  (+0.00)  // [deg] chan 2
#define IPH3  (+0.00)  // [deg] chan 3




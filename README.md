# EMONTX3-CONTINUOUS

C. B. Markwardt

This is emontx3-continuous, a firmware for emonTx energy monitors
using an Arduino microcontroller.  Its key benefits are that it
provides high rate (19230 samples/second) continuous sample of your
power usage and reports cumulative energy use like a utility meter.
It reports your active and reactive power usage to allow you to
distinguish between resistive, capacitive and inductive loads in your
household.  It also reports your mains voltage, frequency and mains
quality.  It is designed to work with the "EmonESP" ESP8266 wifi
output, transformer input, and 5V USB.

This firmware is recommended for users who:
  * intend to connect via EmonESP (not the built-in RFM69) 
  * have an AC transformer input to measure mains voltage 
  * want to achieve maximum accuracy through calibration (which
    requires rebuilding the firmware using the Arduino IDE)

This firmware is not recommended for users who:
  * use an emonBase (via the on-board RFM69); because RFM69 support is not possible
  * don't have the AC-AC voltage sensor adapter; because the features
    of emonTx3-continuous require the mains voltage input.

## Summary of Version Changes

 * version 1005 - initial release to public
 * version 1006 - bug fix for pulse counting; add ADC_NOTICE_CHAN to allow disregarding one or more current transformer inputs
    
## Introduction

This firmware is designed to work with the emonTx version 3 series of
hardware available from openenergymonitor.org.  The hardware consists
of up to four current transformers to measure mains power usage, as
well as a pulse counter for energy meter monitoring.  The hardware
also has a RFM69 transmitter that can send data to a base station.
However, this firmware cannot use the RFM69 transmitter and is
designed to work with an ESP8266 "EmonESP" to transmit to a data
collector.

As of this writing, the standard firmware available for the EmonTX
performs periodic monitoring of power usage, and hence is not a true
energy monitor.  

The standard firmware wakes up periodically, samples the mains power
on each input in turn, and then goes to sleep for 1 second.  This
strategy will be acceptable for casual monitoring but does have
limitations.  Power usage spikes or transients that happen between
periodic wake-ups will be missed.

Also, the standard firmware (using EmonLib) samples the inputs slowly
using the standard Arduino library functions, and in a non-uniform way
that makes interpretation more difficult.  Home power usage often has
short spikes that can be missed by this strategy.  To have accurate
measures of true power usage, one desires high rate and uniform
sampling.

This firmware, emonTx3-continuous, solves these problems.  It performs
continuous (non-stop) monitoring of all four hardware inputs to
provide true measures of home power usage.  The four inputs are
sampled at high rate (up to 19230 samples per second, 3846 samples per
input channel per second), which provides more than 64 samples per
mains AC waveform.  It measures mains voltage, current, in-phase
(active) power and out-of-phase (reactive) power.  Reactive power is
signed to allow to distinguish between capacitive loads (such as LED
lighting) and inductive loads (such as fans, compressors and motors).
Finally, this firmware provides diagnostics 


## Features

The emontx3-continuous firmware has the following features.

  * Continuously samples inputs at 19230 samples/second, or 3846
    samples/second for each input channel
  * Intended to be used with "EmonESP" wifi transmitter
  * This sample rate is equivalent to 77 samples per 50 Hz AC waveform
    and 64 samples per 60 Hz AC waveform, which allows to capture most
    fast power transients such as switch-mode power supplies.
  * Supports all four emonTx current transformer inputs
  * Requires AC-AC voltage sense input, so that mains voltage can be
    measured
  * For each input channel reports, RMS current, power factor, active,
    reactive and apparent power.  Active power is power in-phase with
    the mains voltage such as incandescent bulbs; and reactive power
    is 90 degrees out-of-phase with mains voltage (typically AC
    motors, compressors and fans).
  * Allows to disable one ore more current transformer inputs.
  * Reports total cumulative active and reactive energy usage.  This
    is cumulative and builds up over time, just like a utility meter.
  * Reports the AC mains voltage, mains frequency to three digits of
    accuracy, and the AC crest factor, which can be used to diagnose
    AC power faults.
  * Automatically determines mains AC frequency at startup (50 Hz or 60 Hz)
  * Is fully calibratable.  Voltage, current and phase offset
    calibration factors are available.  
  * Adjusts for sample time offsets beteween voltage and current, with
    accuracy of better than 1%.
  * Should be compatible with 3-phase power systems.  The phase offset
    variables can be set to align all inputs to one voltage leg.
  * Measures pulse input to allow utility meter pulse input (not tested).
  * Reports other diagnostic information such as uptime, version, and
    internal buffer sizes.

Features not supported

  * the firmware uses features specific to the Atmel hardware, and
    will be difficult to port to other architectures.
  * the RFM69 transmitter on-board the emonTx is not supported; the
    transmitter software library is potentially compatible with
    emontx3-continuous; however, the reported values are so limited
    that most of the functionality of this firmware would be lost.
  * the standard emonTx temperature/humidity sensor accessory is not
    usable because the DallasTemperature library is not compatible
    with the methods used by emonTx3-continous to achieve high sample
    rates (it is busy-wait driven instead of interrupt-driven).
  * a mains AC-AC transformer is required.  No-voltage input is not
    supported.  Without the AC-AC transformer, accuracy will be much
    lower and you may as well use the standard emonTx firmware.
  * because of the AC-AC transformer requirement, you will also need
    to power the unit separately with 5VDC (with the USB input, for
    example)

## Achievable Accuracy

With full calibration to a suitable standard, it should be possible to
achieve high accuracies.  The author used a 1% accuracy digital
multimeter to calibrate the system. When compared to utility
measurements, the results agreed to within 1%.

## How It Works

The emontx-continuous firmware uses the "free-running" sampling mode
of the standard Atmega328p (Arduino Uno) that is inside the emonTx.
The firmware also functions in a stock Atmega2560 (Arduino Mega)
although it has not been tested with real power monitoring
accessories.  The microcontroller can be programmed to report
continuous samples at high data rate.  In addition, the unit can be
programmed to sample each data input in round-robin fashion, which
allows emontx-continuous to retrieve the mains voltage, and four
current inputs.

### Sample Rate

The maximum recommended sample rate for the Atmega328p is 9615 samples
per second.  However, the sample clock can be slightly overclocked
with little or no loss in accuracy.  This firmware drives the sample
rate to 19230 samples per second.  With five input channels, one mains
voltage and four current inputs, the sample rate is 3846 samples per
second for each channel, or 260 microseconds between samples.
Surprisingly, this is faster sampling than the ~400 microseconds per
sample that the standard firmware achieves for only a single channel
at a time, and emontx-continuous samples all four inputs continuously.

For a 50 Hz AC mains voltage, the resulting sample rate is 76.92
samples per waveform.  For a 60 Hz AC mains voltage, the sample rate
is 64 samples per waveform.  In both cases, this provides this
provides measurements that approach "true RMS" in quality (typically
~100 samples or more per waveform).  Today's homes have devices that
draw power irregularly: either in short spikes for most switch mode
power supplies; or as partial waveforms such as dimmer switches.  The
sampling emontx-continuous provides is sufficient to retrieve most of
these short transient power usage styles.  At the given sample rate,
emontx-continuous can retrieve up to 6th harmonic distortions.

### Interrupt Driven Sampling and Ring Buffer

High sample rates do come at a cost.  Only a small amount of
processing can be done between samples.  Rather than attempt to
perform all computations between samples, this firmware takes a
different approach.  

Samples are recorded by an interrupt handler.  When five complete
samples are ready, the handler places the results in an ADC ring
buffer.  In this way, the interrupt handler can be kept short with low
overhead.  It is the responsibility of the main program to retrieve
samples from the buffer and process them.

The main advantage of the ADC ring buffer is that intensive
computations are allowed to take longer than one ADC sample.  When the
processor is working on intensive computations, ADC readings will pile
up in the ring buffer.  As long as the ring buffer is deep enough,
readings can pile up momentarily, and then during idle periods the
system can drain the buffer and catch up.  This works as long as the
average processing time per sample takes less than the duration
between samples.  The buffer size has been tuned for this firmware to
15 samples.  In several months of real operation, the buffer depth has
never exceeded 12 samples.

### Fast Math

This firmware uses fast math routines from Atmel's AVR201 library to
make accumulating quantities faster.

### Fast Response

Typically the firmware will report voltage mains information every 10
seconds and power usage every 30 seconds.  However, if you have a
sudden change of usage by more than about an Ampere, a new report will
be issued immediately.  This allows you to get a more accurate picture
of actual power usage, including transients, instead of having to wait
30 seconds for a response.

### Delayed Processing

Despite this strategy, there can still be problems with the high
sample rate.  The most difficult example is serial data output.  The
only way to transmit data to the user is via serial data transmission.  

Thankfully, the Arduino Serial object is interrupt driven.  This means
that when data is requested to be transmitted, it is placed in a
serial output ring buffer, and an interrupt service routine is used to
drain the buffer.  However, if too much data is requested to be
transmitted at one time, the ring buffer is filled and the Serial
library will busy-wait until it is drained.  This can be fatal for the
ADC ring buffer which only has a small number of samples before it
will overflow.

To prevent this, the firmware provides yet another output ring buffer
for measurement readings.  The processing code produces a large number
of readings all at once (usually every 10-30 seconds), and those
readings are placed in a ring buffer.

The emontx-continuous firmware will wait until the output Serial
buffer and ADC ring buffer is drained sufficiently before sending more
data.  During off-peak periods there is plenty of processing power
available to send this data with no other impacts.

### Active and Reactive Power Sampling

Typical firmwares will compute simple active and apparent power values
for each input.  While these outputs are useful, there is more information
available from the data which can reveal insights about power usage.

There are several types of power usage habits within most home
systems.  The standard usage habit we are used to from the past is
resistive loads such as incandescent light bulbs, which are purely
resistive.  Other examples of such loads are baseboard heaters,
underfloor heating elements, and auxiliary heat on home heat systems.
This type of load will produce current draw that is in-phase with the
voltage mains.  In other words, current drawn will peak at exactly the
same time as the voltage.  This is known as active power usage.

Another form of power usage is reactive loads, such as inductive and
capacitive loads.

Motors, compressors, fans and other types of coil devices are
inductive loads.  Other examples include microwaves.  These loads will
draw current with a phase lag.  This means that current will peak 90
degrees later than the voltage peaks.

Capacitive loads are the opposite from inductive lods in the sense
that the current will peak 90 degrees before the voltage peaks.  These
days LED lighting is popular, and they are typically fed by a
capacitive dropper power supply which is a capacitive load.  Sodium
arc lamps are also strongly capacitive.

Both types of reactive load do not actually draw power from the
utility's generators, and are typically not billed.  However, reactive
loads they do cause an increased current load on utility lines.

Finally, as mentioned before there are more complicated types of
loads.  Today's small electronics power supplies typically have a more
spiky current draw behavior that is neither in-phase nor 90 degrees
out-of-phase of the mains voltage.  The are simply higher harmonics
that you can be aware of.

emontx-continuous allows you to diagnose all types of power usage.  It
reports several quantities.

It reports the active and reactive power usage (see below for data
readings reported).  The reactive power usage is a *signed* quantity:
a positive value indicates that capacitive loads such as LED lighting
dominate power usage; a negative value indicates that inductive loads
such as compressors or motors dominate.  In this way you can
scrutinize your power usage more carefully and see which kinds of
loads are present on your home system.  Typically, the active power
usage is what is billed by your utility.

It also reports the power factor and total current usage.  The power
factor gives a cruder but more global view of your power usage habits.
Power factor of unity indicates a purely resistive load.  A power
factor of less than unity indicates some combination of reactive loads
or more spiky loads such as switch mode power supplies.  If you
compute (active_power) / (power_factor), you will obtain your overall
apparent power, which is an indicator of total power you use, whether
active or reactive.  Total current usage also indicates if you are
meeting or exceeding current requirements of your home wiring.

In order to provide these measurements, emontx-continuous must have
available a 90-degree out-of-phase or quadrature sample of the mains
voltage.  emontx-continuous uses a simple way to look back in time to
retrieve this quantity.  It keeps a short record of voltage samples
and when needed uses a lookup function to retrieve the sample nearest
to 90 degrees in the past.  Since this sample will not fall *exactly*
90 degrees in the past, the firmware uses a simple correction factor
method which achieves accuracies of 0.1% or better for sinusoidal
mains voltages.

Using these techniques, emontx-continuous is able to provide a wide
range of useful and interesting measurements about your utility usage.

## Building and Using the Firmware

### Building and Installing

Unlike the standard firmware, there are no signiicant Arduino library
requirements.  Just download this Arduino sketch as a zip archive, and
extract it in your Arduino documents folder.  When you start the Arduino
application, you should see emontx3-continuous as a choice to open.

This firmware was tested with Arduino version 1.8.4, but should be
compatible with earlier and later versions.

Under the Tools menu, select Board -> Arduino/Genuino.  Click the
"build" button, which is the large checkmark icon.  The firmware
should build without errors.

Disconnect your emontx from the mains and connect it to your computer
using a mini-USB cable.  Also disconnect any EmonESP, since it will
interfere with programming.  Your emontx should show up as a virtual
COM port or tty device; check the Tools -> Port menu and select your
new device.

Finally, click the upload button (the right-arrow button), which
should upload and verify the firmware.

### Using the Firmware

The firmware should start immediately upon power-up.  To use it,
follow these steps.

Disconnect the emontx from your computer.  

Connect an EmonESP device, and then connect the emontx to your power
inputs (AC-AC transformer, and up to 4 current transformer inputs).
Lastly, connect USB power.  To ensure a clean start-up, you can press
the reset button on the device.

If this is the first time you are using an EmonESP, follow its
installation instructions to connect it to your wifi network and
configure output for either emonCMS or MQTT.

After reset, the emontx device should activate immediately.  Within
about 10 seconds of settling time, the first measurements should begin
to appear.

If you forget to connect your current transformers, or change your
sensor arrangement, press the reset button so that emontx-continuous
can recognize its new inputs.

### Debugging or Calibration

If you are debugging or calibrating your emonTx, you do not have to
connect the EmonESP device.  Instead, keep your emontx connected to
your computer and open the Arduino serial monitor.  The output of
emonTx is pure text that you can read and diagnose.

### Configuring

The firmware is configured to work right away with no extra settings.
However, in practice you may want to change a few things.

emontx3-continuous will automatically disregard any disconnected
current transformer channels.  This functionality relies upon a
pull-down resistor installed in the standard emontx3 hardware that
sets an input to zero if the physical plug is not connected.  If you
are using non-standard hardware, or if your inputs are sometimes
unreliable, you can disable some channels if you wish.  To do this,
edit the cont.h file and locate the line ADC_NOTICE_CHANS, and follow
the instructions there.  emontx3-continuous will still disregard
channels that are disconnected, but it will also ignore any channels
that you designate.

Another area where you will likely want to configure your firmware is
detailed calibration for your specific sensors and hardware.  See
below in the Calibration section for more information.

## Available Readings

The emontx-continuous provides many readings about your home power
system.  Since it is different and more advanced than the standard
emonTx firmware, do not expect the readings to be exactly the same.
You will need to develop new charts and visualizations for all the
data you are producing.  Here is a description of the data fields.

Readings are produced with a 4-character name, such as "vrms" which
identify the reading, and may be an integer or floating point value.
Note that some readings start with an underscore, such as _ever.
These are intended to be MQTT "retained" readings that are kept for
late clients, and are typically reserved for readings which are
diagnostic in nature.

### Mains Voltage

emontx-continuous provides readings about your mains voltage,
frequency and quality.  This can inform you about the quality of mains
voltage you are receiving from your utility.  These values are
reported every 10 seconds.

 * **vman** - mains AC voltage dip switch setting, in volts.  Either 120 or 240.
 * **vrms** - mains AC voltage, in volts.
 * **vfrq** - mains AC frequency, in Hertz.
 * **vcrs** - mains AC crest factor, as a fraction.  This is a
     diagnostic of your mains voltage quality.  Crest factor is
     defined as Vmax/Vac where Vmax is the maximum voltage and Vac is
     the AC (rms) voltage.  For typical AC voltage, the crest factor
     is 1.414; deviations indicate non-sinusoidal voltage condition.

### Power Usage

emontx-continuous reports power usage of each of the four inputs.  It
reports the rms current, active and reactive power, and power factor.
These readings are produced every 30 seconds, and represent the
average power usage over the past 30 second interval.

 * **irmsN** - for current sensor N, rms current usage in Amps.
 * **pacN** - for current sensor N, active power usage in Watts.
 * **preN** - for current sensor N, reactive power usage in Watts.  A
   positive value indicates capacitive load, negative indicates
   inductive.
 * **powN** - for current sensor N, fractional power factor.  Power
     factor is defined as powN = pacN / papN, where papN =
     (vrms*irmsN) is the apparent power usage.

### Cumulative energy monitoring

emontx-continuous reports cumulative energy usage just like an energy
meter.  It is the true continuous sum of energy usage without
interruptions, averaged over the full waveform of every AC cycle.  The
reported values are produced every 60 seconds, and represent the sum
of all available input channels.

 * **_enac** - total cumulative active energy usage, in kWh, for all
     four input channels combined.  
 * **_enre** - total cumulative reactive energy usage, in kWh, for all
     four input channels combined.  Your
     power company typically does not bill you for this energy usage.

### Pulse counter

If you have a utility meter with LED pulser, you can retrieve these
pulses with a pulse sensor.

 * **pulse** - The reported value is total number of pulses received
      since startup.

### Diagnostics

These diagnostics are typically not useful for reporting home energy
usage, but are useful for diagnosing the state of the device itself.

 * **_ever** - emonTx firmware version, as a 4-digit integer number.
 * **_uptm** - total system uptime, in seconds, since last reset.
 * **_adcd** - maximum ADC ring buffer depth.  A diagnostic which indicates
     possible processing overload.
 * **_novr** - number of ADC samples lost due to ring buffer overflow.
     Any value different than zero indicates processor overload.
 * **vdel** - correction factor for out-of-phase voltage readings, as
     a fractional quantity.


## Calibration

You can use this firmware with default calibration constants, but you
should expect few percent errors in the results.  For the best
accuracy you must calibrate your unit individually.  Here are some
guidelines of how to achive this.

**WARNING**: This calibration process involves working with high
voltage mains electricity.  You must use proper caution.  Only use 
measurement equipment rated for the voltages in your system.

What you will need:
 1. A digital multimeter rated for mains voltage
 2. A test device that uses power.  It should be a primarily resistive device like a toaster.  Anything with a motor or fan is not recommended.  Anything with a microprocessor like a computer or television is not recommended.
 3. An extension cord that you can sacrifice (destroy).
 4. Ability to rebuild Arduino firmware and upload.

The test device should use a large amount of power (current) so that
it provides a good test signal.  Toasters, toaster ovens and electric
heaters are often good choices because they are high power and
resistive in nature.  Anything with a motor or microprocessor will
have large reactive load and will not be suitable for calibration.

You must modify the extension cord by separating the wires.  Do not
remove the insulation, but rather separate the individual wires with
insulation intact.  You will be measuring the current through only one
wire using the clamp-on current transformers.  Since you will be
measuring current, you will also want to cut one of the wires and
splice it together with a wire-nut.  It is most safe to cut and splice
the neutral side, if available, and clamp the current transformers to
the live side.

### Setup
 1. Plug the emonTx AC-AC transformer and extension cord into the same receptacle.
 2. Set the digital multimeter to AC volts, and place the measuring leads of the digital multimeter into the mains receptacle.  WARNING: this is dangerous!
 3. Optional (but useful): rebuild firmware with debugging enabled.
    1. Open the firmware in the Arduino IDE.
    2. Switch to the cont.h tab.
    3. Edit the line which says #define DEBUG_CONT and remove the first two slashes ("//") to uncomment that line.
    4. Rebuild the firmware and upload to your emontx3.
 4. Activate your emonTx and wait for output.

### Voltage Calibration
 5. When a vrms=NNNN.NN number appears, use your multimeter to take a reading of the mains voltage at the same time.  

    Example: vrms=239.82 and multimeter reads 241.96.
 6. Compute the voltage calibration ratio of your emonTx.  Calculate the ratio of (multimeter/vrms).  Example: (241.96/239.82) = 1.00892.
 7. Open cal.h of the firmware source and edit the line for your voltage system.  Change the 1.0000 to your calibration ratio.

    Example: 

    #define VCAL_240VAC (230.0/11.116 * 1.00892)  // 240VAC transformer

### Current Calibration
 8. Attach your clamp current transformers around the live wire.
 9. Safely insert the multimeter into the extension cord circuit.
     1. IMPORTANT: disconnect the extension cord from the mains
     2. Unsplice the extension cord.
     3. Set your digital multimeter to AC current (> 1 Amp).  You will probably have to move the leads to a different input as well.
     4. Securely attach the leads to each end of the unspliced extension cord wire.  Alligator clips are recommended.
     5. Wrap exposed sections with electrical tape to protect them.
     6. Examine the cord for any shorts and correct.
     7. Plug in extension cord to mains receptacle.
     8. Plug test device into extension cord.
 10. First activate your test device.
 11. Second activate the emonTx
 12. When emonTx current readings appear as irm0 through irm3 numbers, take a multimeter current reading at the same time.

    Example: irm0=2.44 and multimeter reads 2.42
 13. Compute the current calibration ratio of your emonTx for each channel.  Calculate the ratio of (multimeter/irmN).  Example: (2.42/2.44) = 0.9918.
 14. Edit cal.h and edit the corresponding lines for ICALN.  Change the 1.0000 to the current calibration ratio for that channel.

     Example:

     #define ICAL0 (ICAL/22*0.9918)
 15. Repeat previous two steps for each input channel you have anbled.
  

### Reactive Load Calibration
 16. No need to rebuild the firmware before this step.
 17. With the test device still active, capture a single output line that has pac0, pre0, pac1, pre1 and so on.  These are the measured actie and reactive components.

     Example:  pac0=550.80,pre0=21.70
 17. For the 0th channel compute the phase calibration ratio (pre0/pac0).  Preserve the sign of the values; it is OK if this ratio is negative.

     Example: (21.70/550.80) = +0.0393972
 18. Compute the phase calibration angle using ATAN(ratio) where ATAN is the arctangent function in degrees.

     Example: ATAN(+0.0393972) = +2.26  (degrees)

 19. Open cal.h of the firmware and edit the line for IPH0 to put in this phase calibration angle.

     Example: #define IPH0  (+2.26)

 20. Repeat above three steps for remaining channels that you have enabled (pac1,pre1; pac2,pre2; pac3,pre3)

### Rebuild Firmware

 21. Disconnect your emonTx from the mains and connect to your computer.
 22. Rebuild and upload the firmware with your new calibration constants using the Arduino IDE.
     1. If you edited cont.h as described above, before rebuilding, open cont.h and place two slashes ("//") before #define DEBUG_CONT to comment the debugging line.  Then rebuild.
 23. Disconnect your emonTx from your computer and reconnect to mains.
 24. Repeat above calibration steps to verify that your emonTx is now reporting correct values.

     Number reported with vrms= should match multimeter reading of voltage measured at same receptacle.

     Number reported with irmN= should match multimeter reading when multimeter is in circuit.

     For a resistive load like a toaster, the preN values should be near zero.  They may not be exactly zero, but within ~20 Watts is OK.

You are done! 


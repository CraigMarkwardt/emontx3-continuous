# emontx3-continuous Sample Firmware

Here are two sample firmware files.  If you want to try uploading
them, use avrdude for your platform.  However, I recommend that you
rebuild the firmware within the Arduino IDE.  Using the Arduino IDE is
pretty easy and it gives you more control.  Also, you will need to
gain familiarity with rebuilding if you attempt calibration.

 * **emontx3-continuous-debug-1005.hex** - firmware version 1005 with debugging enabled.  Reports come more frequently which allows you to see responses to changes more quickly.
 * **emontx3-continuous-std-1005.hex** - standard firmware version 1005 with debugging disabled.  Reports come less frequently for a typical home automation datalogging system.  Internally the system still keeps full energy usage information.

This driver is for the PCI the intel device 82540EM. It uses read and write to
read and write to and from the LED register to control the LEDs on the device.

The write function does not directly write to the LED register with the user
input. It takes a packed u8 that has bit flags for LED options for each LED.
This is so the userspace program isnt directly writing to the LED register which
could overwrite reserved bits and other settings we are not using.

The goal of the driver is to set the LEDs to turn on or off, and blink when they
are on if desired.



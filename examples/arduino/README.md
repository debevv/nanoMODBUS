## Arduino examples

This folder contains nanoMODBUS examples for Arduino.   
To build and load a sketch with the Arduino IDE, copy `nanomodbus.c` and `nanomodbus.h` inside its folder.

`client-rtu` and `server-rtu` are meant to be used with two Arduinos connected via their TX0/RX0 serial pins.

If you have [arduino-cli](https://github.com/arduino/arduino-cli) installed, you can run
`examples/arduino/compile-examples.sh` from the top-level nanoMODBUS directory to compile the examples.
They will be available in the `build` folder.

# Arduino-GBx-Reader-Writer

Read or write Game Boy save game to/from PC.



Table of contents
-----------------

- [Introduction](#introduction)
- [Pinout](#pinout)
- [How to](#how-to)
- [Compatibility](#compatibility)
- [Example](#example)
	- [Setup](#setup)
	- [Use](#use)
- [TODO](#todo)
- [License](#license)



Introduction
------------

While searching for a way to read and write save games from my game boy cartridges (because you know...if the battery die you say goodbye to save data) I didn't come across any cheap solution.
After some search I stumbled on ATmega1284p from way back when I started using Arduino and learning about AVR. I did remember this ATmega1284p (already on breadboard!!!) had maniacbug code to work with Arduino! Lucky me!!
Then I started reading about cartridges and how to communicate with them, destroyed an Nintendo DS to get the cartridge connector, removed the protection to be compatible with DMG and CGB cartridges and it was time to code!
This works with ATmega1284p (Mighty 1284P from [maniacbug](https://github.com/maniacbug/mighty-1284p), but [MightyCore](https://github.com/MCUdude/MightyCore) should also be compatible!
WARNING: you need RS232 to USB converter, like the TDIFT232R, and use `PD0` and `PD1` pins.

Helpful websites:
[The Cartridge Header](https://gbdev.gg8.se/wiki/articles/The_Cartridge_Header)
[Memory Bank Controllers](https://gbdev.gg8.se/wiki/articles/Memory_Bank_Controllers#Multicart_MBCs)
[GBCartRead – Gameboy Cart Reader](https://www.insidegadgets.com/projects/gbcartread-gameboy-cart-reader)



Pinout
------------

| ATmega1284p | Cartridge     |
| ---         | ---           |
| PD4         | WritePin      |
| PD5         | ReadPin       |
| PD6         | ChipSelectPin |
| PC0         | A0            |
| PC1         | A1            |
| PC2         | A2            |
| PC3         | A3            |
| PC4         | A4            |
| PC5         | A5            |
| PC6         | A6            |
| PC7         | A7            |
| PA0         | A8            |
| PA1         | A9            |
| PA2         | A10           |
| PA3         | A11           |
| PA4         | A12           |
| PA5         | A13           |
| PA6         | A14           |
| PA7         | A15           |
| PB0         | D0            |
| PB0         | D1            |
| PB0         | D2            |
| PB0         | D3            |
| PB0         | D4            |
| PB0         | D5            |
| PB0         | D6            |
| PB0         | D7            |



How to
------------

1. Follow the pinout to connect the cartridge connector to ATmega1284p (don't forget the RS2323 to USB converter)
2. Connect the Arduino to your PC and upload the sketch.
3. Compile the C program with a simple `make`
4. Run the program and supply the __MINIMAL__ arguments:
    * The port name of your Arduino e.g. `-t /dev/ttyACM0`



Compatibility
----------------------
I've developed on macOS Catalina version 0.15.7 (19H2), but it should work with any POSIX compatible OS.
Porting to Windows are still ongoing.

### TTY device names
- In Mac the ports are: `/dev/cu.usbmodem14931` or `/dev/cu.usbserial-A600JW4P`
- In Linux are: `/dev/ttyACM*`, where * should be 0. (you need to be root, or be in [group-mode](http://playground.arduino.cc/Linux/All#Permission))



Example
------------


### Setup
```
$ git clone git@github.com:DMRodrigues/Arduino-GBx-Reader-Writer.git
$ cd Arduino-GBx-Reader-Writer
$ make
```

### Use
`./gbx-reader-writer -p /dev/cu.usbserial-A600JW4P`
Then interact with the shell by pressing any number from 0 - 4.



TODO
------------
- Keep porting the C code to Windows

- RAM write function

- Test Game Boy Advance games



License
------------

The MIT License (MIT)

Copyright (c) 2020 Diogo Miguel Rodrigues

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

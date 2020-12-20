# Arduino-GBx-Reader-Writer

Read or Write your Game Boy games save data with an Arduino and PC.



Table of contents
-----------------

- [Introduction](#introduction)
	- [Notes](#notes)
- [About Game Boy Cartridges](#about-game-boy-cartridges)
- [Pinout](#pinout)
- [Compatibility](#compatibility)
- [Setup](#setup)
	- [Arduino](#arduino)
	- [Windows](#windows)
	- [macOS & Linux](#macos-linux)
- [How To](#how-to)
	- [USB devices names](#usb-devices-name)
	- [Windows](#windows)
	- [macOS & Linux](#macos-linux)
- [Examples](#examples)
	- [Windows](#windows)
	- [macOS & Linux](#macos-linux)
- [TODO](#todo)
- [License](#license)



Introduction
------------

While searching for a way to read save games from my game boy cartridges (because you know...if the battery dies you say goodbye to save data) I didn't come across any cheap solution.

After some search I stumbled on ATmega1284p from way back when I started using Arduino and learning about AVR. I did remember this ATmega1284p (already on breadboard!!!) had [maniacbug](https://github.com/maniacbug/mighty-1284p) code to work with Arduino! Lucky me!!

Then I started reading about cartridges and how to communicate with them, destroyed an Nintendo DS to get the cartridge connector, removed the protection to be compatible with GB and GBC cartridges and it was time to code!


#### Notes

You will need an RS232 to USB converter (like the FT232R) to connect the ATmega1284p to PC.

This works with ATmega1284p (Mighty 1284P from [maniacbug](https://github.com/maniacbug/mighty-1284p)) and [MightyCore](https://github.com/MCUdude/MightyCore). Use the default configurations with 16Mhz external crystal.



About Game Boy Cartridges
------------

- [The Cartridge Header](https://gbdev.gg8.se/wiki/articles/The_Cartridge_Header)
- [Memory Bank Controllers](https://gbdev.gg8.se/wiki/articles/Memory_Bank_Controllers#Multicart_MBCs)
- [GBCartRead – Gameboy Cart Reader](https://www.insidegadgets.com/projects/gbcartread-gameboy-cart-reader)



Pinout
------------

| ATmega1284p | Cartridge     |
| ----------- | ------------- |
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
| PB1         | D1            |
| PB2         | D2            |
| PB3         | D3            |
| PB4         | D4            |
| PB5         | D5            |
| PB6         | D6            |
| PB7         | D7            |



Compatibility
----------------------

I've developed and tested on macOS Catalina version 0.15.7 (19H2) and also tested on Debian x64 machine with GCC 8. The code should be compatible with any POSIX OS.

On Windows I recommend installing Visual Studio to compile the code or use the provided x86 executable.



Setup
------------

### Arduino
1. Follow the pinout to connect the cartridge connector to ATmega1284p
2. Connect any compatible RS232 to USB converter to pins `PD0` and `PD1`

### Windows
1. Upper right corner: __Code__ -> __Download ZIP__

### macOS & Linux

```
$ git clone git@github.com:DMRodrigues/Arduino-GBx-Reader-Writer.git
$ cd Arduino-GBx-Reader-Writer
```



How To
------------

### USB devices names
- macOS: `/dev/cu.usbmodem*` or `/dev/cu.usbserial-*`
- Linux: `/dev/ttyUSB*` or `/dev/ttyACM*` (you need to be root or in [group-mode](http://playground.arduino.cc/Linux/All#Permission))
- Windows: `COM*`

### Windows
1. Connect the Arduino to your PC and upload the sketch
2. Navigate to executables folder and open the command line
3. Execute the `GBx-Reader-Writer.exe` and provide the USB port
    * USB port e.g. `COM9`
4. Interact with the shell by choosing 0 - 4.

### macOS & Linux
1. Connect the Arduino to your PC and upload the sketch
2. Open the command line
3. Compile the C program with `make`
4. Execute `gbx-reader-writer` and provide the USB port
    * USB port e.g. `-p /dev/ttyUSB0`
5. Interact with the shell by choosing 0 - 4.



Examples
------------

### Windows
```
GBx-Reader-Writer.exe COM9
```

### macOS & Linux
```
./gbx-reader-writer -p /dev/ttyUSB0
```



TODO
------------

- Game Boy Advance games (WARNING: 5v from Arduino will damage the GBA cartridges that works with 3.3v)



License
------------

MIT License

Copyright (c) 2020 Diogo Rodrigues

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

dsPIC30F bootloader and PC software for updating firmware by serial port
========================================================================

Overview
--------

Suppose you have a device (DEVICE) with a dsPIC30F microprocessor
(MICROPROCESSOR) inside. You have software (TARGET FIRMWARE) for this
microprocessor.

Then you develop target firmware you program the firmware into the device
by a programmer (PROGRAMMER), for example, ICD or PICKit.

However, sometimes you need to update target firmware in the device after
the device has been made. It may be required, for example, if the updating
shall be done at an end user or the device construction does not allow to
connect a programmer.

One way to do it without a programmer is using a serial connection between
the microprocessor and a personal computer (HOST PC).

In this case, there is an additional part of the microprocessor firmware
named bootloader firmware (BOOTLOADER FIRMWARE). This part receives
commands from the serial port and executes them. The commands are sent by
the host PC.

To send the command form the HOST PC there is a special software named
loader (LOADER). It is an application for the PC that loads the target
firmware file (in Intel Hex Format) and sends it to the serial port.

The project contains two parts: the bootloader firmware and the loader.


Project features
----------------

- download/update the target firmware in the microprocessor by serial port
- download firmware from the microprocessor to the host PC
- does not protect your target firmware from downloading
- verify the target firmware in the microprocessor
- erase the target firmware in the microprocessor (without the bootloader
  firmware)
- cannot change the FUSE bits in the microprocessor
- no special preparation required for the target firmware
- status LED can be controlled by bootloader firmware
- Microchip XC16 compiler is used to build the target firmware
- the HOST PC shall have OS Window XP or later
- Microsoft Visual Studio is used to build the loader

Supporting microprocessors:
- dsPIC30F2010, dsPIC30F2011, dsPIC30F2012
- dsPIC30F3010, dsPIC30F3011, dsPIC30F3012, dsPIC30F3013, dsPIC30F3014
- dsPIC30F4011, dsPIC30F4012, dsPIC30F4013
- dsPIC30F5011, dsPIC30F5013, dsPIC30F5015, dsPIC30F5016
- dsPIC30F6010, dsPIC30F6011, dsPIC30F6012, dsPIC30F6013, dsPIC30F6014,
  dsPIC30F6015
- dsPIC30F6010A, dsPIC30F6011A, dsPIC30F6012A, dsPIC30F6013A,
  dsPIC30F6014A

Not supporting microprocessors:
- dsPIC30F1010
- dsPIC30F2020, dsPIC30F2023


Additional documentation
------------------------

See in the 'Documents' folder:
- Bootloader Memory Maps
- Build Bootloader
- Command Line
- Serial Protocol


Quick step-by-step tutorial
---------------------------

Create the bootloader firmware for your project. the bootloader firmware
code is in the 'Bootloader-firmware' folder.

First, create a config file for your project. Copy the 'config-example.h'
file to your config file, for example, 'config-my-project.h'. Correct the
file for adopting to your project.
   
Change the FUSE bits. They shall be identical to your target firmware
FUSEs. The bootloader cannot change the FUSE bits at all.

Set the actual microprocessor frequency (FCY). It very important because
the frequency is used for serial port speed calculation and for some
delays.

Select the UART number and TX and RX pins.

Say the bootloader about using the watchdog timer in your FUSE bits.

You can optionally define the LED pin. If it is defined the bootloader will
display its status by this LED.

Define an initial delay. During this delay, the bootloader waits for
packets from the serial port. If there are no packets the bootloader
starts the target firmware.

Build your bootloader firmware, for example:

	make CPU_MODEL=30P6012A CONFIG_FILE=config-my-project.has

Where:
	CPU_MODEL - microprocessor model in your project
	CONFIG_FILE - your config file
	
More in 'Build Bootloader.txt' in the 'Documents' folder.

Now you have the the 'bootloader.hex' file. Download it to your device.

Connect your device to the PC.

Turn on your device. If your use LED for displaying the bootloader status,
you can see the fast blinking.

Download your target firmware to the device by the loader (for example):

	loader.exe -p COM3 your-target-firmware.hex

More in 'Command Line.txt' in the 'Documents' folder.


License
-------

The project is licensed under the GNU General Public License version 2 or
any later version.


Project rules
-------------

I tried to create the code as simple as possible. So, the bootloader
firmware was written in C language with a few inline assembler statements.
I did do any code size optimization which can be made the code more
difficult. Also, the bootloader firmware uses standard C library by xc16
compiler.


Roadmap
-------

- microprocessors dsPIC30F1010, dsPIC30F2020, dsPIC30F2023
- more complicated board (now only one LED connected to GPIO is supported)
- option for switching to FRC when the bootloader is executing and switch
  back to the primary oscillator before starting the target firmware

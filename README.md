# Description

A fourth stage bootloader for the Raspberry Pi 4 Model B 2GB. 


# Bootloader Stages

The Raspberry Pi 4 boot sequence is as follows, chaining together three 
bootloader stages.

1. The first stage bootloader is stored in ROM. It loads the secondary
stage bootloader from SPI EEPROM. This step exists to recover a faulty
second stage bootloader/program.
2. The second stage bootloader is stored in SPI EEPROM. It initialises clocks
and SDRAM and then loads the third stage bootloader `/boot/start4.elf` from 
secondary storage.
3. The third stage bootloader is firmware blob `/boot/start4.elf`, stored on 
secondary storage. It loads the Linux kernel `/boot/kernel{7l,8}.img` and 
device tree from secondary storage into RAM, and switches execution to the 
kernel.

This bootloader program is implemented as a (redundant) fourth stage bootloader,
renamed to `/boot/kernel7l.img` so it is loaded by the third stage bootloader instead
of the kernel. Although this is implemented as a redundant step in the boot sequence,
it is done because the first three bootloader stages are run on the VideoCore/VPU, 
while the loaded "kernel" is run on the ARM CPU - and there is far more official 
documentation available for the ARM CPU than the VPU.


# Usage

## Imager?

TODO

## #define partition?

TODO

## error signalling?

TODO


# Resources

Technical documentation and other resources referenced when writing the source,
which will help in understanding the source.

**General**
* [BCM2711 datasheet](https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf): 
SoC used in the Raspberry Pi 4 Model B
* [BCM2711 device tree source](https://github.com/raspberrypi/firmware): used to find some 
memory-mapped register base addresses not available in the BCM2711 datasheet, clock rates, 
etc. To get the source download the linked repo and convert the Raspberry Pi 4 Model B 
device tree blob to its source with device tree compiler command `dtc -I dtb -O dts boot/bcm2711-rpi-4-b.dtb`. 
Note the addresses in the source are legacy master addresses (refer BCM2711 datasheet for more info).

**ARM CPU**
* [ARM Cortex-A Series Programmer's Guide for ARMv7-A](https://developer.arm.com/documentation/den0013/d):
the 32-bit architecture that the bootloader is compiled to
* [ARM Developer Suite Assembler Guide](https://developer.arm.com/documentation/dui0068/b): ARM assembly
instruction reference
* [ARM Cortex-A72 MPCore Processor Technical Reference Manual](https://developer.arm.com/documentation/100095/0003):
the ARM processor running the bootloader. Contains helpful information on system registers.
* [ARM Developer Suite Developer Guide](https://developer.arm.com/documentation/dui0056/d):
has extra info on interrupt handlers

**VideoCore Mailboxes**
Official documentation on the VideoCore mailboxes is scarce. Although not all "official" the following 
were still helpful (and just to reference concepts, not code).
* https://github.com/raspberrypi/firmware/wiki/Mailboxes
* https://jsandler18.github.io/extra/mailbox.html
* https://jsandler18.github.io/extra/prop-channel.html
* https://github.com/raspberrypi/linux/blob/rpi-6.1.y/include/soc/bcm2835/raspberrypi-firmware.h: 
lists tags not documented in the wiki
* https://github.com/raspberrypi/firmware/blob/master/extra/dt-blob.dts#L2456-L2462:
VC device tree GPIO expander pins

**SD**
* [BCM2835 Datasheet](https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf):
SoC used by earlier Raspberry Pis such as the Raspberry Pi 1, but provides info on SD/MMC registers
not available in the BCM2711 datasheet
* [SD Host Controller Specification Version 3.00](https://www.sdcard.org/downloads/pls/archives/): 
the host controller specification which the SD peripheral implements
* [SD Physical Layer Specification Version 3.01](https://www.sdcard.org/downloads/pls/archives/):
the physical layer specification which the SD peripheral implements


TODO
- put in links/references for Bootloader Stages section?
- as find and use  more resources add them to resources section
- need a config.txt (add a sample somewhere)
- mention only works with sd not usb
- add boot/ dir to git tracking?

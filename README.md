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
while the loaded "kernel" is run on the ARM CPU, and there is far more official 
documentation available for the ARM CPU than the VPU.

For more info see the following links.
* https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#raspberry-pi-4-boot-flow
* https://www.raspberrypi.com/documentation/computers/configuration.html#start-elf-start_x-elf-start_db-elf-start_cd-elf-start4-elf-start4x-elf-start4db-elf-start4cd-elf
* https://www.raspberrypi.com/documentation/computers/configuration.html#kernel-files


# Usage

## Imager

The imager is used to image a partition. The imaged partition is called the "image partition"
here, and is where the bootloader looks for the OS (and its data) to load. The image partition
can be created with `fdisk`, and shall be a MBR primary partition large enough to store the OS
files. The image partition does not need to be formatted with a file system, as the image is 
stored as raw bytes at the start of the partition.

Compile the imager with `make imager`. Run it with help arguments `-h` or `--help` to
see how to use it to image a partition.

TODO mention where to fin default files to make image out of. also mention default provided
dtb assumes root file system is on 2nd partition in its boot args. mention which raspbian
pulled them from 

## Bootloader

To let the bootloader know which partition to look for the image on, before compiling, in 
the `Makefile` set the `image_partition` variable to the partition number of the image 
partition (e.g. 2).

Cross compile the bootloader with `make bootloader`.
TODO 
- minimum set of files needed on the /boot part of the SD card. 
- install by moving the bootloader to kernel7l.img on the SD card's boot partition
- explain don't need to cross compile if doing this on the rpi itself. 
	explain which makefile variable to set to cross compile

## error signalling?

TODO


# Resources

Technical documentation and other resources referenced when writing the source,
which will help in understanding the source.

**General**
* [BCM2711 datasheet](https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf): 
SoC used in the Raspberry Pi 4 Model B
* [BCM2711 device tree source](https://github.com/raspberrypi/firmware): used to find some peripheral
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



TODO ADD THIS TO THE ARM CPU SECTION: AAPCS arm procedure call standard https://github.com/ARM-software/abi-aa/blob/844a79fd4c77252a11342709e3b27b2c9f590cf1/aapcs32/aapcs32.rst - helpful to know which registers are safe to use when writing assembly that will be mixed with compiled C

TODO
- as find and use more resources add them to resources section
- mention minimum /boot partition files like config.txt needed 
	(see uncommitted boot-part dir; provide committed default files)
- mention only works with sd not usb
- add "booting ARM linux" instructions to resources? (after find it's needed)
https://www.kernel.org/doc/html/latest/arm/booting.html
http://www.simtec.co.uk/products/SWLINUX/files/booting_article.html#d0e309
- add troubleshooting section on selecting a cmdline?

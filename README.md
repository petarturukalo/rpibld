# Description

A fourth stage bootloader for the Raspberry Pi 4 Model B, for booting 32-bit 
ARM Linux from a SD card.


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

This bootloader program is implemented as a (redundant) fourth stage bootloader, installed
to `/boot/bootloader` and property `kernel=bootloader` set in `/boot/config.txt` for the 
third stage bootloader to load it instead of the kernel. It could have just been installed 
to `/boot/kernel7l.img` (`kernel7l.img` and not `kernel8.img` because this bootloader is 32-bit,
not 64-bit) but renaming it to `bootloader` makes for less confusion. Although this is implemented 
as a redundant step in the boot sequence, it is done because the first three bootloader stages are 
run on the VideoCore/VPU, while the loaded "kernel" is run on the ARM CPU, and there is far more 
official documentation available for the ARM CPU than the VPU.

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

TODO mention where to find default files to make image out of (TODO add image files section?). 
also mention default provided dtb assumes root file system is on 2nd partition in its boot args. 
mention which raspbian pulled them from and tested to work with

## Bootloader

To let the bootloader know which partition to look for the image on, before compiling, in 
the `Makefile` set the `image_partition` variable to the partition number of the image 
partition (e.g. 3).

Cross compile the bootloader with `make bootloader`.
TODO 
- minimum set of files needed on the /boot part of the SD card. 
- install by moving the bootloader to /boot/bootloader on the SD card's boot partition
- explain don't need to cross compile if doing this on the rpi itself. 
	explain which makefile variable to set to cross compile

## Troubleshooting

TODO troubleshooting bootloader: explain LED and mini UART debug serial (only transmit)
TODO explain kernel troubleshooting: earlycon, kconfig debugs (retest which are actually needed), etc.


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
* https://elinux.org/BCM2835_datasheet_errata#p12: fixes of errors in BCM2711 datasheet mini UART documentation

**ARM CPU**
* [ARM Cortex-A Series Programmer's Guide for ARMv7-A](https://developer.arm.com/documentation/den0013/d):
the 32-bit architecture that the bootloader is compiled to
* [ARM Developer Suite Assembler Guide](https://developer.arm.com/documentation/dui0068/b): ARM assembly
instruction reference
* [ARM Cortex-A72 MPCore Processor Technical Reference Manual](https://developer.arm.com/documentation/100095/0003):
the ARM processor running the bootloader. Contains helpful information on system registers.
* [ARM Developer Suite Developer Guide](https://developer.arm.com/documentation/dui0056/d):
has extra info on interrupt handlers
* [AAPCS arm procedure call standard](https://github.com/ARM-software/abi-aa/blob/844a79fd4c77252a11342709e3b27b2c9f590cf1/aapcs32/aapcs32.rst): 
helpful to know which registers are safe to use when writing assembly that will be mixed with compiled C

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

**Other**
* [Booting ARM Linux](https://www.kernel.org/doc/html/latest/arm/booting.html): set up required to boot ARM Linux
* [ARM stub](https://github.com/raspberrypi/tools/blob/master/armstubs/armstub7.S): code firmware places at address 
0x0 start of RAM, which is executed before my bootloader
* [CoreLink GIC-400 Generic Interrupt Controller Technical Reference Manual](https://developer.arm.com/documentation/ddi0471/latest/): 
useful in understanding the ARM stub GIC code
* [ARM Generic Interrupt Controller Architecture Specification](https://developer.arm.com/documentation/ihi0069/h/):
contains definitions of register fields not present in the GIC-400 Technical Reference Manual


TODO

PUT BELOW 2 REFS IN SECTION ON GETTING THE ZIMAGE AND DTB
* [Raspberry Pi Linux kernel](https://github.com/raspberrypi/linux)
* [Building Linux](https://www.raspberrypi.com/documentation/computers/linux_kernel.html#building-the-kernel)

- fix up ARM stub explanation
- as find and use more resources add them to resources section
- mention minimum /boot partition files like config.txt needed 
	(see uncommitted boot-part dir; provide committed default files)
- mention only works with sd not usb
- add troubleshooting section on selecting a cmdline?
- mention need to have device driver modules installed matching version of kernel using 
- device tree needs specific settings in order to boot (here's one i prepared earlier); see
doc/boot/dt.txt


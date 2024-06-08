# Description

A Raspberry Pi 4 Model B fourth stage bootloader, for booting 32-bit 
ARM Linux from an SD card.


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
- https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#raspberry-pi-4-and-raspberry-pi-5-boot-flow 
- https://www.raspberrypi.com/documentation/computers/configuration.html#start-elf
- https://www.raspberrypi.com/documentation/computers/configuration.html#kernel-files-img 


# Usage

## Imager

The imager is used to image a partition. The imaged partition is called the "image partition"
here, and is where the bootloader looks for the OS (and its data) to load. The image partition
can be created with `fdisk`, and shall be a MBR primary partition large enough to store the OS
files. The image partition does not need to be formatted with a file system, as the image is 
stored as raw bytes at the start of the partition.

Compile the imager with `make imager`. Run it with help arguments `-h` or `--help` to
see how to use it to image a partition.

## Image Files

The imager takes a 32-bit Linux kernel ARM zImage and device tree blob (DTB) files as parameters.
See [1] for building these files yourself.

Note the default built DTB is incomplete and can't be used to boot successfully. This is because 
it's expected this DTB is loaded by the Raspberry Pi firmware, which will modify it and complete its
missing/empty properties before executing the kernel. Since this bootloader is what loads the DTB 
instead of the firmware, these modifications are not done. To work around this, the modifications must 
be made to the device tree sources before compilation.

The following lists the modifications required of the device tree sources in order for Linux to boot 
successfully.

1. The booted kernel uses the command-line parameters specified in the `bootargs` property of the device tree
`/chosen` node, and ignores the parameters given in the kernel config `CONFIG_CMDLINE`. As is typical
of a kernel command line, it must specify the partition of the root filesystem, e.g. a minimal command line
specifying partition 2 as the root filesystem is
`8250.nr_uarts=1 console=ttyS0,115200 console=tty1 root=/dev/mmcblk0p2 rootfstype=ext4 fsck.repair=yes rootwait`.
See [2] for a complete list and explanation of the kernel command-line parameters.
2. The `reg` property of the `/memory` node is empty and must be filled. I use 
`reg = <0x00 0x00 0x3b400000 0x00 0x40000000 0x40000000>;`, which I got by looking at the device tree
of a running Linux with decompilation command `dtc -I fs /sys/firmware/devicetree/base`. Note
my Raspberry Pi is a 2 GB version: I have not tested this on a Pi with a different size of RAM, 
but it's very likely the value of this `reg` property will be different depending on the size of 
the Pi's RAM (2, 4, or 8 GB).

If you don't want to go through all of these steps, I have provided a ready DTB/kernel in `data/img` with the 
modifications listed above, i.e. DTB with kernel command line specifying root filesystem on partition 2 and
memory set to 2 GB. Note these were compiled from RPI Linux [3] version 6.1.35: you may run into issues if
there's a mismatch in version between kernel and modules, such as the Pi not running smoothly, so try also 
installing modules matching this same version if troubleshooting. See [4] for more info on RPI Linux versioning.

[1] https://www.raspberrypi.com/documentation/computers/linux_kernel.html#building  
[2] https://www.kernel.org/doc/html/latest/admin-guide/kernel-parameters.html  
[3] https://github.com/raspberrypi/linux  
[4] https://www.raspberrypi.com/documentation/computers/linux_kernel.html#identify-your-kernel-version

## Bootloader

To let the bootloader know which partition to look for the image on, before compiling, 
set the `image_partition` variable in the `Makefile` to the partition number of the image 
partition (e.g. 3).

Compile the bootloader with `make bootloader`. By default it is configured to cross compile using
`arm-none-eabi-gcc`, but this can be changed via the `cross_prefix` variable in the `Makefile`.

Install the bootloader on your SD card by first mounting its `/boot` partition on `mnt-boot`,
and then running `make install`. WARNING this will also install the minimum set of boot files 
required for the bootloader to run and run successfully, overwriting existing files, e.g. the 
third stage bootloader / firmware, `start4.elf`, and the `config.txt`. Explanations of the 
contents of `config.txt` are littered throughout the source.

## Troubleshooting

If the bootloader hits an error it will stop and continuously signal an error code by flashing 
the LED a certain number of times (see `bld/error.h` for more info and the error codes that are 
signalled).

More helpful, however, is the bootloader also logs its progress and any errors encountered over 
the mini UART transmit line. See `bld/uart.h:uart_init()` for info on the serial parameters 
that the receiver is required to match.

If an error occurs after the bootloader jumps to the kernel (i.e. after the bootloader logs that 
it is jumping to the kernel) then you may see output logged to the consoles, monitor and/or serial.
If a serial console is used then it shall use the same mini UART as the bootloader, and so must be 
configured with the same transmitter parameters as in the bootloader. This can be achieved with 
kernel command-line parameters `console=ttyS0,115200` and `8250.nr_uarts=1`.

If the error occurs before the consoles are initialised then you may not see any output.
A work around for this is to use the `earlycon` kernel command-line param for early serial output, e.g. 
`ignore_loglevel keep_bootcon earlycon=uart8250,mmio32,0xfe215040`. Ensure kernel config 
`CONFIG_SERIAL_EARLYCON=y` is also set.

Again, if the error occurs before the early serial console is initialised, e.g. if the kernel wasn't
given a DTB, then you may still not see any output. To work around this, look into using the
following kernel configs.

	CONFIG_DEBUG_LL=y
	CONFIG_DEBUG_LL_UART_8250=y
	CONFIG_DEBUG_UART_PHYS=0xfe215040
	CONFIG_DEBUG_UART_VIRT=0xf0215040
	CONFIG_DEBUG_LL_UART_8250_WORD=y
	CONFIG_DEBUG_UNCOMPRESS=y


# Resources

Technical documentation and other resources referenced when writing the source,
which will help in understanding the source.

**General**
- [BCM2711 datasheet](https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf): 
SoC used in the Raspberry Pi 4 Model B
- [BCM2711 device tree source](https://github.com/raspberrypi/firmware): used to find some peripheral
memory-mapped register base addresses not available in the BCM2711 datasheet, clock rates, 
etc. To get the source download the linked repo and convert the Raspberry Pi 4 Model B 
device tree blob to its source with device tree compiler command `dtc -I dtb -O dts boot/bcm2711-rpi-4-b.dtb`. 
Note the addresses in the source are legacy master addresses (refer BCM2711 datasheet for more info).
- https://elinux.org/BCM2835_datasheet_errata#p12: fixes of errors in BCM2711 datasheet mini UART documentation

**ARM CPU**
- [ARM Cortex-A Series Programmer's Guide for ARMv7-A](https://developer.arm.com/documentation/den0013/d):
the 32-bit architecture that the bootloader is compiled to
- [ARM Developer Suite Assembler Guide](https://developer.arm.com/documentation/dui0068/b): ARM assembly
instruction reference
- [ARM Cortex-A72 MPCore Processor Technical Reference Manual](https://developer.arm.com/documentation/100095/0003):
the ARM processor running the bootloader. Contains helpful information on system registers.
- [ARM Developer Suite Developer Guide](https://developer.arm.com/documentation/dui0056/d):
has extra info on interrupt handlers
- [AAPCS arm procedure call standard](https://github.com/ARM-software/abi-aa/blob/844a79fd4c77252a11342709e3b27b2c9f590cf1/aapcs32/aapcs32.rst): 
helpful to know which registers are safe to use when writing assembly that will be mixed with compiled C

**VideoCore Mailboxes**  
Official documentation on the VideoCore mailboxes is scarce. Although not all "official" the following 
were still helpful (and just to reference concepts, not code).
- https://github.com/raspberrypi/firmware/wiki/Mailboxes
- https://jsandler18.github.io/extra/mailbox.html
- https://jsandler18.github.io/extra/prop-channel.html
- https://github.com/raspberrypi/linux/blob/rpi-6.1.y/include/soc/bcm2835/raspberrypi-firmware.h: 
lists tags not documented in the wiki
- https://github.com/raspberrypi/firmware/blob/master/extra/dt-blob.dts#L2456-L2462:
VC device tree GPIO expander pins

**SD**
- [BCM2835 Datasheet](https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf):
SoC used by earlier Raspberry Pis such as the Raspberry Pi 1, but provides info on SD/MMC registers
not available in the BCM2711 datasheet
- [SD Host Controller Specification Version 3.00](https://www.sdcard.org/downloads/pls/archives/): 
the host controller specification which the SD peripheral implements
- [SD Physical Layer Specification Version 3.01](https://www.sdcard.org/downloads/pls/archives/):
the physical layer specification which the SD peripheral implements

**Other**
- [Booting ARM Linux](https://docs.kernel.org/6.2/arm/booting.html): set up required to boot ARM Linux
- [ARM stub](https://github.com/raspberrypi/tools/blob/master/armstubs/armstub7.S): code firmware places at address 
0x0 start of RAM, which is executed before this bootloader
- [CoreLink GIC-400 Generic Interrupt Controller Technical Reference Manual](https://developer.arm.com/documentation/ddi0471/latest/): 
useful in understanding the ARM stub GIC code
- [ARM Generic Interrupt Controller Architecture Specification](https://developer.arm.com/documentation/ihi0069/h/):
contains definitions of register fields not present in the GIC-400 Technical Reference Manual


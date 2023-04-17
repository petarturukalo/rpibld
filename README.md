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
while the loaded "kernel" is run on the ARM CPU — and there is far more official 
documentation available for the ARM CPU than the VPU.


# Usage

## Imager?

TODO

## #define partition?

TODO

## error signalling?

TODO


TODO
- put in links/references for Bootloader Stages section?
- add resources section for technical documentation

srcs=$(shell find bld -name '*.[cS]' -print)
objs=$(patsubst %.c, %.o, $(srcs))
objs:=$(patsubst %.S, %.o, $(objs))
# Cross compilation prefix.
cross_prefix=arm-none-eabi-
linker_script=boot.ld
# The MBR primary partition that the imager imaged and that the
# bootloader will load the OS from.
image_partition=3
CFLAGS=-nostdlib -r -march=armv7ve -Wunused -Werror=undef -Iinclude \
	-DIMAGE_PARTITION=$(image_partition)
LDFLAGS=-T $(linker_script) -static 


bootloader: bld/bootloader.elf
	$(cross_prefix)objcopy -O binary $< $@

bld/bootloader.elf: $(objs) $(linker_script)
	$(cross_prefix)ld $(LDFLAGS) $(objs) -o $@

bld/%.o: bld/%.[cS]
	$(cross_prefix)gcc $(CFLAGS) $< -o $@ 


imager: img/img.c include/img.h
	$(cross-prefix)gcc -Iinclude $< -o $@


clean:
	find bld -name '*.o' -print -delete
	rm bld/bootloader.elf bootloader imager

install:
	sudo cp -fv data/boot/* mnt-boot
	sudo cp -fv bootloader mnt-boot


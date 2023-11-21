srcs=$(shell find bld -name '*.[cS]' -print)
objs=$(patsubst %.c, %.o, $(srcs))
objs:=$(patsubst %.S, %.o, $(objs))
deps=$(patsubst %.o, %.d, $(objs))
# Cross compilation prefix.
cross_prefix=
linker_script=boot.ld
# The MBR primary partition that the imager imaged and that the
# bootloader will load the OS from.
image_partition=
CFLAGS=-nostdlib -r -march=armv7ve -Wunused -Iinclude
ifdef image_partition
CFLAGS+=-DIMAGE_PARTITION=$(image_partition)
endif
LDFLAGS=-T $(linker_script) -static 

bootloader: bld/bootloader.elf
	$(cross_prefix)objcopy -O binary $< $@

-include $(deps)

bld/bootloader.elf: $(objs) $(linker_script)
	$(cross_prefix)ld $(LDFLAGS) $(objs) -o $@

bld/%.o: bld/%.[cS]
	$(cross_prefix)gcc $(CFLAGS) -MMD $< -o $@ 


imager: img/img.c include/img.h
	gcc -Iinclude $< -o $@


clean:
	find bld -name '*.[od]' -print -delete
	rm bld/bootloader.elf bootloader imager

install:
	sudo cp -fv data/boot/* mnt-boot
	sudo cp -fv bootloader mnt-boot


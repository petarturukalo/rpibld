# Copyright (C) 2023 Petar Turukalo
# SPDX-License-Identifier: GPL-2.0

srcs = $(shell find bld -name '*.[cS]' -print)
objs = $(patsubst %.c,%.o,$(srcs))
objs := $(patsubst %.S,%.o,$(objs))
deps = $(patsubst %.o,%.d,$(objs))
# Cross compilation prefix.
cross_prefix = arm-none-eabi-
linker_script = boot.ld
# The MBR primary partition that the imager imaged and that the
# bootloader will load the OS from.
image_partition = 
CFLAGS = -c -march=armv7ve -Wunused -iquote include -ffreestanding
ifdef image_partition
CFLAGS += -DIMAGE_PARTITION=$(image_partition)
endif
LDFLAGS = -T $(linker_script) -nostdlib

bootloader: bld/bootloader.elf
	$(cross_prefix)objcopy -O binary $< $@

-include $(deps)

bld/bootloader.elf: $(objs) $(linker_script)
	$(cross_prefix)ld $(LDFLAGS) $(objs) -o $@

bld/%.o: bld/%.[cS]
	$(cross_prefix)gcc $(CFLAGS) -MMD $< -o $@ 


imager: img/img.c include/img.h
	gcc -iquote include $< -o $@


clean:
	find bld -name '*.[od]' -print -delete
	rm bld/bootloader.elf bootloader imager

install:
	sudo cp -fv data/boot/* mnt-boot
	sudo cp -fv bootloader mnt-boot


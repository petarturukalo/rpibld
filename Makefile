srcs=$(shell find bld -name '*.[cS]' -print)
objs=$(patsubst %.c, %.o, $(srcs))
objs:=$(patsubst %.S, %.o, $(objs))
# Cross compilation prefix.
# TODO test using regular gcc on rpi to compile this
# TODO explain if changing cross_prefix might need to change libgcc_searchdir also
cross_prefix=arm-none-eabi-
linker_script=boot.ld
libgcc_searchdir=/usr/lib/gcc/arm-none-eabi/12.2.0
# The MBR primary partition that the imager imaged and that the
# bootloader will load the OS from.
image_partition=3
# TODO -Werror=undef doing anything?
# TODO arch was armv7-a (why did i make it -a and not armv7? it's armv7-a in the web docs i'm referencing)
# TODO need -c compile flag? prob coz -r?
CFLAGS=-nostdlib -r -march=armv7ve -Wunused-variable -Werror=undef -Iinclude \
	-DIMAGE_PARTITION=$(image_partition)
       
# Link with -lgcc to resolve undefined reference to __aeabi_idivmod, 
# needed to use the C modulo % operator. TODO confirm still needed
LDFLAGS=-T $(linker_script) -static -L $(libgcc_searchdir) --no-warn-rwx-segments
LDLIBS=-lgcc


bootloader: bld/bootloader.elf
	$(cross_prefix)objcopy -O binary $< $@

bld/bootloader.elf: $(objs) $(linker_script)
	$(cross_prefix)ld $(LDFLAGS) $(objs) $(LDLIBS) -o $@

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


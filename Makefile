# TODO use find instead?
objs=$(patsubst bld/%.S, build/%.o, $(wildcard bld/*.S))
objs+=$(patsubst bld/%.c, build/%.o, $(wildcard bld/*.c))
objs+=$(patsubst bld/sd/%.c, build/sd/%.o, $(wildcard bld/sd/*.c))
# Cross compilation prefix.
# TODO include trailing '-' so can use regular gcc on rpi
# by setting this to blank?
# TODO explain if changing cross_prefix might need to change libgcc_searchdir also
cross_prefix=arm-none-eabi
linker_script=boot.ld
libgcc_searchdir=/usr/lib/gcc/arm-none-eabi/12.2.0
# The MBR primary partition that the imager imaged and that the
# bootloader will load the OS from.
image_partition=3
# TODO -Werror=undef doing anything?
# TODO arch was armv7-a (why did i make it -a and not armv7? it's armv7-a in the web docs i'm referencing)
CFLAGS=-nostdlib -r -march=armv7ve -Wunused-variable -Werror=undef -Iinclude \
	-DIMAGE_PARTITION=$(image_partition)
       
# Link with -lgcc to resolve undefined reference to __aeabi_idivmod, 
# needed to use the C modulo % operator. 
LDFLAGS=-T $(linker_script) -static -L $(libgcc_searchdir)
LDLIBS=-lgcc


bootloader: build/bootloader.elf
	$(cross_prefix)-objcopy -O binary $< $@

build/bootloader.elf: $(objs) $(linker_script)
	$(cross_prefix)-ld $(LDFLAGS) $(objs) $(LDLIBS) -o $@

build/%.o: bld/%.S
	$(cross_prefix)-as $< -o $@

build/%.o: bld/%.c
	$(cross_prefix)-gcc $(CFLAGS) $< -o $@


imager: img/img.c 
	$(cross-prefix)-gcc -Iinclude $< -o $@


clean:
	find build -name '*.o' -print -delete
	rm build/bootloader.elf bootloader imager

# TODO rm install and uninstall - just used to automate testing
install:
	sudo cp -v bootloader mnt-boot/kernel7l.img

uninstall:
	sudo rm -v mnt-boot/kernel7l.img


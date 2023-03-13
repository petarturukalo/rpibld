objs=$(patsubst src/%.S, build/%.o, $(wildcard src/*.S))
objs+=$(patsubst src/%.c, build/%.o, $(wildcard src/*.c))
# Cross compilation prefix.
# TODO include trailing '-' so can use regular gcc on rpi
# by setting this to blank?
cross_prefix=arm-none-eabi
linker_script=boot.ld
CFLAGS=-nostdlib -r -march=armv7-a
LDFLAGS=-T $(linker_script)

bootloader: build/bootloader.elf
	$(cross_prefix)-objcopy -O binary $< $@

build/bootloader.elf: $(objs) $(linker_script)
	$(cross_prefix)-ld $(LDFLAGS) $(objs) -o $@

build/%.o: src/%.S
	$(cross_prefix)-as $< -o $@

build/%.o: src/%.c
	$(cross_prefix)-gcc $(CFLAGS) $< -o $@

clean:
	rm build/* bootloader

# TODO rm install and uninstall - just used to automate testing
install:
	sudo cp -v bootloader mnt-boot/kernel7l.img

uninstall:
	sudo rm -v mnt-boot/kernel7l.img


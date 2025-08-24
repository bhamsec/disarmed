ARCH=aarch64
GNU_EFI_DIR=gnu-efi
MKGPT_DIR=./mkgpt
MKGPT=./mkgpt/mkgpt

TARGET=$(ARCH)-unknown-windows
CC=clang
CFLAGS=-target $(TARGET) -I$(GNU_EFI_DIR)/inc -I$(GNU_EFI_DIR)/inc/$(ARCH) -I$(GNU_EFI_DIR)/inc/protocol -ffreestanding -fshort-wchar -mno-red-zone -g
LD=clang
LDFLAGS=-target $(TARGET) -nostdlib -Wl,-entry:efi_main -Wl,-subsystem:efi_application -fuse-ld=lld-link

QEMU_BIOS_PATH=/usr/share/edk2/$(ARCH)/QEMU_CODE.fd
QEMUFLAGS=-nographic -cpu neoverse-n1 -machine virt -smp 1 -drive if=pflash,format=raw,readonly=on,file=$(QEMU_BIOS_PATH) -hda hdimage.bin 

all: fat.img

$(MKGPT): $(MKGPT_DIR)
	cd $(MKGPT_DIR); automake --add-missing; autoreconf; ./configure
	make -C $(MKGPT_DIR)

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

data.o: $(GNU_EFI_DIR)/lib/data.c
	$(CC) $(CFLAGS) -c $(GNU_EFI_DIR)/lib/data.c -o data.o

BOOTX64.EFI: main.o data.o
	$(LD) $(LDFLAGS) -o BOOTX64.EFI main.o data.o

fat.img: BOOTX64.EFI
	dd if=/dev/zero of=fat.img bs=1k count=1440
	mformat -i fat.img -f 1440 ::
	mmd -i fat.img ::/EFI
	mmd -i fat.img ::/EFI/BOOT
	mcopy -i fat.img BOOTX64.EFI ::/EFI/BOOT

hdimage.bin: fat.img $(MKGPT)
	$(MKGPT) -o hdimage.bin --image-size 4096 --part fat.img --type system

run: hdimage.bin
	qemu-system-$(ARCH) $(QEMUFLAGS)

debug: hdimage.bin
	qemu-system-$(ARCH) $(QEMUFLAGS) -s -S

deploy: BOOTX64.EFI
	scp BOOTX64.EFI essen-arp:srv

clean:
	make -C $(GNU_EFI_DIR) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) clean || true
	rm -rf $(GNU_EFI_DIR)/$(ARCH) || true
	make -C $(MKGPT_DIR) clean || true
	make -C asm clean || true
	rm -f main.o data.o BOOTX64.EFI fat.img hdimage.bin compile_flags.txt || true

compile_flags.txt:
	echo "$(CFLAGS)" | sed -e 's/ /\n/g' > compile_flags.txt

.PHONY: clean run debug deploy

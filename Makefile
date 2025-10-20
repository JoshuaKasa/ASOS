# ===== Tools =====
ASM     = nasm
CC      = gcc
LD      = ld
OBJCOPY = objcopy
QEMU    = qemu-system-i386
PYTHON  = python3

# ===== Flags =====
CFLAGS  = -m32 -ffreestanding -Wall -Wextra -nostdlib -fno-pic -fno-stack-protector
LDFLAGS = -m elf_i386 -nostdlib -T $(abspath $(KERNEL_DIR)/linker.ld) -z max-page-size=0x1000

# ===== Outputs =====
STAGE1  = mbr.bin
STAGE2  = bootloader.bin
KERNEL  = kernel.bin
DISK    = disk.img

# ===== Directories =====
KERNEL_DIR  = kernel
LIB_DIR     = lib
UI_DIR      = ui
APP_DIR     = app
TOOLS_DIR   = tools

# ===== Sources =====
KERNEL_SRC  := $(wildcard $(KERNEL_DIR)/*.c)
KERNEL_ASM  := $(wildcard $(KERNEL_DIR)/*.asm)
KERNEL_OBJ  := $(KERNEL_SRC:.c=.o) $(KERNEL_ASM:.asm=.o)
KERNEL_ELF  = $(KERNEL_DIR)/kernel.elf

LIB_SRC     := $(wildcard $(LIB_DIR)/*.c)
LIB_OBJ     := $(LIB_SRC:.c=.o)

UI_SRC      := $(wildcard $(UI_DIR)/*.c)
UI_OBJ      := $(UI_SRC:.c=.o)

APP_SRC     := $(wildcard $(APP_DIR)/*.c)
APP_OBJ     := $(APP_SRC:.c=.o)
APP_ELF     := $(APP_SRC:.c=.elf)
APP_BIN     := $(APP_SRC:.c=.bin)
APP_BASE 	= 0x00300000

# ===== Targets =====
all: build fs run

# --- Stage 1 (MBR) ---
$(STAGE1): mbr.asm
	$(ASM) -f bin $< -o $@

# --- Stage 2 (Bootloader) ---
$(STAGE2): bootloader.asm $(KERNEL)
	@ksize=$$(stat -c%s $(KERNEL)); \
	 ksecs=$$((($$ksize + 511)/512)); \
	 echo "[*] Kernel size: $$ksize bytes ($$ksecs sectors)"; \
	 $(ASM) -f bin -DKERNEL_SECTORS=$$ksecs $< -o $@

# --- Generic C compilation ---
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_DIR)/%.o: $(KERNEL_DIR)/%.asm
	$(ASM) -f elf32 $< -o $@

# --- Kernel build ---
$(KERNEL_ELF): $(KERNEL_OBJ) $(LIB_OBJ) $(UI_OBJ)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJ) $(LIB_OBJ) $(UI_OBJ)

$(KERNEL): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

# --- App compilation (C → OBJ → ELF → BIN) ---
$(APP_DIR)/start.o: $(APP_DIR)/start.asm
	$(ASM) -f elf32 $< -o $@

$(APP_DIR)/%.o: $(APP_DIR)/%.c
	$(CC) -m32 -ffreestanding -fno-pic -fno-stack-protector -nostdlib -O0 -c $< -o $@

$(APP_DIR)/%.elf: $(APP_DIR)/start.o $(APP_DIR)/%.o $(LIB_DIR)/string.o $(LIB_DIR)/stdlib.o
	$(LD) -m elf_i386 -N -Ttext $(APP_BASE) -e _start -o $@ $^


$(APP_DIR)/%.bin: $(APP_DIR)/%.elf
	$(OBJCOPY) -O binary $< $@

# --- Disk image ---
$(DISK): $(STAGE1) $(STAGE2) $(KERNEL)
	@echo "[+] Creating disk image..."
	dd if=/dev/zero of=$(DISK) bs=512 count=131072 2>/dev/null
	@echo "[+] Writing Stage 1 (MBR)..."
	dd if=$(STAGE1) of=$(DISK) bs=512 count=1 conv=notrunc 2>/dev/null
	@echo "[+] Writing Stage 2 (Bootloader)..."
	dd if=$(STAGE2) of=$(DISK) bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "[+] Writing Kernel..."
	dd if=$(KERNEL) of=$(DISK) bs=512 seek=5 conv=notrunc 2>/dev/null
	@echo "[D] Disk image ready!"

# --- Build kernel + disk ---
build: $(DISK)
	@echo "[D] Build completed successfully!"

# --- Add filesystem (ASOFS) ---
fs: $(DISK) $(APP_BIN)
	@echo "[+] Writing ASOFS superblock + apps..."
	$(PYTHON) $(TOOLS_DIR)/make_asofs.py

# --- Run QEMU ---
run:
	$(QEMU) -vga std -drive format=raw,file=$(DISK),if=ide -m 128M -machine pc

# --- Cleanup ---
clean:
	rm -f $(STAGE1) $(STAGE2) $(DISK)
	rm -f $(KERNEL_DIR)/*.o $(KERNEL_DIR)/*.elf $(KERNEL)
	rm -f $(LIB_DIR)/*.o
	rm -f $(UI_DIR)/*.o
	rm -f $(APP_DIR)/*.o $(APP_DIR)/*.elf $(APP_DIR)/*.bin
	@echo "[–] Cleaned build files."

.PHONY: all build fs run clean

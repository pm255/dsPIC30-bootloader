ifndef CPU_MODEL
$(error CPU_MODEL parameter is not specified)
endif

ifndef CONFIG_FILE
$(error CONFIG_FILE parameter is not specified)
endif

ifndef LINKER_SCRIPT
LINKER_SCRIPT = ./gld/p$(CPU_MODEL).gld
endif

TARGET_NAME = bootloader

RM = del
CC = xc16-gcc.exe
BIN2HEX = xc16-bin2hex.exe

CFLAGS = -mcpu=$(CPU_MODEL) -g -legacy-libc -msmall-code -msmall-data \
-msmall-scalar -mconst-in-data -O2 -fomit-frame-pointer -msmart-io=1 \
-Wall -msfr-warn=off

LDFLAGS = -mcpu=$(CPU_MODEL) -omf=elf -legacy-libc \
-Wl,--script=$(LINKER_SCRIPT),--stack=16,--check-sections,--data-init \
-Wl,--pack-data,--no-handles,--isr,--gc-sections,--fill-upper=0 \
-Wl,--stackguard=16,--no-ivt,--no-force-link,--smart-io

OBJS = main.o

.PHONY: all clean

all: $(TARGET_NAME).hex

clean:
	$(RM) $(OBJS)
	$(RM) $(TARGET_NAME).elf
	$(RM) $(TARGET_NAME).hex

%.o: %.c $(CONFIG_FILE)
	$(CC) $< -o $@ -c -DCONFIG_FILE='"$(CONFIG_FILE)"' $(CFLAGS)

$(TARGET_NAME).elf: $(OBJS) $(LINKER_SCRIPT)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.hex: %.elf
	$(BIN2HEX) -a -v -omf=elf $<

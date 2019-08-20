CC = avr-gcc
CFLAGS = -Wall -Os -mmcu=atmega328p
OBJCOPY = avr-objcopy

OBJ = bootloader.o uart.o


all: bootloader.hex

%.o: %.c
	$(CC) $(CFLAGS) -c $<

bootloader.elf: $(OBJ)
	$(CC) $(CFLAGS) -Wl,--section-start=.text=0x7000 -o bootloader.elf $(OBJ)

bootloader.hex: bootloader.elf
	$(OBJCOPY) bootloader.elf -O ihex bootloader.hex

clean:
	rm -f *.o *.elf *.hex

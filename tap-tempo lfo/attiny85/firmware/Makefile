# This is a prototype Makefile. Modify it according to your needs.
# You should at least check the settings for
# DEVICE ....... The AVR device you compile for
# CLOCK ........ Target AVR clock rate in Hertz
# OBJECTS ...... The object files created from your source files. This list is
#                usually the same as the list of source files with suffix ".o".
# PROGRAMMER ... Options to avrdude which define the hardware you use for
#                uploading to the AVR and the interface where this hardware
#                is connected. We recommend that you leave it undefined and
#                add settings like this to your ~/.avrduderc file:
#                   default_programmer = "stk500v2"
#                   default_serial = "avrdoper"
# FUSES ........ Parameters for avrdude to flash the fuses appropriately.

#
# To include external clock input capabilities the reset pin must be used, and
# this means the device must be programmed in High Voltage Serial Programming
# mode (HVSP) rather than your standard ISP mode. Note that this requires a
# different hardware setup (see the STK500 manual for more info).
#
# ENABLE_EXT_CLK=1 (default) -> Disable the reset pin and use for I/O instead.
#
# ENABLE_EXT_CLK=0 -> ISP possible, but external clock sync feature not
#                     included.
#

ENABLE_EXT_CLK  := 1
ifeq ($(ENABLE_EXT_CLK), 1)
    PROG_MODE  = hvsp
    HFUSE      = 0x5f
else
    PROG_MODE  = v2
    HFUSE      = 0xdf
endif

DEVICE     = attiny85
CLOCK      = 8000000
PROGRAMMER = -c stk500$(PROG_MODE)
OBJECTS    = main.o switching.o signaling.o
FUSES      = -U lfuse:w:0xe2:m -U hfuse:w:$(HFUSE):m -U efuse:w:0xff:m -U lock:w:0xfe:m
TARGET     = tt_lfo_85

CFLAGS += -Os -g -std=c99 -Wall -DF_CPU=$(CLOCK) -mmcu=$(DEVICE) -DENABLE_EXT_CLK=$(ENABLE_EXT_CLK)

#Fuse settings: Programmed = 0, unprogrammed = 1

# lfuse = Fuse low byte. 0xe2 = CKDIV8:1
#								CKOUT:1
#								SUT1:1
#								SUT0:0
#								CKSEL3:0
#								CKSEL2:0
#								CKSEL1:1
#								CKSEL0:0

# hfuse = Fuse high byte. 0xdf = 	RSTDISBL:1
#									DWEN:1
#									SPIEN:0
#									WDTON:1
#									EESAVE:1
#									BODLEVEL2:1
#									BODLEVEL1:1
#									BODLEVEL0:1

# efuse = Extended fuse. 0xff = SELFPRGEN:1

# lock = Lock fuse. 0xfd =	LB2:0
#							LB1:1
#
# LB Mode 2 = Further programming disabled

# ATMega8 fuse bits used above (fuse bits for other devices are different!):
# Example for 8 MHz internal oscillator
# Fuse high byte:
# 0xd9 = 1 1 0 1   1 0 0 1 <-- BOOTRST (boot reset vector at 0x0000)
#        ^ ^ ^ ^   ^ ^ ^------ BOOTSZ0
#        | | | |   | +-------- BOOTSZ1
#        | | | |   +---------- EESAVE (set to 0 to preserve EEPROM over chip erase)
#        | | | +-------------- CKOPT (clock option, depends on oscillator type)
#        | | +---------------- SPIEN (if set to 1, serial programming is disabled)
#        | +------------------ WDTON (if set to 0, watchdog is always on)
#        +-------------------- RSTDISBL (if set to 0, RESET pin is disabled)
# Fuse low byte:
# 0x24 = 0 0 1 0   0 1 0 0
#        ^ ^ \ /   \--+--/
#        | |  |       +------- CKSEL 3..0 (8M internal RC)
#        | |  +--------------- SUT 1..0 (slowly rising power)
#        | +------------------ BODEN (if 0, brown-out detector is enabled)
#        +-------------------- BODLEVEL (if 0: 4V, if 1: 2.7V)
#
# For computing fuse byte values for other devices and options see
# the fuse bit calculator at http://www.engbedded.com/fusecalc/


# Tune the lines below only if you know what you are doing:

AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE) -B 10
COMPILE = avr-gcc $(CFLAGS)

# symbolic targets:
all:	$(TARGET).hex

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

.c.s:
	$(COMPILE) -S $< -o $@

flash:	all
	$(AVRDUDE) -U flash:w:$(TARGET).hex:i

fuse:
	$(AVRDUDE) $(FUSES)

# Xcode uses the Makefile targets "", "clean" and "install"
install: flash fuse

# if you use a bootloader, change the command below appropriately:
load: all
	bootloadHID $(TARGET).hex

clean:
	rm -f $(TARGET).elf $(OBJECTS)

# file targets:
$(TARGET).elf: $(OBJECTS)
	$(COMPILE) -o $(TARGET).elf $(OBJECTS)

$(TARGET).hex: $(TARGET).elf
	rm -f $(TARGET).hex
	avr-objcopy -j .text -j .data -O ihex $(TARGET).elf $(TARGET).hex
	avr-size --format=avr --mcu=$(DEVICE) $(TARGET).elf
# If you have an EEPROM section, you must also create a hex file for the
# EEPROM and add it to the "flash" target.

# Targets for code debugging and analysis:
disasm:	$(TARGET).elf
	avr-objdump -d $(TARGET).elf

cpp:
	$(COMPILE) -E $(TARGET).c

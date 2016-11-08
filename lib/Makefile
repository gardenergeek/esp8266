# Com port to dev board
PORT     = COM3

# SDK Base directory
SDKBASE = C:/projekt/tester/esp_iot_rtos_sdk/

AR      = xtensa-lx106-elf-ar
CC      = xtensa-lx106-elf-gcc
CPP     = xtensa-lx106-elf-gcc
LD      = xtensa-lx106-elf-gcc
NM      = xt-nm
OBJCOPY = xtensa-lx106-elf-objcopy
OD      = xtensa-lx106-elf-objdump

ESPTOOL		?= C:\Python27\python.exe C:\projekt\esp8266\esptool.py

INCLUDES =  -I $(SDKBASE)include
INCLUDES += -I $(SDKBASE)include/espressif
INCLUDES += -I $(SDKBASE)extra_include
INCLUDES += -I $(SDKBASE)include/lwip
INCLUDES += -I $(SDKBASE)include/lwip/lwip
INCLUDES += -I $(SDKBASE)include/lwip/ipv4
INCLUDES += -I $(SDKBASE)include/lwip/ipv6
INCLUDES += -I C:/xtensa-lx106-elf/xtensa-lx106-elf/include

# don't change -Os (or add other -O options) otherwise FLASHMEM and FSTR data will be duplicated in RAM
CFLAGS    = -g -save-temps -Os -Wpointer-arith -Wundef -Werror -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals
CFLAGS   += -D__ets__ 
CFLAGS   += -DICACHE_FLASH
CFLAGS   += -fno-exceptions

# -fno-rtti
#CFLAGS   += -fno-threadsafe-statics
#CFLAGS   += -fno-use-cxa-atexit

LDFLAGS     = -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static
LDFLAGS    += -Wl,--gc-sections

LD_SCRIPT   = eagle.app.v6.ld
#LD_SCRIPT   = eagle.app.v6.new.2048.ld

SDK_LIBDIR  = -L$(SDKBASE)lib
SDK_LDDIR   = $(SDKBASE)/ld


# linking libgccirom.a instead of libgcc.a causes reset when working with flash memory (ie spi_flash_erase_sector)
# linking libcirom.a causes conflicts with come std c routines (like strstr, strchr...)
LIBS        = -lmain -lgcc -lfreertos -lnet80211 -lphy -lwpa -lcrypto -llwip -lpp -lminic -lhal
#-lminic -lm -lgcc -lhal -lphy -lpp -lnet80211 -lwpa -lmain -lfreertos -llwip -lcrypto

OBJ  = coregpio.o coregpiomanager.o

SRCDIR = ./src/

all: libcoregpio.a

libcoregpio.a: $(OBJ)
	$(AR) cru $@ $^
	

%.o: $(SRCDIR)%.c $(wildcard $(SRCDIR)*.h)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@


%.o: $(SRCDIR)%.cpp $(wildcard $(SRCDIR)*.h)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	

clean:
	@rm -f *.a
	@rm -f *.o
	@rm -f *.out
	@rm -f *.bin
	@rm -f *.ii
	@rm -f *.s
	@rm -f *.expand
	@rm -f *.map
	@rm -f *.dump
	@rm -f *.hex
	


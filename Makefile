# ---------- CONFIG SECTION ---------- # 

PORT:=/dev/ttyUSB0
BITRATE:=115200
MODE:=dio

GCC_FOLDER:=/opt/esp8266/esp-open-sdk/xtensa-lx106-elf
ESPTOOL_PY:=/usr/share/esptool/esptool.py
SDK:=/opt/esp8266/esp_iot_sdk_v1.3.0/

# ------------------------------------ # 

FW_FILE_1:=image.elf-0x00000.bin
FW_FILE_2:=image.elf-0x40000.bin
TARGET_OUT:=image.elf
all : $(TARGET_OUT) $(FW_FILE_1) $(FW_FILE_2)

SRCS:=user/fake6502.c user/generate_video.c user/user_main.c 

XTLIB:=$(SDK)/lib
XTGCCLIB:=$(GCC_FOLDER)/lib/gcc/xtensa-lx106-elf/4.8.5/libgcc.a
FOLDERPREFIX:=$(GCC_FOLDER)/bin
CC:=$(FOLDERPREFIX)/$(PREFIX)gcc
PREFIX:=$(FOLDERPREFIX)/xtensa-lx106-elf-

CFLAGS:=-mlongcalls -I$(SDK)/include -Iinclude -O3 -I$(SDK)/include/ -DICACHE_FLASH 
LDFLAGS_CORE:=-nostdlib -Wl,--start-group -lmain -lnet80211 -lcrypto -lssl -lwpa -llwip -lpp -lphy -Wl,--end-group -lgcc -T$(SDK)/ld/eagle.app.v6.ld

LINKFLAGS:= $(LDFLAGS_CORE) -B$(XTLIB)

$(TARGET_OUT) : $(SRCS)
	$(PREFIX)gcc $(CFLAGS) $^  $(LINKFLAGS) -o $@

$(FW_FILE_1): $(TARGET_OUT)
	PATH=$(FOLDERPREFIX):$$PATH;$(ESPTOOL_PY) elf2image --flash_mode $(MODE) $(TARGET_OUT)

$(FW_FILE_2): $(TARGET_OUT)
	PATH=$(FOLDERPREFIX):$$PATH;$(ESPTOOL_PY) elf2image --flash_mode $(MODE) $(TARGET_OUT)

flash :	
	$(ESPTOOL_PY) -b $(BITRATE) --port $(PORT) write_flash --flash_mode $(MODE) 0x00000 image.elf-0x00000.bin 0x40000 image.elf-0x40000.bin

credentials :
	python -c "open('wifi_credentials.bin', 'wb').write(''.join(i.ljust(32, '\0') for i in ['$(ssid)','$(password)']))"
	$(ESPTOOL_PY) --port $(PORT) write_flash 0x3c000 wifi_credentials.bin
	rm wifi_credentials.bin	

clean :
	rm -rf user/*.o driver/*.o $(TARGET_OUT) $(FW_FILE_1) $(FW_FILE_2)

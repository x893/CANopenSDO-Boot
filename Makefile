################################################################################
# Makefile for CANopenSDO-Boot (converted from Keil uVision project)
################################################################################

###############################################################################
# Toolchain
###############################################################################

TOOLCHAIN_PATH := E:/Tools/GNU/arm-none-eabi-14.3/bin

CC      := $(TOOLCHAIN_PATH)/arm-none-eabi-gcc
AS      := $(TOOLCHAIN_PATH)/arm-none-eabi-gcc
OBJCOPY := $(TOOLCHAIN_PATH)/arm-none-eabi-objcopy
SIZE    := $(TOOLCHAIN_PATH)/arm-none-eabi-size

BUILD_DIR := build

###############################################################################
# Target
###############################################################################

TARGET  := CANopenSDO-Boot.elf
BIN     := CANopenSDO-Boot.bin

###############################################################################
# CPU / MCU options
###############################################################################

MCU     := -mcpu=cortex-m4 -mthumb

###############################################################################
# Directories and files
###############################################################################

# GCC startup and linker script (from app/startup/gcc)
STARTUP_S  := app/startup/gcc/startup_at32f415.S
LDSCRIPT   := app/startup/gcc/at32f415_flash.ld

###############################################################################
# Sources (from CANopenSDO-Boot.uvprojx)
###############################################################################

# AT32F415 firmware library
SRCS := \
  at32_fwlib/libraries/drivers/src/at32f415_crm.c \
  at32_fwlib/libraries/drivers/src/at32f415_exint.c \
  at32_fwlib/libraries/drivers/src/at32f415_flash.c \
  at32_fwlib/libraries/drivers/src/at32f415_gpio.c \
  at32_fwlib/libraries/drivers/src/at32f415_misc.c \
  at32_fwlib/libraries/drivers/src/at32f415_pwc.c \
  at32_fwlib/libraries/drivers/src/at32f415_tmr.c \
  at32_fwlib/libraries/drivers/src/at32f415_usart.c \
  at32_fwlib/libraries/drivers/src/at32f415_usb.c \
  at32_fwlib/libraries/drivers/src/at32f415_can.c \
  at32_fwlib/libraries/drivers/src/at32f415_dma.c \
  at32_fwlib/libraries/drivers/src/at32f415_adc.c \
  at32_fwlib/libraries/drivers/src/at32f415_spi.c \
  at32_fwlib/libraries/drivers/src/at32f415_debug.c \
  at32_fwlib/libraries/drivers/src/at32f415_wwdt.c \
  at32_fwlib/libraries/drivers/src/at32f415_wdt.c \
  at32_fwlib/libraries/drivers/src/at32f415_crc.c \
  at32_fwlib/libraries/drivers/src/at32f415_i2c.c

# Core / application
SRCS += \
  app/core/system_at32f415.c \
  app/core/at32f415_int.c \
  app/core/main.c

# Board support
SRCS += \
  app/boards/boards.c

# CANopenNode AT32 glue and OD
SRCS += \
  canopen_at32/CO_app_AT32.c \
  canopen_at32/CO_driver_AT32.c \
  od/OD.c

# CANopenNode core
SRCS += \
  canopennode/CANopen.c

# CANopenNode / CiA 301
SRCS += \
  canopennode/301/CO_Emergency.c \
  canopennode/301/CO_fifo.c \
  canopennode/301/CO_HBconsumer.c \
  canopennode/301/CO_NMT_Heartbeat.c \
  canopennode/301/CO_Node_Guarding.c \
  canopennode/301/CO_ODinterface.c \
  canopennode/301/CO_PDO.c \
  canopennode/301/CO_SDOclient.c \
  canopennode/301/CO_SDOserver.c \
  canopennode/301/CO_SYNC.c \
  canopennode/301/CO_TIME.c \
  canopennode/301/crc16-ccitt.c

# CANopenNode / CiA 303
SRCS += \
  canopennode/303/CO_LEDs.c

# CANopenNode / CiA 304
SRCS += \
  canopennode/304/CO_GFC.c \
  canopennode/304/CO_SRDO.c

# CANopenNode / CiA 305
SRCS += \
  canopennode/305/CO_LSSmaster.c \
  canopennode/305/CO_LSSslave.c

# CANopenNode / storage
SRCS += \
  canopennode/storage/CO_storage.c

ASM_SRCS := $(STARTUP_S)

OBJS     := $(addprefix $(BUILD_DIR)/,$(SRCS:.c=.o)) \
            $(addprefix $(BUILD_DIR)/,$(ASM_SRCS:.S=.o))

###############################################################################
# Include paths and defines (from Cads -> VariousControls)
###############################################################################

INCLUDES := \
  -Iapp/core \
  -Iapp/drivers \
  -Iapp/boards \
  -Iod \
  -Icanopen_at32 \
  -Icanopennode \
  -Icanopennode/301 \
  -Iat32_fwlib/libraries/drivers/inc \
  -Iat32_fwlib/libraries/cmsis/cm4/core_support \
  -Iat32_fwlib/libraries/cmsis/cm4/device_support \
  -Iat32_fwlib/middlewares/usb_drivers/inc \
  -Iat32_fwlib/middlewares/usbd_class/cdc

DEFS := \
  -DNO_USE_WDT \
  -DAT32F415CCT7 \
  -DUSE_STDPERIPH_DRIVER

###############################################################################
# Flags
###############################################################################
#  -g3
CFLAGS  := $(MCU) -Os -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unused-variable $(DEFS) $(INCLUDES) \
	-Wno-unused-parameter \
	-Wno-unused-but-set-parameter

ASFLAGS := $(MCU) -g3
LDFLAGS := $(MCU) -Wl,--gc-sections -T$(LDSCRIPT)

###############################################################################
# Default target
###############################################################################

.PHONY: all
all: $(TARGET) $(BIN)

###############################################################################
# Build rules
###############################################################################

$(TARGET): $(OBJS)
	@echo "LD $(notdir $@)"
	@$(CC) $(LDFLAGS) -o $@ $^
	@$(SIZE) $@

$(BIN): $(TARGET)
	@echo "OBJCOPY $(notdir $@)"
	@$(OBJCOPY) -O binary $< $@

# C sources
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC $(notdir $<)"
	@$(CC) $(CFLAGS) -c $< -o $@

# Assembly sources
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "AS $(notdir $<)"
	@$(AS) $(ASFLAGS) -c $< -o $@

###############################################################################
# Other targets
###############################################################################

.PHONY: clean
clean:
	@echo "Clean"
	@$(RM) $(OBJS) $(TARGET) $(BIN)


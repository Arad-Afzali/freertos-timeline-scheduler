# Timeline Scheduler Makefile
# Windows Compatible Version

PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump
SIZE = $(PREFIX)size
GDB = $(PREFIX)gdb

BUILD_DIR = build
SRC_DIR = src
FREERTOS_DIR = group50-config-arad-FreeRTOS-Kernel/FreeRTOS-Kernel
INC_DIR = include

TARGET = scheduler
LINKER_SCRIPT = linker_script.ld

MCU = cortex-m3
ARCH = -mcpu=$(MCU) -mthumb

FREERTOS_SRC = \
    $(FREERTOS_DIR)/tasks.c \
    $(FREERTOS_DIR)/queue.c \
    $(FREERTOS_DIR)/list.c \
    $(FREERTOS_DIR)/timers.c \
    $(FREERTOS_DIR)/portable/MemMang/heap_4.c \
    $(FREERTOS_DIR)/portable/GCC/ARM_CM3/port.c

APP_SRC = $(wildcard $(SRC_DIR)/*.c)

SOURCES = $(FREERTOS_SRC) $(APP_SRC)

INCLUDES = \
    -I$(INC_DIR) \
    -I$(FREERTOS_DIR)/include \
    -I$(FREERTOS_DIR)/portable/GCC/ARM_CM3

CFLAGS = $(ARCH) \
    -Wall -Wextra -Wpedantic \
    -g3 -gdwarf-2 \
    -O0 \
    -ffunction-sections \
    -fdata-sections \
    -fno-common \
    -fmessage-length=0 \
    $(INCLUDES)

LDFLAGS = $(ARCH) \
    -T$(LINKER_SCRIPT) \
    -specs=nosys.specs \
    -specs=nano.specs \
    -Wl,--gc-sections \
    -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref \
    -Wl,--print-memory-usage

OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(SOURCES:.c=.o)))

.PHONY: all clean size help

all: $(BUILD_DIR)/$(TARGET).elf
	@echo.
	@echo ===== Build Complete =====
	@$(SIZE) $<
	@echo.

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) | $(BUILD_DIR)
	@echo Linking: $@
	$(CC) $(LDFLAGS) -o $@ $^
	@$(OBJCOPY) -O binary $@ $(BUILD_DIR)/$(TARGET).bin
	@$(OBJDUMP) -h -S $@ > $(BUILD_DIR)/$(TARGET).lst

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo Compiling: $<
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(FREERTOS_DIR)/%.c | $(BUILD_DIR)
	@echo Compiling: $<
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(FREERTOS_DIR)/portable/MemMang/%.c | $(BUILD_DIR)
	@echo Compiling: $<
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(FREERTOS_DIR)/portable/GCC/ARM_CM3/%.c | $(BUILD_DIR)
	@echo Compiling: $<
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

size: $(BUILD_DIR)/$(TARGET).elf
	@echo.
	@echo ===== Memory Usage =====
	@$(SIZE) --format=berkeley $<

clean:
	@echo Cleaning build artifacts...
	@if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
	@echo Clean complete.

help:
	@echo Timeline Scheduler Makefile
	@echo.
	@echo Targets:
	@echo   all    - Build the project (default)
	@echo   clean  - Remove build artifacts
	@echo   size   - Show memory usage
	@echo   help   - Show this help message
	@echo.
	@echo Toolchain: ARM GNU Toolchain 13.2.Rel1
	@echo QEMU: xPack QEMU Arm 8.2.6-1

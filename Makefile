# Directories
MSPGCC_ROOT_DIR = /home/alexn/dev/tools/msp430-gcc
MSPGCC_BIN_DIR = $(MSPGCC_ROOT_DIR)/bin
MSPGCC_INCLUDE_DIR = $(MSPGCC_ROOT_DIR)/include
MSP430_FLASHER_DIR = /home/alexn/dev/tools/MSPFlasher_1.3.20

INCLUDE_DIRS = $(MSPGCC_INCLUDE_DIR) $(MSPGCC_BIN_DIR)
LIB_DIRS = $(INCLUDE_DIRS)

BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin
OBJ_DIR = $(BUILD_DIR)/obj

SRC_DIR = src
FW_DIR = $(SRC_DIR)/fw

# Toolchain
CC = $(MSPGCC_BIN_DIR)/msp430-elf-gcc
RM = rm
MSP430_FLASHER = LD_LIBRARY_PATH=$(MSP430_FLASHER_DIR) $(MSP430_FLASHER_DIR)/MSP430Flasher
CREATE_HEX_OUTFILE = $(MSPGCC_BIN_DIR)/msp430-elf-objcopy
CPPCHECK = cppcheck

# Files
## Output Files
TARGET = $(BIN_DIR)/run_sumobot
HEX_FILE = $(TARGET).hex

## Input Files
SRC_NAMES = main.c
SRC_FILES = $(addprefix $(FW_DIR)/, $(SRC_NAMES))

OBJ_NAMES = $(SRC_NAMES:.c=.o)
OBJ_FILES = $(addprefix $(OBJ_DIR)/ $(OBJ_NAMES))

# Flags
## Compiler and Linker Flags
MCU = msp430f5529
W_FLAGS = -Wall -Wextra -Werror -Wshadow 
C_FLAGS = -mmcu=$(MCU) $(WFLAGS) $(addprefix -I, $(INCLUDE_DIRS)) -Og -g
LD_FLAGS = -mmcu=$(MCU) $(addprefix -I, $(LIB_DIRS))
## Flash Flags
DEVICE = -n $(MCU)
EXIT_SPECS = -z [VCC, RESET]
VERIFY = -v
PROG_FILE = -w $(HEX_FILE) 
FLASH_FLAGS = $(DEVICE) $(EXIT_SPECS) $(VERIFY) $(PROG_FILE)

# Build
## Executable
$(HEX_FILE): $(TARGET)
	$(CREATE_HEX_OUTFILE) -O ihex $^ $(HEX_FILE)

## Linking
$(TARGET): $(OBJ_FILES)
	$(CC) $(LD_FLAGS) $^ -o $@

## Compiling
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(C_FLAGS) -c -o $@ $^

# PHONIES
.PHONY: all clean flash cppcheck

all: $(TARGET)

clean:
	$(RM) -r $(BUILD_DIR)

flash: $(HEX_FILE)
	/bin/bash -c "$(MSP430_FLASHER) $(FLASH_FLAGS)"

cppcheck: 
	@$(CPPCHECK) --quiet --enable=all --error-exitcode=1 --inline-suppr \
		-I $(INCLUDE_DIR) \
		$(SRC_FILES)
		-i externals/printf

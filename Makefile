# Directories
TOOLS_DIR = ${TOOLS_PATH}
MSPGCC_ROOT_DIR = $(TOOLS_DIR)/msp430-gcc
MSPGCC_BIN_DIR = $(MSPGCC_ROOT_DIR)/bin
MSPGCC_INCLUDE_DIR = $(MSPGCC_ROOT_DIR)/include
MSP430_FLASHER_DIR = $(TOOLS_DIR)/MSPFlasher_1.3.20

INCLUDE_DIRS = $(MSPGCC_INCLUDE_DIR) $(MSPGCC_BIN_DIR) $(FW_DIR) /external /external/printf
LIB_DIRS = $(INCLUDE_DIRS)

BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin
OBJ_DIR = $(BUILD_DIR)/obj

SRC_DIR = src
FW_DIR = $(SRC_DIR)/fw
MOTORS_DIR = $(FW_DIR)/motors
TEST_DIR = $(FW_DIR)/test
DRIVERS_DIR = $(FW_DIR)/drivers
APP_DIR = $(FW_DIR)/app
COMMON_DIR = $(FW_DIR)/common

# Toolchain
CC = $(MSPGCC_BIN_DIR)/msp430-elf-gcc
RM = rm
MSP430_FLASHER = LD_LIBRARY_PATH=$(MSP430_FLASHER_DIR) $(MSP430_FLASHER_DIR)/MSP430Flasher
CREATE_HEX_OUTFILE = $(MSPGCC_BIN_DIR)/msp430-elf-objcopy
CPPCHECK = cppcheck
FORMAT = clang-format-14
# Files
## Output Files
TARGET = $(BIN_DIR)/run_sumobot
HEX_FILE = $(TARGET).hex

## Input Files
SRC_FILES_APP = drive.c enemy.c
SRC_FILES_DRIVERS = i2c.c uart.c
SRC_FILES_TEST = test.c
SRC_FILES_MOTOR = motors.c
SRC_FILES = $(FW_DIR)/main.c \
			$(addprefix $(APP_DIR)/, $(SRC_FILES_APP)) \
			$(addprefix $(DRIVERS_DIR)/, $(SRC_FILES_DRIVERS)) \
			$(addprefix $(TEST_DIR)/, $(SRC_FILES_TEST)) \
			$(addprefix $(MOTORS_DIR)/, $(SRC_FILES_MOTOR))

HEADER_FILES = $(COMMON_DIR)/defines.h \
	$(addprefix $(APP_DIR)/, $(SRC_FILES_APP:.c=.h)) \
	$(addprefix $(DRIVERS_DIR)/, $(SRC_FILES_DRIVERS:.c=.h))  \
	$(addprefix $(TEST_DIR)/, $(SRC_FILES_TEST:.c=.h)) \
	$(addprefix $(MOTORS_DIR)/, $(SRC_FILES_MOTOR:.c=.h))

OBJ_NAMES = $(SRC_FILES:.c=.o)
OBJ_FILES = $(patsubst $(FW_DIR)/%, $(OBJ_DIR)/%, $(OBJ_NAMES))

# Flags
## Compiler and Linker Flags
MCU = msp430f5529
W_FLAGS = -Wall -Wextra -Werror -Wshadow 
C_FLAGS = -mmcu=$(MCU) $(W_FLAGS) $(addprefix -I, $(INCLUDE_DIRS)) -Og -g
LD_FLAGS = -mmcu=$(MCU) $(addprefix -L, $(LIB_DIRS))
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
$(TARGET): $(OBJ_FILES) $(HEADER_FILES)
	echo $(HEADER_FILES)
	@mkdir -p $(dir $@)
	$(CC) $(LD_FLAGS) $^ -o $@

## Compiling
$(OBJ_DIR)/%.o: $(FW_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -c -o $@ $^

# PHONIES
.PHONY: all clean flash cppcheck format

all: $(TARGET)

clean:
	$(RM) -r $(BUILD_DIR)

flash: $(HEX_FILE)
	/bin/bash -c "$(MSP430_FLASHER) $(FLASH_FLAGS)"

cppcheck: 
	@$(CPPCHECK) --quiet --enable=all --error-exitcode=1 --inline-suppr \
		-I $(INCLUDE_DIRS) \
		-i externals/printf \
		-v \
		$(SRC_FILES)

format:
	@$(FORMAT) -i $(SRC_FILES) $(HEADER_FILES)

# Check arguments
ifeq ($(HW), LAUNCHPAD) # Compile for launchpad
TARGET_HW = launchpad
else
ifeq ($(HW), SUMOBOT) # Compile for PCB
TARGET_HW = sumobot
# Don't require HW arg if doing make clean, cppcheck, or format
else
ifeq ($(MAKECMDGOALS), clean)
else
ifeq ($(MAKECMDGOALS), cppcheck)
else
ifeq ($(MAKECMDGOALS), format)
# Must provide HW arg for make otherwise
else
$(error "Must pass HW=LAUNCHPAD or HW=SUMOBOT)
endif # Format
endif # Cppcheck
endif # Clean
endif # SumoBot
endif # Launchpad

TARGET_NAME=$(TARGET_HW)
ifneq ($(TEST),) # TEST argument
ifeq ($(findstring test_, $(TEST)),)
$(error "TEST=$(TEST) is invalid (test function must start with test_)")
else
TARGET_NAME=$(TEST)
endif
endif

# Defines
HW_DEFINE = $(addprefix -D, $(HW))
TEST_DEFINE = $(addprefix -DTEST=, $(TEST))
PRINTF_DEFINE = $(addprefix -D, PRINTF_INCLUDE_CONFIG_H)
DEFINES = $(HW_DEFINE) $(TEST_DEFINE) $(PRINTF_DEFINE)

# Directories
TOOLS_DIR = ${TOOLS_PATH}
MSPGCC_ROOT_DIR = $(TOOLS_DIR)/msp430-gcc
MSPGCC_BIN_DIR = $(MSPGCC_ROOT_DIR)/bin
MSPGCC_INCLUDE_DIR = $(MSPGCC_ROOT_DIR)/include
MSP430_FLASHER_DIR = $(TOOLS_DIR)/MSPFlasher_1.3.20

INCLUDE_DIRS = $(MSPGCC_INCLUDE_DIR) $(MSPGCC_BIN_DIR) $(FW_DIR) .
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
EXTERNALS_DIR = externals
PRINTF_DIR = $(EXTERNALS_DIR)/printf

# Toolchain
CC = $(MSPGCC_BIN_DIR)/msp430-elf-gcc
RM = rm
MSP430_FLASHER = LD_LIBRARY_PATH=$(MSP430_FLASHER_DIR) $(MSP430_FLASHER_DIR)/MSP430Flasher
MSP430_ELF_SIZE = $(MSPGCC_BIN_DIR)/msp430-elf-size
MSP430_READ_ELF = $(MSPGCC_BIN_DIR)/msp430-elf-readelf
CREATE_HEX_OUTFILE = $(MSPGCC_BIN_DIR)/msp430-elf-objcopy
CPPCHECK = cppcheck
FORMAT = clang-format-14
ADDR2LINE = $(MSPGCC_BIN_DIR)/msp430-elf-addr2line 

# Files
## Output Files
TARGET = $(BIN_DIR)/$(TARGET_HW)/$(TARGET_NAME)
HEX_FILE = $(TARGET).hex

## Input Files
ifndef TEST
MAIN_SRC_FILE = $(FW_DIR)/main.c
else
MAIN_SRC_FILE = $(TEST_DIR)/$(TEST).c
endif
SRC_FILES_APP = drive.c enemy.c line.c
SRC_FILES_DRIVERS = io.c led.c mcu_init.c uart.c ring_buffer.c pwm.c drv8848.c adc.c qre1113.c
SRC_FILES_MOTOR = motors.c
SRC_FILES_COMMON = assert_handler.c trace.c
SRC_FILES_PRINTF = printf.c

# Don't include test/test.c in compilation because it has its own main() function
# and you can only have 1 when compiling. The test.c is compiled separately with build_tests.sh
# in CI with Github Actions through .github/workflows/ci.yml
SRC_FILES = $(addprefix $(PRINTF_DIR)/, $(SRC_FILES_PRINTF)) \
			$(addprefix $(APP_DIR)/, $(SRC_FILES_APP)) \
			$(addprefix $(DRIVERS_DIR)/, $(SRC_FILES_DRIVERS)) \
			$(addprefix $(MOTORS_DIR)/, $(SRC_FILES_MOTOR)) \
			$(addprefix $(COMMON_DIR)/, $(SRC_FILES_COMMON)) \

HEADER_FILES = $(COMMON_DIR)/defines.h \
	$(addprefix $(PRINTF_DIR)/, $(SRC_FILES_PRINTF:.c=.h)) \
	$(addprefix $(APP_DIR)/, $(SRC_FILES_APP:.c=.h)) \
	$(addprefix $(DRIVERS_DIR)/, $(SRC_FILES_DRIVERS:.c=.h))  \
	$(addprefix $(MOTORS_DIR)/, $(SRC_FILES_MOTOR:.c=.h)) \
	$(addprefix $(COMMON_DIR)/, $(SRC_FILES_COMMON:.c=.h)) \

OBJ_NAMES = $(SRC_FILES:.c=.o)
MAIN_OBJ_NAMES = $(MAIN_SRC_FILE:.c=.o)
OBJ_FILES = $(patsubst $(FW_DIR)/%, $(OBJ_DIR)/%, $(OBJ_NAMES))
MAIN_OBJ_FILES = $(patsubst $(FW_DIR)/%, $(OBJ_DIR)/%, $(MAIN_OBJ_NAMES))

# Flags
## Compiler and Linker Flags
MCU = msp430f5529
W_FLAGS = -Wall -Wextra -Werror -Wshadow 
C_FLAGS = -mmcu=$(MCU) $(W_FLAGS) $(addprefix -I, $(INCLUDE_DIRS)) $(DEFINES) -Og -g -fshort-enums
LD_FLAGS = -mmcu=$(MCU) $(DEFINES) $(addprefix -L, $(LIB_DIRS)) -Wl,-Map=$(TARGET).map
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
$(TARGET): $(OBJ_FILES) $(MAIN_OBJ_FILES) $(HEADER_FILES)
	@mkdir -p $(dir $@)
	$(CC) $(LD_FLAGS) $^ -o $@

## Compiling
$(OBJ_DIR)/%.o: $(FW_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -c -o $@ $^
ifndef TEST
$(OBJ_DIR)/main.o: $(FW_DIR)/main.c
	@mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -c -o $@ $^
else
$(OBJ_DIR)/test/$(TEST).o: $(TEST_DIR)/test.c
	@mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -c -o $@ $^
endif	

# PHONIES
.PHONY: all clean flash cppcheck format

all: $(TARGET)

clean:
	$(RM) -rf $(BUILD_DIR)

flash: $(HEX_FILE)
	/bin/bash -c "$(MSP430_FLASHER) $(FLASH_FLAGS)"

CPPCHECK_INCLUDES = $(FW_DIR)
CPPCHECK_IGNORE = externals/printf
cppcheck: 
	@$(CPPCHECK) --quiet --enable=all --error-exitcode=1 --inline-suppr \
		$(addprefix -I, $(CPPCHECK_INCLUDES)) \
		$(addprefix -i, $(CPPCHECK_IGNORE)) \
		-v \
		--suppress=missingInclude \
		--suppress=missingIncludeSystem \
		--suppress=unmatchedSuppression \
		--suppress=unusedFunction \
		$(filter-out externals/printf/printf.c, $(SRC_FILES))

FORMAT_INCLUDES = $(filter-out $(FORMAT_IGNORE), $(SRC_FILES)) $(filter-out $(FORMAT_IGNORE), $(HEADER_FILES))
FORMAT_IGNORE = externals/printf/printf.h \
				externals/printf/printf.c
format:
	$(FORMAT) --verbose -i $(FORMAT_INCLUDES)

size: $(TARGET)
	@$(MSP430_ELF_SIZE) $(TARGET)

symbols: $(TARGET)
	# List symbols table sorted by size
	@$(MSP430_READ_ELF) -s $(TARGET) | sort -n -k3 

addr2line: $(TARGET)
	@$(ADDR2LINE) -e $(TARGET) $(ADDR)

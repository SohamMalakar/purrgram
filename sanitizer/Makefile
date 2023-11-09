# Define variables for directories and compiler options
SRC_DIR := ../src
BUILD_DIR := ../build
CC := gcc
CFLAGS := -Wall -Wextra -Ofast

# Define the target and dependencies
TARGET := $(BUILD_DIR)/sanitizer
SOURCES := main.c $(SRC_DIR)/scanner.c

# Define the default target
all: $(TARGET)

# Build the target executable
$(TARGET): $(SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

# Define a rule to run the program with optional arguments
run: $(TARGET)
	$(TARGET) $(ARGS)

# Define a rule to clean up the build directory
clean:
	rm -f $(TARGET)

.PHONY: all run clean


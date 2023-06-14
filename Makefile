# Compiler
CC = gcc

# Directories
BUILD_DIR = build
DEBUG_DIR = $(BUILD_DIR)/debug
RELEASE_DIR = $(BUILD_DIR)/release
SRC_DIR = src
TEST_DIR = test

# Project name
PROJECT_NAME = purr

# Source files
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(DEBUG_DIR)/%.o,$(SRC_FILES))

# Flags
CFLAGS = -Wall -Wextra -I$(SRC_DIR) -g
LDFLAGS = -lm

# Targets
.PHONY: all debug release clean run

all: debug release

debug: $(DEBUG_DIR)/$(PROJECT_NAME)

release: $(RELEASE_DIR)/$(PROJECT_NAME)

$(DEBUG_DIR)/$(PROJECT_NAME): $(OBJ_FILES)
	@mkdir -p $(DEBUG_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(RELEASE_DIR)/$(PROJECT_NAME): CFLAGS += -O2
$(RELEASE_DIR)/$(PROJECT_NAME): $(SRC_FILES)
	@mkdir -p $(RELEASE_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(DEBUG_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(DEBUG_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

run: debug
	@$(DEBUG_DIR)/$(PROJECT_NAME) $(ARGS)

clean:
	@rm -rf $(BUILD_DIR)

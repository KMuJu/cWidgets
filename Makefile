# Makefile for building a NetworkManager C example using libnm

CC = cc
PKG_CFLAGS = $(shell pkg-config --cflags libnm gtk4)
PKG_LIBS = $(shell pkg-config --libs libnm gtk4)

# Debug + sanitizer flags
DEBUG_FLAGS = -g -O0
SANITIZE_FLAGS = -fsanitize=address,undefined

CFLAGS = $(PKG_CFLAGS) $(DEBUG_FLAGS) $(SANITIZE_FLAGS)
LDFLAGS = $(PKG_LIBS) $(SANITIZE_FLAGS)
TARGET = networkmanager
SRC = networkmanager.c
BUILD_DIR = .

.PHONY: all clean compile_commands

all: compile_commands $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

compile_commands:
	bear --append --output $(BUILD_DIR)/compile_commands.json -- \
		$(MAKE) --no-print-directory internal_build

.PHONY: internal_build
internal_build:
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) compile_commands.json

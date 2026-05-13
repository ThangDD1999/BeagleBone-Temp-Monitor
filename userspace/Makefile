# Compiler configuration
CC      = arm-linux-gnueabihf-gcc
CFLAGS  = -Wall -I./common
LDFLAGS = -lpthread -lrt

# Directory configuration
APP_DIR    = apps
BIN_DIR    = bin
SYSTEMD_DIR = systemd

# Source files
COLLECTOR_SRC = $(APP_DIR)/collector/main.c
SERVER_SRC    = $(APP_DIR)/server/main.c

# Output binaries
COLLECTOR_BIN = $(BIN_DIR)/collector
SERVER_BIN    = $(BIN_DIR)/server

.PHONY: all clean setup

# Default target
all: setup $(COLLECTOR_BIN) $(SERVER_BIN)

# Create bin directory if it doesn't exist
setup:
	@mkdir -p $(BIN_DIR)

# Build collector
$(COLLECTOR_BIN): $(COLLECTOR_SRC)
	$(CC) $(CFLAGS) $(COLLECTOR_SRC) -o $(COLLECTOR_BIN) $(LDFLAGS)
	@echo "[Build] Collector compiled to $(COLLECTOR_BIN)"

# Build server
$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER_BIN) $(LDFLAGS)
	@echo "[Build] Server compiled to $(SERVER_BIN)"

# Clean build artifacts
clean:
	rm -rf $(BIN_DIR)
	@echo "[Clean] Removed $(BIN_DIR) directory"
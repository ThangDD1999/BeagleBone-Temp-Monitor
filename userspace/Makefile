CC      = arm-linux-gnueabihf-gcc
CFLAGS  = -Wall -I./common
LDFLAGS = -lpthread -lrt

COLLECTOR_SRC = collector/main.c
SERVER_SRC    = server/main.c

.PHONY: all collector server clean

all: collector server

collector: $(COLLECTOR_SRC)
	$(CC) $(CFLAGS) $(COLLECTOR_SRC) -o collector/collector $(LDFLAGS)

server: $(SERVER_SRC)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o server/server $(LDFLAGS)

clean:
	rm -f collector/collector server/server

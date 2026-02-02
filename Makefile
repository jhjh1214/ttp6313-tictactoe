CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -g -std=c99
LDFLAGS = 

# Source directories
SRCDIR = src
INCDIR = include
OBJDIR = obj

# Create obj directory if it doesn't exist
$(shell mkdir -p $(OBJDIR))

# Targets
.PHONY: all clean run-server setup

all: setup server game_process client

setup:
	@mkdir -p data
	@echo "Created data directory for database files"

server: src/server.c src/database.c src/ipc.c include/ipc.h include/database.h
	$(CC) $(CFLAGS) src/server.c src/database.c src/ipc.c -o server $(LDFLAGS)
	@echo "Built server"

game_process: src/game_process.c src/ipc.c src/database.c include/ipc.h include/database.h
	$(CC) $(CFLAGS) src/game_process.c src/ipc.c src/database.c -o game_process $(LDFLAGS)
	@echo "Built game_process"

client: src/client.c src/ipc.c include/ipc.h
	$(CC) $(CFLAGS) src/client.c src/ipc.c -o client $(LDFLAGS)
	@echo "Built client"

clean:
	rm -f server game_process client
	rm -rf $(OBJDIR)
	rm -f /tmp/game_notify
	@echo "Cleaned build files"

clean-all: clean
	rm -rf data
	@echo "Cleaned all files including database"

run-server: server game_process
	./server

help:
	@echo "Available targets:"
	@echo "  all          - Build all executables (default)"
	@echo "  server       - Build server only"
	@echo "  game_process - Build game_process only"
	@echo "  client       - Build client only"
	@echo "  clean        - Remove executables and build files"
	@echo "  clean-all    - Remove executables and database"
	@echo "  run-server   - Build and run server"
	@echo "  help         - Show this help message"
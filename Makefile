# Compiler und Flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude
LDFLAGS =

# Quellcode-Dateien
SRC = \
    src/main.c \
    src/image_converter.c \
    src/dislocker_runner.c \
    src/loop_device.c \
    src/partition_parser.c \
    src/mount_selector.c \
    src/image_merger.c

# Objekt-Dateien (automatisch abgeleitet)
OBJ = $(SRC:.c=.o)

# Ziel-Binary
BIN = forensic_unlock

# Standard-Build-Ziel
all: $(BIN)

# Link-Schritt
$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Objekt-Dateien aus C-Dateien erzeugen
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Aufräumen
clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean

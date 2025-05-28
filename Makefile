CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude
LDFLAGS =

SRC = \
    src/main.c \
    src/image_converter.c \
    src/dislocker_runner.c \
    src/loop_device.c \
    src/partition_parser.c \
    src/mount_selector.c \
    src/image_merger.c

OBJ = $(SRC:.c=.o)

BIN = forensic_unlock

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean

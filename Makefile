CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
LDFLAGS =

SRC = src/main.c \
      src/mount_selector.c \
      src/image_converter.c \
      src/partition_parser.c \
      src/dislocker_runner.c \
      src/loop_device.c \
      src/mapper.c \

OBJ = $(SRC:.c=.o)

INC = -Iinclude

BIN = forensic_unlock

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(INC) $(LDFLAGS)

clean:
	rm -f $(BIN) *.o src/*.o

.PHONY: all clean

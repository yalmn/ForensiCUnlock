# Compiler und Flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_GNU_SOURCE -O2 -Iinclude
LDFLAGS =

# Quellcode-Dateien ohne main.c, damit die Unit-Tests sie mitbenutzen können
LIB_SRC = \
    src/exec_utils.c \
    src/image_converter.c \
    src/partition_parser.c \
    src/dislocker_runner.c \
    src/image_merger.c

LIB_OBJ = $(LIB_SRC:.c=.o)
HEADERS = $(wildcard include/*.h)

# Ziel-Binary
BIN = forensic_unlock
UNIT_TEST_BIN = tests/unit_tests

# Standard-Build-Ziel
all: $(BIN)

$(BIN): src/main.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(UNIT_TEST_BIN): tests/unit_tests.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Objekt-Dateien aus C-Dateien erzeugen
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

# Unit-Tests (Linux, root für die Mount-Tests)
test: $(BIN) $(UNIT_TEST_BIN)
	./$(UNIT_TEST_BIN)

# Komplette Tests mit echten BitLocker-Images (Linux, root, FUSE)
integration-test: $(BIN)
	./tests/integration_tests.sh

# Aufräumen
clean:
	rm -f src/*.o tests/*.o $(BIN) $(UNIT_TEST_BIN)

.PHONY: all test integration-test clean

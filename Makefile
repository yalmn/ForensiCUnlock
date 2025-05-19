CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
LDFLAGS =

SRC = \
    src/main.c \
    src/mount_selector.c \
    src/image_converter.c \
    src/partition_parser.c \
    src/dislocker_runner.c \
    src/loop_device.c \
    src/mapper.c

INC = -Iinclude

BIN = forensic_unlock

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(INC) $(LDFLAGS)

clean:
	rm -f $(BIN) *.o src/*.o

.PHONY: all clean

# ========= CUnit-Tests =========
CUNIT_INC = -I/usr/include/CUnit
CUNIT_LIB = -lcunit

TEST_SRCS = \
    tests/test_dislocker_runner.c \
    tests/test_partition_parser.c \
    tests/test_image_converter.c \
    tests/test_loop_device.c \
    tests/test_mapper.c \
    tests/test_mount_selector.c

TEST_BIN = run_tests

$(TEST_BIN): $(SRC) $(TEST_SRCS)
	$(CC) $(CFLAGS) $(INC) $(CUNIT_INC) \
		-Dsystem=my_system \
		-Dpopen=my_popen -Dpclose=my_pclose \
		-DMOUNT_POINT=\"/tmp/mselXXXXXX\" \
		$(SRC) $(TEST_SRCS) \
		-o $(TEST_BIN) $(LDFLAGS) $(CUNIT_LIB)

.PHONY: tests

tests: $(TEST_BIN)
	./$(TEST_BIN)

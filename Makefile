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

OBJ = $(SRC:.c=.o)

INC = -Iinclude

BIN = forensic_unlock

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(INC) $(LDFLAGS)

clean:
	rm -f $(BIN) *.o src/*.o run_test_*

.PHONY: all clean tests run_tests

# ========= CUnit-Tests =========
CUNIT_INC = -I/usr/include/CUnit
CUNIT_LIB = -lcunit

# Dislocker_runner tests
run_test_dislocker_runner: src/dislocker_runner.c tests/test_dislocker_runner.c
	$(CC) $(CFLAGS) -D_GNU_SOURCE $(INC) $(CUNIT_INC) \
		-Dsystem=my_system \
		src/dislocker_runner.c tests/test_dislocker_runner.c \
		-o $@ $(LDFLAGS) $(CUNIT_LIB)

# Partition_parser tests
run_test_partition_parser: src/partition_parser.c tests/test_partition_parser.c
	$(CC) $(CFLAGS) -D_GNU_SOURCE $(INC) $(CUNIT_INC) \
		-Dpopen=my_popen -Dpclose=my_pclose \
		src/partition_parser.c tests/test_partition_parser.c \
		-o $@ $(LDFLAGS) $(CUNIT_LIB)

# Image_converter tests
run_test_image_converter: src/image_converter.c tests/test_image_converter.c
	$(CC) $(CFLAGS) -D_GNU_SOURCE $(INC) $(CUNIT_INC) \
		-Dsystem=my_system -Daccess=my_access \
		src/image_converter.c tests/test_image_converter.c \
		-o $@ $(LDFLAGS) $(CUNIT_LIB)

# Loop_device tests
run_test_loop_device: src/loop_device.c tests/test_loop_device.c
	$(CC) $(CFLAGS) -D_GNU_SOURCE $(INC) $(CUNIT_INC) \
		-Dpopen=my_popen -Dpclose=my_pclose \
		src/loop_device.c tests/test_loop_device.c \
		-o $@ $(LDFLAGS) $(CUNIT_LIB)

# Mapper tests
run_test_mapper: src/mapper.c tests/test_mapper.c
	$(CC) $(CFLAGS) -D_GNU_SOURCE $(INC) $(CUNIT_INC) \
		-Dpopen=my_popen -Dpclose=my_pclose -Dsystem=my_system \
		src/mapper.c tests/test_mapper.c \
		-o $@ $(LDFLAGS) $(CUNIT_LIB)

# Mount_selector tests
run_test_mount_selector: src/mount_selector.c tests/test_mount_selector.c
	$(CC) $(CFLAGS) -D_GNU_SOURCE $(INC) $(CUNIT_INC) \
		-Dsystem=my_system -UMOUNT_POINT -DMOUNT_POINT="/tmp/mselXXXXXX" \
		src/mount_selector.c tests/test_mount_selector.c \
		-o $@ $(LDFLAGS) $(CUNIT_LIB)

# Run all tests
tests: run_test_dislocker_runner \
       run_test_partition_parser \
       run_test_image_converter \
       run_test_loop_device \
       run_test_mapper \
       run_test_mount_selector

run_tests: tests

# Execute tests sequentially
run_tests:
	@echo "Running all unit tests..."
	@./run_test_dislocker_runner || exit 1
	@./run_test_partition_parser || exit 1
	@./run_test_image_converter || exit 1
	@./run_test_loop_device || exit 1
	@./run_test_mapper || exit 1
	@./run_test_mount_selector || exit 1

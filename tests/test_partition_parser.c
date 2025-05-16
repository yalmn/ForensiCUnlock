/*
 * Unit-Tests für parse_bdp_line in partition_parser.c
 * Kompilieren mit:
 *   gcc -Wall -Wextra -std=c11 tests/test_partition_parser.c -o test_partition_parser
 * und ausführen mit:
 *   ./test_partition_parser
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

// Include implementation direkt, um auch die static parse_bdp_line() zu testen
#include "../src/partition_parser.c"

// Test-Makros
#define EXPECT_INT(actual, expected)                               \
    do                                                             \
    {                                                              \
        if ((actual) != (expected))                                \
        {                                                          \
            fprintf(stderr, "[FAIL] %s:%d: erwartet %d, war %d\n", \
                    __FILE__, __LINE__, (expected), (actual));     \
            exit(1);                                               \
        }                                                          \
    } while (0)

#define EXPECT_UINT64(actual, expected)                                \
    do                                                                 \
    {                                                                  \
        if ((actual) != (expected))                                    \
        {                                                              \
            fprintf(stderr, "[FAIL] %s:%d: erwartet %llu, war %llu\n", \
                    __FILE__, __LINE__,                                \
                    (unsigned long long)(expected),                    \
                    (unsigned long long)(actual));                     \
            exit(1);                                                   \
        }                                                              \
    } while (0)

int main(void)
{
    printf("Starte Tests für parse_bdp_line()...\n");

    // Test 1: Basic data partition
    {
        const char *line = " 2: 000002048      0000050000      Basic data partition";
        int slot;
        uint64_t start, length;
        int ok = parse_bdp_line(line, &slot, &start, &length);
        EXPECT_INT(ok, 1);
        EXPECT_INT(slot, 2);
        EXPECT_UINT64(start, 2048);
        EXPECT_UINT64(length, 50000);
    }

    // Test 2: NTFS label
    {
        const char *line = " 3: 000010000      000020000      NTFS";
        int slot;
        uint64_t start, length;
        int ok = parse_bdp_line(line, &slot, &start, &length);
        EXPECT_INT(ok, 1);
        EXPECT_INT(slot, 3);
        EXPECT_UINT64(start, 10000);
        EXPECT_UINT64(length, 20000);
    }

    // Test 3: Nicht passende Zeile
    {
        const char *line = " 1: 000000000      000000001      Linux filesystem";
        int slot;
        uint64_t start, length;
        int ok = parse_bdp_line(line, &slot, &start, &length);
        EXPECT_INT(ok, 0);
    }

    printf("[OK] Alle parse_bdp_line() Tests bestanden.\n");
    return 0;
}

#include <CUnit/Basic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "partition_parser.h"

/* Fake-Output von „parted -m -s ... unit s print“ */
static const char *fake_parted =
    "/dev/fake:xxx:xxx:xt\n"
    "1:2048s:4095s:ntfs:Some label:*\n"
    "2:4096s:8191s:ext4:Other   :\n";

/* Stubs für popen()/pclose() */
FILE *my_popen(const char *cmd, const char *mode)
{
    (void)cmd;
    (void)mode;
    return fmemopen((void *)fake_parted,
                    strlen(fake_parted),
                    "r");
}
int my_pclose(FILE *fp)
{
    return fclose(fp);
}

/* Tests */
void test_find_bdp_partition_found(void)
{
    PartitionInfo info;
/* popen überschreiben */
#undef popen
#define popen my_popen
#undef pclose
#define pclose my_pclose

    CU_ASSERT_TRUE(find_bdp_partition("ignore", &info));
    CU_ASSERT_EQUAL(info.slot, 1);
    CU_ASSERT_EQUAL(info.start, 2048);
    CU_ASSERT_EQUAL(info.length, 4095 - 2048 + 1);
}

void test_find_bdp_partition_not_found(void)
{
    /* Fake-Output ohne NTFS */
    static const char *no_ntfs =
        "/dev/fake:xxx:xxx:xt\n"
        "1:100s:200s:fat32:Label:\n";
    FILE *my_popen2(const char *, const char *)
    {
        return fmemopen((void *)no_ntfs, strlen(no_ntfs), "r");
    }
#undef popen
#define popen my_popen2
#undef pclose
#define pclose my_pclose

    PartitionInfo info;
    CU_ASSERT_FALSE(find_bdp_partition("ignore", &info));
}

int main()
{
    CU_initialize_registry();
    CU_pSuite suite = CU_add_suite("partition_parser", NULL, NULL);
    CU_add_test(suite, "Partition gefunden", test_find_bdp_partition_found);
    CU_add_test(suite, "Keine Partition", test_find_bdp_partition_not_found);
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    return CU_get_error();
}

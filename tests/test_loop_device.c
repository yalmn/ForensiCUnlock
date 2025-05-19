#include <CUnit/Basic.h>
#include <string.h>
#include <stdio.h>
#include "loop_device.h"

/* Stub für popen()/fgets()/pclose() */
static const char *fake_out = "/dev/loop42\n";
FILE *my_popen(const char *cmd, const char *mode)
{
    (void)cmd;
    (void)mode;
    return fmemopen((void *)fake_out, strlen(fake_out), "r");
}
int my_pclose(FILE *fp)
{
    return fclose(fp);
}

#define popen my_popen
#define pclose my_pclose

void test_setup_loop_device_success(void)
{
    char buf[32] = {0};
    CU_ASSERT_TRUE(setup_loop_device("/some/file", buf, sizeof(buf)));
    CU_ASSERT_STRING_EQUAL(buf, "/dev/loop42");
}

void test_setup_loop_device_fail(void)
{
/* popen schlägt fehl → NULL */
#undef popen
    FILE *my_popen2(const char *, const char *) { return NULL; }
#define popen my_popen2

    char buf[32];
    CU_ASSERT_FALSE(setup_loop_device("/some/file", buf, sizeof(buf)));
}

int main(void)
{
    CU_initialize_registry();
    CU_pSuite s = CU_add_suite("loop_device", NULL, NULL);
    CU_add_test(s, "losetup OK", test_setup_loop_device_success);
    CU_add_test(s, "losetup KO", test_setup_loop_device_fail);
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    return CU_get_error();
}

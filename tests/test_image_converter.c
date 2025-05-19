#include <CUnit/Basic.h>
#include <string.h>
#include <unistd.h>
#include "image_converter.h"

/* Stubs für system() und access() */
static int stub_system_ret = 0;
static int stub_access_ret = 0;
static char last_cmd[1024];

int my_system(const char *cmd)
{
    strncpy(last_cmd, cmd, sizeof(last_cmd) - 1);
    last_cmd[sizeof(last_cmd) - 1] = '\0';
    return stub_system_ret;
}
int my_access(const char *path, int mode)
{
    (void)mode;
    return stub_access_ret;
}

/* Makros, damit unsere Stubs benutzt werden */
#define system my_system
#define access my_access

void test_convert_ewf_success(void)
{
    stub_system_ret = 0;
    stub_access_ret = 0;
    CU_ASSERT_TRUE(convert_ewf_to_raw("/fake/dir", "/out/dir"));
    CU_ASSERT_STRING_EQUAL(
        last_cmd,
        "mkdir -p '/out/dir' && xmount --in ewf '/fake/dir'/image.E01 --out raw '/out/dir'");
}

void test_convert_ewf_no_raw(void)
{
    stub_system_ret = 0;
    stub_access_ret = -1; /* RAW-Image „nicht gefunden“ */
    CU_ASSERT_FALSE(convert_ewf_to_raw("/fake/dir", "/out/dir"));
}

int main(void)
{
    CU_initialize_registry();
    CU_pSuite s = CU_add_suite("image_converter", NULL, NULL);
    CU_add_test(s, "EWF→RAW erfolgreich", test_convert_ewf_success);
    CU_add_test(s, "RAW nicht gefunden", test_convert_ewf_no_raw);
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    return CU_get_error();
}

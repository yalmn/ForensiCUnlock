#include <CUnit/Basic.h>
#include <stdint.h>
#include <string.h>

/* Stubs für system() */
static int stub_return = 0;
static char last_cmd[1024];
int my_system(const char *cmd)
{
    strncpy(last_cmd, cmd, sizeof(last_cmd) - 1);
    last_cmd[sizeof(last_cmd) - 1] = '\0';
    return stub_return;
}

/* dislocker_runner.h */
int run_dislocker(const char *image_path, uint64_t start_sector,
                  const char *key, const char *output_path);

/* Testfälle */
void test_run_dislocker_success(void)
{
    stub_return = 0; /* System‐Call „erfolgreich“ */
    CU_ASSERT_TRUE(run_dislocker("img.bin", 123, "geheim", "/tmp/out"));
    CU_ASSERT_STRING_EQUAL(
        last_cmd,
        "mkdir -p '/tmp/out' && dislocker -V 'img.bin' -O 62976 -p'geheim' -r -- '/tmp/out'");
}

void test_run_dislocker_failure(void)
{
    stub_return = 1; /* System‐Call schlägt fehl */
    CU_ASSERT_FALSE(run_dislocker("img.bin", 0, "", "/tmp/foo"));
    CU_ASSERT_STRING_EQUAL(
        last_cmd,
        "mkdir -p '/tmp/foo' && dislocker -V 'img.bin' -O 0 -p'' -r -- '/tmp/foo'");
}

int main()
{
    CU_initialize_registry();
    CU_pSuite suite = CU_add_suite("dislocker_runner", NULL, NULL);
    CU_add_test(suite, "erfolgreicher Aufruf", test_run_dislocker_success);
    CU_add_test(suite, "fehlgeschlagener Aufruf", test_run_dislocker_failure);
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    return CU_get_error();
}

#include <CUnit/Basic.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "mapper.h"

/* Stub für popen()/fscanf()/pclose() bei blockdev */
static const char *fake_secs = "10000";
FILE *my_popen(const char *cmd, const char *mode)
{
    (void)cmd;
    (void)mode;
    return fmemopen((void *)fake_secs, strlen(fake_secs), "r");
}
int my_pclose(FILE *fp)
{
    return fclose(fp);
}
#define popen my_popen
#define pclose my_pclose

/* Stub für system() bei dmsetup */
static int stub_system_ret = 0;
static char last_cmd[600];
int my_system(const char *cmd)
{
    strncpy(last_cmd, cmd, sizeof(last_cmd) - 1);
    last_cmd[sizeof(last_cmd) - 1] = '\0';
    return stub_system_ret;
}
#define system my_system

void test_create_mapping_file_success(void)
{
    char tmpl[] = "/tmp/mymapXXXXXX";
    char *out = mkdtemp(tmpl);
    CU_ASSERT_PTR_NOT_NULL(out);

    CU_ASSERT_TRUE(create_mapping_file(
        "loop0", "loop1",
        200, 300,
        out));
    /* Datei existiert? */
    char mapf[512];
    snprintf(mapf, sizeof(mapf), "%s/dmsetup.txt", out);
    FILE *f = fopen(mapf, "r");
    CU_ASSERT_PTR_NOT_NULL(f);

    /* Lies die dritte Zeile: */
    char line[256];
    fgets(line, sizeof(line), f);
    fgets(line, sizeof(line), f);
    fgets(line, sizeof(line), f);
    CU_ASSERT_STRING_EQUAL(
        line,
        "500 9500 linear loop0 500\n");
    fclose(f);
}

void test_create_mapping_file_blockdev_fail(void)
{
/* popen liefert NULL → Fehler */
#undef popen
    FILE *my_popen2(const char *, const char *) { return NULL; }
#define popen my_popen2

    char tmpl[] = "/tmp/nomapXXXXXX";
    char *out = mkdtemp(tmpl);
    CU_ASSERT_PTR_NOT_NULL(out);
    CU_ASSERT_FALSE(create_mapping_file("a", "b", 0, 1, out));
}

void test_setup_dm_device(void)
{
    char tmpl[] = "/tmp/dmXXXXXX";
    char *out = mkdtemp(tmpl);
    CU_ASSERT_PTR_NOT_NULL(out);

    /* Erstelle Dummy-Datei, damit der Pfad stimmt */
    char mapf[512];
    snprintf(mapf, sizeof(mapf), "%s/dmsetup.txt", out);
    FILE *f = fopen(mapf, "w");
    CU_ASSERT_PTR_NOT_NULL(f);
    fclose(f);

    stub_system_ret = 0;
    CU_ASSERT_TRUE(setup_dm_device(out));
    CU_ASSERT_STRING_EQUAL(
        last_cmd,
        "dmsetup create merged < '/tmp/dmXXXXXX/dmsetup.txt'");
}

int main(void)
{
    CU_initialize_registry();
    CU_pSuite s = CU_add_suite("mapper", NULL, NULL);
    CU_add_test(s, "Mapping File OK", test_create_mapping_file_success);
    CU_add_test(s, "Mapping File Fail", test_create_mapping_file_blockdev_fail);
    CU_add_test(s, "DM Device erstellen", test_setup_dm_device);
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    return CU_get_error();
}

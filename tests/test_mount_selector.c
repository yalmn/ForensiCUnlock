#include <CUnit/Basic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "mount_selector.h"

/* Stub für system() */
static int stub_system_ret = 0;
int my_system(const char *cmd)
{
    (void)cmd;
    return stub_system_ret;
}
#define system my_system

void test_auto_mount_and_find_ewf(void)
{
    /* 1) Temp-Mountpoint anlegen */
    char tmpl[] = "/tmp/mselXXXXXX";
    char *mp = mkdtemp(tmpl);
    CU_ASSERT_PTR_NOT_NULL(mp);

    /* 2) Dummy-Unterverzeichnis mit „image.E01“ */
    char sub[512];
    snprintf(sub, sizeof(sub), "%s/splitdir", mp);
    CU_ASSERT_TRUE(mkdir(sub, 0755) == 0);
    FILE *f = fopen(strcat(sub, "/image.E01"), "w");
    CU_ASSERT_PTR_NOT_NULL(f);
    fclose(f);

    /* 3) Ausgabe-Puffer */
    char sel[512] = {0};

    /* stub: mkdir + ewfmount + Mount succeed */
    stub_system_ret = 0;
    /* Nun aufrufen – wir übergeben als „device“ einen echten Pfad mit .E01 */
    char device_path[512];
    strcpy(device_path, sub);
    strcat(device_path, "/image.E01");

    /* Unterdrücke das original MOUNT_POINT, kompiliere c mit:
     *   -DMOUNT_POINT=\"<mp>\"
     */
    CU_ASSERT_TRUE(auto_mount_and_find_ewf(device_path, sel, sizeof(sel)));
    CU_ASSERT_STRING_EQUAL(sel, sub);
}

int main(void)
{
    CU_initialize_registry();
    CU_pSuite s = CU_add_suite("mount_selector", NULL, NULL);
    CU_add_test(s, "Find EWF-Split", test_auto_mount_and_find_ewf);
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    return CU_get_error();
}

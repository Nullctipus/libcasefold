#define _XOPEN_SOURCE 500
#include "Unity/src/unity.h"

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ftw.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <linux/openat2.h>


void setUp(void) {
}

void tearDown(void) {
}

void test_open(void) {
    int rv;
    rv = open("testd/Test/atest", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = open("testd//test/atest", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = open("testd/./test/atest", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = open("testd/../testd/test/atest", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = open("testd/TEST/ATEST", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = open("testd/TEST/AST", O_RDONLY);
    TEST_ASSERT_EQUAL(rv, -1);
    rv = open("testf", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = open("tEstf", O_RDONLY);
    TEST_ASSERT_EQUAL(rv, -1);
}

void test_openat(void) {
    int dir;
    int rv;
    dir = open("testd", O_DIRECTORY | O_RDONLY);
    rv = openat(dir, "Test/atest", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = openat(dir, "test/atest", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = openat(dir,"/test/atest", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = openat(dir,"./test/atest", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = openat(dir,"../testd/test/atest", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = openat(dir,"../testF", O_RDONLY);
    TEST_ASSERT_EQUAL(rv, -1);
    rv = openat(dir, "TEST/ATEST", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = openat(dir, "TEST/AST", O_RDONLY);
    TEST_ASSERT_EQUAL(rv, -1);
    close(dir);
    dir = open(".",O_RDONLY);
    rv = openat(dir, "testf", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(rv, -1);
    close(rv);
    rv = openat(dir, "tEstf", O_RDONLY);
    TEST_ASSERT_EQUAL(rv, -1);
    close(dir);
}
void test_creat(void) {
    int fd;
    fd = creat("testd/test/newfile", 0644);
    TEST_ASSERT_NOT_EQUAL(fd, -1);
    close(fd);

    // case-insensitive
    fd = creat("testd/TEST/NEWFILE2", 0644);
    TEST_ASSERT_NOT_EQUAL(fd, -1);
    close(fd);

    // nonexistent path
    fd = creat("testd/BADDIR/FILE", 0644);
    TEST_ASSERT_EQUAL(fd, -1);
}


void test_fopen(void) {
    FILE *fp;
    fp = fopen("testd/Test/atest", "r");
    TEST_ASSERT_NOT_NULL(fp);
    fclose(fp);
    fp = fopen("testd/test/atest", "r");
    TEST_ASSERT_NOT_NULL(fp);
    fclose(fp);
    fp = fopen("testd/TEST/ATEST", "r");
    TEST_ASSERT_NOT_NULL(fp);
    fclose(fp);
    fp = fopen("testd/TEST/AST", "r");
    TEST_ASSERT_NULL(fp);
    fp = fopen("testf", "r");
    TEST_ASSERT_NOT_NULL(fp);
    fclose(fp);
    fp = fopen("tEstf", "r");
    TEST_ASSERT_NULL(fp);
}
void test_freopen(void) {
    FILE *fp = fopen("/dev/null", "w");
    TEST_ASSERT_NOT_NULL(fp);

    // Replace with a file in folded path
    FILE *fp2 = freopen("testd/test/atest", "r", fp);
    TEST_ASSERT_NOT_NULL(fp2);
    fclose(fp2);

    // Replace with bad path
    fp = fopen("/dev/null", "w");
    fp2 = freopen("testd/TEST/AST", "r", fp);
    TEST_ASSERT_NULL(fp2);
}

void test_openat_symlink(void) {
    symlink("testd", "testd_link");

    int dir = open("testd_link", O_DIRECTORY | O_RDONLY);
    int fd = openat(dir, "Test/atest", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(fd, -1);
    close(fd);
    close(dir);

    unlink("testd_link");
}
#ifdef SYSCALLS
void test_openat2(void) {
    struct open_how how = {
        .flags = O_RDONLY,
    };

    int dir = open("testd", O_DIRECTORY | O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(dir, -1);

    // Case-insensitive path under CF_WD
    int fd = syscall(SYS_openat2, dir, "Test/atest", &how, sizeof(how));
    TEST_ASSERT_NOT_EQUAL(fd, -1);
    close(fd);

    // Nonexistent file under CF_WD
    fd = syscall(SYS_openat2, dir, "TEST/AST", &how, sizeof(how));
    TEST_ASSERT_EQUAL(fd, -1);

    // Path outside CF_WD should still work (assuming /etc/passwd exists)
    fd = syscall(SYS_openat2, AT_FDCWD, "/etc/passwd", &how, sizeof(how));
    TEST_ASSERT_NOT_EQUAL(fd, -1);
    close(fd);

    close(dir);
}
void test_syscall_open_variants(void) {
    int fd;

    // --- open() via syscall ---
    fd = syscall(SYS_open, "testd/Test/atest", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(fd, -1);
    close(fd);

    // Nonexistent (should fail)
    fd = syscall(SYS_open, "testd/TEST/AST", O_RDONLY);
    TEST_ASSERT_EQUAL(fd, -1);

    // --- openat() via syscall ---
    int dir = syscall(SYS_open, "testd", O_DIRECTORY | O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(dir, -1);

    fd = syscall(SYS_openat, dir, "Test/atest", O_RDONLY);
    TEST_ASSERT_NOT_EQUAL(fd, -1);
    close(fd);

    fd = syscall(SYS_openat, dir, "TEST/AST", O_RDONLY);
    TEST_ASSERT_EQUAL(fd, -1);

    close(dir);

    // --- creat() via syscall ---
    fd = syscall(SYS_creat, "testd/Test/newfile_sys", 0644);
    TEST_ASSERT_NOT_EQUAL(fd, -1);
    close(fd);

    // Try creating in a bad directory
    fd = syscall(SYS_creat, "testd/BADDIR/newfile_sys", 0644);
    TEST_ASSERT_EQUAL(fd, -1);
}

#endif


static int deleteall(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    switch (tflag) {
        case FTW_F:
            remove(fpath);
            return 0;
        case FTW_D:
        case FTW_DNR:
        case FTW_DP:
            rmdir(fpath);
            return 0;
        default:
            printf("not expected %d\n", tflag);
            return FTW_STOP;
    }
    return 0;
}

int main(void) {
    char* preload = getenv("LD_PRELOAD");
    if (preload == NULL) {
        fprintf(stderr, "environment variable LD_PRELOAD not set\n");
        return 1;
    }
    struct stat statbuf; {
        char cwd[PATH_MAX - 10];
        char cwd2[PATH_MAX];
        getcwd(cwd, PATH_MAX - 10);
        snprintf(cwd2, PATH_MAX, "%s/testd", cwd);
        setenv("CF_WD", cwd2, 1);
    }
    int rv;
    if (stat("testd", &statbuf) != -1) {
        nftw("testd", deleteall, 8,FTW_DEPTH | FTW_PHYS);
    }
    if (stat("testf", &statbuf) != -1) {
        remove("testf");
    }
    rv = mkdir("testd", 0755);
    if (rv == -1) {
        perror("mkdir");
        exit(1);
    }
    rv = creat("testf", 0755);
    if (rv == -1) {
        perror("creat");
        exit(1);
    }
    rv = mkdir("testd/TeST", 0755);
    if (rv == -1) {
        perror("mkdir");
        exit(1);
    }
    rv = creat("testd/TeST/AtEst", 0666);
    if (rv == -1) {
        perror("creat");
        exit(1);
    }
    close(rv);
    rv = creat("testd/TeST/AtsT", 0666);
    if (rv == -1) {
        perror("creat");
        exit(1);
    }
    close(rv);
    UNITY_BEGIN();
    RUN_TEST(test_open);
    RUN_TEST(test_openat);
    RUN_TEST(test_openat_symlink);
    RUN_TEST(test_creat);
    RUN_TEST(test_fopen);
    RUN_TEST(test_freopen);
#ifdef SYSCALLS
    RUN_TEST(test_openat2);
    RUN_TEST(test_syscall_open_variants);
#endif

    rv = UNITY_END();
    if (stat("testd", &statbuf) != -1) {
        nftw("testd", deleteall, 8,FTW_DEPTH | FTW_PHYS);
    }
    if (stat("testf", &statbuf) != -1) {
        remove("testf");
    }
    return rv;
}

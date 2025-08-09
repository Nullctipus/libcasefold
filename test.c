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
#include <string.h>
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
void test_remove(void) {
    int fd;
    fd = remove("testd/test/newfilE");
    TEST_ASSERT_NOT_EQUAL(fd, -1);

    // case-insensitive
    fd = remove("testd/TEST/newfile2");
    TEST_ASSERT_NOT_EQUAL(fd, -1);

    // nonexistent path
    fd = remove("testd/BADDIR/FILE");
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
void test_readdir(void) {
    DIR *dir;

    // Open with matching case
    dir = opendir("testd/Test");
    TEST_ASSERT_NOT_NULL(dir);
    struct dirent *ent = NULL;
    int found = 0;
    while ((ent = readdir(dir))) {
        if (strcasecmp(ent->d_name, "atest") == 0) {
            found = 1;
        }
    }
    closedir(dir);
    TEST_ASSERT_TRUE(found);

    // Open with different case
    dir = opendir("testd/TEST");
    TEST_ASSERT_NOT_NULL(dir);
    closedir(dir);

    // Nonexistent
    dir = opendir("testd/NoSuchDir");
    TEST_ASSERT_NULL(dir);
}

void test_chdir(void) {
    char backup_cwd[PATH_MAX];
    getcwd(backup_cwd, PATH_MAX);
    char cwd[PATH_MAX];

    // Change dir with different case
    int rv = chdir("testd/TEST");
    TEST_ASSERT_EQUAL(0, rv);
    getcwd(cwd, sizeof(cwd));
    TEST_ASSERT_NOT_NULL(strstr(cwd, "testd/TeST")); // actual stored case

    // Back to original CF_WD
    const char *cfwd = getenv("CF_WD");
    TEST_ASSERT_EQUAL(0, chdir(cfwd));

    // Fail case
    rv = chdir("testd/NOSUCHDIR");
    TEST_ASSERT_EQUAL(-1, rv);

    chdir(backup_cwd);
}

void test_unlink(void) {
    // Create temp file
    int fd = creat("testd/Test/tempfile", 0644);
    TEST_ASSERT_NOT_EQUAL(fd, -1);
    close(fd);

    // Unlink with wrong case in path
    int rv = unlink("testd/TEST/TEMPFILE");
    TEST_ASSERT_EQUAL(0, rv);

    // Confirm it’s gone
    rv = access("testd/Test/tempfile", F_OK);
    TEST_ASSERT_EQUAL(-1, rv);
}

void test_stat(void) {
    struct stat st;

    // Existing file, wrong case
    int rv = stat("testd/TEST/ATEST", &st);
    TEST_ASSERT_EQUAL(0, rv);

    // Nonexistent
    rv = stat("testd/TEST/AST", &st);
    TEST_ASSERT_EQUAL(-1, rv);
}
void test_lstat(void) {
    struct stat st;

    // Existing file, wrong case
    int rv = lstat("testd/TEST/ATEST", &st);
    TEST_ASSERT_EQUAL(0, rv);

    // Nonexistent
    rv = lstat("testd/TEST/AST", &st);
    TEST_ASSERT_EQUAL(-1, rv);
}

void test_rename(void) {
    // Create source file
    int fd = creat("testd/Test/oldname", 0644);
    TEST_ASSERT_NOT_EQUAL(fd, -1);
    close(fd);

    // Rename with wrong case in source
    int rv = rename("testd/TEST/OLDNAME", "testd/Test/newname");
    TEST_ASSERT_EQUAL(0, rv);

    // Check new file exists
    rv = access("testd/Test/newname", F_OK);
    TEST_ASSERT_EQUAL(0, rv);
}
void test_access(void) {
    int rv = access("testd/Test/ATest",F_OK);
    TEST_ASSERT_EQUAL(0, rv);
    rv = access("testd/TEst/notexist", F_OK);
    TEST_ASSERT_NOT_EQUAL(0, rv);
}

void test_mkdir_rmdir(void) {
    int rv = mkdir("testd/Test/ATt", 0644);
    TEST_ASSERT_EQUAL(0, rv);
    rv = mkdir("testd/Tests/ATest", 0644);
    TEST_ASSERT_NOT_EQUAL(0, rv);

    rv = rmdir("testd/Tests/ATest");
    TEST_ASSERT_NOT_EQUAL(0, rv);
    rv = rmdir("testd/Test/ATt");
    TEST_ASSERT_EQUAL(0, rv);

}
void test_realpath(void) {
    char buf[PATH_MAX];
    char* rv = realpath("testd/Test/ATest", buf);
    TEST_ASSERT_NOT_NULL(rv);
    rv = realpath("testd/Tests/ATest", buf);
    TEST_ASSERT_NULL(rv);
    rv = realpath("testd/Test/ATest/testing", buf);
    TEST_ASSERT_NULL(rv);
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

    int rv;
    struct stat statbuf;
    if (stat("testd", &statbuf) != -1) {
        nftw("testd", deleteall, 8,FTW_DEPTH | FTW_PHYS);
    }
    if (stat("testf", &statbuf) != -1) {
        remove("testf");
    }
    rv = mkdir("testd", 0755);
    {
        char cwd[PATH_MAX - 10];
        char cwd2[PATH_MAX];
        getcwd(cwd, PATH_MAX - 10);
        snprintf(cwd2, PATH_MAX, "%s/testd", cwd);
        setenv("CF_WD", cwd2, 1);
    }
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
    RUN_TEST(test_remove);
    RUN_TEST(test_readdir);
    RUN_TEST(test_chdir);
    RUN_TEST(test_unlink);
    RUN_TEST(test_stat);
    RUN_TEST(test_lstat);
    RUN_TEST(test_rename);
    RUN_TEST(test_access);
    RUN_TEST(test_mkdir_rmdir);
    RUN_TEST(test_realpath);

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

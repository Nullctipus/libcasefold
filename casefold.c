#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>
static int usage(char **argv) {
    fprintf(stderr, "Usage: %s target_executable\n", argv[0]);
    return 1;
}
static const char *dll_name = DLL_NAME;
static char own_dir[PATH_MAX];
static const char *dll_dirs[] = {
    own_dir,
    LIB_DIR,
    "/lib",
    "/usr/lib",
    "/usr/local/lib",
    "/lib64",
    NULL
};
static void set_own_dir() {

    readlink("/proc/self/exe", own_dir, PATH_MAX);
    char* last_slash = strrchr(own_dir, '/');
    if (last_slash == NULL) {
        memcpy(own_dir,"/dev/null/",2);
    }else {
        *last_slash = '\0';
    }
}

int main(int argc, char **argv){
    {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        setenv("CF_WD", cwd, 0);
    }
    if (argc < 2) {
        return usage(argv);
    }
    char buff[PATH_MAX];
    char* prefix = NULL;
    set_own_dir();

    for (int i = 0; dll_dirs[i] != NULL; i++) {
        snprintf(buff, PATH_MAX, "%s/%s", dll_dirs[i], dll_name);
        if (access(buff,R_OK) != -1) {
            prefix = buff;
            break;
        }
    }
    if (prefix == NULL) {
        fprintf(stderr, "cannot find " DLL_NAME " plaase reinstall\n");
        return 1;
    }
#ifndef IS_MAC
    char* preload = getenv("LD_PRELOAD");
    if (preload == NULL) {
        setenv("LD_PRELOAD", buff, 1);
    }
    else {
        size_t len = snprintf(NULL, 0, "%s;%s", preload,buff);
        char* new_preload = malloc(len+1);
        snprintf(new_preload, len+1, "%s;%s", preload,buff);
        setenv("LD_PRELOAD", new_preload, 1);
        free(new_preload);
    }
#else
    setenv("DYLD_INSERT_LIBRARIES", buff, 1);
    setenv("DYLD_FORCE_FLAT_NAMESPACE", "1", 1);
#endif
    execvp(argv[1], &argv[1]);
    perror("casefold failed to start process");
    return 0;
}

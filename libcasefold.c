#undef _GNU_SOURCE
#define _GNU_SOURCE

#include <dirent.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>
#ifdef SYSCALLS
#include <linux/openat2.h>
#include <sys/syscall.h>
#endif


#ifdef DEBUG
#define PDBG(s,...) printf(s, ##__VA_ARGS__)
#else
#define PDBG(s,...)
#endif

#ifdef THREAD_SAFE
#include <pthread.h>
pthread_once_t init_once = PTHREAD_ONCE_INIT;
#endif


int (*true_open)(const char *path, int flags, ...);

int (*true_openat)(int dirfd, const char *path, int flags, ...);

int (*true_creat)(const char *path, mode_t mode);

FILE *(*true_fopen)(const char *path, const char *mode);

FILE *(*true_freopen)(const char *path, const char *mode, FILE *stream);
#ifdef SYSCALLS
long int (*true_syscall)(long int call, ...);
#endif
int init_l;

static void *load_sym(const char *symname, void *proxyfunc) {
    void *func_ptr = dlsym(RTLD_NEXT, symname);
    if (!func_ptr) {
        fprintf(stderr, "cannot find symbol %s\n", symname);
        exit(1);
    }
    PDBG("%s at %p\n", symname, func_ptr);
    if (proxyfunc == func_ptr) {
        PDBG("cyclical reference abort\n");
        abort();
    }
    return func_ptr;
}

#define INIT() init_lib(__FUNCTION__)

#define LOAD_SYM(sym) true_##sym = load_sym(#sym, sym)

static void do_init(void) {
    init_l = 1;
    LOAD_SYM(open);
    LOAD_SYM(openat);
    LOAD_SYM(creat);
    LOAD_SYM(fopen);
    LOAD_SYM(freopen);
#ifdef SYSCALLS
    LOAD_SYM(syscall);
#endif
    char *cf_wd = getenv("CF_WD");
    if (cf_wd == NULL) {
        char buf[PATH_MAX];
        getcwd(buf,PATH_MAX);
        setenv("CF_WD", buf, 0);
    }
}

static void init_lib(const char *caller) {
#ifndef DEBUG
    (void) caller;
#endif
    if (!init_l)
        PDBG("called from %s\n", caller);
#ifndef THREAD_SAFE
    if (init_l) return;
    do_init();
#else
    pthread_once(&init_once, do_init);
#endif
}


#if __GNUC__ > 2
__attribute__((constructor))
static void gcc_init(void) {
    INIT();
}
#endif


char *as_real_path(const char *path) {
    char *path_dup = strdup(path);
    char buf[PATH_MAX];
    if (path_dup[0] == '/') {
        buf[0] = '\0';
    } else {
        getcwd(buf,PATH_MAX);
    }
    size_t offset = strlen(buf);
    char *tok = path_dup;
    while (1) {
        char *slash = strchr(tok, '/');
        if (slash) {
            *slash = '\0';
        }
        if (tok[0] == '.') {
            switch (tok[1]) {
                case '\0':
                    break;
                case '.':
                    char *right = strrchr(buf, '/');
                    if (right) {
                        offset = right - buf;
                        *right = '\0';
                    }
                    break;
                default:
                    buf[offset] = '/';
                    offset++;
                    strncpy(buf + offset, tok,PATH_MAX - offset);
                    offset += strlen(tok);
            }
        } else if (tok[0] != '\0') {
            buf[offset] = '/';
            offset++;
            strncpy(buf + offset, tok,PATH_MAX - offset);
            offset += strlen(tok);
        }

        if (slash == NULL) {
            free(path_dup);
            return strdup(buf);
        }
        *slash = '/';
        tok = slash + 1;
    }
}

char *get_folded_path(const char *path) {
    if (path == NULL) {
        return NULL;
    }
    const char *cf_wd = getenv("CF_WD");
    size_t prefix_len = strlen(cf_wd);
    char *full_path = as_real_path(path);
    if (strncmp(cf_wd, full_path, prefix_len) != 0) {
        free(full_path);
        return NULL;
    }

    DIR *wd = opendir(getenv("CF_WD"));
    struct dirent *entry = NULL;
    char *tok = full_path + prefix_len + 1;
    char *slash = NULL;
    while (1) {
        if (slash) {
            *slash = '/';
            tok = slash + 1;
        }
        slash = strchr(tok, '/');
        if (slash) {
            *slash = '\0';
        }
        size_t tok_len = strlen(tok);
        if (tok_len == 0) {
            continue;
        }

        while ((entry = readdir(wd))) {
            char *dirname = entry->d_name;
            (void) dirname;
            if (strcasecmp(entry->d_name, tok) == 0) {
                break;
            }
        }
        closedir(wd);
        if (entry == NULL) {
            if (slash) {
                *slash = '/';
            }
            return full_path;
        }
        memcpy(tok, entry->d_name, tok_len);
        if (slash == NULL) {
            return full_path;
        }
        wd = opendir(full_path);
    }
}

bool dirfd_path(char (*buf)[4096], const int fd, const char *path) {
    char proc_path[64];
    snprintf(proc_path, 64, "/proc/self/fd/%d", fd);
    ssize_t len = readlink(proc_path, *buf,PATH_MAX);
    if (len == -1) {
#ifdef DEBUG
        perror("readlink");
#endif

        return false;
    }
    (*buf)[len] = '/';
    strncpy(*buf + len + 1, path,PATH_MAX - len - 1);
    return true;
}

int open(const char *path, int flags, ...) {
    PDBG("open %s\n", path);
    va_list ap;
    va_start(ap, flags);
    void *arg = va_arg(ap, void*);
    va_end(ap);
    char *folded = get_folded_path(path);
    if (folded == NULL) {
        return true_open(path, flags, arg);
    } else {
        PDBG("folded to %s\n", folded);
        int fd = true_open(folded, flags, arg);
        free(folded);
        return fd;
    }
}

int openat(int dirfd, const char *path, int flags, ...) {
    PDBG("openat %s\n", path);
    va_list ap;
    va_start(ap, flags);
    void *arg = va_arg(ap, void*);
    va_end(ap);
    char buf[PATH_MAX];
    if (!dirfd_path(&buf, dirfd, path)) return true_openat(dirfd, path, flags, arg);
    char *folded = get_folded_path(buf);
    if (folded == NULL) {
        return true_openat(dirfd, path, flags, arg);
    } else {
        PDBG("folded to %s\n", folded);
        int fd = true_open(folded, flags, arg);
        free(folded);
        return fd;
    }
}

int creat(const char *path, mode_t mode) {
    PDBG("creat %s\n", path);
    char *folded = get_folded_path(path);
    if (folded == NULL) {
        return true_creat(path, mode);
    } else {
        PDBG("folded to %s\n", folded);
        int fd = true_creat(folded, mode);
        free(folded);
        return fd;
    }
}

FILE *fopen(const char *path, const char *mode) {
    PDBG("fopen %s\n", path);
    char *folded = get_folded_path(path);
    if (folded == NULL) {
        return true_fopen(path, mode);
    } else {
        PDBG("folded to %s\n", folded);
        FILE *fd = true_fopen(folded, mode);
        free(folded);
        return fd;
    }
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
    PDBG("freopen %s\n", path);
    char *folded = get_folded_path(path);
    if (folded == NULL) {
        return true_freopen(path, mode, stream);
    } else {
        PDBG("folded to %s\n", folded);
        FILE *fd = true_freopen(folded, mode, stream);
        free(folded);
        return fd;
    }
}

#ifdef SYSCALLS
long int syscall(long int call, ...) {
    va_list ap;
    size_t a, b, c, d, e, f;
    va_start(ap, call);
    a = va_arg(ap, size_t);
    b = va_arg(ap, size_t);
    c = va_arg(ap, size_t);
    d = va_arg(ap, size_t);
    e = va_arg(ap, size_t);
    f = va_arg(ap, size_t);
    va_end(ap);
    switch (call) {
        case SYS_open:
        case SYS_creat: {
            char *folded = get_folded_path((char*)a);
            if (folded == NULL) {
                return true_syscall(call, a, b, c, d, e, f);
            } else {
                PDBG("folded to %s\n", folded);
                int fd = true_syscall(call, folded, b, c, d, e, f);
                free(folded);
                return fd;
            }
        }
        case SYS_openat:
        case SYS_openat2: {
            PDBG("call SYS_openat(2) %s \n", (char*)b);
            char buf[4096];
            if (!dirfd_path(&buf, (int) a, (char *) b)) return true_syscall(call, a, b, c, d, e, f);

            char *folded = get_folded_path(buf);
            char *cw = getenv("CF_WD");
            size_t cw_len = strlen(cw);
            bool subdir = strncmp(folded, cw, cw_len) == 0;
            if (folded == NULL || !subdir) {
                return true_syscall(call, a, b, c, d, e, f);
            } else {
                PDBG("folded to %s\n", folded);
                char *sub = folded + cw_len + 1;
                long int ret = true_syscall(call, a, sub, c, d, e, f);
                free(folded);
                return ret;
            }
        }
        default:
            return true_syscall(call, a, b, c, d, e, f);
    }
}
#endif

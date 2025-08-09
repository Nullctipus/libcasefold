#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef SYSCALLS
#include <linux/openat2.h>
#include <sys/syscall.h>
#endif

#ifdef DEBUG
#define PDBG(s, ...) printf(s, ##__VA_ARGS__)
#else
#define PDBG(s, ...)
#endif

#ifdef THREAD_SAFE
#include <pthread.h>
pthread_once_t init_once = PTHREAD_ONCE_INIT;
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

int (*true_open)(const char *path, int flags, ...);

int (*true_openat)(int dirfd, const char *path, int flags, ...);

int (*true_creat)(const char *path, mode_t mode);

int (*true_remove)(const char *path);

FILE *(*true_fopen)(const char *path, const char *mode);

FILE *(*true_freopen)(const char *path, const char *mode, FILE *stream);

DIR *(*true_opendir)(const char *path);

int (*true_chdir)(const char *path);

int (*true_access)(const char *path, int type);

int (*true_stat)(const char *path, struct stat *buf);

int (*true_lstat)(const char *path, struct stat *buf);

int (*true_rename)(const char *old, const char *new);

int (*true_mkdir)(const char *path, int mode);

int (*true_rmdir)(const char *path);

int (*true_link)(const char *from, const char *to);

int (*true_symlink)(const char *from, const char *to);

int (*true_unlink)(const char *path);

ssize_t (*true_readlink)(const char *path, char *buf, size_t bufsiz);

char *(*true_realpath)(const char *path, char *buf);

int (*true_chmod)(const char *path, mode_t mode);

int (*true_statx)(int fd, const char *path, int flags, unsigned int mask,
                  struct statx *buf);

#ifdef SYSCALLS
long int (*true_syscall)(long int call, ...);
#endif
static void do_init(void) {
  init_l = 1;
  LOAD_SYM(open);
  LOAD_SYM(openat);
  LOAD_SYM(creat);
  LOAD_SYM(remove);
  LOAD_SYM(fopen);
  LOAD_SYM(freopen);
  LOAD_SYM(opendir);
  LOAD_SYM(chdir);
  LOAD_SYM(access);
  LOAD_SYM(stat);
  LOAD_SYM(lstat);
  LOAD_SYM(rename);
  LOAD_SYM(mkdir);
  LOAD_SYM(rmdir);
  LOAD_SYM(link);
  LOAD_SYM(symlink);
  LOAD_SYM(unlink);
  LOAD_SYM(readlink);
  LOAD_SYM(realpath);
  LOAD_SYM(chmod);
  LOAD_SYM(statx);
#ifdef SYSCALLS
  LOAD_SYM(syscall);
#endif
  char *cf_wd = getenv("CF_WD");
  if (cf_wd == NULL) {
    char buf[PATH_MAX];
    getcwd(buf, PATH_MAX);
    setenv("CF_WD", buf, 0);
  }
}

static void init_lib(const char *caller) {
#ifndef DEBUG
  (void)caller;
#endif
  if (!init_l)
    PDBG("called from %s\n", caller);
#ifndef THREAD_SAFE
  if (init_l)
    return;
  do_init();
#else
  pthread_once(&init_once, do_init);
#endif
}

#if __GNUC__ > 2
__attribute__((constructor)) static void gcc_init(void) { INIT(); }
#endif

static char *as_real_path(const char *path) {
  char *path_dup = strdup(path);
  char buf[PATH_MAX];
  if (path_dup[0] == '/') {
    buf[0] = '\0';
  } else {
    getcwd(buf, PATH_MAX);
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
        strncpy(buf + offset, tok, PATH_MAX - offset);
        offset += strlen(tok);
      }
    } else if (tok[0] != '\0') {
      buf[offset] = '/';
      offset++;
      strncpy(buf + offset, tok, PATH_MAX - offset);
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

static char *get_folded_path(const char *path) {
  if (path == NULL || *path == '\0') {
    return NULL;
  }
  const char *cf_wd = getenv("CF_WD");
  size_t prefix_len = strlen(cf_wd);
  char *full_path = as_real_path(path);
  if (strncmp(cf_wd, full_path, prefix_len) != 0) {
    free(full_path);
    return NULL;
  }

  DIR *wd = true_opendir(cf_wd);

  struct dirent *entry = NULL;
  char *tok = full_path + prefix_len + 1;
  char *slash = NULL;
  if (strlen(tok) == 0) {
    closedir(wd);
    return full_path;
  }
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
      (void)dirname;
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
    wd = true_opendir(full_path);
    if (wd == NULL) {
      free(full_path);
      return NULL;
    }
  }
}

#define SIMPLE_OVERLOAD(return_type, name)                                     \
  return_type name(const char *path) {                                         \
    INIT();                                                                    \
    PDBG(#name " %s\n", path);                                                 \
    char *folded = get_folded_path(path);                                      \
    if (folded == NULL) {                                                      \
      return_type rv = true_##name(path);                                      \
      PDBG("returned %ld\n", (size_t)rv);                                      \
      return rv;                                                               \
    }                                                                          \
    PDBG("folded to %s\n", folded);                                            \
    return_type fd = true_##name(folded);                                      \
    free(folded);                                                              \
    return fd;                                                                 \
  }

#define SIMPLE_OVERLOAD1(return_type, name, args, passthrough)                 \
  return_type name(const char *path, args passthrough) {                       \
    INIT();                                                                    \
    PDBG(#name " %s\n", path);                                                 \
    char *folded = get_folded_path(path);                                      \
    if (folded == NULL) {                                                      \
      return_type rv = true_##name(path, passthrough);                         \
      PDBG("returned %ld\n", (size_t)rv);                                      \
      return rv;                                                               \
    }                                                                          \
    PDBG("folded to %s\n", folded);                                            \
    return_type fd = true_##name(folded, passthrough);                         \
    free(folded);                                                              \
    return fd;                                                                 \
  }
#define SIMPLE_OVERLOAD2(return_type, name, arg0, arg1, pass0, pass1)          \
  return_type name(const char *path, arg0 pass0, arg1 pass1) {                 \
    INIT();                                                                    \
    PDBG(#name " %s\n", path);                                                 \
    char *folded = get_folded_path(path);                                      \
    if (folded == NULL) {                                                      \
      return_type rv = true_##name(path, pass0, pass1);                        \
      PDBG("returned %ld\n", (size_t)rv);                                      \
      return rv;                                                               \
    }                                                                          \
    PDBG("folded to %s\n", folded);                                            \
    return_type fd = true_##name(folded, pass0, pass1);                        \
    free(folded);                                                              \
    return fd;                                                                 \
  }

static bool dirfd_path(char (*buf)[PATH_MAX], const int fd, const char *path) {
  ssize_t len = -1;
  if (fd == AT_FDCWD) {
    getcwd((char *)buf, PATH_MAX);
    len = (ssize_t)strlen((const char *)buf);
  } else {
    char proc_path[64];
    snprintf(proc_path, 64, "/proc/self/fd/%d", fd);
    len = true_readlink(proc_path, *buf, PATH_MAX);
    if (len == -1) {
#ifdef DEBUG
      perror("readlink");
#endif

      return false;
    }
  }

  (*buf)[len] = '/';
  strncpy(*buf + len + 1, path, PATH_MAX - len - 1);
  return true;
}

SIMPLE_OVERLOAD(DIR *, opendir)
SIMPLE_OVERLOAD(int, chdir)
SIMPLE_OVERLOAD(int, remove)
SIMPLE_OVERLOAD(int, rmdir)
SIMPLE_OVERLOAD(int, unlink);

SIMPLE_OVERLOAD1(FILE *, fopen, const char *, mode)
SIMPLE_OVERLOAD1(int, access, int, mode);
SIMPLE_OVERLOAD1(int, creat, mode_t, mode)
SIMPLE_OVERLOAD1(int, chmod, mode_t, mode)
SIMPLE_OVERLOAD1(int, lstat, struct stat *, buf)
SIMPLE_OVERLOAD1(int, stat, struct stat *, buf)
SIMPLE_OVERLOAD1(int, mkdir, mode_t, mode)
SIMPLE_OVERLOAD1(char *, realpath, char *, buf);

SIMPLE_OVERLOAD2(FILE *, freopen, const char *, FILE *, mode, stream)
SIMPLE_OVERLOAD2(ssize_t, readlink, char *, size_t, buf, bufsize)

int open(const char *path, int flags, ...) {
  INIT();
  PDBG("open %s\n", path);
  va_list ap;
  va_start(ap, flags);
  void *arg = va_arg(ap, void *);
  va_end(ap);
  char *folded = get_folded_path(path);
  if (folded == NULL) {
    int fd = true_open(path, flags, arg);
    PDBG("returned %d\n", fd);
    return fd;
  }
  PDBG("folded to %s\n", folded);
  int fd = true_open(folded, flags, arg);
  free(folded);
  return fd;
}

int openat(int dirfd, const char *path, int flags, ...) {
  INIT();
  PDBG("openat %s\n", path);
  va_list ap;
  va_start(ap, flags);
  void *arg = va_arg(ap, void *);
  va_end(ap);
  char buf[PATH_MAX];
  if (!dirfd_path(&buf, dirfd, path))
    return true_openat(dirfd, path, flags, arg);
  char *folded = get_folded_path(buf);
  if (folded == NULL) {
    int fd = true_openat(dirfd, path, flags, arg);
    PDBG("returned %d\n", fd);
    return fd;
  }
  PDBG("folded to %s\n", folded);
  int fd = true_open(folded, flags, arg);
  free(folded);
  return fd;
}

static int twopath(int (*linker)(const char *path1, const char *path2),
                   const char *oldpath, const char *newpath) {
  char *oldfolded = get_folded_path(oldpath);
  char *newfolded = get_folded_path(newpath);
  if (oldfolded == NULL) {
    if (newfolded == NULL) {
      return linker(oldpath, newpath);
    }
    int rv = linker(oldpath, newfolded);
    free(newfolded);
    return rv;
  }
  if (newfolded == NULL) {
    int rv = linker(oldfolded, newpath);
    free(oldfolded);
    return rv;
  }
  int rv = linker(oldfolded, newfolded);
  free(oldfolded);
  free(newfolded);
  return rv;
}

int link(const char *oldpath, const char *newpath) {
  INIT();
  PDBG("link %s %s\n", oldpath, newpath);
  return twopath(true_link, oldpath, newpath);
}

int symlink(const char *oldpath, const char *newpath) {
  INIT();
  PDBG("symlink %s %s\n", oldpath, newpath);
  return twopath(true_symlink, oldpath, newpath);
}

int rename(const char *oldpath, const char *newpath) {
  INIT();
  PDBG("rename %s %s\n", oldpath, newpath);
  return twopath(true_rename, oldpath, newpath);
}

int statx(int fd, const char *path, int flags, unsigned int mask,
          struct statx *statbuf) {
  INIT();
  PDBG("statx %s\n", path);
  char buf[PATH_MAX];
  if (!dirfd_path(&buf, fd, path))
    return true_statx(fd, path, flags, mask, statbuf);
  char *folded = get_folded_path(buf);
  if (folded == NULL) {
    int rv = true_statx(fd, path, flags, mask, statbuf);
    PDBG("returned %d\n", rv);
    return rv;
  }
  char *cf_wd = getenv("CF_WD");
  size_t len = strlen(cf_wd);
  char *sub = folded + len + 1;
  int dfd = true_open(cf_wd, O_DIRECTORY | O_RDONLY);

  PDBG("folded to %s\n", folded);
  int ret = true_statx(dfd, sub, flags, mask, statbuf);
  free(folded);
  close(dfd);
  return ret;
}

#ifdef SYSCALLS
long int syscall(long int call, ...) {
  INIT();
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
    char *folded = get_folded_path((char *)a);
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
    PDBG("call SYS_openat(2) %s \n", (char *)b);
    char buf[4096];
    if (!dirfd_path(&buf, (int)a, (char *)b))
      return true_syscall(call, a, b, c, d, e, f);

    char *folded = get_folded_path(buf);
    char *cw = getenv("CF_WD");
    size_t cw_len = strlen(cw);
    if (folded == NULL || strncmp(folded, cw, cw_len) != 0) {
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

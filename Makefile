
prefix = $(HOME)/.local
includedir = $(prefix)/include
libdir = $(prefix)/lib
confdir = $(prefix)/etc
datadir = $(prefix)/share

exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin

CCFLAGS = -MD -Wall -O0 -g -std=c99 -D_GNU_SOURCE -pipe -DTHREAD_SAFE -Werror -DDEBUG
LDFLAGS = -shared  -fPIC
INC =
PIC = -fPIC

LD_SET_SONAME = -Wl
INSTALL_FLAGS = -D -m

-include config.mak

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDSO_SUFFIX = so
endif
ifeq ($(UNAME_S),Darwin)
    LDSO_SUFFIX = dylib
endif

LDSO_PATHNAME = libcasefold.$(LDSO_SUFFIX)


SHARED_LIBS = $(LDSO_PATHNAME)

CCFLAGS+=$(USER_CFLAGS) $(OS_CFLAGS)
LDFLAGS+=$(USER_LDFLAGS) $(OS_LDFLAGS)
CXXFLAGS+=$(CCFLAGS) $(USER_CFLAGS) $(OS_CFLAGS)
CFLAGS_MAIN=-DLIB_DIR=\"$(libdir)\" -DINSTALL_PREFIX=\"$(prefix)\" -DDLL_NAME=\"$(LDSO_PATHNAME)\" -DSYSCONFDIR=\"$(confdir)\"

all: test $(LDSO_PATHNAME) casefold

install:
	install -d $(DESTDIR)/$(libdir) $(DESTDIR)/$(bindir)
	install $(INSTALL_FLAGS) 755 casefold $(DESTDIR)/$(bindir)
	install $(INSTALL_FLAGS) 644 $(LDSO_PATHNAME) $(DESTDIR)/$(libdir)

clean:
	rm -f unity.o unity.d libcasefold.o libcasefold.d test.o test.d casefold.o casefold.d

%.o: %.c
	$(CC) $(CCFLAGS) $(CFLAGS_MAIN) $(INC) $(PIC) -c -o $@ $<

$(LDSO_PATHNAME): libcasefold.o
	$(CC) $(LDFLAGS) -o $@ libcasefold.o

casefold: casefold.o
	$(CC) casefold.o -o casefold

test: unity.o test.o
	$(CC) test.o unity.o -o test

run-test : test $(LDSO_PATHNAME) casefold
	./casefold ./test

unity.o: Unity/src/unity.c
	$(CC) $(CCFLAGS) $(CFLAGS_MAIN) $(PIC) -c -o $@ $<

.PHONY: all clean install test run-test

-include unity.d

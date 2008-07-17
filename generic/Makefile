# This is a temporary Makefile, better one being part of milestone 4

ifeq ($(shell uname -s),Darwin)
    DEFS                = -D_INT64_TYPE -D_LONGLONG_TYPE -D_ILP32
    INCLUDES            = -I./darwin
    SHLIB_CFLAGS        = -dynamiclib
    SHLIB_SUFFIX        = .dylib
else
    INCLUDES            = -I./solaris
    SHLIB_CFLAGS        = -Kpic -G -z text -z defs
    SHLIB_SUFFIX        = .so
endif

CC                      = cc
CFLAGS                  = ${SHLIB_CFLAGS} -g -DUSE_TCL_STUBS ${DEFS} ${INCLUDES}
SHLIB_LD_LIBS           = -ldtrace -ltclstub8.4 -lc

all: dtrace${SHLIB_SUFFIX}

dtrace${SHLIB_SUFFIX}: dtrace.c dtrace.h
	${CC} ${CFLAGS} -o $@ $< ${SHLIB_LD_LIBS}

clean:
	rm -rf dtrace${SHLIB_SUFFIX}*

test: dtrace${SHLIB_SUFFIX}
	echo "load dtrace${SHLIB_SUFFIX}; source ../tests/dtrace.test" | \
	TCLTEST_OPTIONS="${TCLTEST_OPTIONS}" tclsh

.PHONY: all clean test

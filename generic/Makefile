# This is a temporary Makefile, better one being part of milestone 4

ifeq ($(shell uname -s),Darwin)
    DEFS                = -D_INT64_TYPE -D_LONGLONG_TYPE -D_ILP32
    INCLUDES            = -I../darwin
    SHLIB_CFLAGS        = -dynamiclib
    SHLIB_SUFFIX        = .dylib
else
    INCLUDES            = -I../solaris
    SHLIB_CFLAGS        = -Kpic -G -z text -z defs
    SHLIB_SUFFIX        = .so
endif

CC                      = cc
DEFS                    += -DUSE_TCL_STUBS -DTCL_THREADS -DHAVE_STRING_H \
                           -DPACKAGE_NAME=\"dtrace\" -DPACKAGE_VERSION=\"1.0\"
CFLAGS                  = -g ${SHLIB_CFLAGS} ${DEFS} -I. -I- ${INCLUDES}
SHLIB_LD_LIBS           = -ldtrace -ltclstub8.4 -lc -lproc

all: dtrace${SHLIB_SUFFIX}

dtrace${SHLIB_SUFFIX}: dtrace.c dtrace.h
	${CC} ${CFLAGS} -o $@ $< ${SHLIB_LD_LIBS}

clean:
	rm -rf dtrace${SHLIB_SUFFIX}*

test: dtrace${SHLIB_SUFFIX}
	TCLLIBPATH="." TCLTEST_OPTIONS="${TCLTEST_OPTIONS}" \
	tclsh ../tests/all.tcl

.PHONY: all clean test

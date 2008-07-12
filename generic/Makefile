# This is temporary Makefile, better one being part of milestone 4

dtrace.so: dtrace.c dtrace.h
	cc -Kpic -G -g -z text -z defs  -DUSE_TCL_STUBS -o dtrace.so dtrace.c -L/usr/lib -ldtrace -ltclstub8.4 -lc

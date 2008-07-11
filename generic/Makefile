# This is temporary Makefile, better one being part of milestone 4

dtrace.so: dtrace.c dtrace.h
	cc -Kpic -O -G -g -z text  -o dtrace.so dtrace.c -L/usr/lib -ltclstub8.4 -lproc -lctf -lelf -ldtrace -DUSE_TCL_STUBS

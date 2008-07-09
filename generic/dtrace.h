#ifndef __DTRACE_H
#define __DTRACE_H

#define PACKAGE_VERSION	"1.0"
#define NS		"dtrace"
#define ERROR_CLASS 	"DTRACE"

#define MAX_HANDLES	32

#include <dtrace.h>
#include <libproc.h>
#include <tcl.h>

int handles_count = 0;
dtrace_hdl_t *handles [MAX_HANDLES];

extern Tcl_Namespace* Tcl_CreateNamespace(Tcl_Interp*, const char*, ClientData, 
		Tcl_NamespaceDeleteProc*);

#endif /* __DTRACE_H */

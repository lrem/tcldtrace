#ifndef __DTRACE_H
#define __DTRACE_H

#define PACKAGE_VERSION "1.0"
#define NS "dtrace"

#include <dtrace.h>
#include <libproc.h>
#include <tcl.h>

extern Tcl_Namespace* Tcl_CreateNamespace(Tcl_Interp*, const char*, ClientData, 
		Tcl_NamespaceDeleteProc*);

#endif /* __DTRACE_H */

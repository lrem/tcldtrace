#include <strings.h>
#include "dtrace.h"

char *index_to_handle (const int index)
{
	static char buf[16];
	snprintf(buf, 16, "dtrace_handle%d", index);
	return buf;
}

int handle_to_index (const char *handle)
{
	return atoi(handle + 13);
}

int Open (ClientData cd, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
	if(handles_count == MAX_HANDLES)
	{
		Tcl_AppendResult(interp, "::dtrace::open max handles reached",
				NULL);
		Tcl_SetErrorCode(interp, ERROR_CLASS, "MAX_HANDLES", NULL);
		return TCL_ERROR;
	}

	int flags = 0;
	int error;

	if(objc > 1)
	{
		int i = objc - 1;
		if(strcmp(Tcl_GetString(objv[i]), "0") == 0)
		{
			flags |= DTRACE_O_NODEV;
		}
	}

	handles[handles_count] = dtrace_open(DTRACE_VERSION, flags, &error);

	if(handles[handles_count] == NULL)
	{
		Tcl_AppendResult(interp, "::dtrace::open libdtrace error: ",
				dtrace_errmsg(NULL, error), NULL);
		char errnum[16];
		snprintf(errnum, 16, "%d", error);
		Tcl_SetErrorCode(interp, ERROR_CLASS, "LIB", errnum, NULL);
		return TCL_ERROR;
	}

	Tcl_SetResult(interp, index_to_handle(handles_count), TCL_VOLATILE);
	handles_count++;

	return TCL_OK;
}

int Close (ClientData cd, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
	int index = handle_to_index(Tcl_GetString(objv[1]));

	if(index < 0 || handles[index] == NULL)
	{
		Tcl_AppendResult(interp, "::dtrace::close bad handle", NULL);
		Tcl_SetErrorCode(interp, ERROR_CLASS, "HANDLE", NULL);
		return TCL_ERROR;
	}

	dtrace_close(handles[index]);
	handles[index] = NULL;

	return TCL_OK;
}

int Dtrace_Init (Tcl_Interp *interp)
{
	Tcl_Namespace *namespace;
	int major;
	int minor;

	/* As I understood it, Tcl_InitStubs is to make the extension linkable
	 * against any version of interpreter that supports it. (Not only 
	 * against the binary available at compile time).
	 */
	if (Tcl_InitStubs(interp, "8.4", 0) == NULL 
			|| Tcl_PkgRequire(interp, "Tcl", "8.4", 0) == NULL) 
	{
		return TCL_ERROR;
	}

	if (Tcl_PkgProvide(interp, "dtrace", PACKAGE_VERSION) != TCL_OK) {
		return TCL_ERROR;
	}

	namespace = Tcl_CreateNamespace(interp, NS, NULL, NULL);
	if(namespace == NULL)
		return TCL_ERROR;

	Tcl_CreateObjCommand(interp, NS "::open", (Tcl_ObjCmdProc *) Open,
			(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	Tcl_CreateObjCommand(interp, NS "::close", (Tcl_ObjCmdProc *) Close,
			(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	Tcl_GetVersion(&major, &minor, NULL, NULL);
	if(8 <= major && 5 <= minor)
	{
		/* Tcl_Export(interp, namespace, "*", 0);
		Tcl_CreateEnsemble(interp, NS, namespace, 0); */
		Tcl_Eval(interp, "namespace eval " NS " {\n"
				"namespace export *\n"
				"namespace ensemble create\n"
				"}");
	}

	return TCL_OK;
}

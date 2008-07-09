#include "dtrace.h"

int Open (ClientData cd, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
	printf("hi!\n");

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

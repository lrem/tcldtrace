/* Copyright header {{{
 * Copyright (c) 2008, Remigiusz 'lRem' Modrzejewski
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Remigiusz Modrzejewski nor the
 *       names of his contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Remigiusz Modrzejewski ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Remigiusz Modrzejewski BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 * }}} */

#include <strings.h>
#include "dtrace.h"

/* get_option {{{
 *
 *	Gets the value of a given option.
 *
 * Results:
 *	The option value as a null terminated string.
 *
 * Side effects:
 *	None.
 */

static char *get_option (
	handle_data *hd, 
	const char *option) 
{
    static char value[12];

    if (internal_option(option)) {
	/* Only one defined for now. */
	snprintf(value, 12, "%d", hd->options.foldpdesc);
    } 
    else {
	dtrace_optval_t opt;

	if (dtrace_getopt(hd->handle, option+1, &opt) != 0) {
	    return NULL;
	}
	if (opt == DTRACEOPT_UNSET) {
	    opt = 0;
	}
	snprintf(value, 12, "%d", opt);
    }
    return value;
}
/*}}}*/

/* set_option {{{
 *
 *	Sets an option to the given value.
 *
 * Results:
 *	1 on success, 0 on failure.
 *
 * Side effects:
 *	The option is changed in the extension state variables or passed
 *	to libdtrace.
 */

static int set_option (
	handle_data *hd, 
	const char *option, 
	const char *value)
{
    if (internal_option(option)) {
	/* Only one defined for now. */
	if (value[0] == '0' && value[1] == 0) {
	    hd->options.foldpdesc = 0;
	}
	else {
	    hd->options.foldpdesc = 1;
	}
    } 
    else if (dtrace_setopt(hd->handle, option+1, value) != 0) {
	return 0;
    }
    return 1;
}
/*}}}*/

/* get_hd {{{
 *
 *	Given a Tcl-level handle, gets the libdtrace handle and options,
 *	from the interpreter associated hash table.
 *
 * Results:
 *	A pointer to handle_data structure on succes.
 *	NULL on failure.
 *
 * Side effects:
 *	None.
 */

static handle_data *get_hd (
	Tcl_Interp *interp, 
	Tcl_Obj *__id)
{
    int _id;
    char *id;
    Tcl_HashTable *htable;
    Tcl_HashEntry *hentry;

    if (Tcl_GetIntFromObj(interp, __id, &_id) != TCL_OK) {
	return NULL;
    }
    id = (char*)(intptr_t) _id;
    htable = Tcl_GetAssocData(interp, EXTENSION_NAME, NULL);
    if (htable == NULL) {
	Tcl_Panic(EXTENSION_NAME " hash table not found");
    }
    hentry = Tcl_FindHashEntry(htable, id);
    if (hentry == NULL) {
	return NULL;
    }
    return Tcl_GetHashValue(hentry);
}
/*}}}*/

/* del_hd {{{
 *
 *	Delete the hash entry and free the memory taken by a handle_data
 *	structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Hash entry is deleted.
 *	Memory of handle_data (without the dtrace_hdl_t handle) is freed.
 */

static void del_hd (
	Tcl_Interp *interp, 
	Tcl_Obj *__id)
{
    int _id;
    char *id;
    Tcl_HashTable *htable;
    Tcl_HashEntry *hentry;

    Tcl_GetIntFromObj(interp, __id, &_id);
    id = (char*)(intptr_t) _id;
    htable = Tcl_GetAssocData(interp, EXTENSION_NAME, NULL);
    if (htable == NULL) {
	Tcl_Panic(EXTENSION_NAME " hash table not found");
    }
    hentry = Tcl_FindHashEntry(htable, id);
    if (hentry == NULL) {
	Tcl_Panic(EXTENSION_NAME " hash entry to be deleted not found");
    }
    ckfree(Tcl_GetHashValue(hentry));
    Tcl_DeleteHashEntry(hentry);
}
/*}}}*/

/* new_hd {{{
 *
 *	Registers a new handle_data structure.
 *
 * Results:
 *	A pointer to handle_data on success.
 *	NULL on failure.
 *
 * Side effects:
 *	Memory is ckalloc'ed.
 *	Hash entry is inserted.
 *	The static next_free_id is incremented in a thread-safe way.
 */

static handle_data *new_hd (
	Tcl_Interp *interp, 
	int *id)
{
    Tcl_HashTable *htable = Tcl_GetAssocData(interp, EXTENSION_NAME, NULL);
    Tcl_HashEntry *hentry;
    handle_data *hd;
    int isNew;

    if (htable == NULL) {
	Tcl_Panic(EXTENSION_NAME " hash table not found");
    }

    Tcl_MutexLock(idMutex);
    *id = next_free_id;
    next_free_id++;
    Tcl_MutexUnlock(idMutex);

    hd = (handle_data*) ckalloc(sizeof(handle_data));
    hentry = Tcl_CreateHashEntry(htable, (char*) *id, &isNew);
    if (!isNew) {
	Tcl_Panic(EXTENSION_NAME " duplicate hash table entry");
    }
    Tcl_SetHashValue(hentry, hd);
    return hd;
}
/*}}}*/

/* Open {{{
 *
 *	Implements the ::dtrace::open command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Handle data registered into interpreter associated hash table.
 *	A dtrace handle is opened.
 *	Initial options are set.
 */

static int Open (
	ClientData cd, 
	Tcl_Interp *interp, 
	int objc, 
	Tcl_Obj *const objv[]) 
{
    int flags = 0;
    int error; 
    int id;
    int i;
    handle_data *hd;

    if (objc > 1 && objc % 2 == 0) {
	int instrument;

	if (Tcl_GetBooleanFromObj(interp, objv[objc-1], &instrument)
		!= TCL_OK) {
	    Tcl_AppendResult(interp, "\n", COMMAND, " bad usage", NULL);
	    Tcl_SetErrorCode(interp, ERROR_CLASS, "USAGE", NULL);
	    return TCL_ERROR;
	}

	if (!instrument) {
	    flags |= DTRACE_O_NODEV;
	}
    }

    hd = new_hd(interp, &id);
    if (hd == NULL) {
	Tcl_AppendResult(interp, COMMAND, " problems creating a handle ",
		"- bug or OOM", NULL);
	return TCL_ERROR;
    }
    hd->handle = dtrace_open(DTRACE_VERSION, flags, &error);

    if (hd->handle == NULL) {
	char errnum[16];

	Tcl_AppendResult(interp, COMMAND, " libdtrace error: ",
		dtrace_errmsg(NULL, error), NULL);
	snprintf(errnum, 16, "%d", error);
	Tcl_SetErrorCode(interp, ERROR_CLASS, "LIB", errnum, NULL);
	return TCL_ERROR;
    }

    for (i = 1; i < objc - 1; i+=2) {
	if (set_option(hd, Tcl_GetString(objv[i]),
		Tcl_GetString(objv[i+1])) == 0) {
	    Tcl_AppendResult(interp, COMMAND, " bad option initialization ",
		    Tcl_GetString(objv[i]), NULL);
	    Tcl_SetErrorCode(interp, ERROR_CLASS, "OPTION", NULL);
	    return TCL_ERROR;
	}
    }

    Tcl_SetObjResult(interp, Tcl_NewIntObj(id));

    return TCL_OK;
}
/*}}}*/

/* Close {{{
 *
 *     Implements the ::dtrace::close command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Dtrace handle gets closed.
 *	Handle data unregistered from interpreter associated hash table.
 */

static int Close (
	ClientData cd, 
	Tcl_Interp *interp, 
	int objc, 
	Tcl_Obj *const objv[]) 
{
    handle_data *hd;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "handle");
	Tcl_SetErrorCode(interp, ERROR_CLASS, "USAGE", NULL);
	return TCL_ERROR;
    }

    hd = get_hd(interp, objv[1]);

    if (hd == NULL || hd->handle == NULL) {
	Tcl_AppendResult(interp, COMMAND,  " bad handle", NULL);
	Tcl_SetErrorCode(interp, ERROR_CLASS, "HANDLE", NULL);
	return TCL_ERROR;
    }

    dtrace_close(hd->handle);
    hd->handle = NULL;
    del_hd(interp, objv[1]);

    return TCL_OK;
}
/*}}}*/

/* Conf {{{
 *
 *	Implements the ::dtrace::configure command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Given options are set.
 */

int Conf (
	ClientData cd, 
	Tcl_Interp *interp, 
	int objc, 
	Tcl_Obj *const objv[]) 
{
    handle_data *hd;
    int i;

    if (objc % 2 == 1 && objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "handle ?option value ...?");
	Tcl_SetErrorCode(interp, ERROR_CLASS, "USAGE", NULL);
	return TCL_ERROR;
    }
    hd = get_hd(interp, objv[1]);
    if (hd == NULL || hd->handle == NULL) {
	Tcl_AppendResult(interp, COMMAND, " bad handle", NULL);
	Tcl_SetErrorCode(interp, ERROR_CLASS, "HANDLE", NULL);
	return TCL_ERROR;
    }
    if (objc == 2) {
	/* Return the set of so called "basic options".
	 * These are a little arbitrary, listed on:
	 * http://dev.lrem.net/tcldtrace/wiki/CommandsList
	 */
	Tcl_Obj *list = Tcl_NewListObj(0, NULL);

	for (i = 0; basic_options[i] != NULL; i++) {
	    char *value;

	    Tcl_ListObjAppendElement(interp, list,
		    Tcl_NewStringObj(basic_options[i], -1));
	    value = get_option(hd, basic_options[i]);
	    /* This should not fail, unless something in libdtrace
	     * changes, or we have a bug...
	     */
	    if (value == NULL) {
		Tcl_AppendResult(interp, COMMAND, " a basic "
			"option failed, this sould never happen!", NULL);
		Tcl_SetErrorCode(interp, ERROR_CLASS, "BUG", NULL);
		return TCL_ERROR;
	    }
	    Tcl_ListObjAppendElement(interp, list,
		    Tcl_NewStringObj(value, -1));
	}

	Tcl_SetObjResult(interp, list);
	return TCL_OK;
    }
    if (objc == 3) {
	char *option = Tcl_GetString(objv[2]);
	char *value = get_option(hd, option);

	if (value == NULL) {
	    Tcl_AppendResult(interp, COMMAND, " bad option ", option, NULL);
	    Tcl_SetErrorCode(interp, ERROR_CLASS, "OPTION", NULL);
	    return TCL_ERROR;
	}

	Tcl_SetResult(interp, value, TCL_VOLATILE);
	return TCL_OK;
    }

    /* We got a list of options to set, empty string to return. */
    for (i = 2; i < objc-1; i += 2) {
	if (set_option(hd, Tcl_GetString(objv[i]),
		Tcl_GetString(objv[i+1])) == 0) {
	    Tcl_AppendResult(interp, COMMAND, " bad option change ",
		    Tcl_GetString(objv[i]), NULL);
	    Tcl_SetErrorCode(interp, ERROR_CLASS, "OPTION", NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}
/*}}}*/

/* Dtrace_DeInit {{{
 *
 *	Exit time cleanup.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All remaining dtrace_hdl_t's are closed.
 */

void Dtrace_DeInit (
	ClientData htable)
{
    Tcl_HashSearch searchPtr;
    Tcl_HashEntry *hentry = Tcl_FirstHashEntry(htable, &searchPtr);

    while (hentry != NULL)
    {
	handle_data *hd = (handle_data*) Tcl_GetHashValue(hentry);
	dtrace_close(hd->handle);
	ckfree((void*) hd);
	hentry = Tcl_NextHashEntry(&searchPtr);
    }
    Tcl_DeleteHashTable(htable);
}
/*}}}*/

/* onDestroy {{{
 *
 *      Called when interpreter is destroyed instead of exiting.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Dtrace_DeInit is called to clean up.
 *      Interpreter associated hash table is erased.
 */

void onDestroy (
	ClientData htable, 
	Tcl_Interp *interp)
{
    Dtrace_DeInit(htable);
}
/*}}}*/

/* Dtrace_Init {{{
 *
 *	Initializes the package.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	The dtrace package is created.
 *	Package data is associated with the interpreter.
 *	A call to Dtrace_DeInit is scheduled on unassociation of the data.
 *	A namespace and an ensamble are created.
 *	New commands added to the interpreter:
 *		::dtrace::open
 *		::dtrace::close
 *		::dtrace::configure
 */

int Dtrace_Init (
	Tcl_Interp *interp) 
{

    Tcl_Namespace *namespace;
    int major;
    int minor;
    Tcl_HashTable *htable;

    /* As I understood it, Tcl_InitStubs is to make the extension linkable
     * against any version of interpreter that supports it. (Not only
     * against the binary available at compile time).
     */
    if (Tcl_InitStubs(interp, "8.4", 0) == NULL
	    || Tcl_PkgRequire(interp, "Tcl", "8.4", 0) == NULL) {
	return TCL_ERROR;
    }

    htable = (Tcl_HashTable*) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(htable, TCL_ONE_WORD_KEYS);
    Tcl_SetAssocData(interp, EXTENSION_NAME, onDestroy, htable);
    Tcl_CreateExitHandler(Dtrace_DeInit, htable);

    if (Tcl_PkgProvide(interp, "dtrace", TCLDTRACE_VERSION) != TCL_OK) {
	return TCL_ERROR;
    }

    Tcl_Eval(interp, "namespace eval " NS " {}");

    Tcl_CreateObjCommand(interp, NS "::open", Open, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::close", Close, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::configure", Conf, NULL, NULL);

    Tcl_GetVersion(&major, &minor, NULL, NULL);
    if (8 <= major && 5 <= minor) {
	/* Tcl_Export(interp, namespace, "*", 0);
	   Tcl_CreateEnsemble(interp, NS, namespace, 0); */
	Tcl_Eval(interp, "namespace eval " NS " {\n"
		"namespace export *\n"
		"namespace ensemble create\n"
		"}");
    }

    return TCL_OK;
}
/*}}}*/

/* vim: set cindent ts=8 sw=4 tw=78 foldmethod=marker: */

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

#include "dtrace.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef __APPLE__
#include "arch.h"
#include <mach/machine.h>
#endif

typedef struct ps_prochandle prochandle;

/* Macros {{{ */

/* Repeating code block found in the Open function.
 * We have the handle opened by now, but since we throw an error
 * we also should not provide an usable handle.
 */
#define OpenThrow(code, msg0, msg1) \
    dtrace_close(hd->handle);\
    del_hd(interp, Tcl_NewIntObj(id));\
    Tcl_AppendResult(interp, COMMAND, msg0, msg1, NULL);\
    Tcl_SetErrorCode(interp, ERROR_CLASS, code, NULL);\
    return TCL_ERROR;

/* This macro is used in intermediate callbacks.
 */
#define CallbackEval(interp, objv, cbname, errcode) {\
    static const int s = sizeof(objv)/sizeof(Tcl_Obj*);\
    int i, result;\
    for (i = 0; i < s; i++) {\
	Tcl_IncrRefCount(objv[i]);\
    }\
    result = Tcl_EvalObjv(interp, s, objv, TCL_EVAL_GLOBAL);\
    for (i = 0; i < s; i++) {\
	Tcl_DecrRefCount(objv[i]);\
    }\
    if (result != TCL_OK) {\
	Tcl_AddErrorInfo(interp, "\n    (running " cbname " callback)");\
	Tcl_BackgroundError(interp);\
	return errcode;\
    }\
}

/* Next three are used throughout the command implementations. These do not 
 * need to be implemented as macros because of performance reasons. It's 
 * rather because we need them to return the calling function.
 */
#define objcCheck(arguments, successCondition)\
    if (!(successCondition)) {\
	Tcl_WrongNumArgs(interp, 1, objv, arguments);\
	Tcl_SetErrorCode(interp, ERROR_CLASS, "USAGE", NULL);\
	return TCL_ERROR;\
    }
#define handleCheck()\
    if (hd == NULL || hd->handle == NULL) {\
	Tcl_AppendResult(interp, COMMAND,  " bad handle", NULL);\
	Tcl_SetErrorCode(interp, ERROR_CLASS, "HANDLE", NULL);\
	return TCL_ERROR;\
    }

/* }}} */

/* Helpers {{{ */

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
	snprintf(value, 12, "%lld", opt);
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
    dtrace_data *dd;

    if (Tcl_GetIntFromObj(interp, __id, &_id) != TCL_OK) {
	/* We want a newline between Tcl_GetIntFromObj and own message. */
	Tcl_AppendResult(interp, "\n", NULL);
	return NULL;
    }
    id = (char*)(intptr_t) _id;
    dd = Tcl_GetAssocData(interp, EXTENSION_NAME, NULL);
    if (dd == NULL) {
	Tcl_Panic(EXTENSION_NAME " dtrace data not found");
    }
    htable = dd->handles;
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
    dtrace_data *dd;
    handle_data *hd;
    Tcl_HashSearch searchPtr;

    Tcl_GetIntFromObj(interp, __id, &_id);
    id = (char*)(intptr_t) _id;
    dd = Tcl_GetAssocData(interp, EXTENSION_NAME, NULL);
    if (dd == NULL) {
	Tcl_Panic(EXTENSION_NAME " dtrace data not found");
    }
    htable = dd->handles;
    if (htable == NULL) {
	Tcl_Panic(EXTENSION_NAME " hash table not found");
    }
    hentry = Tcl_FindHashEntry(htable, id);
    if (hentry == NULL) {
	Tcl_Panic(EXTENSION_NAME " hash entry to be deleted not found");
    }

    hd = Tcl_GetHashValue(hentry);
    if (hd->processes) {
	ckfree((void*) hd->processes);
    }
    ckfree((char*) hd);
    Tcl_DeleteHashEntry(hentry);

    hentry = Tcl_FirstHashEntry(dd->programs, &searchPtr);
    while(hentry != NULL) {
	ckfree(Tcl_GetHashValue(hentry));
	Tcl_DeleteHashEntry(hentry);
	hentry = Tcl_NextHashEntry(&searchPtr);
    }
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
    dtrace_data *dd = Tcl_GetAssocData(interp, EXTENSION_NAME, NULL);
    Tcl_HashTable *htable = dd->handles;
    Tcl_HashEntry *hentry;
    handle_data *hd;
    int isNew;

    if (htable == NULL) {
	Tcl_Panic(EXTENSION_NAME " hash table not found");
    }

    Tcl_MutexLock(&idMutex);
    *id = next_free_id;
    next_free_id++;
    Tcl_MutexUnlock(&idMutex);

    hd = (handle_data*) ckalloc(sizeof(handle_data));
    memset(hd, 0, sizeof(handle_data));

    hentry = Tcl_CreateHashEntry(htable, (char*) *id, &isNew);
    if (!isNew) {
	Tcl_Panic(EXTENSION_NAME " duplicate hash table entry");
    }
    Tcl_SetHashValue(hentry, hd);
    return hd;
}
/*}}}*/

/* register_pd {{{
 *
 *	Registers a program_data into handle_data.
 *
 * Results:
 *	A Tcl_Obj* identifying the inserted program_data.
 *
 * Side effects:
 *	Hash entry is inserted.
 *	The static next_free_pid is incremented in a thread-safe way.
 */

static Tcl_Obj *register_pd (
	Tcl_Interp *interp,
	handle_data *hd,
	program_data *pd)
{
    dtrace_data *dd = Tcl_GetAssocData(interp, EXTENSION_NAME, NULL);
    Tcl_HashTable *htable;
    Tcl_HashEntry *hentry;
    int isNew;
    int id;

    if (dd == NULL) {
	Tcl_Panic(EXTENSION_NAME " dtrace data not found");
    }
    htable = dd->programs;
    if (htable == NULL) {
	Tcl_Panic(EXTENSION_NAME " hash table not found");
    }

    Tcl_MutexLock(&pidMutex);
    id = next_free_pid;
    next_free_pid++;
    Tcl_MutexUnlock(&pidMutex);

    hentry = Tcl_CreateHashEntry(htable, (char*) id, &isNew);
    if (!isNew) {
	Tcl_Panic(EXTENSION_NAME " duplicate hash table entry");
    }

    pd->hd = hd;

    Tcl_SetHashValue(hentry, pd);
    return Tcl_NewIntObj(id);
}
/*}}}*/

/* get_pd {{{
 *
 *	Given a Tcl-level program handle, gets the corresponding program_data.
 *
 * Results:
 *	A pointer to program_data structure on succes.
 *	NULL on failure.
 *
 * Side effects:
 *	None.
 */

static program_data *get_pd (
	Tcl_Interp *interp,
	Tcl_Obj *__id)
{
    int _id;
    char *id;
    Tcl_HashTable *htable;
    Tcl_HashEntry *hentry;
    dtrace_data *dd;

    if (Tcl_GetIntFromObj(interp, __id, &_id) != TCL_OK) {
	/* We want a newline between Tcl_GetIntFromObj and own message. */
	Tcl_AppendResult(interp, "\n", NULL);
	return NULL;
    }
    id = (char*)(intptr_t) _id;
    dd = Tcl_GetAssocData(interp, EXTENSION_NAME, NULL);
    if (dd == NULL) {
	Tcl_Panic(EXTENSION_NAME " dtrace data not found");
    }
    htable = dd->programs;
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

/* bufType {{{
 *
 *	Gets a human readable string from action type number.
 *
 * Results:
 *	A Tcl string object containing the action type, or "UNKNOWN".
 *
 * Side effects:
 *	None.
 */

static Tcl_Obj *bufType (
	const dtrace_actkind_t number)
{
    switch (number) {
	case DTRACEACT_DIFEXPR:
	    return Tcl_NewStringObj("DIFEXPR", -1);
	case DTRACEACT_PRINTF:
	    return Tcl_NewStringObj("PRINTF", -1);
	case DTRACEACT_PRINTA:
	    return Tcl_NewStringObj("PRINTA", -1);
	case DTRACEACT_USTACK:
	    return Tcl_NewStringObj("USTACK", -1);
	case DTRACEACT_JSTACK:
	    return Tcl_NewStringObj("JSTACK", -1);
	case DTRACEACT_STACK:
	    return Tcl_NewStringObj("STACK", -1);
	default:
	    return Tcl_NewStringObj("UNKNOWN", -1);
    }
}
/*}}}*/

/* dropKind {{{
 *
 *	Gets a human readable string from drop type number.
 *
 * Results:
 *	A Tcl string object containing the drop type, or "UNKNOWN".
 *
 * Side effects:
 *	None.
 */

static Tcl_Obj *dropKind (
	const dtrace_dropkind_t number)
{
    switch (number) {
	case DTRACEDROP_PRINCIPAL:
	    return Tcl_NewStringObj("PRINCIPAL", -1);
	case DTRACEDROP_AGGREGATION:
	    return Tcl_NewStringObj("AGGREGATION", -1);
	case DTRACEDROP_DYNAMIC:
	    return Tcl_NewStringObj("DYNAMIC", -1);
	case DTRACEDROP_DYNRINSE:
	    return Tcl_NewStringObj("DYNRINSE", -1);
	case DTRACEDROP_DYNDIRTY:
	    return Tcl_NewStringObj("DYNDIRTY", -1);
	case DTRACEDROP_SPEC:
	    return Tcl_NewStringObj("SPEC", -1);
	case DTRACEDROP_SPECBUSY:
	    return Tcl_NewStringObj("SPECBUSY", -1);
	case DTRACEDROP_SPECUNAVAIL:
	    return Tcl_NewStringObj("SPECUNAVAIL", -1);
	case DTRACEDROP_STKSTROVERFLOW:
	    return Tcl_NewStringObj("STKSTROVERFLOW", -1);
	case DTRACEDROP_DBLERROR:
	    return Tcl_NewStringObj("DBLERROR", -1);
	default:
	    return Tcl_NewStringObj("UNKNOWN", -1);
    }
}
/*}}}*/

/* formatProbeDesc {{{
 *
 *	Format probe description according to settings.
 *
 * Results:
 *	A Tcl string/list object containing the probe description.
 *
 * Side effects:
 *	None.
 */

static Tcl_Obj *formatProbeDesc (
	const handle_data *hd,
	const dtrace_probedesc_t *pdesc
	)
{
    if (hd->options.foldpdesc) {
	char name[128];

	snprintf(name, sizeof(name), "%s:%s:%s:%s", pdesc->dtpd_provider,
		pdesc->dtpd_mod, pdesc->dtpd_func, pdesc->dtpd_name);
	return Tcl_NewStringObj(name, -1);
    }
    else {
	Tcl_Obj *desc[4];
	desc[0] = Tcl_NewStringObj(pdesc->dtpd_provider, -1);
	desc[1] = Tcl_NewStringObj(pdesc->dtpd_mod, -1);
	desc[2] = Tcl_NewStringObj(pdesc->dtpd_func, -1);
	desc[3] = Tcl_NewStringObj(pdesc->dtpd_name, -1);
	return Tcl_NewListObj(4, desc);
    }
}
/*}}}*/

/* cutWhiteSpace {{{
 *
 *	Get rid of leading and trailing whitespace.
 *
 * Results:
 *	Tcl_Obj string object cleared from the whitespace.
 *
 * Side effects:
 *	None.
 */

static Tcl_Obj *cutWhiteSpace (
	const char *csource
	)
{
    /* We need to get own copy, because we need to put a terminating NULL
     * quite probably somewhere in the middle of the string (and we get it
     * const).
     */
    char buf[TUPLE_STRLEN];
    char *source = (char*) csource;
    int size;
    int i;

    while (*source == ' ') {
	source++;
    }

    for (i = 0; source[i] && i < TUPLE_STRLEN; i++) {
	buf[i] = source[i];
	if (buf[i] != ' ') {
	    size = i;
	}
    }
    buf[++size] = 0;
    
    return Tcl_NewStringObj(buf, size);
}
/*}}}*/

/* }}} */

/* Intermediate callbacks {{{ */

/* chew {{{
 *
 *	Intermediate callback between libdtrace and Tcl code.
 *
 * Results:
 *	DTRACE_CONSUME_THIS upon successful consumption.
 *
 * Side effects:
 *	Appropriate Tcl callback is called.
 */

static int chew (
	const dtrace_probedata_t *data,
	void *arg)
{
    handle_data *hd = (handle_data*) arg;
    dtrace_probedesc_t *pdesc = data->dtpda_pdesc;
    processorid_t cpu = data->dtpda_cpu;
    Tcl_Obj *objv[5];

    if (hd->callbacks[cb_probe_desc] == NULL) {
	return DTRACE_CONSUME_THIS;
    }

    objv[0] = hd->callbacks[cb_probe_desc];

    objv[1] = formatProbeDesc(hd, pdesc);

    objv[2] = Tcl_NewIntObj(cpu);

    objv[3] = Tcl_NewIntObj(pdesc->dtpd_id);

    objv[4] = hd->args[cb_probe_desc];

    CallbackEval(hd->interp, objv, "probe_desc", DTRACE_CONSUME_ERROR);

    return DTRACE_CONSUME_THIS;
}
/*}}}*/

/* chewrec {{{
 *
 *	Intermediate callback between libdtrace and Tcl code.
 *
 * Results:
 *	DTRACE_CONSUME_THIS upon successful consumption.
 *
 * Side effects:
 *	Appropriate Tcl callback is called.
 */

static int chewrec (
	const dtrace_probedata_t *data,
	const dtrace_recdesc_t *rec,
	void *arg)
{
    return DTRACE_CONSUME_THIS;
}
/*}}}*/

/* bufhandler {{{
 *
 *	Intermediate callback between libdtrace and Tcl code.
 *
 * Results:
 *	DTRACE_HANDLE_OK upon successful consumption.
 *
 * Side effects:
 *	Appropriate Tcl callback is called.
 */

static int bufhandler (
	const dtrace_bufdata_t *bufdata,
	void *arg)
{
    handle_data *hd = (handle_data*) arg;
    dtrace_probedata_t *data = bufdata->dtbda_probe;
    dtrace_probedesc_t *pdesc = data->dtpda_pdesc;
    processorid_t cpu = data->dtpda_cpu;
    Tcl_Obj *objv[7];

    if(hd->callbacks[cb_probe_output] == NULL) {
	return DTRACE_HANDLE_OK;
    }

    objv[0] = hd->callbacks[cb_probe_output];

    objv[1] = formatProbeDesc(hd, pdesc);

    objv[2] = Tcl_NewIntObj(cpu);

    objv[3] = Tcl_NewIntObj(pdesc->dtpd_id);

    objv[4] = hd->args[cb_probe_output];

    objv[5] = bufType(bufdata->dtbda_recdesc->dtrd_action);

    objv[6] = Tcl_NewStringObj(bufdata->dtbda_buffered, -1);

    CallbackEval(hd->interp, objv, "probe_output", DTRACE_HANDLE_ABORT);

    return DTRACE_HANDLE_OK;
}
/*}}}*/

/* agghandler {{{
 *
 *	Intermediate callback between libdtrace and Tcl code.
 *
 * Results:
 *	DTRACE_HANDLE_OK upon successful consumption.
 *
 * Side effects:
 *	Appropriate Tcl callback is called.
 */

static int agghandler (
	const dtrace_bufdata_t *bufdata,
	void *arg)
{
    handle_data *hd = (handle_data*) arg;
    aggregation_data *agg = &hd->agg;

    /* This finally turned out to be unneeded.
    if (hd->agg.desc != bufdata->dtbda_aggdata->dtada_desc) {
	printf("Aggregation description changed!\n");
	hd->agg.desc = bufdata->dtbda_aggdata->dtada_desc;

    }
    */

    if (bufdata->dtbda_recdesc) {
	if (DTRACEACT_ISAGG(bufdata->dtbda_recdesc->dtrd_action)) {
	    /* This is the value. By now, we got all the tuple members inside
	     * hd->agg->tuple. Next we have to add that list to the list, 
	     * add the value and clean up after constructing the tuple.
	     */
	    if (agg->tuple == NULL) {
		Tcl_Panic(EXTENSION_NAME " empty aggregation tuple");
	    }

	    Tcl_ListObjAppendElement(hd->interp, agg->list, agg->tuple);
	    Tcl_ListObjAppendElement(hd->interp, agg->list,
		    cutWhiteSpace(bufdata->dtbda_buffered));
	    agg->tuple = NULL;
	}
	else {
	    if (agg->tuple == NULL) {
		agg->tuple = Tcl_NewListObj(0, NULL);
	    }

	    Tcl_ListObjAppendElement(hd->interp, agg->tuple,
		    cutWhiteSpace(bufdata->dtbda_buffered));
	}
    }
    else {
	/* This is the extra whitespace libdtrace serves us, ignore. */
    }

    return DTRACE_HANDLE_OK;
}
/*}}}*/

/* buf_or_agg {{{
 *
 * 	Multiplexer between handlers for aggregation and normal actions
 * 	buffers.
 *
 * Results:
 *	DTRACE_HANDLE_OK upon successful consumption.
 *
 * Side effects:
 *	Appropriate buffered ouptut handler is called.
 */

static int buf_or_agg (
	const dtrace_bufdata_t *bufdata,
	void *arg)
{
    if (bufdata->dtbda_aggdata) {
	return agghandler(bufdata, arg);
    }
    else {
	return bufhandler(bufdata, arg);
    }
}
/*}}}*/

/* drophandler {{{
 *
 *	Intermediate callback between libdtrace and Tcl code.
 *
 * Results:
 *	DTRACE_HANDLE_OK upon successful consumption.
 *
 * Side effects:
 *	Appropriate Tcl callback is called.
 */

static int drophandler (
	const dtrace_dropdata_t *dropdata,
	void *arg)
{
    handle_data *hd = (handle_data*) arg;
    processorid_t cpu = dropdata->dtdda_cpu;
    Tcl_Obj *objv[6];

    if(hd->callbacks[cb_drop] == NULL) {
	return DTRACE_HANDLE_OK;
    }

    objv[0] = hd->callbacks[cb_drop];

    objv[1] = Tcl_NewIntObj(cpu);

    objv[2] = dropKind(dropdata->dtdda_kind);

    objv[3] = Tcl_NewIntObj(dropdata->dtdda_drops);

    objv[4] = Tcl_NewStringObj(dropdata->dtdda_msg, -1);

    objv[5] = hd->args[cb_drop];

    CallbackEval(hd->interp, objv, "drop", DTRACE_HANDLE_ABORT);

    return DTRACE_HANDLE_OK;
}
/*}}}*/

/* errhandler {{{
 *
 *	Intermediate callback between libdtrace and Tcl code.
 *
 * Results:
 *	DTRACE_HANDLE_OK upon successful consumption.
 *
 * Side effects:
 *	Appropriate Tcl callback is called.
 */

static int errhandler (
	const dtrace_errdata_t *errdata,
	void *arg)
{
    handle_data *hd = (handle_data*) arg;
    dtrace_probedesc_t *pdesc = errdata->dteda_pdesc;
    processorid_t cpu = errdata->dteda_cpu;
    Tcl_Obj *objv[5];

    if(hd->callbacks[cb_error] == NULL) {
	return DTRACE_HANDLE_OK;
    }

    objv[0] = hd->callbacks[cb_error];

    objv[1] = formatProbeDesc(hd, pdesc);

    objv[2] = Tcl_NewIntObj(cpu);

    objv[3] = Tcl_NewStringObj(errdata->dteda_msg, -1);

    objv[4] = hd->args[cb_error];

    CallbackEval(hd->interp, objv, "error", DTRACE_HANDLE_ABORT);

    return DTRACE_HANDLE_OK;
}
/*}}}*/

/* prochandler {{{
 *
 *	Intermediate callback between libdtrace and Tcl code.
 *
 * Results:
 *	DTRACE_CONSUME_THIS upon successful consumption.
 *
 * Side effects:
 *	Appropriate Tcl callback is called.
 */

static void prochandler (
	prochandle *proc,
	const char *msg,
	void *arg)
{
    handle_data *hd = (handle_data*) arg;
    Tcl_Obj *objv[4];

    if(hd->callbacks[cb_proc] == NULL) {
	return;
    }

    objv[0] = hd->callbacks[cb_proc];

    objv[1] = Tcl_NewIntObj(Pstatus(proc)->pr_pid);

    objv[2] = Tcl_NewStringObj(msg, -1);

    objv[3] = hd->args[cb_proc];

    CallbackEval(hd->interp, objv, "proc", );
}
/*}}}*/

/* listProbe {{{
 *
 *	Intermediate callback between libdtrace and Tcl code.
 *
 * Results:
 *	0 upon successful consumption.
 *
 * Side effects:
 *	Appropriate Tcl callback is called.
 */

static int listProbe (
	dtrace_hdl_t *handle,
	const dtrace_probedesc_t *pdesc,
	void *varg
	)
{
    list_arg *arg = (list_arg*) varg;
    Tcl_Obj *objv[3];

    objv[0] = arg->proc;

    objv[1] = formatProbeDesc(arg->hd, pdesc);

    objv[2] = arg->args;

    CallbackEval(arg->hd->interp, objv, "list", DTRACE_HANDLE_ABORT);

    return 0;
}
/*}}}*/

/* listStatement {{{
 *
 *	Intermediate callback before listProbe.
 *
 * Results:
 *	0 upon successful consumption.
 *
 * Side effects:
 *	The listProbe callback is called, probably multiple times.
 */

static int listStatement (
	dtrace_hdl_t *handle,
	dtrace_prog_t *program,
	dtrace_stmtdesc_t *statement,
	void *varg
	)
{
    list_arg *arg = (list_arg*) varg;
    dtrace_ecbdesc_t *enabled = statement->dtsd_ecbdesc;

    if (enabled == arg->last) {
	return 0;
    }
    arg->last = enabled;

    if (dtrace_probe_iter(handle, &enabled->dted_probe, listProbe, arg) != 0) {
	/* AFAIR this should be caught at compile time... */
	Tcl_Panic(EXTENSION_NAME " unmatched statement %s:%s:%s:%s: %s\n",
		enabled->dted_probe.dtpd_provider,
		enabled->dted_probe.dtpd_mod,
		enabled->dted_probe.dtpd_func,
		enabled->dted_probe.dtpd_name,
		dtrace_errmsg(handle, dtrace_errno(handle)));
    }

    return 0;
}
/*}}}*/

/* }}} */

/* Command implementations {{{ */

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

    /* Can't use the objcCheck macro here, as it's a more complicated case.*/
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

	del_hd(interp, Tcl_NewIntObj(id));
	Tcl_AppendResult(interp, COMMAND, " libdtrace error: ",
		dtrace_errmsg(NULL, error), NULL);
	snprintf(errnum, 16, "%d", error);
	Tcl_SetErrorCode(interp, ERROR_CLASS, "LIB", errnum, NULL);
	return TCL_ERROR;
    }

    /* Reasonable defaults. */
    if (dtrace_setopt(hd->handle, "bufsize", "4m") != 0
	    || dtrace_setopt(hd->handle, "aggsize", "4m") != 0
#ifdef __APPLE__
	    || dtrace_setopt(g_dtp, "stacksymbols", "enabled") != 0
	    || dtrace_setopt(g_dtp, "arch", string_for_arch(host_arch)) != 0
#endif
	    ) {
	OpenThrow("OPTION", " can't set default options ", NULL);
    }

    for (i = 1; i < objc - 1; i+=2) {
	if (set_option(hd, Tcl_GetString(objv[i]),
		    Tcl_GetString(objv[i+1])) == 0) {
	    OpenThrow("OPTION", " bad option initialization ",
		    Tcl_GetString(objv[i]));
	}
    }

    if (dtrace_handle_err(hd->handle, &errhandler, hd) == -1) {
	OpenThrow("LIB", " failed to establish error handler", NULL);
    }

    if (dtrace_handle_drop(hd->handle, &drophandler, hd) == -1) {
	OpenThrow("LIB", " failed to establish drop handler", NULL);
    }

    if (dtrace_handle_proc(hd->handle, &prochandler, hd) == -1) {
	OpenThrow("LIB", " failed to establish proc handler", NULL);
    }

    if (dtrace_handle_buffered(hd->handle, &buf_or_agg, hd) == -1) {
	OpenThrow("LIB", " failed to establish buffered handler", NULL);
    }

    hd->interp = interp;

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
    int i;

    objcCheck("handle", objc == 2);

    hd = get_hd(interp, objv[1]);
    handleCheck();

    dtrace_close(hd->handle);
    hd->handle = NULL;
    for (i = 0; i < CALLBACKS_COUNT; i++) {
	if(hd->callbacks[i]) {
	    Tcl_DecrRefCount(hd->callbacks[i]);
	    Tcl_DecrRefCount(hd->args[i]);
	}
    }
    del_hd(interp, objv[1]);

    return TCL_OK;
}
/*}}}*/

/* Configure {{{
 *
 *	Implements the ::dtrace::configure command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Given options are set.
 */

int Configure (
	ClientData cd,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[])
{
    handle_data *hd;
    int i;

    objcCheck("handle ?option value ...?", objc % 2 == 0 || objc == 3);

    hd = get_hd(interp, objv[1]);
    handleCheck();

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

/* Compile {{{
 *
 *     Implements the ::dtrace::compile command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Given program gets compiled.
 *	Pointer to the compiled program gets registered within the handle
 *	data, and handle to it set as the command result.
 */

static int Compile (
	ClientData cd,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[])
{
    handle_data *hd;
    program_data *pd;
    char *source;
    int argc;
    char **argv;

    objcCheck("handle program ?argument0 argument1 ...?", 3 <= objc);

    hd = get_hd(interp, objv[1]);
    handleCheck();

    source = Tcl_GetString(objv[2]);
    pd = (program_data*) ckalloc(sizeof(program_data));
    argc = objc - 3;
    argv = (char**) ckalloc(sizeof(char*) * argc);
    if(objc > 3) {
	int i, j = 0;
	for(i = 3; i < objc; i++) {
	    argv[j++] = Tcl_GetString(objv[i]);
	}
    }

    pd->compiled = dtrace_program_strcompile(hd->handle, source,
	    DTRACE_PROBESPEC_NAME, DTRACE_C_PSPEC, argc, argv);

    ckfree((char*) argv);

    if(pd->compiled == NULL) {
	char errnum[16];

	Tcl_AppendResult(interp, COMMAND, " libdtrace error: ",
		dtrace_errmsg(NULL, dtrace_errno(hd->handle)), NULL);
	snprintf(errnum, 16, "%d", dtrace_errno(hd->handle));
	Tcl_SetErrorCode(interp, ERROR_CLASS, "LIB", errnum, NULL);

	ckfree((char*) pd);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, register_pd(interp, hd, pd));
    return TCL_OK;
}
/*}}}*/

/* Exec_or_Info {{{
 *
 *	This is internal to not duplicate code between Exec and Info.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 * 	Program gets executed (probably).
 * 	Result is set to a dict with executed program info.
 */

static int Exec_or_Info (
	ClientData cd,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[],
	int exec)
{
    handle_data *hd;
    program_data *pd;
    dtrace_proginfo_t info;
    Tcl_Obj *results[8];
    int exec_result;

    
    objcCheck("compiled_program", objc==2);

    pd = get_pd(interp, objv[1]);
    if (pd == NULL) {
	Tcl_AppendResult(interp, COMMAND,  " bad program handle", NULL);
	Tcl_SetErrorCode(interp, ERROR_CLASS, "HANDLE", NULL);
	return TCL_ERROR;
    }

    hd = pd->hd;
    handleCheck();

    if(exec) {
	exec_result = dtrace_program_exec(hd->handle, pd->compiled, &info);
    }
    else {
	dtrace_program_info(hd->handle, pd->compiled, &info);
	exec_result = 0;
    }
    if (exec_result == -1) {
    	Tcl_AppendResult(interp, COMMAND,  " failed enable the probe", NULL);
	Tcl_SetErrorCode(interp, ERROR_CLASS, "EXEC", NULL);
	return TCL_ERROR;
    }

    results[0] = Tcl_NewStringObj("aggregates", -1);
    results[1] = Tcl_NewIntObj(info.dpi_aggregates);
    results[2] = Tcl_NewStringObj("recgens", -1);
    results[3] = Tcl_NewIntObj(info.dpi_recgens);
    results[4] = Tcl_NewStringObj("matches", -1);
    results[5] = Tcl_NewIntObj(info.dpi_matches);
    results[6] = Tcl_NewStringObj("speculations", -1);
    results[7] = Tcl_NewIntObj(info.dpi_speculations);

    Tcl_SetObjResult(interp, Tcl_NewListObj(8, results));
    return TCL_OK;
}
/*}}}*/

/* Exec {{{
 *
 *     Implements the ::dtrace::exec command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 * 	Program gets executed.
 * 	Result is set to a dict with executed program info.
 */

static int Exec (
	ClientData cd,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[])
{
    return Exec_or_Info (cd, interp, objc, objv, 1);
}
/*}}}*/

/* Info {{{
 *
 *     Implements the ::dtrace::info command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 * 	Result is set to a dict with executed program info.
 */

static int Info (
	ClientData cd,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[])
{
    return Exec_or_Info (cd, interp, objc, objv, 0);
}
/*}}}*/

/* Go {{{
 *
 *     Implements the ::dtrace::go command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Tracing is started.
 *	Callbacks are registered.
 */

static int Go (
	ClientData cd,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[])
{
    handle_data *hd;
    int i;

    objcCheck("handle ?callback {proc ?arg?} ...?", 2 <= objc 
	    && objc % 2 == 0);

    hd = get_hd(interp, objv[1]);
    handleCheck();

    for (i = 2; i < objc; i+=2) {
	int callback;
	int lobjc;
	Tcl_Obj **lobjv;

	if (Tcl_GetIndexFromObj(interp, objv[i], callbackNames,
		    "callback name", 0, &callback) != TCL_OK) {
	    /* Tcl_GetIndexFromObj prints a message for us. */
	    Tcl_SetErrorCode(interp, ERROR_CLASS, "USAGE", NULL);
	    return TCL_ERROR;
	}

	if (Tcl_ListObjGetElements(interp, objv[i+1], &lobjc, &lobjv)
		!= TCL_OK || lobjc != 2) {
	    /* Tcl_ListObjGetElements prints a message for us.
	     * But we want to instruct how to do it right.*/
	    Tcl_AppendResult(interp,
		    "callback spec should be {proc {?arg0 arg1 ...?}}",
		    NULL);
	    Tcl_SetErrorCode(interp, ERROR_CLASS, "USAGE", NULL);
	    return TCL_ERROR;
	}

	if(0 <= callback && callback < CALLBACKS_COUNT) {
	    hd->callbacks[callback] = lobjv[0];
	    hd->args[callback] = lobjv[1];
	}
	else {
	    Tcl_Panic(EXTENSION_NAME " bad callback id");
	}

	Tcl_IncrRefCount(lobjv[0]);
	Tcl_IncrRefCount(lobjv[1]);
    }

    if (dtrace_go(hd->handle) == -1) {
	char errnum[16];

	Tcl_AppendResult(interp, COMMAND, " libdtrace error: ",
		dtrace_errmsg(NULL, dtrace_errno(hd->handle)), NULL);
	snprintf(errnum, 16, "%d", dtrace_errno(hd->handle));
	Tcl_SetErrorCode(interp, ERROR_CLASS, "LIB", errnum, NULL);

	return TCL_ERROR;
    }

    for (i = 0; i < hd->proc_count; i++)
	dtrace_proc_continue(hd->handle, hd->processes[i]);

    return TCL_OK;
}
/*}}}*/

/* Stop {{{
 *
 *     Implements the ::dtrace::stop command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Tracing is stopped.
 */

static int Stop (
	ClientData cd,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[])
{
    handle_data *hd;

    objcCheck("handle", objc == 2);

    hd = get_hd(interp, objv[1]);
    handleCheck();

    if (dtrace_stop(hd->handle) == -1) {
	char errnum[16];

	Tcl_AppendResult(interp, COMMAND, " libdtrace error: ",
		dtrace_errmsg(NULL, dtrace_errno(hd->handle)), NULL);
	snprintf(errnum, 16, "%d", dtrace_errno(hd->handle));
	Tcl_SetErrorCode(interp, ERROR_CLASS, "LIB", errnum, NULL);

	return TCL_ERROR;
    }

    return TCL_OK;
}
/*}}}*/

/* Process {{{
 *
 *     Implements the ::dtrace::process command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Callbacks are called for awaiting data.
 */

static int Process (
	ClientData cd,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[])
{
    handle_data *hd;
    int sleep = 0;

    objcCheck("handle ?sleep?", 2 <= objc && objc <= 3);


    if(objc == 3) {
	if (Tcl_GetBooleanFromObj(interp, objv[2], &sleep) != TCL_OK) {
	    Tcl_AppendResult(interp, "\n", COMMAND, " bad usage", NULL);
	    Tcl_SetErrorCode(interp, ERROR_CLASS, "USAGE", NULL);
	    return TCL_ERROR;
	}
    }

    hd = get_hd(interp, objv[1]);
    handleCheck();

    if (sleep) {
	dtrace_sleep(hd->handle);
    }

    if (dtrace_work(hd->handle, NULL, chew, chewrec, hd) == -1) {
	char errnum[16];

	Tcl_AppendResult(interp, COMMAND, " libdtrace error: ",
		dtrace_errmsg(NULL, dtrace_errno(hd->handle)), NULL);
	snprintf(errnum, 16, "%d", dtrace_errno(hd->handle));
	Tcl_SetErrorCode(interp, ERROR_CLASS, "LIB", errnum, NULL);

	return TCL_ERROR;
    }

    return TCL_OK;
}
/*}}}*/

/* List {{{
 *
 *     Implements the ::dtrace::list command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 *	Callbacks are called for probes matched by given program.
 */

static int List (
	ClientData cd,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[])
{
    handle_data *hd;
    program_data *pd;
    list_arg arg;

    objcCheck("compiled_program list_callback arg", objc == 4);

    pd = get_pd(interp, objv[1]);
    if (pd == NULL) {
	Tcl_AppendResult(interp, COMMAND,  " bad program handle", NULL);
	Tcl_SetErrorCode(interp, ERROR_CLASS, "HANDLE", NULL);
	return TCL_ERROR;
    }

    hd = pd->hd;
    handleCheck();

    arg.last = NULL;
    arg.hd = hd;
    arg.proc = objv[2];
    arg.args = objv[3];

    dtrace_stmt_iter(hd->handle, pd->compiled,
	    (dtrace_stmt_f *)listStatement, &arg);

    return TCL_OK;
}
/*}}}*/

/* Aggregations {{{
 *
 *	Implements the ::dtrace::aggregations command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 * 	A set of internal callbacks is run to collect data.
 * 	Result is set to a dict with the aggregations.
 */

static int Aggregations (
	ClientData cd,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[])
{
    handle_data *hd;

    objcCheck("handle", objc == 2);
 
    hd = get_hd(interp, objv[1]);
    handleCheck();

    hd->agg.list = Tcl_NewListObj(0, NULL);
    /* Content appended within the callback. */
    if (dtrace_aggregate_print(hd->handle, NULL, NULL) == -1) {
	char errnum[16];

	Tcl_AppendResult(interp, COMMAND, " libdtrace error: ",
		dtrace_errmsg(NULL, dtrace_errno(hd->handle)), NULL);
	snprintf(errnum, 16, "%d", dtrace_errno(hd->handle));
	Tcl_SetErrorCode(interp, ERROR_CLASS, "LIB", errnum, NULL);

	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, hd->agg.list);
    return TCL_OK;
}
/*}}}*/

/* Grab {{{
 *
 *	Implements the ::dtrace::grab command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 * 	A process is grabbed.
 */

static int Grab (
	ClientData cd,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[])
{
    handle_data *hd;
    prochandle *proc;
    pid_t pid;
    long lpid;

    objcCheck("handle pid", objc == 3);

    hd = get_hd(interp, objv[1]);
    handleCheck();

    if (Tcl_GetLongFromObj(interp, objv[2], &lpid) != TCL_OK) {
	Tcl_AppendResult(interp, "\n", COMMAND,  " bad pid", NULL);
	Tcl_SetErrorCode(interp, ERROR_CLASS, "USAGE", NULL);
	return TCL_ERROR;
    }
    pid = lpid;

    proc = dtrace_proc_grab(hd->handle, pid, 0);
    if (proc == NULL) {
	char errnum[16];

	Tcl_AppendResult(interp, COMMAND, " libdtrace error: ",
		dtrace_errmsg(hd->handle, dtrace_errno(hd->handle)), NULL);
	snprintf(errnum, 16, "%d", dtrace_errno(hd->handle));
	Tcl_SetErrorCode(interp, ERROR_CLASS, "LIB", errnum, NULL);

	return TCL_ERROR;
    }

    /* Process grabbed. Now we need to record it inside handle data. This is
     * done with an ever-growing variable size vector. Which we grow now, if
     * it's needed. Note it's initialized with memset() in new_hd().
     */
    if (hd->proc_count == hd->proc_capacity) {
	hd->proc_capacity = 2 * hd->proc_capacity + 1;
	hd->processes = (prochandle**) ckrealloc((char*) hd->processes, 
		hd->proc_capacity * sizeof (prochandle*));
    }
    hd->processes[hd->proc_count] = proc;
    hd->proc_count++;

    return TCL_OK;
}
/*}}}*/

/* Launch {{{
 *
 *	Implements the ::dtrace::launch command.
 *
 * Results:
 *	Standard Tcl result.
 *
 * Side effects:
 * 	An already-grabbed process is created.
 */

static int Launch (
	ClientData cd,
	Tcl_Interp *interp,
	int objc,
	Tcl_Obj *const objv[])
{
    handle_data *hd;
    prochandle *proc;
    char **argv;
    int i;

    objcCheck("handle cmd", 3 <= objc);

    hd = get_hd(interp, objv[1]);
    handleCheck();

    argv = (char**) ckalloc(sizeof(char*) * (objc - 1));
    for (i = 2; i < objc; i++) {
	argv[i-2] = Tcl_GetString(objv[i]);
    }
    argv[i-2] = NULL;
    proc = dtrace_proc_create(hd->handle, argv[0], argv);
    ckfree((char*)argv);

    if (proc == NULL) {
	char errnum[16];

	Tcl_AppendResult(interp, COMMAND, " libdtrace error: ",
		dtrace_errmsg(hd->handle, dtrace_errno(hd->handle)), NULL);
	snprintf(errnum, 16, "%d", dtrace_errno(hd->handle));
	Tcl_SetErrorCode(interp, ERROR_CLASS, "LIB", errnum, NULL);

	return TCL_ERROR;
    }

    /* Process grabbed. Now we need to record it inside handle data. This is
     * done with an ever-growing variable size vector. Which we grow now, if
     * it's needed. Note it's initialized with memset() in new_hd().
     */
    if (hd->proc_count == hd->proc_capacity) {
	hd->proc_capacity = 2 * hd->proc_capacity + 1;
	hd->processes = (prochandle**) ckrealloc((char*) hd->processes, 
		hd->proc_capacity * sizeof (prochandle*));
    }
    hd->processes[hd->proc_count] = proc;
    hd->proc_count++;

    return TCL_OK;
}
/*}}}*/

/* }}} */

/* Module (de)initialization {{{ */

/* Dtrace_DeInit {{{
 *
 *	Exit time cleanup.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All remaining dtrace_hdl_t's are closed.
 *	All remaining memory is freed.
 */

void Dtrace_DeInit (
	ClientData dd)
{
    Tcl_HashTable *htable = ((dtrace_data*) dd)->handles;
    Tcl_HashSearch searchPtr;
    Tcl_HashEntry *hentry = Tcl_FirstHashEntry(htable, &searchPtr);

    while (hentry != NULL) {
	handle_data *hd = (handle_data*) Tcl_GetHashValue(hentry);

	dtrace_close(hd->handle);
	ckfree((void*) hd);
	if (hd->processes) {
	    ckfree((void*) hd->processes);
	}
	hentry = Tcl_NextHashEntry(&searchPtr);
    }
    Tcl_DeleteHashTable(htable);
    ckfree((void*) htable);

    htable = ((dtrace_data*) dd)->programs;
    hentry = Tcl_FirstHashEntry(htable, &searchPtr);
    while(hentry != NULL) {
	ckfree(Tcl_GetHashValue(hentry));
	hentry = Tcl_NextHashEntry(&searchPtr);
    }
    Tcl_DeleteHashTable(htable);
    ckfree((void*) htable);
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
 *      Interpreter associated data is erased.
 */

void onDestroy (
	ClientData dd,
	Tcl_Interp *interp)
{
    Dtrace_DeInit(dd);
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
 *		::dtrace::exec
 *		::dtrace::info
 *		::dtrace::go
 *		::dtrace::stop
 *		::dtrace::process
 *		::dtrace::list
 *		::dtrace::aggregations
 *		::dtrace::grab
 *		::dtrace::launch
 */

int Dtrace_Init (
	Tcl_Interp *interp)
{

    /* Tcl_Namespace *namespace; */
    int major;
    int minor;
    dtrace_data *dd;

    /* As I understood it, Tcl_InitStubs is to make the extension linkable
     * against any version of interpreter that supports it. (Not only
     * against the binary available at compile time).
     */
    if (Tcl_InitStubs(interp, "8.4", 0) == NULL
	    || Tcl_PkgRequire(interp, "Tcl", "8.4", 0) == NULL) {
	return TCL_ERROR;
    }

    dd = (dtrace_data*) ckalloc(sizeof(dtrace_data));
    dd->handles = (Tcl_HashTable*) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(dd->handles, TCL_ONE_WORD_KEYS);
    dd->programs = (Tcl_HashTable*) ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(dd->programs, TCL_ONE_WORD_KEYS);
    Tcl_SetAssocData(interp, EXTENSION_NAME, onDestroy, dd);
    Tcl_CreateExitHandler(Dtrace_DeInit, dd);

    if (Tcl_PkgProvide(interp, "dtrace", TCLDTRACE_VERSION) != TCL_OK) {
	return TCL_ERROR;
    }

    Tcl_Eval(interp, "namespace eval " NS " {}");

    Tcl_CreateObjCommand(interp, NS "::open", Open, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::close", Close, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::configure", Configure, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::compile", Compile, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::exec", Exec, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::info", Info, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::go", Go, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::stop", Stop, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::process", Process, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::list", List, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::aggregations", Aggregations, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::grab", Grab, NULL, NULL);
    Tcl_CreateObjCommand(interp, NS "::launch", Launch, NULL, NULL);

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

/* }}} */

/* vim: set cindent ts=8 sw=4 tw=78 foldmethod=marker: */

/*
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
 */
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
    if(strncmp(handle, "dtrace_handle", 13) != 0)
        return -1;
    return atoi(handle + 13);
}

char *get_option(int index, const char *option) 
{
    static char value [12];
    if(internal_option(option)) {
        /* Only one defined for now. */
        snprintf(value, 12, "%d", options[index].foldpdesc);
    } 
    else {
        dtrace_optval_t opt;
        if(dtrace_getopt(handles[index], option+1, &opt) != 0) {
            return NULL;
        }
        if(opt == DTRACEOPT_UNSET)
            opt = 0;
        snprintf(value, 12, "%d", opt);
    }
    return value;
}

int set_option(int index, const char *option, const char *value) 
{
    if(internal_option(option)) {
        /* Only one defined for now. */
        if(value[0] == '0' && value[1] == 0)
            options[index].foldpdesc = 0;
        else
            options[index].foldpdesc = 1;
    } 
    else {
        if(dtrace_setopt(handles[index], option+1, value) != 0) {
            return 0;
        }
    }
    return 1;
}

int Open (ClientData cd, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) 
{
    if(handles_count == MAX_HANDLES) {
        Tcl_AppendResult(interp, COMMAND, " max handles reached", NULL);
        Tcl_SetErrorCode(interp, ERROR_CLASS, "MAX_HANDLES", NULL);
        return TCL_ERROR;
    }

    int flags = 0;
    int error;

    if(objc > 1 && objc % 2 == 0) {
        char *last = Tcl_GetString(objv[objc - 1]);

        if(last[0] == '1') {
            flags |= DTRACE_O_NODEV;
        }

        if(strlen(last) != 1) {
            Tcl_AppendResult(interp, COMMAND, " bad usage", NULL);
            Tcl_SetErrorCode(interp, ERROR_CLASS, "USAGE", NULL);
            return TCL_ERROR;
        }
    }

    handles[handles_count] = dtrace_open(DTRACE_VERSION, flags, &error);

    if(handles[handles_count] == NULL) {
        Tcl_AppendResult(interp, COMMAND, " libdtrace error: ",
                dtrace_errmsg(NULL, error), NULL);
        char errnum[16];
        snprintf(errnum, 16, "%d", error);
        Tcl_SetErrorCode(interp, ERROR_CLASS, "LIB", errnum, NULL);
        return TCL_ERROR;
    }

    for(int i = 1; i < objc - 1; i+=2) {
        if(set_option(handles_count, Tcl_GetString(objv[i]), 
                    Tcl_GetString(objv[i+1])) == 0) {
            Tcl_AppendResult(interp, COMMAND, " bad option initialization ",
                    Tcl_GetString(objv[i]), NULL);
            Tcl_SetErrorCode(interp, ERROR_CLASS, "OPTION", NULL);
            return TCL_ERROR;
        }
    }

    Tcl_SetResult(interp, index_to_handle(handles_count), TCL_VOLATILE);
    handles_count++;

    return TCL_OK;
}

int Close (ClientData cd, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) 
{
    int index = handle_to_index(Tcl_GetString(objv[1]));

    if(objc != 2) {
        Tcl_AppendResult(interp, COMMAND, " bad usage", NULL);
        Tcl_SetErrorCode(interp, ERROR_CLASS, "USAGE", NULL);
        return TCL_ERROR;
    }

    if(index < 0 || handles[index] == NULL) {
        Tcl_AppendResult(interp, COMMAND,  " bad handle", NULL);
        Tcl_SetErrorCode(interp, ERROR_CLASS, "HANDLE", NULL);
        return TCL_ERROR;
    }

    dtrace_close(handles[index]);
    handles[index] = NULL;

    return TCL_OK;
}

int Conf (ClientData cd, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) 
{
    if(objc % 2 == 1 && objc != 3) {
        Tcl_AppendResult(interp, COMMAND, " bad usage", NULL);
        Tcl_SetErrorCode(interp, ERROR_CLASS, "USAGE", NULL);
        return TCL_ERROR;
    }
    int index = handle_to_index(Tcl_GetString(objv[1]));
    if(index < 0 || handles[index] == NULL) {
        Tcl_AppendResult(interp, COMMAND, " bad handle", NULL);
        Tcl_SetErrorCode(interp, ERROR_CLASS, "HANDLE", NULL);
        return TCL_ERROR;
    }
    if(objc == 2) {
        /* Return the set of so called "basic options".
         * These are a little arbitrary, listed on:
         * http://dev.lrem.net/tcldtrace/wiki/CommandsList
         */
        Tcl_Obj *list = Tcl_NewListObj(0, NULL);
        for(int i = 0; basic_options[i] != NULL; i++) {
            Tcl_ListObjAppendElement(interp, list,
                    Tcl_NewStringObj(basic_options[i],-1));
            char *value = get_option(index, basic_options[i]);
            /* This should not fail, unless something in libdtrace
             * changes, or we have a bug...
             */
            if(value == NULL) {
                Tcl_AppendResult(interp, COMMAND, " a basic "
                        "option failed, this sould never happen!", NULL);
                Tcl_SetErrorCode(interp, ERROR_CLASS, "BUG", NULL);
                return TCL_ERROR;
            }
            Tcl_ListObjAppendElement(interp, list, Tcl_NewStringObj(value, -1));
        }

        Tcl_SetObjResult(interp, list);
        return TCL_OK;
    }
    if(objc == 3) {
        char *option = Tcl_GetString(objv[2]);
        char *value = get_option(index, option);
        if(value == NULL) {
            Tcl_AppendResult(interp, COMMAND, " bad option ", option, NULL);
            Tcl_SetErrorCode(interp, ERROR_CLASS, "OPTION", NULL);
            return TCL_ERROR;
        }

        Tcl_SetResult(interp, value, TCL_VOLATILE);
        return TCL_OK;
    }

    /* We got a list of options to set, empty string to return. */
    for(int i = 2; i < objc-1; i+=2) {
        if(set_option(index, Tcl_GetString(objv[i]), 
                    Tcl_GetString(objv[i+1])) == 0) {
            Tcl_AppendResult(interp, COMMAND, " bad option change ",
                    Tcl_GetString(objv[i]), NULL);
            Tcl_SetErrorCode(interp, ERROR_CLASS, "OPTION", NULL);
            return TCL_ERROR;
        }
    }
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
            || Tcl_PkgRequire(interp, "Tcl", "8.4", 0) == NULL) {
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
    Tcl_CreateObjCommand(interp, NS "::configure", (Tcl_ObjCmdProc *) Conf,
            (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    Tcl_GetVersion(&major, &minor, NULL, NULL);
    if(8 <= major && 5 <= minor) {
        /* Tcl_Export(interp, namespace, "*", 0);
           Tcl_CreateEnsemble(interp, NS, namespace, 0); */
        Tcl_Eval(interp, "namespace eval " NS " {\n"
                "namespace export *\n"
                "namespace ensemble create\n"
                "}");
    }

    return TCL_OK;
}

/* vim: set cindent ts=8 sw=4 et: */

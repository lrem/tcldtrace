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
#ifndef __DTRACE_H
#define __DTRACE_H

#define TCLDTRACE_VERSION	"1.0"
#define TCLDTRACE_MAJOR	        1
#define TCLDTRACE_MINOR	        0

#define EXTENSION_NAME          "dtrace"
#define NS		        EXTENSION_NAME
#define ERROR_CLASS 	        "DTRACE"

#define COMMAND		        Tcl_GetString(objv[0])

#include <dtrace.h>
#include <libproc.h>
#include <tcl.h>


#define internal_option(a) (strcmp(a, "-foldpdesc") == 0)
typedef struct options_s {
    int foldpdesc;
} options_t;

typedef struct dtrace_data {
    Tcl_HashTable *handles;
    Tcl_HashTable *programs;
} dtrace_data;

typedef struct handle_data {
    dtrace_hdl_t *handle;
    options_t options;

    /* Callbacks and args for them */
    Tcl_Obj *probe_desc;
    Tcl_Obj *probe_desc_args;
    Tcl_Obj *probe_output;
    Tcl_Obj *probe_output_args;
    Tcl_Obj *drop;
    Tcl_Obj *drop_args;
    Tcl_Obj *error;
    Tcl_Obj *error_args;
    Tcl_Obj *proc;
    Tcl_Obj *proc_args;
} handle_data;

typedef struct program_data {
    dtrace_prog_t *compiled;
    handle_data *hd;
} program_data;

const char *basic_options[] = {
    "-flowindent",
    "-quiet",
    "-bufsize",
    "-foldpdesc",
    NULL
};

/* Explicitly common across all threads. Should be only incremented.
 * Gives us an unique identifier for each handle.
 */
TCL_DECLARE_MUTEX(idMutex)
int next_free_id = 1;
TCL_DECLARE_MUTEX(pidMutex)
int next_free_pid = 1;

static enum callbacks {
    cb_probe_desc, 
    cb_probe_output, 
    cb_drop, 
    cb_error, 
    cb_proc
};

static const char *callbackNames[] = {
    "probe_desc", 
    "probe_output", 
    "drop",
    "error", 
    "proc", 
    NULL
};

#endif /* __DTRACE_H */

/* vim: set cindent ts=8 sw=4 et tw=80 foldmethod=marker: */

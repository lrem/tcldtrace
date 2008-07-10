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
#ifndef __DTRACE_H
#define __DTRACE_H

#define DTRACE_VERSION	"1.0"
#define DTRACE_MAJOR	1
#define DTRACE_MINOR	0

#define NS		"dtrace"
#define ERROR_CLASS 	"DTRACE"

#define MAX_HANDLES	32

#define COMMAND		Tcl_GetString(objv[0])

#include <dtrace.h>
#include <libproc.h>
#include <tcl.h>

int handles_count = 0;
dtrace_hdl_t *handles [MAX_HANDLES];

#define internal_option(a) (strcmp(a, "-foldpdesc") == 0)
typedef struct options_s {
    int foldpdesc;
}
options_t;
options_t options [MAX_HANDLES];
char *basic_options[] = {
    "-flowindent",
    "-quiet",
    "-bufsize",
    "-foldpdesc",
    NULL
};

extern Tcl_Namespace* Tcl_CreateNamespace(Tcl_Interp*, const char*, ClientData, 
        Tcl_NamespaceDeleteProc*);

#endif /* __DTRACE_H */

/* vim: set cindent ts=8 sw=4 et: */

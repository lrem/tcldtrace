/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_LWP_H
#define	_SYS_LWP_H

#pragma ident	"@(#)lwp.h	1.35	05/06/08 SMI"

#if !defined(__APPLE__)
#include <sys/synch.h>
#include <sys/ucontext.h>
#else /* is Apple Mac OS X */

#ifdef KERNEL
#ifndef _KERNEL
#define _KERNEL /* Solaris vs. Darwin */
#endif
#endif

#include "darwin_shim.h" /* In lieu of Solaris <sys/synch.h> and <sys/ucontext.h> */
#endif /* __APPLE__ */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * lwp create flags
 */
#define	LWP_DAEMON	0x00000020
#define	LWP_DETACHED	0x00000040
#define	LWP_SUSPENDED	0x00000080

/*
 * Definitions for user programs calling into the _lwp interface.
 */
struct lwpinfo {
	timestruc_t lwp_utime;
	timestruc_t lwp_stime;
	long	    lwpinfo_pad[64];
};

#if defined(_SYSCALL32)

/* Kernel's view of user ILP32 lwpinfo structure */

struct lwpinfo32 {
	timestruc32_t	lwp_utime;
	timestruc32_t	lwp_stime;
	int32_t		lwpinfo_pad[64];
};

#endif	/* _SYSCALL32 */

#define	_LWP_FSBASE	0
#define	_LWP_GSBASE	1

#define	_LWP_SETPRIVATE	0
#define	_LWP_GETPRIVATE	1

#ifndef _KERNEL

typedef uint_t lwpid_t;

int		_lwp_kill(lwpid_t, int);
int		_lwp_info(struct lwpinfo *);
lwpid_t		_lwp_self(void);
int		_lwp_suspend(lwpid_t);
int		_lwp_continue(lwpid_t);
#if defined(__i386) || defined(__amd64)
int		_lwp_private(int, int, void *);
#endif	/* __i386 || __amd64 */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LWP_H */

/* dbg_hif.h
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 *
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/*
 * ARM debugger toolbox : dbg_hif.c
 * Description of the Dbg_HostosInterface structure.  This is *NOT*
 * part of the debugger toolbox, but it is required by 2 back ends
 * (armul & pisd) and two front ends (armsd & wdbg), so putting it
 * in the toolbox is the only way of avoiding multiple copies.
 */

/*
 * RCS 1.1.1.1
 * Checkin 1999/04/16 01:34:27
 */

#ifndef dbg_hif__h
#define dbg_hif__h

#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#  warning not including "config.h".
# endif /* __GNUC__ && !__STRICT_ANSI__ */
#endif /* HAVE_CONFIG_H */

#if defined(STDC_HEADERS) || defined(HAVE_STDARG_H)
# include <stdarg.h>
#elif defined(HAVE_VARARGS_H)
# include <varargs.h>
#else
# if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#  warning "dbg_hif.h expects either <stdarg.h> or <varargs.h> to be included."
# endif /* __GNUC__ && !__STRICT_ANSI__ */
#endif /* STDC_HEADERS || HAVE_STDARG_H */

typedef void Hif_DbgPrint(void *arg, const char *format, va_list ap);
typedef void Hif_DbgPause(void *arg);

typedef void Hif_WriteC(void *arg, int c);
typedef int Hif_ReadC(void *arg);
typedef int Hif_Write(void *arg, char const *buffer, int len);
typedef char *Hif_GetS(void *arg, char *buffer, int len);

typedef void Hif_RDIResetProc(void *arg);

struct Dbg_HostosInterface {
    Hif_DbgPrint *dbgprint;
    Hif_DbgPause *dbgpause;
    void *dbgarg;

    Hif_WriteC *writec;
    Hif_ReadC *readc;
    Hif_Write *write;
    Hif_GetS *gets;
    void *hostosarg;

    Hif_RDIResetProc *reset;
    void *resetarg;
};

#endif /* !dbg_hif__h */

/* EOF */

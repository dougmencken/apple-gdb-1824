/*---------------------------------------------------------------------------*
 |                                                                           |
 |                            <<< Unmangler.h >>>                            |
 |                                                                           |
 |                         C++ Function Name Decoding                        |
 |                                                                           |
 |                 Copyright Apple Computer, Inc. 1988-1991                  |
 |                           All rights reserved.                            |
 |                                                                           |
 *---------------------------------------------------------------------------*/

#ifndef __UNMANGLER__
#define __UNMANGLER__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int unmangle(char *dst, char *src, int limit);
/*
 * This function unmangles C++ mangled symbols.
 * (i.e. a symbol with a type signature)
 * The mangled C string is passed in "src" and the unmangled C string is
 * returned in "dst". Up to "limit" characters (not including terminating
 * null) may be retured in "dst".
 *
 * The function returns,
 *
 * -1 => error, probably because symbol was not mangled, but looked like it
 *	0 => symbol was NOT mangled; not copied either
 *	1 => symbol was mangled; unmangled result fit in buffer
 *	2 => symbol was mangled; unmangd result truncd to fit in buffer (\0 written)
 *
 * Caution: the src and dst string must not overlap!
 */

pascal int Unmangle(char *dst, char *src, int limit);
/*
 * This function unmangles C++ mangled symbols.
 * (i.e. a symbol with a type signature)
 * The mangled Pascal string is passed in "src" and the unmangled Pascal
 * string is returned in "dst". Up to "limit" chars may be retured in "dst".
 *
 * The function returns,
 *
 * -1 => error, probably because symbol was not mangled, but looked like it
 *	0 => symbol was NOT mangled; not copied either
 *	1 => symbol was mangled; unmangled result fit in buffer
 *	2 => symbol was mangled; unmangd result truncd to fit in buffer (\0 written)
 *
 * This function is identical to unmangle() above except all strings are Pascal
 * instead of C strings.
 *
 * Caution: the src and dst string must not overlap!
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !__UNMANGLER__ */

/* EOF */

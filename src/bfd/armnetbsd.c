/* armnetbsd.c: BFD back-end for NetBSD/ARM a.out-ish binaries.
 * Copyright 1999, 2000, 2001, 2002, 2005
 * Free Software Foundation, Inc.  */
/*
This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St., 5th Floor, Boston, MA 02110-1301, USA */

#ifndef BYTES_IN_WORD
# define BYTES_IN_WORD 4
#endif /* !BYTES_IN_WORD */
#undef TARGET_IS_BIG_ENDIAN_P

#define	TARGET_PAGE_SIZE	4096
#define	SEGMENT_SIZE		TARGET_PAGE_SIZE

#define	DEFAULT_ARCH		bfd_arch_arm
#define	DEFAULT_MID 		M_ARM6_NETBSD
#if !defined(MACHTYPE_OK) && defined(M_ARM6_NETBSD)
# define MACHTYPE_OK(mtype) ((mtype) == M_ARM6_NETBSD)
#endif /* !MACHTYPE_OK && M_ARM6_NETBSD */

/* Do not "beautify" the CONCAT* macro args.  Traditional C will not
   remove whitespace added here, and thus will fail to concatenate
   the tokens.  */
#define MY(OP) 			CONCAT2 (armnetbsd_,OP)

/* This needs to start with a.out so GDB knows it is an a.out variant: */
#define TARGETNAME 		"a.out-arm-netbsd"

/* this needs to go after the usage of the CONCAT* macro mentioned above,
 * but before any other headers are included, or prototypes for functions
 * are declared: */
#if defined(__GNUC__) && (__GNUC__ >= 4) && !defined(__clang__)
 # pragma GCC diagnostic ignored "-Wtraditional"
#endif /* gcc 4+ && !__clang__ */

#include "netbsd.h"

/* silence '-Wunused-macros' warning: */
#ifdef BYTES_IN_WORD
# undef BYTES_IN_WORD
#endif /* BYTES_IN_WORD */

/* EOF */

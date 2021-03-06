/* tm-altos.h
   Target definitions for GDB on an Altos 3068 (m68k running SVR2)
   Copyright 1987, 1989, 1991, 1993 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* The child target cannot deal with floating registers.  */
#define CANNOT_STORE_REGISTER(regno) ((regno) >= FP0_REGNUM)

/* Define BPT_VECTOR if it is different than the default.
   This is the vector number used by traps to indicate a breakpoint. */

#define BPT_VECTOR 0xe

/* Address of end of stack space.  */

#define STACK_END_ADDR_COMMENTED_OUT_VERSION (0xffffff)
#define STACK_END_ADDR (0x1000000)

/* Amount PC must be decremented by after a breakpoint.
   On the Altos, the kernel resets the pc to the trap instr */

#define DECR_PC_AFTER_BREAK 0

/* The only reason this is here is the tm-altos.h reference below. It
   was moved back here from tm-m68k.h.  FIXME? */

extern CORE_ADDR altos_skip_prologue  (CORE_ADDR);
#define SKIP_PROLOGUE(pc) (altos_skip_prologue (pc))

#include "m68k/tm-m68k.h"

/* EOF */

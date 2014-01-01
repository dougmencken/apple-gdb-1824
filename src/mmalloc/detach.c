/* Finish access to a mmap'd malloc managed region.
   Copyright 1992 Free Software Foundation, Inc.

   Contributed by Fred Fish at Cygnus Support.   fnf@cygnus.com

This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

#include "mmprivate.h"

/* Terminate access to a mmalloc managed region by unmapping all memory pages
 * associated with the region, and closing the file descriptor if it is one
 * that we opened.
 *
 * Returns NULL on success.
 *
 * Returns the malloc descriptor on failure, which can subsequently be used
 * for further action, such as obtaining more information about the nature of
 * the failure by examining the preserved errno value.
 *
 * Note that the malloc descriptor that we are using is currently located in
 * region we are about to unmap, so we first make a local copy of it on the
 * stack and use the copy.
 */

PTR
mmalloc_detach (md)
     PTR md;
{
  struct mdesc *mdp;
  int fd;

	if (md == NULL) {
		return md;
	}

  mdp = MD_TO_MDP (md);
  fd = mdp -> fd;

	if (mdp->child != NULL) {
		return mmalloc_detach (mdp->child);
	}

  /* Now unmap all the pages associated with this region by asking for a
   * negative increment equal to the current size of the region.
   */
  
#ifdef HAVE_MSYNC
# ifdef MS_SYNC
  msync (mdp -> base, mdp -> top - mdp -> base, MS_SYNC | MS_INVALIDATE);
# else
  msync (mdp -> base, mdp -> top - mdp -> base);
# endif /* MS_SYNC */
#endif /* HAVE_MSYNC */
  munmap (mdp -> base, mdp -> top - mdp -> base);

  if (fd > 0)
    {
      close (fd);
    }
  md = NULL;
  
  return md;
}

/* EOF */

/* exp_printify - printable versions of random ASCII strings

Written by: Don Libes, NIST, 2/6/90

Design and implementation of this program was paid for by U.S. tax
dollars.  Therefore it is public domain.  However, the author and NIST
would appreciate credit if this program or parts of it are used.

*/

#include "expect_cf.h"
#include "tcl.h"
#include <stdio.h>
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# else
#  warning exp_printify.c expects a string-related header to be included.
# endif /* HAVE_STRINGS_H */
#endif /* HAVE_STRING_H */
#ifdef NO_STDLIB_H
# include "../compat/stdlib.h"
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>		/* for malloc */
# else
#  ifdef HAVE_MALLOC_H
#   include <malloc.h>
#  else
#   ifdef HAVE_MALLOC_MALLOC_H
#    include <malloc/malloc.h>
#   else
#    warning exp_printify.c expects a header that provides malloc() to be included.
#   endif /* HAVE_MALLOC_MALLOC_H */
#  endif /* HAVE_MALLOC_H */
# endif /* HAVE_STDLIB_H */
#endif /*NO_STDLIB_H*/
#include <ctype.h>

/* generate printable versions of random ASCII strings. Primarily used */
/* by cmdExpect when -d forces it to print strings it is examining. */
char *
exp_printify(s)
char *s;
{
	static int destlen = 0;
	static char *dest = 0;
	char *d;		/* ptr into dest */
	unsigned int need;

	if (s == 0) return("<null>");

	/* worst case is every character takes 4 to printify */
	need = strlen(s)*4 + 1;
	if (need > destlen) {
		if (dest) ckfree(dest);
		dest = ckalloc(need);
		destlen = need;
	}

	for (d = dest;*s;s++) {
		if (*s == '\r') {
			strcpy(d,"\\r");		d += 2;
		} else if (*s == '\n') {
			strcpy(d,"\\n");		d += 2;
		} else if (*s == '\t') {
			strcpy(d,"\\t");		d += 2;
		} else if (isascii(*s) && isprint(*s)) {
			*d = *s;			d += 1;
		} else {
			sprintf(d,"\\x%02x",*s & 0xff);	d += 4;
		}
	}
	*d = '\0';
	return(dest);
}

/* EOF */

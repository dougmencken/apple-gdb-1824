/* ieee-float.c
   IEEE floating point support routines, for GDB, the GNU Debugger.
   Copyright (C) 1991 Free Software Foundation, Inc.

This file is part of GDB.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "param.h"
#include "ieee-float.h"
#include <math.h>		/* ldexp */

/* Convert an IEEE extended float to a double. FROM is the address
 * of the extended float. Store the double in *TO. */
void ieee_extended_to_double(ext_format, from, to)
     struct ext_format *ext_format;
     char *from;
     double *to;
{
  unsigned char *ufrom = (unsigned char *)from;
  double dto;
  unsigned long mant0, mant1, exponent;

  bcopy(&from[MANBYTE_H], &mant0, 4);
  bcopy(&from[MANBYTE_L], &mant1, 4);
  exponent = ((ufrom[EXPBYTE_H] & (unsigned char)~SIGNMASK) << 8) | ufrom[EXPBYTE_L];

#if 0
  /* We cannot do anything useful with a NaN anyway, so ignore its difference.
   * It will end up as Infinity or something close.  */
  if (exponent == EXT_EXP_NAN) {
    /* We have a NaN source.  */
    dto = 0.123456789;	/* Not much else useful to do -- we do NOT know if
						 * the host system even *has* NaNs, nor how to
						 * generate an innocuous one if it does.  */
  } else
#endif /* 0 */
         if ((exponent == 0) && (mant0 == 0) && (mant1 == 0)) {
    dto = 0;
  } else {
    /* Build the result algebraically.  Might go infinite, underflow, etc;
       who cares. */
    mant0 |= 0x80000000;
    dto = ldexp  ((double)mant0, exponent - EXT_EXP_BIAS - 31);
    dto += ldexp ((double)mant1, exponent - EXT_EXP_BIAS - 31 - 32);
    if (ufrom[EXPBYTE_H] & SIGNMASK)	/* If negative... */
      dto = -dto;			/* ...negate.  */
  }
  *to = dto;
}

/* The converse: convert the double *FROM to an extended float
   and store where TO points.  */

void
double_to_ieee_extended (ext_format, from, to)
     struct ext_format *ext_format;
     double *from;
     char *to;
{
  double dfrom = *from;
  unsigned long twolongs[2];
  unsigned long mant0, mant1, exponent;
  unsigned char tobytes[8];

  bzero (to, TOTALSIZE);
  if (dfrom == 0) {
    return;			/* Result is zero */
  }
  if (dfrom != dfrom) {
    /* From is NaN */
    to[EXPBYTE_H] = (unsigned char)(EXT_EXP_NAN >> 8);
    to[EXPBYTE_L] = (unsigned char)EXT_EXP_NAN;
    to[MANBYTE_H] = 1; /* Be sure it is not infinity, but NaN value is irrel */
    return;			/* Result is NaN */
  }
  if (dfrom < 0) {
    to[SIGNBYTE] |= SIGNMASK;	/* Set negative sign */
  }
  /* How to tell an infinity from an ordinary number?  FIXME-someday */

  /* The following code assumes that the host has IEEE doubles.  FIXME-someday.
   * It also assumes longs are 32 bits!  FIXME-someday.  */
  bcopy(from, twolongs, 8);
  bcopy(from, tobytes, 8);
#if HOST_BYTE_ORDER == BIG_ENDIAN
  exponent = ((tobytes[1] & 0xF0) >> 4) | (tobytes[0] & 0x7F) << 4;
  mant0 = (twolongs[0] << 11) | twolongs[1] >> 21;
  mant1 = (twolongs[1] << 11);
#else
  exponent = ((tobytes[6] & 0xF0) >> 4) | (tobytes[7] & 0x7F) << 4;
  mant0 = (twolongs[1] << 11) | twolongs[0] >> 21;
  mant1 = (twolongs[0] << 11);
#endif /* HOST_BYTE_ORDER */

  /* Fiddle with leading 1-bit, implied in double, explicit in extended: */
  if (exponent == 0) {
    mant0 &= 0x7FFFFFFF;
  } else {
    mant0 |= 0x80000000;
  }

  exponent -= DBL_EXP_BIAS;				/* Get integer exp */
  exponent += EXT_EXP_BIAS;				/* Offset for extended */

  /* OK, now store it in extended format: */
  to[EXPBYTE_H] |= (unsigned char)(exponent >> 8);	/* Retain sign */
  to[EXPBYTE_L] =  (unsigned char) exponent;

  bcopy(&mant0, &to[MANBYTE_H], 4);
  bcopy(&mant1, &to[MANBYTE_L], 4);
}


#ifdef DEBUG
/* Test some numbers to see that extended/double conversion works for them: */
ieee_test(n)
     int n;
{
  union { double d; int i[2]; } di;
  double result;
  int i;
  char exten[16];
  extern struct ext_format ext_format_68881;

  for ((i = 0); (i < n); i++) {
    di.i[0] = (random() << 16) | (random() & 0xffff);
    di.i[1] = (random() << 16) | (random() & 0xffff);
    double_to_ieee_extended (&ext_format_68881, &di.d, exten);
    ieee_extended_to_double (&ext_format_68881, exten, &result);
	  if (di.d != result) {
		  printf ("Differ: %x %x %g => %x %x %g\n", di.d, di.d, result, result);
	  }
  }
}

#endif /* DEBUG */

/* EOF */

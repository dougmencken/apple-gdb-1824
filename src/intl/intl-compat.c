/* intl-compat.c: Stub functions to call gettext functions from GNU gettext
 * Library.
 * Copyright (C) 1995 Software Foundation, Inc. */
/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, 51 Franklin Street - 5th Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#else
# if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#  warning intl-compat.c expects "config.h" to be included.
# endif /* __GNUC__ && !__STRICT_ANSI__ */
#endif /* HAVE_CONFIG_H */

#include "libgettext.h"

/* @@ end of prolog @@ */


#undef gettext
#undef dgettext
#undef dcgettext
#undef textdomain
#undef bindtextdomain


char *bindtextdomain(const char *domainname, const char *dirname)
{
  return bindtextdomain__(domainname, dirname);
}


char *dcgettext(const char *domainname, const char *msgid, int category)
{
  return dcgettext__(domainname, msgid, category);
}


char *dgettext(const char *domainname, const char *msgid)
{
  return dgettext__(domainname, msgid);
}


char *gettext(const char *msgid)
{
  return gettext__(msgid);
}


char *textdomain(const char *domainname)
{
  return textdomain__(domainname);
}

/* EOF */

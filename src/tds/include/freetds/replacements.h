/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _replacements_h_
#define _replacements_h_

#include <stdarg.h>
#include "tds_sysdep_public.h"
#include <freetds/sysdep_private.h>

/* these headers are needed for basename */
# include <string.h>

#if !HAVE_POLL
#include <freetds/replacements/poll.h>
#endif /* !HAVE_POLL */

#ifdef __cplusplus
extern "C"
{
#endif

#if !HAVE_STRLCPY
size_t tds_strlcpy(char *dest, const char *src, size_t len);
#undef strlcpy
#define strlcpy(d,s,l) tds_strlcpy(d,s,l)
#endif


#ifndef AI_FQDN
#define AI_FQDN 0
#endif

/* 
 * Microsoft's C Runtime library is missing strcasecmp and strncasecmp. 
 * Other Win32 C runtime libraries, notably MinGW, may define it.
 * There is no symbol uniquely defined in Microsoft's header files that 
 * can be used by the preprocessor to know whether we're compiling for
 * Microsoft's library or not (or which version).  Thus there's no
 * way to automatically decide whether or not to define strcasecmp
 * in terms of stricmp.  
 * 
 * The Microsoft *compiler* defines _MSC_VER.  On the assumption that
 * anyone using their compiler is also using their library, the below 
 * tests check _MSC_VER as a proxy. 
 */
#if defined(_WIN32)
# if !defined(strcasecmp) && defined(_MSC_VER) 
#     define  strcasecmp(A, B) stricmp((A), (B))
# endif
# if !defined(strncasecmp) && defined(_MSC_VER)
#     define  strncasecmp(x,y,z) strnicmp((x),(y),(z))
# endif

#undef gettimeofday
int tds_gettimeofday (struct timeval *tv, void *tz);
#define gettimeofday tds_gettimeofday

/* Older MinGW-w64 versions don't define these flags. */
#if defined(__MINGW32__) && !defined(AI_ADDRCONFIG)
#  define AI_ADDRCONFIG 0x00000400
#endif
#if defined(__MINGW32__) && !defined(AI_V4MAPPED)
#  define AI_V4MAPPED 0x00000800
#endif

#endif

#if defined(_WIN32) && defined(_MSC_VER)
#define tds_strtoll _strtoi64
#else
#define tds_strtoll strtoll
#endif

#if !HAVE_SOCKETPAIR
int tds_socketpair(int domain, int type, int protocol, TDS_SYS_SOCKET sv[2]);
#define socketpair(d,t,p,s) tds_socketpair(d,t,p,s)
#endif

#ifdef __cplusplus
}
#endif

#endif

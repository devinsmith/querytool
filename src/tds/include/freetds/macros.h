/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2010-2017  Frediano Ziglio
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

#ifndef _freetds_macros_h_
#define _freetds_macros_h_

#include <stddef.h>

#include "tds_sysdep_public.h"

#define TDS_ZERO_FREE(x) do {free((x)); (x) = NULL;} while(0)
#define TDS_VECTOR_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define TDS_OFFSET(type, field) offsetof(type, field)

#if defined(__GNUC__) && __GNUC__ >= 3
# define TDS_LIKELY(x)	__builtin_expect(!!(x), 1)
# define TDS_UNLIKELY(x)	__builtin_expect(!!(x), 0)
#else
# define TDS_LIKELY(x)	(x)
# define TDS_UNLIKELY(x)	(x)
#endif

#define TDS_INT2PTR(i) ((void*)(((char*)0)+((intptr_t)(i))))
#define TDS_PTR2INT(p) ((int)(((char*)(p))-((char*)0)))

#endif

/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
 * Copyright (C) 2005-2015  Frediano Ziglio
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

#include <config.h>

#include <stdio.h>

#include <sys/time.h>
#include <time.h>

#if defined(HAVE_GETUID) && defined(HAVE_GETPWUID)
#include <pwd.h>
#endif

#include <string.h>

#include <freetds/utils.h>

struct tm *
tds_localtime_r(const time_t *timep, struct tm *result)
{
	struct tm *tm;

#if defined(_REENTRANT) && !defined(_WIN32)
#if HAVE_FUNC_LOCALTIME_R_TM
	tm = localtime_r(timep, result);
#else
	tm = NULL;
	if (!localtime_r(timep, result))
		tm = result;
#endif /* HAVE_FUNC_LOCALTIME_R_TM */
#else
	tm = localtime(timep);
	if (tm) {
		memcpy(result, tm, sizeof(*result));
		tm = result;
	}
#endif
	return tm;
}

char *
tds_timestamp_str(char *str, int maxlen)
{
#if !defined(_WIN32) && !defined(_WIN64)
	struct tm *tm;
	struct tm res;
	time_t t;

#if HAVE_GETTIMEOFDAY
	struct timeval tv;
	char usecs[10];

	gettimeofday(&tv, NULL);
	t = tv.tv_sec;
#else
	/*
	 * XXX Need to get a better time resolution for
	 * systems that don't have gettimeofday().
	 */
	time(&t);
#endif

	tm = tds_localtime_r(&t, &res);

/**	strftime(str, maxlen - 6, "%Y-%m-%d %H:%M:%S", tm); **/
	strftime(str, maxlen - 6, "%H:%M:%S", tm);

#if HAVE_GETTIMEOFDAY
	sprintf(usecs, ".%06lu", (long) tv.tv_usec);
	strcat(str, usecs);
#endif

#else /* _WIN32 */
	SYSTEMTIME st;

	GetLocalTime(&st);
	_snprintf(str, maxlen - 1, "%02u:%02u:%02u.%03u", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	str[maxlen - 1] = 0;
#endif

	return str;
}




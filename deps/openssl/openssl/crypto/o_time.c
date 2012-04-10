/* crypto/o_time.c -*- mode:C; c-file-style: "eay" -*- */
/* Written by Richard Levitte (richard@levitte.org) for the OpenSSL
 * project 2001.
 */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2008.
 */
/* ====================================================================
 * Copyright (c) 2001 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <openssl/e_os2.h>
#include <string.h>
#include "o_time.h"

#ifdef OPENSSL_SYS_VMS
# if __CRTL_VER >= 70000000 && \
     (defined _POSIX_C_SOURCE || !defined _ANSI_C_SOURCE)
#  define VMS_GMTIME_OK
# endif
# ifndef VMS_GMTIME_OK
#  include <libdtdef.h>
#  include <lib$routines.h>
#  include <lnmdef.h>
#  include <starlet.h>
#  include <descrip.h>
#  include <stdlib.h>
# endif /* ndef VMS_GMTIME_OK */
#endif

struct tm *OPENSSL_gmtime(const time_t *timer, struct tm *result)
	{
	struct tm *ts = NULL;

#if defined(OPENSSL_THREADS) && !defined(OPENSSL_SYS_WIN32) && !defined(OPENSSL_SYS_OS2) && (!defined(OPENSSL_SYS_VMS) || defined(gmtime_r)) && !defined(OPENSSL_SYS_MACOSX) && !defined(OPENSSL_SYS_SUNOS)
	/* should return &data, but doesn't on some systems,
	   so we don't even look at the return value */
	gmtime_r(timer,result);
	ts = result;
#elif !defined(OPENSSL_SYS_VMS) || defined(VMS_GMTIME_OK)
	ts = gmtime(timer);
	if (ts == NULL)
		return NULL;

	memcpy(result, ts, sizeof(struct tm));
	ts = result;
#endif
#if defined( OPENSSL_SYS_VMS) && !defined( VMS_GMTIME_OK)
	if (ts == NULL)
		{
		static $DESCRIPTOR(tabnam,"LNM$DCL_LOGICAL");
		static $DESCRIPTOR(lognam,"SYS$TIMEZONE_DIFFERENTIAL");
		char logvalue[256];
		unsigned int reslen = 0;
		struct {
			short buflen;
			short code;
			void *bufaddr;
			unsigned int *reslen;
		} itemlist[] = {
			{ 0, LNM$_STRING, 0, 0 },
			{ 0, 0, 0, 0 },
		};
		int status;
		time_t t;

		/* Get the value for SYS$TIMEZONE_DIFFERENTIAL */
		itemlist[0].buflen = sizeof(logvalue);
		itemlist[0].bufaddr = logvalue;
		itemlist[0].reslen = &reslen;
		status = sys$trnlnm(0, &tabnam, &lognam, 0, itemlist);
		if (!(status & 1))
			return NULL;
		logvalue[reslen] = '\0';

		t = *timer;

/* The following is extracted from the DEC C header time.h */
/*
**  Beginning in OpenVMS Version 7.0 mktime, time, ctime, strftime
**  have two implementations.  One implementation is provided
**  for compatibility and deals with time in terms of local time,
**  the other __utc_* deals with time in terms of UTC.
*/
/* We use the same conditions as in said time.h to check if we should
   assume that t contains local time (and should therefore be adjusted)
   or UTC (and should therefore be left untouched). */
#if __CRTL_VER < 70000000 || defined _VMS_V6_SOURCE
		/* Get the numerical value of the equivalence string */
		status = atoi(logvalue);

		/* and use it to move time to GMT */
		t -= status;
#endif

		/* then convert the result to the time structure */

		/* Since there was no gmtime_r() to do this stuff for us,
		   we have to do it the hard way. */
		{
		/* The VMS epoch is the astronomical Smithsonian date,
		   if I remember correctly, which is November 17, 1858.
		   Furthermore, time is measure in thenths of microseconds
		   and stored in quadwords (64 bit integers).  unix_epoch
		   below is January 1st 1970 expressed as a VMS time.  The
		   following code was used to get this number:

		   #include <stdio.h>
		   #include <stdlib.h>
		   #include <lib$routines.h>
		   #include <starlet.h>

		   main()
		   {
		     unsigned long systime[2];
		     unsigned short epoch_values[7] =
		       { 1970, 1, 1, 0, 0, 0, 0 };

		     lib$cvt_vectim(epoch_values, systime);

		     printf("%u %u", systime[0], systime[1]);
		   }
		*/
		unsigned long unix_epoch[2] = { 1273708544, 8164711 };
		unsigned long deltatime[2];
		unsigned long systime[2];
		struct vms_vectime
			{
			short year, month, day, hour, minute, second,
				centi_second;
			} time_values;
		long operation;

		/* Turn the number of seconds since January 1st 1970 to
		   an internal delta time.
		   Note that lib$cvt_to_internal_time() will assume
		   that t is signed, and will therefore break on 32-bit
		   systems some time in 2038.
		*/
		operation = LIB$K_DELTA_SECONDS;
		status = lib$cvt_to_internal_time(&operation,
			&t, deltatime);

		/* Add the delta time with the Unix epoch and we have
		   the current UTC time in internal format */
		status = lib$add_times(unix_epoch, deltatime, systime);

		/* Turn the internal time into a time vector */
		status = sys$numtim(&time_values, systime);

		/* Fill in the struct tm with the result */
		result->tm_sec = time_values.second;
		result->tm_min = time_values.minute;
		result->tm_hour = time_values.hour;
		result->tm_mday = time_values.day;
		result->tm_mon = time_values.month - 1;
		result->tm_year = time_values.year - 1900;

		operation = LIB$K_DAY_OF_WEEK;
		status = lib$cvt_from_internal_time(&operation,
			&result->tm_wday, systime);
		result->tm_wday %= 7;

		operation = LIB$K_DAY_OF_YEAR;
		status = lib$cvt_from_internal_time(&operation,
			&result->tm_yday, systime);
		result->tm_yday--;

		result->tm_isdst = 0; /* There's no way to know... */

		ts = result;
		}
		}
#endif
	return ts;
	}

/* Take a tm structure and add an offset to it. This avoids any OS issues
 * with restricted date types and overflows which cause the year 2038
 * problem.
 */

#define SECS_PER_DAY (24 * 60 * 60)

static long date_to_julian(int y, int m, int d);
static void julian_to_date(long jd, int *y, int *m, int *d);

int OPENSSL_gmtime_adj(struct tm *tm, int off_day, long offset_sec)
	{
	int offset_hms, offset_day;
	long time_jd;
	int time_year, time_month, time_day;
	/* split offset into days and day seconds */
	offset_day = offset_sec / SECS_PER_DAY;
	/* Avoid sign issues with % operator */
	offset_hms  = offset_sec - (offset_day * SECS_PER_DAY);
	offset_day += off_day;
	/* Add current time seconds to offset */
	offset_hms += tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
	/* Adjust day seconds if overflow */
	if (offset_hms >= SECS_PER_DAY)
		{
		offset_day++;
		offset_hms -= SECS_PER_DAY;
		}
	else if (offset_hms < 0)
		{
		offset_day--;
		offset_hms += SECS_PER_DAY;
		}

	/* Convert date of time structure into a Julian day number.
	 */

	time_year = tm->tm_year + 1900;
	time_month = tm->tm_mon + 1;
	time_day = tm->tm_mday;

	time_jd = date_to_julian(time_year, time_month, time_day);

	/* Work out Julian day of new date */
	time_jd += offset_day;

	if (time_jd < 0)
		return 0;

	/* Convert Julian day back to date */

	julian_to_date(time_jd, &time_year, &time_month, &time_day);

	if (time_year < 1900 || time_year > 9999)
		return 0;

	/* Update tm structure */

	tm->tm_year = time_year - 1900;
	tm->tm_mon = time_month - 1;
	tm->tm_mday = time_day;

	tm->tm_hour = offset_hms / 3600;
	tm->tm_min = (offset_hms / 60) % 60;
	tm->tm_sec = offset_hms % 60;

	return 1;
		
}

/* Convert date to and from julian day
 * Uses Fliegel & Van Flandern algorithm
 */
static long date_to_julian(int y, int m, int d)
{
	return (1461 * (y + 4800 + (m - 14) / 12)) / 4 +
		(367 * (m - 2 - 12 * ((m - 14) / 12))) / 12 -
		(3 * ((y + 4900 + (m - 14) / 12) / 100)) / 4 +
		d - 32075;
}

static void julian_to_date(long jd, int *y, int *m, int *d)
	{
	long  L = jd + 68569;
	long  n = (4 * L) / 146097;
	long  i, j;

	L = L - (146097 * n + 3) / 4;
	i = (4000 * (L + 1)) / 1461001;
	L = L - (1461 * i) / 4 + 31;
	j = (80 * L) / 2447;
	*d = L - (2447 * j) / 80;
	L = j / 11;
	*m = j + 2 - (12 * L);
	*y = 100 * (n - 49) + i + L;
	}

#ifdef OPENSSL_TIME_TEST

#include <stdio.h>

/* Time checking test code. Check times are identical for a wide range of
 * offsets. This should be run on a machine with 64 bit time_t or it will
 * trigger the very errors the routines fix.
 */

int main(int argc, char **argv)
	{
	long offset;
	for (offset = 0; offset < 1000000; offset++)
		{
		check_time(offset);
		check_time(-offset);
		check_time(offset * 1000);
		check_time(-offset * 1000);
		}
	}

int check_time(long offset)
	{
	struct tm tm1, tm2;
	time_t t1, t2;
	time(&t1);
	t2 = t1 + offset;
	OPENSSL_gmtime(&t2, &tm2);
	OPENSSL_gmtime(&t1, &tm1);
	OPENSSL_gmtime_adj(&tm1, 0, offset);
	if ((tm1.tm_year == tm2.tm_year) &&
	    (tm1.tm_mon == tm2.tm_mon) &&
	    (tm1.tm_mday == tm2.tm_mday) &&
	    (tm1.tm_hour == tm2.tm_hour) &&
	    (tm1.tm_min == tm2.tm_min) &&
	    (tm1.tm_sec == tm2.tm_sec))
		return 1;
	fprintf(stderr, "TIME ERROR!!\n");
	fprintf(stderr, "Time1: %d/%d/%d, %d:%02d:%02d\n",
			tm2.tm_mday, tm2.tm_mon + 1, tm2.tm_year + 1900,
			tm2.tm_hour, tm2.tm_min, tm2.tm_sec);
	fprintf(stderr, "Time2: %d/%d/%d, %d:%02d:%02d\n",
			tm1.tm_mday, tm1.tm_mon + 1, tm1.tm_year + 1900,
			tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
	return 0;
	}

#endif

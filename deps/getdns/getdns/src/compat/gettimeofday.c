/*
 * Copyright (c) 2016 Christian Huitema <huitema@huitema.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

 /*
  * Numerous places in the code make reference to the Unix/Linux
  * "gettimeofday()" function, which is not available in the standard
  * windows libraries. This code provides a compatible implementation.
  */
#include "config.h"

#ifndef HAVE_GETTIMEOFDAY

int gettimeofday(struct timeval* tv, void* tz)
{
	FILETIME ft;
	uint64_t now = 0;

	/*
	 * The GetSystemTimeAsFileTime API returns  the number
	 * of 100-nanosecond intervals since January 1, 1601 (UTC),
	 * in FILETIME format.
	 */
	GetSystemTimeAsFileTime(&ft);

	/*
	 * Convert to plain 64 bit format, without making
	 * assumptions about the FILETIME structure alignment.
	 */
	now |= ft.dwHighDateTime;
	now <<= 32;
	now |= ft.dwLowDateTime;
	/*
	 * Convert units from 100ns to 1us
	 */
	now /= 10;
	/*
	 * Account for microseconds elapsed between 1601 and 1970.
	 */
	now -= 11644473600000000ULL;

	if (tv != NULL)
	{
		uint64_t sec = now / 1000000;
		uint64_t usec = now % 1000000;

		tv->tv_sec = (long)sec;
		tv->tv_usec = (long)usec;
	}

	if (tz != NULL)
	{
	    /*
		 * TODO: implement a timezone retrieval function.
		 * Not urgent, since the GetDNS code always set this parameter to NULL.
		 */ 
		return -1;
	}

	return 0;
}
#endif /* HAVE_GETTIMEOFDAY */

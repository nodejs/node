/*	$OpenBSD: explicit_bzero.c,v 1.3 2014/06/21 02:34:26 matthew Exp $ */
/*
 * Public domain.
 * Written by Matthew Dempsky.
 */
#include "config.h"
#include <string.h>

void
explicit_bzero(void *buf, size_t len)
{
#ifdef GETDNS_ON_WINDOWS
	SecureZeroMemory(buf, len);
#else
	memset(buf, 0, len);
#endif
}

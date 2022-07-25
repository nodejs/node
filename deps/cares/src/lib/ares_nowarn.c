
/* Copyright (C) 2010-2013 by Daniel Stenberg
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */


#include "ares_setup.h"

#ifdef HAVE_ASSERT_H
#  include <assert.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#if defined(__INTEL_COMPILER) && defined(__unix__)

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#endif /* __INTEL_COMPILER && __unix__ */

#define BUILDING_ARES_NOWARN_C 1

#include "ares_nowarn.h"

#ifndef HAVE_LIMITS_H
/* systems without <limits.h> we guess have 16 bit shorts, 32bit ints and
   32bit longs */
#  define CARES_MASK_SSHORT  0x7FFF
#  define CARES_MASK_USHORT  0xFFFF
#  define CARES_MASK_SINT    0x7FFFFFFF
#  define CARES_MASK_UINT    0xFFFFFFFF
#  define CARES_MASK_SLONG   0x7FFFFFFFL
#  define CARES_MASK_ULONG   0xFFFFFFFFUL
#else
#  define CARES_MASK_SSHORT  SHRT_MAX
#  define CARES_MASK_USHORT  USHRT_MAX
#  define CARES_MASK_SINT    INT_MAX
#  define CARES_MASK_UINT    UINT_MAX
#  define CARES_MASK_SLONG   LONG_MAX
#  define CARES_MASK_ULONG   ULONG_MAX
#endif

/*
** unsigned size_t to signed long
*/

long aresx_uztosl(size_t uznum)
{
#ifdef __INTEL_COMPILER
#  pragma warning(push)
#  pragma warning(disable:810) /* conversion may lose significant bits */
#endif

  return (long)(uznum & (size_t) CARES_MASK_SLONG);

#ifdef __INTEL_COMPILER
#  pragma warning(pop)
#endif
}

/*
** unsigned size_t to signed int
*/

int aresx_uztosi(size_t uznum)
{
#ifdef __INTEL_COMPILER
#  pragma warning(push)
#  pragma warning(disable:810) /* conversion may lose significant bits */
#endif

  return (int)(uznum & (size_t) CARES_MASK_SINT);

#ifdef __INTEL_COMPILER
#  pragma warning(pop)
#endif
}

/*
** unsigned size_t to signed short
*/

short aresx_uztoss(size_t uznum)
{
#ifdef __INTEL_COMPILER
#  pragma warning(push)
#  pragma warning(disable:810) /* conversion may lose significant bits */
#endif

  return (short)(uznum & (size_t) CARES_MASK_SSHORT);

#ifdef __INTEL_COMPILER
#  pragma warning(pop)
#endif
}

/*
** signed int to signed short
*/

short aresx_sitoss(int sinum)
{
#ifdef __INTEL_COMPILER
#  pragma warning(push)
#  pragma warning(disable:810) /* conversion may lose significant bits */
#endif

  DEBUGASSERT(sinum >= 0);
  return (short)(sinum & (int) CARES_MASK_SSHORT);

#ifdef __INTEL_COMPILER
#  pragma warning(pop)
#endif
}

/*
** signed long to signed int
*/

int aresx_sltosi(long slnum)
{
#ifdef __INTEL_COMPILER
#  pragma warning(push)
#  pragma warning(disable:810) /* conversion may lose significant bits */
#endif

  DEBUGASSERT(slnum >= 0);
  return (int)(slnum & (long) CARES_MASK_SINT);

#ifdef __INTEL_COMPILER
#  pragma warning(pop)
#endif
}

/*
** signed ares_ssize_t to signed int
*/

int aresx_sztosi(ares_ssize_t sznum)
{
#ifdef __INTEL_COMPILER
#  pragma warning(push)
#  pragma warning(disable:810) /* conversion may lose significant bits */
#endif

  DEBUGASSERT(sznum >= 0);
  return (int)(sznum & (ares_ssize_t) CARES_MASK_SINT);

#ifdef __INTEL_COMPILER
#  pragma warning(pop)
#endif
}

/*
** signed ares_ssize_t to unsigned int
*/

unsigned int aresx_sztoui(ares_ssize_t sznum)
{
#ifdef __INTEL_COMPILER
#  pragma warning(push)
#  pragma warning(disable:810) /* conversion may lose significant bits */
#endif

  DEBUGASSERT(sznum >= 0);
  return (unsigned int)(sznum & (ares_ssize_t) CARES_MASK_UINT);

#ifdef __INTEL_COMPILER
#  pragma warning(pop)
#endif
}

/*
** signed int to unsigned short
*/

unsigned short aresx_sitous(int sinum)
{
#ifdef __INTEL_COMPILER
#  pragma warning(push)
#  pragma warning(disable:810) /* conversion may lose significant bits */
#endif

  DEBUGASSERT(sinum >= 0);
  return (unsigned short)(sinum & (int) CARES_MASK_USHORT);

#ifdef __INTEL_COMPILER
#  pragma warning(pop)
#endif
}

#if defined(__INTEL_COMPILER) && defined(__unix__)

int aresx_FD_ISSET(int fd, fd_set *fdset)
{
  #pragma warning(push)
  #pragma warning(disable:1469) /* clobber ignored */
  return FD_ISSET(fd, fdset);
  #pragma warning(pop)
}

void aresx_FD_SET(int fd, fd_set *fdset)
{
  #pragma warning(push)
  #pragma warning(disable:1469) /* clobber ignored */
  FD_SET(fd, fdset);
  #pragma warning(pop)
}

void aresx_FD_ZERO(fd_set *fdset)
{
  #pragma warning(push)
  #pragma warning(disable:593) /* variable was set but never used */
  FD_ZERO(fdset);
  #pragma warning(pop)
}

unsigned short aresx_htons(unsigned short usnum)
{
#if (__INTEL_COMPILER == 910) && defined(__i386__)
  return (unsigned short)(((usnum << 8) & 0xFF00) | ((usnum >> 8) & 0x00FF));
#else
  #pragma warning(push)
  #pragma warning(disable:810) /* conversion may lose significant bits */
  return htons(usnum);
  #pragma warning(pop)
#endif
}

unsigned short aresx_ntohs(unsigned short usnum)
{
#if (__INTEL_COMPILER == 910) && defined(__i386__)
  return (unsigned short)(((usnum << 8) & 0xFF00) | ((usnum >> 8) & 0x00FF));
#else
  #pragma warning(push)
  #pragma warning(disable:810) /* conversion may lose significant bits */
  return ntohs(usnum);
  #pragma warning(pop)
#endif
}

#endif /* __INTEL_COMPILER && __unix__ */

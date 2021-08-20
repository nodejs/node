
/* Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright (C) 2004-2009 by Daniel Stenberg
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

#include "ares.h"
#include "ares_library_init.h"
#include "ares_private.h"

/* library-private global and unique instance vars */

#ifdef USE_WINSOCK
fpGetNetworkParams_t ares_fpGetNetworkParams = ZERO_NULL;
fpSystemFunction036_t ares_fpSystemFunction036 = ZERO_NULL;
fpGetAdaptersAddresses_t ares_fpGetAdaptersAddresses = ZERO_NULL;
fpGetBestRoute2_t ares_fpGetBestRoute2 = ZERO_NULL;
#endif

#if defined(ANDROID) || defined(__ANDROID__)
#include "ares_android.h"
#endif

/* library-private global vars with source visibility restricted to this file */

static unsigned int ares_initialized;
static int          ares_init_flags;

/* library-private global vars with visibility across the whole library */

/* Some systems may return either NULL or a valid pointer on malloc(0).  c-ares should
 * never call malloc(0) so lets return NULL so we're more likely to find an issue if it
 * were to occur. */

static void *default_malloc(size_t size) { if (size == 0) { return NULL; } return malloc(size); }

#if defined(WIN32)
/* We need indirections to handle Windows DLL rules. */
static void *default_realloc(void *p, size_t size) { return realloc(p, size); }
static void default_free(void *p) { free(p); }
#else
# define default_realloc realloc
# define default_free free
#endif
void *(*ares_malloc)(size_t size) = default_malloc;
void *(*ares_realloc)(void *ptr, size_t size) = default_realloc;
void (*ares_free)(void *ptr) = default_free;

#ifdef USE_WINSOCK
static HMODULE hnd_iphlpapi;
static HMODULE hnd_advapi32;
#endif


static int ares_win32_init(void)
{
#ifdef USE_WINSOCK

  hnd_iphlpapi = 0;
  hnd_iphlpapi = LoadLibraryW(L"iphlpapi.dll");
  if (!hnd_iphlpapi)
    return ARES_ELOADIPHLPAPI;

  ares_fpGetNetworkParams = (fpGetNetworkParams_t)
    GetProcAddress(hnd_iphlpapi, "GetNetworkParams");
  if (!ares_fpGetNetworkParams)
    {
      FreeLibrary(hnd_iphlpapi);
      return ARES_EADDRGETNETWORKPARAMS;
    }

  ares_fpGetAdaptersAddresses = (fpGetAdaptersAddresses_t)
    GetProcAddress(hnd_iphlpapi, "GetAdaptersAddresses");
  if (!ares_fpGetAdaptersAddresses)
    {
      /* This can happen on clients before WinXP, I don't
         think it should be an error, unless we don't want to
         support Windows 2000 anymore */
    }

  ares_fpGetBestRoute2 = (fpGetBestRoute2_t)
    GetProcAddress(hnd_iphlpapi, "GetBestRoute2");
  if (!ares_fpGetBestRoute2)
    {
      /* This can happen on clients before Vista, I don't
         think it should be an error, unless we don't want to
         support Windows XP anymore */
    }

  /*
   * When advapi32.dll is unavailable or advapi32.dll has no SystemFunction036,
   * also known as RtlGenRandom, which is the case for Windows versions prior
   * to WinXP then c-ares uses portable rand() function. Then don't error here.
   */

  hnd_advapi32 = 0;
  hnd_advapi32 = LoadLibraryW(L"advapi32.dll");
  if (hnd_advapi32)
    {
      ares_fpSystemFunction036 = (fpSystemFunction036_t)
        GetProcAddress(hnd_advapi32, "SystemFunction036");
    }

#endif
  return ARES_SUCCESS;
}


static void ares_win32_cleanup(void)
{
#ifdef USE_WINSOCK
  if (hnd_advapi32)
    FreeLibrary(hnd_advapi32);
  if (hnd_iphlpapi)
    FreeLibrary(hnd_iphlpapi);
#endif
}


int ares_library_init(int flags)
{
  int res;

  if (ares_initialized)
    {
      ares_initialized++;
      return ARES_SUCCESS;
    }
  ares_initialized++;

  if (flags & ARES_LIB_INIT_WIN32)
    {
      res = ares_win32_init();
      if (res != ARES_SUCCESS)
        return res;  /* LCOV_EXCL_LINE: can't test Win32 init failure */
    }

  ares_init_flags = flags;

  return ARES_SUCCESS;
}

int ares_library_init_mem(int flags,
                          void *(*amalloc)(size_t size),
                          void (*afree)(void *ptr),
                          void *(*arealloc)(void *ptr, size_t size))
{
  if (amalloc)
    ares_malloc = amalloc;
  if (arealloc)
    ares_realloc = arealloc;
  if (afree)
    ares_free = afree;
  return ares_library_init(flags);
}


void ares_library_cleanup(void)
{
  if (!ares_initialized)
    return;
  ares_initialized--;
  if (ares_initialized)
    return;

  if (ares_init_flags & ARES_LIB_INIT_WIN32)
    ares_win32_cleanup();

#if defined(ANDROID) || defined(__ANDROID__)
  ares_library_cleanup_android();
#endif

  ares_init_flags = ARES_LIB_INIT_NONE;
  ares_malloc = malloc;
  ares_realloc = realloc;
  ares_free = free;
}


int ares_library_initialized(void)
{
#ifdef USE_WINSOCK
  if (!ares_initialized)
    return ARES_ENOTINITIALIZED;
#endif
  return ARES_SUCCESS;
}

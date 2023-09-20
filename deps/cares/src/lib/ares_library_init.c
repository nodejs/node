
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
#include "ares_private.h"

/* library-private global and unique instance vars */

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

int ares_library_init(int flags)
{
  if (ares_initialized)
    {
      ares_initialized++;
      return ARES_SUCCESS;
    }
  ares_initialized++;

  /* NOTE: ARES_LIB_INIT_WIN32 flag no longer used */

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

  /* NOTE: ARES_LIB_INIT_WIN32 flag no longer used */

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

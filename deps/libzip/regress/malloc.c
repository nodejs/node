/*
  malloc.c -- override *alloc() to allow testing special cases
  Copyright (C) 2015-2021 Dieter Baron and Thomas Klausner

  This file is part of ckmame, a program to check rom sets for MAME.
  The authors can be contacted at <ckmame@nih.at>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
  3. The name of the author may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
/* #include <string.h> */
#include <errno.h>
#define __USE_GNU
#include <dlfcn.h>
#undef __USE_GNU

#include "config.h"

#if !defined(RTLD_NEXT)
#define RTLD_NEXT RTLD_DEFAULT
#endif

#if defined(HAVE___PROGNAME)
extern char *__progname;
#endif

#if defined(HAVE_GETPROGNAME)
/* all fine */
#else
const char *
getprogname(void) {
#if defined(HAVE___PROGNAME)
    return __progname;
#else
    return NULL;
#endif
}
#endif

static int inited = 0;
static size_t count = 0;
static size_t max_count = 0;
static size_t min_size = 0;
static void *(*real_malloc)(size_t size) = NULL;
static void *(*real_calloc)(size_t number, size_t size) = NULL;
static void *(*real_realloc)(void *ptr, size_t size) = NULL;

static const char *myname = NULL;

/* TODO: add sentinel -- check if particular size is malloced before doing other stuff */
/* TODO: catch free for sentinel too */
/* TODO: optionally, catch malloc of particular size */

static void
init(void) {
    char *foo;
    myname = getprogname();
    if (!myname)
        myname = "(unknown)";
    if ((foo = getenv("MALLOC_MAX_COUNT")) != NULL)
        max_count = strtoul(foo, NULL, 0);
    if ((foo = getenv("MALLOC_MIN_SIZE")) != NULL)
        min_size = strtoul(foo, NULL, 0);
    real_calloc = dlsym(RTLD_NEXT, "calloc");
    if (!real_calloc)
        abort();
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    if (!real_malloc)
        abort();
    real_realloc = dlsym(RTLD_NEXT, "realloc");
    if (!real_realloc)
        abort();
    inited = 1;
}

void *
calloc(size_t number, size_t size) {
    void *ret;

    if (!inited) {
        init();
    }

    if (number >= min_size / size && count >= max_count) {
        errno = ENOMEM;
        return NULL;
    }

    ret = real_calloc(number, size);
    if (size >= min_size) {
        count++;
    }

    return ret;
}

void *
malloc(size_t size) {
    void *ret;

    if (!inited) {
        init();
    }

    if (size >= min_size && count >= max_count) {
        errno = ENOMEM;
        return NULL;
    }

    ret = real_malloc(size);
    if (size >= min_size) {
        count++;
    }

    return ret;
}

void *
realloc(void *ptr, size_t size) {
    void *ret;

    if (!inited) {
        init();
    }

    if (size >= min_size && count >= max_count) {
        errno = ENOMEM;
        return NULL;
    }

    ret = real_realloc(ptr, size);
    if (size >= min_size) {
        count++;
    }

    return ret;
}

/*
** Bundled memory allocator.
** Donated to the public domain.
*/

#ifndef _LJ_ALLOC_H
#define _LJ_ALLOC_H

#include "lj_def.h"

#ifndef LUAJIT_USE_SYSMALLOC
LJ_FUNC void *lj_alloc_create(void);
LJ_FUNC void lj_alloc_destroy(void *msp);
LJ_FUNC void *lj_alloc_f(void *msp, void *ptr, size_t osize, size_t nsize);
#endif

#endif

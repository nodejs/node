/*
 * xxHash - Extremely Fast Hash algorithm
 * Copyright (c) Yann Collet - Meta Platforms, Inc
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/*
 * xxhash.c instantiates functions defined in xxhash.h
 */

#define XXH_STATIC_LINKING_ONLY /* access advanced declarations */
#define XXH_IMPLEMENTATION      /* access definitions */

#include "xxhash.h"

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* *****************************************************************************
 * Constructs a dictionary using a heuristic based on the following paper:
 *
 * Liao, Petri, Moffat, Wirth
 * Effective Construction of Relative Lempel-Ziv Dictionaries
 * Published in WWW 2016.
 *
 * Adapted from code originally written by @ot (Giuseppe Ottaviano).
 ******************************************************************************/

/*-*************************************
*  Dependencies
***************************************/
/* qsort_r is an extension. */
#if defined(__linux) || defined(__linux__) || defined(linux) || defined(__gnu_linux__) || \
    defined(__CYGWIN__) || defined(__MSYS__)
#if !defined(_GNU_SOURCE) && !defined(__ANDROID__) /* NDK doesn't ship qsort_r(). */
#define _GNU_SOURCE
#endif
#endif

#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, qsort_r */

#include <string.h> /* memset */
#include <time.h>   /* clock */

#ifndef ZDICT_STATIC_LINKING_ONLY
#  define ZDICT_STATIC_LINKING_ONLY
#endif

#include "../common/mem.h" /* read */
#include "../common/pool.h" /* POOL_ctx */
#include "../common/threading.h" /* ZSTD_pthread_mutex_t */
#include "../common/zstd_internal.h" /* includes zstd.h */
#include "../common/bits.h" /* ZSTD_highbit32 */
#include "../zdict.h"
#include "cover.h"

/*-*************************************
*  Constants
***************************************/
/**
* There are 32bit indexes used to ref samples, so limit samples size to 4GB
* on 64bit builds.
* For 32bit builds we choose 1 GB.
* Most 32bit platforms have 2GB user-mode addressable space and we allocate a large
* contiguous buffer, so 1GB is already a high limit.
*/
#define COVER_MAX_SAMPLES_SIZE (sizeof(size_t) == 8 ? ((unsigned)-1) : ((unsigned)1 GB))
#define COVER_DEFAULT_SPLITPOINT 1.0

/*-*************************************
*  Console display
***************************************/
#ifndef LOCALDISPLAYLEVEL
static int g_displayLevel = 0;
#endif
#undef  DISPLAY
#define DISPLAY(...)                                                           \
  {                                                                            \
    fprintf(stderr, __VA_ARGS__);                                              \
    fflush(stderr);                                                            \
  }
#undef  LOCALDISPLAYLEVEL
#define LOCALDISPLAYLEVEL(displayLevel, l, ...)                                \
  if (displayLevel >= l) {                                                     \
    DISPLAY(__VA_ARGS__);                                                      \
  } /* 0 : no display;   1: errors;   2: default;  3: details;  4: debug */
#undef  DISPLAYLEVEL
#define DISPLAYLEVEL(l, ...) LOCALDISPLAYLEVEL(g_displayLevel, l, __VA_ARGS__)

#ifndef LOCALDISPLAYUPDATE
static const clock_t g_refreshRate = CLOCKS_PER_SEC * 15 / 100;
static clock_t g_time = 0;
#endif
#undef  LOCALDISPLAYUPDATE
#define LOCALDISPLAYUPDATE(displayLevel, l, ...)                               \
  if (displayLevel >= l) {                                                     \
    if ((clock() - g_time > g_refreshRate) || (displayLevel >= 4)) {           \
      g_time = clock();                                                        \
      DISPLAY(__VA_ARGS__);                                                    \
    }                                                                          \
  }
#undef  DISPLAYUPDATE
#define DISPLAYUPDATE(l, ...) LOCALDISPLAYUPDATE(g_displayLevel, l, __VA_ARGS__)

/*-*************************************
* Hash table
***************************************
* A small specialized hash map for storing activeDmers.
* The map does not resize, so if it becomes full it will loop forever.
* Thus, the map must be large enough to store every value.
* The map implements linear probing and keeps its load less than 0.5.
*/

#define MAP_EMPTY_VALUE ((U32)-1)
typedef struct COVER_map_pair_t_s {
  U32 key;
  U32 value;
} COVER_map_pair_t;

typedef struct COVER_map_s {
  COVER_map_pair_t *data;
  U32 sizeLog;
  U32 size;
  U32 sizeMask;
} COVER_map_t;

/**
 * Clear the map.
 */
static void COVER_map_clear(COVER_map_t *map) {
  memset(map->data, MAP_EMPTY_VALUE, map->size * sizeof(COVER_map_pair_t));
}

/**
 * Initializes a map of the given size.
 * Returns 1 on success and 0 on failure.
 * The map must be destroyed with COVER_map_destroy().
 * The map is only guaranteed to be large enough to hold size elements.
 */
static int COVER_map_init(COVER_map_t *map, U32 size) {
  map->sizeLog = ZSTD_highbit32(size) + 2;
  map->size = (U32)1 << map->sizeLog;
  map->sizeMask = map->size - 1;
  map->data = (COVER_map_pair_t *)malloc(map->size * sizeof(COVER_map_pair_t));
  if (!map->data) {
    map->sizeLog = 0;
    map->size = 0;
    return 0;
  }
  COVER_map_clear(map);
  return 1;
}

/**
 * Internal hash function
 */
static const U32 COVER_prime4bytes = 2654435761U;
static U32 COVER_map_hash(COVER_map_t *map, U32 key) {
  return (key * COVER_prime4bytes) >> (32 - map->sizeLog);
}

/**
 * Helper function that returns the index that a key should be placed into.
 */
static U32 COVER_map_index(COVER_map_t *map, U32 key) {
  const U32 hash = COVER_map_hash(map, key);
  U32 i;
  for (i = hash;; i = (i + 1) & map->sizeMask) {
    COVER_map_pair_t *pos = &map->data[i];
    if (pos->value == MAP_EMPTY_VALUE) {
      return i;
    }
    if (pos->key == key) {
      return i;
    }
  }
}

/**
 * Returns the pointer to the value for key.
 * If key is not in the map, it is inserted and the value is set to 0.
 * The map must not be full.
 */
static U32 *COVER_map_at(COVER_map_t *map, U32 key) {
  COVER_map_pair_t *pos = &map->data[COVER_map_index(map, key)];
  if (pos->value == MAP_EMPTY_VALUE) {
    pos->key = key;
    pos->value = 0;
  }
  return &pos->value;
}

/**
 * Deletes key from the map if present.
 */
static void COVER_map_remove(COVER_map_t *map, U32 key) {
  U32 i = COVER_map_index(map, key);
  COVER_map_pair_t *del = &map->data[i];
  U32 shift = 1;
  if (del->value == MAP_EMPTY_VALUE) {
    return;
  }
  for (i = (i + 1) & map->sizeMask;; i = (i + 1) & map->sizeMask) {
    COVER_map_pair_t *const pos = &map->data[i];
    /* If the position is empty we are done */
    if (pos->value == MAP_EMPTY_VALUE) {
      del->value = MAP_EMPTY_VALUE;
      return;
    }
    /* If pos can be moved to del do so */
    if (((i - COVER_map_hash(map, pos->key)) & map->sizeMask) >= shift) {
      del->key = pos->key;
      del->value = pos->value;
      del = pos;
      shift = 1;
    } else {
      ++shift;
    }
  }
}

/**
 * Destroys a map that is inited with COVER_map_init().
 */
static void COVER_map_destroy(COVER_map_t *map) {
  if (map->data) {
    free(map->data);
  }
  map->data = NULL;
  map->size = 0;
}

/*-*************************************
* Context
***************************************/

typedef struct {
  const BYTE *samples;
  size_t *offsets;
  const size_t *samplesSizes;
  size_t nbSamples;
  size_t nbTrainSamples;
  size_t nbTestSamples;
  U32 *suffix;
  size_t suffixSize;
  U32 *freqs;
  U32 *dmerAt;
  unsigned d;
} COVER_ctx_t;

#if !defined(_GNU_SOURCE) && !defined(__APPLE__) && !defined(_MSC_VER)
/* C90 only offers qsort() that needs a global context. */
static COVER_ctx_t *g_coverCtx = NULL;
#endif

/*-*************************************
*  Helper functions
***************************************/

/**
 * Returns the sum of the sample sizes.
 */
size_t COVER_sum(const size_t *samplesSizes, unsigned nbSamples) {
  size_t sum = 0;
  unsigned i;
  for (i = 0; i < nbSamples; ++i) {
    sum += samplesSizes[i];
  }
  return sum;
}

/**
 * Returns -1 if the dmer at lp is less than the dmer at rp.
 * Return 0 if the dmers at lp and rp are equal.
 * Returns 1 if the dmer at lp is greater than the dmer at rp.
 */
static int COVER_cmp(COVER_ctx_t *ctx, const void *lp, const void *rp) {
  U32 const lhs = *(U32 const *)lp;
  U32 const rhs = *(U32 const *)rp;
  return memcmp(ctx->samples + lhs, ctx->samples + rhs, ctx->d);
}
/**
 * Faster version for d <= 8.
 */
static int COVER_cmp8(COVER_ctx_t *ctx, const void *lp, const void *rp) {
  U64 const mask = (ctx->d == 8) ? (U64)-1 : (((U64)1 << (8 * ctx->d)) - 1);
  U64 const lhs = MEM_readLE64(ctx->samples + *(U32 const *)lp) & mask;
  U64 const rhs = MEM_readLE64(ctx->samples + *(U32 const *)rp) & mask;
  if (lhs < rhs) {
    return -1;
  }
  return (lhs > rhs);
}

/**
 * Same as COVER_cmp() except ties are broken by pointer value
 */
#if (defined(_WIN32) && defined(_MSC_VER)) || defined(__APPLE__)
static int WIN_CDECL COVER_strict_cmp(void* g_coverCtx, const void* lp, const void* rp) {
#elif defined(_GNU_SOURCE)
static int COVER_strict_cmp(const void *lp, const void *rp, void *g_coverCtx) {
#else /* C90 fallback.*/
static int COVER_strict_cmp(const void *lp, const void *rp) {
#endif
  int result = COVER_cmp((COVER_ctx_t*)g_coverCtx, lp, rp);
  if (result == 0) {
    result = lp < rp ? -1 : 1;
  }
  return result;
}
/**
 * Faster version for d <= 8.
 */
#if (defined(_WIN32) && defined(_MSC_VER)) || defined(__APPLE__)
static int WIN_CDECL COVER_strict_cmp8(void* g_coverCtx, const void* lp, const void* rp) {
#elif defined(_GNU_SOURCE)
static int COVER_strict_cmp8(const void *lp, const void *rp, void *g_coverCtx) {
#else /* C90 fallback.*/
static int COVER_strict_cmp8(const void *lp, const void *rp) {
#endif
  int result = COVER_cmp8((COVER_ctx_t*)g_coverCtx, lp, rp);
  if (result == 0) {
    result = lp < rp ? -1 : 1;
  }
  return result;
}

/**
 * Abstract away divergence of qsort_r() parameters.
 * Hopefully when C11 become the norm, we will be able
 * to clean it up.
 */
static void stableSort(COVER_ctx_t *ctx) {
#if defined(__APPLE__)
    qsort_r(ctx->suffix, ctx->suffixSize, sizeof(U32),
            ctx,
            (ctx->d <= 8 ? &COVER_strict_cmp8 : &COVER_strict_cmp));
#elif defined(_GNU_SOURCE)
    qsort_r(ctx->suffix, ctx->suffixSize, sizeof(U32),
            (ctx->d <= 8 ? &COVER_strict_cmp8 : &COVER_strict_cmp),
            ctx);
#elif defined(_WIN32) && defined(_MSC_VER)
    qsort_s(ctx->suffix, ctx->suffixSize, sizeof(U32),
            (ctx->d <= 8 ? &COVER_strict_cmp8 : &COVER_strict_cmp),
            ctx);
#elif defined(__OpenBSD__)
    g_coverCtx = ctx;
    mergesort(ctx->suffix, ctx->suffixSize, sizeof(U32),
          (ctx->d <= 8 ? &COVER_strict_cmp8 : &COVER_strict_cmp));
#else /* C90 fallback.*/
    g_coverCtx = ctx;
    /* TODO(cavalcanti): implement a reentrant qsort() when is not available. */
    qsort(ctx->suffix, ctx->suffixSize, sizeof(U32),
          (ctx->d <= 8 ? &COVER_strict_cmp8 : &COVER_strict_cmp));
#endif
}

/**
 * Returns the first pointer in [first, last) whose element does not compare
 * less than value.  If no such element exists it returns last.
 */
static const size_t *COVER_lower_bound(const size_t* first, const size_t* last,
                                       size_t value) {
  size_t count = (size_t)(last - first);
  assert(last >= first);
  while (count != 0) {
    size_t step = count / 2;
    const size_t *ptr = first;
    ptr += step;
    if (*ptr < value) {
      first = ++ptr;
      count -= step + 1;
    } else {
      count = step;
    }
  }
  return first;
}

/**
 * Generic groupBy function.
 * Groups an array sorted by cmp into groups with equivalent values.
 * Calls grp for each group.
 */
static void
COVER_groupBy(const void *data, size_t count, size_t size, COVER_ctx_t *ctx,
              int (*cmp)(COVER_ctx_t *, const void *, const void *),
              void (*grp)(COVER_ctx_t *, const void *, const void *)) {
  const BYTE *ptr = (const BYTE *)data;
  size_t num = 0;
  while (num < count) {
    const BYTE *grpEnd = ptr + size;
    ++num;
    while (num < count && cmp(ctx, ptr, grpEnd) == 0) {
      grpEnd += size;
      ++num;
    }
    grp(ctx, ptr, grpEnd);
    ptr = grpEnd;
  }
}

/*-*************************************
*  Cover functions
***************************************/

/**
 * Called on each group of positions with the same dmer.
 * Counts the frequency of each dmer and saves it in the suffix array.
 * Fills `ctx->dmerAt`.
 */
static void COVER_group(COVER_ctx_t *ctx, const void *group,
                        const void *groupEnd) {
  /* The group consists of all the positions with the same first d bytes. */
  const U32 *grpPtr = (const U32 *)group;
  const U32 *grpEnd = (const U32 *)groupEnd;
  /* The dmerId is how we will reference this dmer.
   * This allows us to map the whole dmer space to a much smaller space, the
   * size of the suffix array.
   */
  const U32 dmerId = (U32)(grpPtr - ctx->suffix);
  /* Count the number of samples this dmer shows up in */
  U32 freq = 0;
  /* Details */
  const size_t *curOffsetPtr = ctx->offsets;
  const size_t *offsetsEnd = ctx->offsets + ctx->nbSamples;
  /* Once *grpPtr >= curSampleEnd this occurrence of the dmer is in a
   * different sample than the last.
   */
  size_t curSampleEnd = ctx->offsets[0];
  for (; grpPtr != grpEnd; ++grpPtr) {
    /* Save the dmerId for this position so we can get back to it. */
    ctx->dmerAt[*grpPtr] = dmerId;
    /* Dictionaries only help for the first reference to the dmer.
     * After that zstd can reference the match from the previous reference.
     * So only count each dmer once for each sample it is in.
     */
    if (*grpPtr < curSampleEnd) {
      continue;
    }
    freq += 1;
    /* Binary search to find the end of the sample *grpPtr is in.
     * In the common case that grpPtr + 1 == grpEnd we can skip the binary
     * search because the loop is over.
     */
    if (grpPtr + 1 != grpEnd) {
      const size_t *sampleEndPtr =
          COVER_lower_bound(curOffsetPtr, offsetsEnd, *grpPtr);
      curSampleEnd = *sampleEndPtr;
      curOffsetPtr = sampleEndPtr + 1;
    }
  }
  /* At this point we are never going to look at this segment of the suffix
   * array again.  We take advantage of this fact to save memory.
   * We store the frequency of the dmer in the first position of the group,
   * which is dmerId.
   */
  ctx->suffix[dmerId] = freq;
}


/**
 * Selects the best segment in an epoch.
 * Segments of are scored according to the function:
 *
 * Let F(d) be the frequency of dmer d.
 * Let S_i be the dmer at position i of segment S which has length k.
 *
 *     Score(S) = F(S_1) + F(S_2) + ... + F(S_{k-d+1})
 *
 * Once the dmer d is in the dictionary we set F(d) = 0.
 */
static COVER_segment_t COVER_selectSegment(const COVER_ctx_t *ctx, U32 *freqs,
                                           COVER_map_t *activeDmers, U32 begin,
                                           U32 end,
                                           ZDICT_cover_params_t parameters) {
  /* Constants */
  const U32 k = parameters.k;
  const U32 d = parameters.d;
  const U32 dmersInK = k - d + 1;
  /* Try each segment (activeSegment) and save the best (bestSegment) */
  COVER_segment_t bestSegment = {0, 0, 0};
  COVER_segment_t activeSegment;
  /* Reset the activeDmers in the segment */
  COVER_map_clear(activeDmers);
  /* The activeSegment starts at the beginning of the epoch. */
  activeSegment.begin = begin;
  activeSegment.end = begin;
  activeSegment.score = 0;
  /* Slide the activeSegment through the whole epoch.
   * Save the best segment in bestSegment.
   */
  while (activeSegment.end < end) {
    /* The dmerId for the dmer at the next position */
    U32 newDmer = ctx->dmerAt[activeSegment.end];
    /* The entry in activeDmers for this dmerId */
    U32 *newDmerOcc = COVER_map_at(activeDmers, newDmer);
    /* If the dmer isn't already present in the segment add its score. */
    if (*newDmerOcc == 0) {
      /* The paper suggest using the L-0.5 norm, but experiments show that it
       * doesn't help.
       */
      activeSegment.score += freqs[newDmer];
    }
    /* Add the dmer to the segment */
    activeSegment.end += 1;
    *newDmerOcc += 1;

    /* If the window is now too large, drop the first position */
    if (activeSegment.end - activeSegment.begin == dmersInK + 1) {
      U32 delDmer = ctx->dmerAt[activeSegment.begin];
      U32 *delDmerOcc = COVER_map_at(activeDmers, delDmer);
      activeSegment.begin += 1;
      *delDmerOcc -= 1;
      /* If this is the last occurrence of the dmer, subtract its score */
      if (*delDmerOcc == 0) {
        COVER_map_remove(activeDmers, delDmer);
        activeSegment.score -= freqs[delDmer];
      }
    }

    /* If this segment is the best so far save it */
    if (activeSegment.score > bestSegment.score) {
      bestSegment = activeSegment;
    }
  }
  {
    /* Trim off the zero frequency head and tail from the segment. */
    U32 newBegin = bestSegment.end;
    U32 newEnd = bestSegment.begin;
    U32 pos;
    for (pos = bestSegment.begin; pos != bestSegment.end; ++pos) {
      U32 freq = freqs[ctx->dmerAt[pos]];
      if (freq != 0) {
        newBegin = MIN(newBegin, pos);
        newEnd = pos + 1;
      }
    }
    bestSegment.begin = newBegin;
    bestSegment.end = newEnd;
  }
  {
    /* Zero out the frequency of each dmer covered by the chosen segment. */
    U32 pos;
    for (pos = bestSegment.begin; pos != bestSegment.end; ++pos) {
      freqs[ctx->dmerAt[pos]] = 0;
    }
  }
  return bestSegment;
}

/**
 * Check the validity of the parameters.
 * Returns non-zero if the parameters are valid and 0 otherwise.
 */
static int COVER_checkParameters(ZDICT_cover_params_t parameters,
                                 size_t maxDictSize) {
  /* k and d are required parameters */
  if (parameters.d == 0 || parameters.k == 0) {
    return 0;
  }
  /* k <= maxDictSize */
  if (parameters.k > maxDictSize) {
    return 0;
  }
  /* d <= k */
  if (parameters.d > parameters.k) {
    return 0;
  }
  /* 0 < splitPoint <= 1 */
  if (parameters.splitPoint <= 0 || parameters.splitPoint > 1){
    return 0;
  }
  return 1;
}

/**
 * Clean up a context initialized with `COVER_ctx_init()`.
 */
static void COVER_ctx_destroy(COVER_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  if (ctx->suffix) {
    free(ctx->suffix);
    ctx->suffix = NULL;
  }
  if (ctx->freqs) {
    free(ctx->freqs);
    ctx->freqs = NULL;
  }
  if (ctx->dmerAt) {
    free(ctx->dmerAt);
    ctx->dmerAt = NULL;
  }
  if (ctx->offsets) {
    free(ctx->offsets);
    ctx->offsets = NULL;
  }
}

/**
 * Prepare a context for dictionary building.
 * The context is only dependent on the parameter `d` and can be used multiple
 * times.
 * Returns 0 on success or error code on error.
 * The context must be destroyed with `COVER_ctx_destroy()`.
 */
static size_t COVER_ctx_init(COVER_ctx_t *ctx, const void *samplesBuffer,
                          const size_t *samplesSizes, unsigned nbSamples,
                          unsigned d, double splitPoint)
{
  const BYTE *const samples = (const BYTE *)samplesBuffer;
  const size_t totalSamplesSize = COVER_sum(samplesSizes, nbSamples);
  /* Split samples into testing and training sets */
  const unsigned nbTrainSamples = splitPoint < 1.0 ? (unsigned)((double)nbSamples * splitPoint) : nbSamples;
  const unsigned nbTestSamples = splitPoint < 1.0 ? nbSamples - nbTrainSamples : nbSamples;
  const size_t trainingSamplesSize = splitPoint < 1.0 ? COVER_sum(samplesSizes, nbTrainSamples) : totalSamplesSize;
  const size_t testSamplesSize = splitPoint < 1.0 ? COVER_sum(samplesSizes + nbTrainSamples, nbTestSamples) : totalSamplesSize;
  /* Checks */
  if (totalSamplesSize < MAX(d, sizeof(U64)) ||
      totalSamplesSize >= (size_t)COVER_MAX_SAMPLES_SIZE) {
    DISPLAYLEVEL(1, "Total samples size is too large (%u MB), maximum size is %u MB\n",
                 (unsigned)(totalSamplesSize>>20), (COVER_MAX_SAMPLES_SIZE >> 20));
    return ERROR(srcSize_wrong);
  }
  /* Check if there are at least 5 training samples */
  if (nbTrainSamples < 5) {
    DISPLAYLEVEL(1, "Total number of training samples is %u and is invalid.", nbTrainSamples);
    return ERROR(srcSize_wrong);
  }
  /* Check if there's testing sample */
  if (nbTestSamples < 1) {
    DISPLAYLEVEL(1, "Total number of testing samples is %u and is invalid.", nbTestSamples);
    return ERROR(srcSize_wrong);
  }
  /* Zero the context */
  memset(ctx, 0, sizeof(*ctx));
  DISPLAYLEVEL(2, "Training on %u samples of total size %u\n", nbTrainSamples,
               (unsigned)trainingSamplesSize);
  DISPLAYLEVEL(2, "Testing on %u samples of total size %u\n", nbTestSamples,
               (unsigned)testSamplesSize);
  ctx->samples = samples;
  ctx->samplesSizes = samplesSizes;
  ctx->nbSamples = nbSamples;
  ctx->nbTrainSamples = nbTrainSamples;
  ctx->nbTestSamples = nbTestSamples;
  /* Partial suffix array */
  ctx->suffixSize = trainingSamplesSize - MAX(d, sizeof(U64)) + 1;
  ctx->suffix = (U32 *)malloc(ctx->suffixSize * sizeof(U32));
  /* Maps index to the dmerID */
  ctx->dmerAt = (U32 *)malloc(ctx->suffixSize * sizeof(U32));
  /* The offsets of each file */
  ctx->offsets = (size_t *)malloc((nbSamples + 1) * sizeof(size_t));
  if (!ctx->suffix || !ctx->dmerAt || !ctx->offsets) {
    DISPLAYLEVEL(1, "Failed to allocate scratch buffers\n");
    COVER_ctx_destroy(ctx);
    return ERROR(memory_allocation);
  }
  ctx->freqs = NULL;
  ctx->d = d;

  /* Fill offsets from the samplesSizes */
  {
    U32 i;
    ctx->offsets[0] = 0;
    for (i = 1; i <= nbSamples; ++i) {
      ctx->offsets[i] = ctx->offsets[i - 1] + samplesSizes[i - 1];
    }
  }
  DISPLAYLEVEL(2, "Constructing partial suffix array\n");
  {
    /* suffix is a partial suffix array.
     * It only sorts suffixes by their first parameters.d bytes.
     * The sort is stable, so each dmer group is sorted by position in input.
     */
    U32 i;
    for (i = 0; i < ctx->suffixSize; ++i) {
      ctx->suffix[i] = i;
    }
    stableSort(ctx);
  }
  DISPLAYLEVEL(2, "Computing frequencies\n");
  /* For each dmer group (group of positions with the same first d bytes):
   * 1. For each position we set dmerAt[position] = dmerID.  The dmerID is
   *    (groupBeginPtr - suffix).  This allows us to go from position to
   *    dmerID so we can look up values in freq.
   * 2. We calculate how many samples the dmer occurs in and save it in
   *    freqs[dmerId].
   */
  COVER_groupBy(ctx->suffix, ctx->suffixSize, sizeof(U32), ctx,
                (ctx->d <= 8 ? &COVER_cmp8 : &COVER_cmp), &COVER_group);
  ctx->freqs = ctx->suffix;
  ctx->suffix = NULL;
  return 0;
}

void COVER_warnOnSmallCorpus(size_t maxDictSize, size_t nbDmers, int displayLevel)
{
  const double ratio = (double)nbDmers / (double)maxDictSize;
  if (ratio >= 10) {
      return;
  }
  LOCALDISPLAYLEVEL(displayLevel, 1,
                    "WARNING: The maximum dictionary size %u is too large "
                    "compared to the source size %u! "
                    "size(source)/size(dictionary) = %f, but it should be >= "
                    "10! This may lead to a subpar dictionary! We recommend "
                    "training on sources at least 10x, and preferably 100x "
                    "the size of the dictionary! \n", (U32)maxDictSize,
                    (U32)nbDmers, ratio);
}

COVER_epoch_info_t COVER_computeEpochs(U32 maxDictSize,
                                       U32 nbDmers, U32 k, U32 passes)
{
  const U32 minEpochSize = k * 10;
  COVER_epoch_info_t epochs;
  epochs.num = MAX(1, maxDictSize / k / passes);
  epochs.size = nbDmers / epochs.num;
  if (epochs.size >= minEpochSize) {
      assert(epochs.size * epochs.num <= nbDmers);
      return epochs;
  }
  epochs.size = MIN(minEpochSize, nbDmers);
  epochs.num = nbDmers / epochs.size;
  assert(epochs.size * epochs.num <= nbDmers);
  return epochs;
}

/**
 * Given the prepared context build the dictionary.
 */
static size_t COVER_buildDictionary(const COVER_ctx_t *ctx, U32 *freqs,
                                    COVER_map_t *activeDmers, void *dictBuffer,
                                    size_t dictBufferCapacity,
                                    ZDICT_cover_params_t parameters) {
  BYTE *const dict = (BYTE *)dictBuffer;
  size_t tail = dictBufferCapacity;
  /* Divide the data into epochs. We will select one segment from each epoch. */
  const COVER_epoch_info_t epochs = COVER_computeEpochs(
      (U32)dictBufferCapacity, (U32)ctx->suffixSize, parameters.k, 4);
  const size_t maxZeroScoreRun = MAX(10, MIN(100, epochs.num >> 3));
  size_t zeroScoreRun = 0;
  size_t epoch;
  DISPLAYLEVEL(2, "Breaking content into %u epochs of size %u\n",
                (U32)epochs.num, (U32)epochs.size);
  /* Loop through the epochs until there are no more segments or the dictionary
   * is full.
   */
  for (epoch = 0; tail > 0; epoch = (epoch + 1) % epochs.num) {
    const U32 epochBegin = (U32)(epoch * epochs.size);
    const U32 epochEnd = epochBegin + epochs.size;
    size_t segmentSize;
    /* Select a segment */
    COVER_segment_t segment = COVER_selectSegment(
        ctx, freqs, activeDmers, epochBegin, epochEnd, parameters);
    /* If the segment covers no dmers, then we are out of content.
     * There may be new content in other epochs, for continue for some time.
     */
    if (segment.score == 0) {
      if (++zeroScoreRun >= maxZeroScoreRun) {
          break;
      }
      continue;
    }
    zeroScoreRun = 0;
    /* Trim the segment if necessary and if it is too small then we are done */
    segmentSize = MIN(segment.end - segment.begin + parameters.d - 1, tail);
    if (segmentSize < parameters.d) {
      break;
    }
    /* We fill the dictionary from the back to allow the best segments to be
     * referenced with the smallest offsets.
     */
    tail -= segmentSize;
    memcpy(dict + tail, ctx->samples + segment.begin, segmentSize);
    DISPLAYUPDATE(
        2, "\r%u%%       ",
        (unsigned)(((dictBufferCapacity - tail) * 100) / dictBufferCapacity));
  }
  DISPLAYLEVEL(2, "\r%79s\r", "");
  return tail;
}

ZDICTLIB_STATIC_API size_t ZDICT_trainFromBuffer_cover(
    void *dictBuffer, size_t dictBufferCapacity,
    const void *samplesBuffer, const size_t *samplesSizes, unsigned nbSamples,
    ZDICT_cover_params_t parameters)
{
  BYTE* const dict = (BYTE*)dictBuffer;
  COVER_ctx_t ctx;
  COVER_map_t activeDmers;
  parameters.splitPoint = 1.0;
  /* Initialize global data */
  g_displayLevel = (int)parameters.zParams.notificationLevel;
  /* Checks */
  if (!COVER_checkParameters(parameters, dictBufferCapacity)) {
    DISPLAYLEVEL(1, "Cover parameters incorrect\n");
    return ERROR(parameter_outOfBound);
  }
  if (nbSamples == 0) {
    DISPLAYLEVEL(1, "Cover must have at least one input file\n");
    return ERROR(srcSize_wrong);
  }
  if (dictBufferCapacity < ZDICT_DICTSIZE_MIN) {
    DISPLAYLEVEL(1, "dictBufferCapacity must be at least %u\n",
                 ZDICT_DICTSIZE_MIN);
    return ERROR(dstSize_tooSmall);
  }
  /* Initialize context and activeDmers */
  {
    size_t const initVal = COVER_ctx_init(&ctx, samplesBuffer, samplesSizes, nbSamples,
                      parameters.d, parameters.splitPoint);
    if (ZSTD_isError(initVal)) {
      return initVal;
    }
  }
  COVER_warnOnSmallCorpus(dictBufferCapacity, ctx.suffixSize, g_displayLevel);
  if (!COVER_map_init(&activeDmers, parameters.k - parameters.d + 1)) {
    DISPLAYLEVEL(1, "Failed to allocate dmer map: out of memory\n");
    COVER_ctx_destroy(&ctx);
    return ERROR(memory_allocation);
  }

  DISPLAYLEVEL(2, "Building dictionary\n");
  {
    const size_t tail =
        COVER_buildDictionary(&ctx, ctx.freqs, &activeDmers, dictBuffer,
                              dictBufferCapacity, parameters);
    const size_t dictionarySize = ZDICT_finalizeDictionary(
        dict, dictBufferCapacity, dict + tail, dictBufferCapacity - tail,
        samplesBuffer, samplesSizes, nbSamples, parameters.zParams);
    if (!ZSTD_isError(dictionarySize)) {
      DISPLAYLEVEL(2, "Constructed dictionary of size %u\n",
                   (unsigned)dictionarySize);
    }
    COVER_ctx_destroy(&ctx);
    COVER_map_destroy(&activeDmers);
    return dictionarySize;
  }
}



size_t COVER_checkTotalCompressedSize(const ZDICT_cover_params_t parameters,
                                    const size_t *samplesSizes, const BYTE *samples,
                                    size_t *offsets,
                                    size_t nbTrainSamples, size_t nbSamples,
                                    BYTE *const dict, size_t dictBufferCapacity) {
  size_t totalCompressedSize = ERROR(GENERIC);
  /* Pointers */
  ZSTD_CCtx *cctx;
  ZSTD_CDict *cdict;
  void *dst;
  /* Local variables */
  size_t dstCapacity;
  size_t i;
  /* Allocate dst with enough space to compress the maximum sized sample */
  {
    size_t maxSampleSize = 0;
    i = parameters.splitPoint < 1.0 ? nbTrainSamples : 0;
    for (; i < nbSamples; ++i) {
      maxSampleSize = MAX(samplesSizes[i], maxSampleSize);
    }
    dstCapacity = ZSTD_compressBound(maxSampleSize);
    dst = malloc(dstCapacity);
  }
  /* Create the cctx and cdict */
  cctx = ZSTD_createCCtx();
  cdict = ZSTD_createCDict(dict, dictBufferCapacity,
                           parameters.zParams.compressionLevel);
  if (!dst || !cctx || !cdict) {
    goto _compressCleanup;
  }
  /* Compress each sample and sum their sizes (or error) */
  totalCompressedSize = dictBufferCapacity;
  i = parameters.splitPoint < 1.0 ? nbTrainSamples : 0;
  for (; i < nbSamples; ++i) {
    const size_t size = ZSTD_compress_usingCDict(
        cctx, dst, dstCapacity, samples + offsets[i],
        samplesSizes[i], cdict);
    if (ZSTD_isError(size)) {
      totalCompressedSize = size;
      goto _compressCleanup;
    }
    totalCompressedSize += size;
  }
_compressCleanup:
  ZSTD_freeCCtx(cctx);
  ZSTD_freeCDict(cdict);
  if (dst) {
    free(dst);
  }
  return totalCompressedSize;
}


/**
 * Initialize the `COVER_best_t`.
 */
void COVER_best_init(COVER_best_t *best) {
  if (best==NULL) return; /* compatible with init on NULL */
  (void)ZSTD_pthread_mutex_init(&best->mutex, NULL);
  (void)ZSTD_pthread_cond_init(&best->cond, NULL);
  best->liveJobs = 0;
  best->dict = NULL;
  best->dictSize = 0;
  best->compressedSize = (size_t)-1;
  memset(&best->parameters, 0, sizeof(best->parameters));
}

/**
 * Wait until liveJobs == 0.
 */
void COVER_best_wait(COVER_best_t *best) {
  if (!best) {
    return;
  }
  ZSTD_pthread_mutex_lock(&best->mutex);
  while (best->liveJobs != 0) {
    ZSTD_pthread_cond_wait(&best->cond, &best->mutex);
  }
  ZSTD_pthread_mutex_unlock(&best->mutex);
}

/**
 * Call COVER_best_wait() and then destroy the COVER_best_t.
 */
void COVER_best_destroy(COVER_best_t *best) {
  if (!best) {
    return;
  }
  COVER_best_wait(best);
  if (best->dict) {
    free(best->dict);
  }
  ZSTD_pthread_mutex_destroy(&best->mutex);
  ZSTD_pthread_cond_destroy(&best->cond);
}

/**
 * Called when a thread is about to be launched.
 * Increments liveJobs.
 */
void COVER_best_start(COVER_best_t *best) {
  if (!best) {
    return;
  }
  ZSTD_pthread_mutex_lock(&best->mutex);
  ++best->liveJobs;
  ZSTD_pthread_mutex_unlock(&best->mutex);
}

/**
 * Called when a thread finishes executing, both on error or success.
 * Decrements liveJobs and signals any waiting threads if liveJobs == 0.
 * If this dictionary is the best so far save it and its parameters.
 */
void COVER_best_finish(COVER_best_t* best,
                      ZDICT_cover_params_t parameters,
                      COVER_dictSelection_t selection)
{
  void* dict = selection.dictContent;
  size_t compressedSize = selection.totalCompressedSize;
  size_t dictSize = selection.dictSize;
  if (!best) {
    return;
  }
  {
    size_t liveJobs;
    ZSTD_pthread_mutex_lock(&best->mutex);
    --best->liveJobs;
    liveJobs = best->liveJobs;
    /* If the new dictionary is better */
    if (compressedSize < best->compressedSize) {
      /* Allocate space if necessary */
      if (!best->dict || best->dictSize < dictSize) {
        if (best->dict) {
          free(best->dict);
        }
        best->dict = malloc(dictSize);
        if (!best->dict) {
          best->compressedSize = ERROR(GENERIC);
          best->dictSize = 0;
          ZSTD_pthread_cond_signal(&best->cond);
          ZSTD_pthread_mutex_unlock(&best->mutex);
          return;
        }
      }
      /* Save the dictionary, parameters, and size */
      if (dict) {
        memcpy(best->dict, dict, dictSize);
        best->dictSize = dictSize;
        best->parameters = parameters;
        best->compressedSize = compressedSize;
      }
    }
    if (liveJobs == 0) {
      ZSTD_pthread_cond_broadcast(&best->cond);
    }
    ZSTD_pthread_mutex_unlock(&best->mutex);
  }
}

static COVER_dictSelection_t setDictSelection(BYTE* buf, size_t s, size_t csz)
{
    COVER_dictSelection_t ds;
    ds.dictContent = buf;
    ds.dictSize = s;
    ds.totalCompressedSize = csz;
    return ds;
}

COVER_dictSelection_t COVER_dictSelectionError(size_t error) {
    return setDictSelection(NULL, 0, error);
}

unsigned COVER_dictSelectionIsError(COVER_dictSelection_t selection) {
  return (ZSTD_isError(selection.totalCompressedSize) || !selection.dictContent);
}

void COVER_dictSelectionFree(COVER_dictSelection_t selection){
  free(selection.dictContent);
}

COVER_dictSelection_t COVER_selectDict(BYTE* customDictContent, size_t dictBufferCapacity,
        size_t dictContentSize, const BYTE* samplesBuffer, const size_t* samplesSizes, unsigned nbFinalizeSamples,
        size_t nbCheckSamples, size_t nbSamples, ZDICT_cover_params_t params, size_t* offsets, size_t totalCompressedSize) {

  size_t largestDict = 0;
  size_t largestCompressed = 0;
  BYTE* customDictContentEnd = customDictContent + dictContentSize;

  BYTE* largestDictbuffer = (BYTE*)malloc(dictBufferCapacity);
  BYTE* candidateDictBuffer = (BYTE*)malloc(dictBufferCapacity);
  double regressionTolerance = ((double)params.shrinkDictMaxRegression / 100.0) + 1.00;

  if (!largestDictbuffer || !candidateDictBuffer) {
    free(largestDictbuffer);
    free(candidateDictBuffer);
    return COVER_dictSelectionError(dictContentSize);
  }

  /* Initial dictionary size and compressed size */
  memcpy(largestDictbuffer, customDictContent, dictContentSize);
  dictContentSize = ZDICT_finalizeDictionary(
    largestDictbuffer, dictBufferCapacity, customDictContent, dictContentSize,
    samplesBuffer, samplesSizes, nbFinalizeSamples, params.zParams);

  if (ZDICT_isError(dictContentSize)) {
    free(largestDictbuffer);
    free(candidateDictBuffer);
    return COVER_dictSelectionError(dictContentSize);
  }

  totalCompressedSize = COVER_checkTotalCompressedSize(params, samplesSizes,
                                                       samplesBuffer, offsets,
                                                       nbCheckSamples, nbSamples,
                                                       largestDictbuffer, dictContentSize);

  if (ZSTD_isError(totalCompressedSize)) {
    free(largestDictbuffer);
    free(candidateDictBuffer);
    return COVER_dictSelectionError(totalCompressedSize);
  }

  if (params.shrinkDict == 0) {
    free(candidateDictBuffer);
    return setDictSelection(largestDictbuffer, dictContentSize, totalCompressedSize);
  }

  largestDict = dictContentSize;
  largestCompressed = totalCompressedSize;
  dictContentSize = ZDICT_DICTSIZE_MIN;

  /* Largest dict is initially at least ZDICT_DICTSIZE_MIN */
  while (dictContentSize < largestDict) {
    memcpy(candidateDictBuffer, largestDictbuffer, largestDict);
    dictContentSize = ZDICT_finalizeDictionary(
      candidateDictBuffer, dictBufferCapacity, customDictContentEnd - dictContentSize, dictContentSize,
      samplesBuffer, samplesSizes, nbFinalizeSamples, params.zParams);

    if (ZDICT_isError(dictContentSize)) {
      free(largestDictbuffer);
      free(candidateDictBuffer);
      return COVER_dictSelectionError(dictContentSize);

    }

    totalCompressedSize = COVER_checkTotalCompressedSize(params, samplesSizes,
                                                         samplesBuffer, offsets,
                                                         nbCheckSamples, nbSamples,
                                                         candidateDictBuffer, dictContentSize);

    if (ZSTD_isError(totalCompressedSize)) {
      free(largestDictbuffer);
      free(candidateDictBuffer);
      return COVER_dictSelectionError(totalCompressedSize);
    }

    if ((double)totalCompressedSize <= (double)largestCompressed * regressionTolerance) {
      free(largestDictbuffer);
      return setDictSelection( candidateDictBuffer, dictContentSize, totalCompressedSize );
    }
    dictContentSize *= 2;
  }
  dictContentSize = largestDict;
  totalCompressedSize = largestCompressed;
  free(candidateDictBuffer);
  return setDictSelection( largestDictbuffer, dictContentSize, totalCompressedSize );
}

/**
 * Parameters for COVER_tryParameters().
 */
typedef struct COVER_tryParameters_data_s {
  const COVER_ctx_t *ctx;
  COVER_best_t *best;
  size_t dictBufferCapacity;
  ZDICT_cover_params_t parameters;
} COVER_tryParameters_data_t;

/**
 * Tries a set of parameters and updates the COVER_best_t with the results.
 * This function is thread safe if zstd is compiled with multithreaded support.
 * It takes its parameters as an *OWNING* opaque pointer to support threading.
 */
static void COVER_tryParameters(void *opaque)
{
  /* Save parameters as local variables */
  COVER_tryParameters_data_t *const data = (COVER_tryParameters_data_t*)opaque;
  const COVER_ctx_t *const ctx = data->ctx;
  const ZDICT_cover_params_t parameters = data->parameters;
  size_t dictBufferCapacity = data->dictBufferCapacity;
  size_t totalCompressedSize = ERROR(GENERIC);
  /* Allocate space for hash table, dict, and freqs */
  COVER_map_t activeDmers;
  BYTE* const dict = (BYTE*)malloc(dictBufferCapacity);
  COVER_dictSelection_t selection = COVER_dictSelectionError(ERROR(GENERIC));
  U32* const freqs = (U32*)malloc(ctx->suffixSize * sizeof(U32));
  if (!COVER_map_init(&activeDmers, parameters.k - parameters.d + 1)) {
    DISPLAYLEVEL(1, "Failed to allocate dmer map: out of memory\n");
    goto _cleanup;
  }
  if (!dict || !freqs) {
    DISPLAYLEVEL(1, "Failed to allocate buffers: out of memory\n");
    goto _cleanup;
  }
  /* Copy the frequencies because we need to modify them */
  memcpy(freqs, ctx->freqs, ctx->suffixSize * sizeof(U32));
  /* Build the dictionary */
  {
    const size_t tail = COVER_buildDictionary(ctx, freqs, &activeDmers, dict,
                                              dictBufferCapacity, parameters);
    selection = COVER_selectDict(dict + tail, dictBufferCapacity, dictBufferCapacity - tail,
        ctx->samples, ctx->samplesSizes, (unsigned)ctx->nbTrainSamples, ctx->nbTrainSamples, ctx->nbSamples, parameters, ctx->offsets,
        totalCompressedSize);

    if (COVER_dictSelectionIsError(selection)) {
      DISPLAYLEVEL(1, "Failed to select dictionary\n");
      goto _cleanup;
    }
  }
_cleanup:
  free(dict);
  COVER_best_finish(data->best, parameters, selection);
  free(data);
  COVER_map_destroy(&activeDmers);
  COVER_dictSelectionFree(selection);
  free(freqs);
}

ZDICTLIB_STATIC_API size_t ZDICT_optimizeTrainFromBuffer_cover(
    void* dictBuffer, size_t dictBufferCapacity, const void* samplesBuffer,
    const size_t* samplesSizes, unsigned nbSamples,
    ZDICT_cover_params_t* parameters)
{
  /* constants */
  const unsigned nbThreads = parameters->nbThreads;
  const double splitPoint =
      parameters->splitPoint <= 0.0 ? COVER_DEFAULT_SPLITPOINT : parameters->splitPoint;
  const unsigned kMinD = parameters->d == 0 ? 6 : parameters->d;
  const unsigned kMaxD = parameters->d == 0 ? 8 : parameters->d;
  const unsigned kMinK = parameters->k == 0 ? 50 : parameters->k;
  const unsigned kMaxK = parameters->k == 0 ? 2000 : parameters->k;
  const unsigned kSteps = parameters->steps == 0 ? 40 : parameters->steps;
  const unsigned kStepSize = MAX((kMaxK - kMinK) / kSteps, 1);
  const unsigned kIterations =
      (1 + (kMaxD - kMinD) / 2) * (1 + (kMaxK - kMinK) / kStepSize);
  const unsigned shrinkDict = 0;
  /* Local variables */
  const int displayLevel = parameters->zParams.notificationLevel;
  unsigned iteration = 1;
  unsigned d;
  unsigned k;
  COVER_best_t best;
  POOL_ctx *pool = NULL;
  int warned = 0;

  /* Checks */
  if (splitPoint <= 0 || splitPoint > 1) {
    LOCALDISPLAYLEVEL(displayLevel, 1, "Incorrect parameters\n");
    return ERROR(parameter_outOfBound);
  }
  if (kMinK < kMaxD || kMaxK < kMinK) {
    LOCALDISPLAYLEVEL(displayLevel, 1, "Incorrect parameters\n");
    return ERROR(parameter_outOfBound);
  }
  if (nbSamples == 0) {
    DISPLAYLEVEL(1, "Cover must have at least one input file\n");
    return ERROR(srcSize_wrong);
  }
  if (dictBufferCapacity < ZDICT_DICTSIZE_MIN) {
    DISPLAYLEVEL(1, "dictBufferCapacity must be at least %u\n",
                 ZDICT_DICTSIZE_MIN);
    return ERROR(dstSize_tooSmall);
  }
  if (nbThreads > 1) {
    pool = POOL_create(nbThreads, 1);
    if (!pool) {
      return ERROR(memory_allocation);
    }
  }
  /* Initialization */
  COVER_best_init(&best);
  /* Turn down global display level to clean up display at level 2 and below */
  g_displayLevel = displayLevel == 0 ? 0 : displayLevel - 1;
  /* Loop through d first because each new value needs a new context */
  LOCALDISPLAYLEVEL(displayLevel, 2, "Trying %u different sets of parameters\n",
                    kIterations);
  for (d = kMinD; d <= kMaxD; d += 2) {
    /* Initialize the context for this value of d */
    COVER_ctx_t ctx;
    LOCALDISPLAYLEVEL(displayLevel, 3, "d=%u\n", d);
    {
      const size_t initVal = COVER_ctx_init(&ctx, samplesBuffer, samplesSizes, nbSamples, d, splitPoint);
      if (ZSTD_isError(initVal)) {
        LOCALDISPLAYLEVEL(displayLevel, 1, "Failed to initialize context\n");
        COVER_best_destroy(&best);
        POOL_free(pool);
        return initVal;
      }
    }
    if (!warned) {
      COVER_warnOnSmallCorpus(dictBufferCapacity, ctx.suffixSize, displayLevel);
      warned = 1;
    }
    /* Loop through k reusing the same context */
    for (k = kMinK; k <= kMaxK; k += kStepSize) {
      /* Prepare the arguments */
      COVER_tryParameters_data_t *data = (COVER_tryParameters_data_t *)malloc(
          sizeof(COVER_tryParameters_data_t));
      LOCALDISPLAYLEVEL(displayLevel, 3, "k=%u\n", k);
      if (!data) {
        LOCALDISPLAYLEVEL(displayLevel, 1, "Failed to allocate parameters\n");
        COVER_best_destroy(&best);
        COVER_ctx_destroy(&ctx);
        POOL_free(pool);
        return ERROR(memory_allocation);
      }
      data->ctx = &ctx;
      data->best = &best;
      data->dictBufferCapacity = dictBufferCapacity;
      data->parameters = *parameters;
      data->parameters.k = k;
      data->parameters.d = d;
      data->parameters.splitPoint = splitPoint;
      data->parameters.steps = kSteps;
      data->parameters.shrinkDict = shrinkDict;
      data->parameters.zParams.notificationLevel = g_displayLevel;
      /* Check the parameters */
      if (!COVER_checkParameters(data->parameters, dictBufferCapacity)) {
        DISPLAYLEVEL(1, "Cover parameters incorrect\n");
        free(data);
        continue;
      }
      /* Call the function and pass ownership of data to it */
      COVER_best_start(&best);
      if (pool) {
        POOL_add(pool, &COVER_tryParameters, data);
      } else {
        COVER_tryParameters(data);
      }
      /* Print status */
      LOCALDISPLAYUPDATE(displayLevel, 2, "\r%u%%       ",
                         (unsigned)((iteration * 100) / kIterations));
      ++iteration;
    }
    COVER_best_wait(&best);
    COVER_ctx_destroy(&ctx);
  }
  LOCALDISPLAYLEVEL(displayLevel, 2, "\r%79s\r", "");
  /* Fill the output buffer and parameters with output of the best parameters */
  {
    const size_t dictSize = best.dictSize;
    if (ZSTD_isError(best.compressedSize)) {
      const size_t compressedSize = best.compressedSize;
      COVER_best_destroy(&best);
      POOL_free(pool);
      return compressedSize;
    }
    *parameters = best.parameters;
    memcpy(dictBuffer, best.dict, dictSize);
    COVER_best_destroy(&best);
    POOL_free(pool);
    return dictSize;
  }
}

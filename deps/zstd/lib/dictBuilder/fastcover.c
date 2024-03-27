/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/*-*************************************
*  Dependencies
***************************************/
#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, free, qsort */
#include <string.h> /* memset */
#include <time.h>   /* clock */

#ifndef ZDICT_STATIC_LINKING_ONLY
#  define ZDICT_STATIC_LINKING_ONLY
#endif

#include "../common/mem.h" /* read */
#include "../common/pool.h"
#include "../common/threading.h"
#include "../common/zstd_internal.h" /* includes zstd.h */
#include "../compress/zstd_compress_internal.h" /* ZSTD_hash*() */
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
#define FASTCOVER_MAX_SAMPLES_SIZE (sizeof(size_t) == 8 ? ((unsigned)-1) : ((unsigned)1 GB))
#define FASTCOVER_MAX_F 31
#define FASTCOVER_MAX_ACCEL 10
#define FASTCOVER_DEFAULT_SPLITPOINT 0.75
#define DEFAULT_F 20
#define DEFAULT_ACCEL 1


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
    if ((clock() - g_time > g_refreshRate) || (displayLevel >= 4)) {             \
      g_time = clock();                                                        \
      DISPLAY(__VA_ARGS__);                                                    \
    }                                                                          \
  }
#undef  DISPLAYUPDATE
#define DISPLAYUPDATE(l, ...) LOCALDISPLAYUPDATE(g_displayLevel, l, __VA_ARGS__)


/*-*************************************
* Hash Functions
***************************************/
/**
 * Hash the d-byte value pointed to by p and mod 2^f into the frequency vector
 */
static size_t FASTCOVER_hashPtrToIndex(const void* p, U32 f, unsigned d) {
  if (d == 6) {
    return ZSTD_hash6Ptr(p, f);
  }
  return ZSTD_hash8Ptr(p, f);
}


/*-*************************************
* Acceleration
***************************************/
typedef struct {
  unsigned finalize;    /* Percentage of training samples used for ZDICT_finalizeDictionary */
  unsigned skip;        /* Number of dmer skipped between each dmer counted in computeFrequency */
} FASTCOVER_accel_t;


static const FASTCOVER_accel_t FASTCOVER_defaultAccelParameters[FASTCOVER_MAX_ACCEL+1] = {
  { 100, 0 },   /* accel = 0, should not happen because accel = 0 defaults to accel = 1 */
  { 100, 0 },   /* accel = 1 */
  { 50, 1 },   /* accel = 2 */
  { 34, 2 },   /* accel = 3 */
  { 25, 3 },   /* accel = 4 */
  { 20, 4 },   /* accel = 5 */
  { 17, 5 },   /* accel = 6 */
  { 14, 6 },   /* accel = 7 */
  { 13, 7 },   /* accel = 8 */
  { 11, 8 },   /* accel = 9 */
  { 10, 9 },   /* accel = 10 */
};


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
  size_t nbDmers;
  U32 *freqs;
  unsigned d;
  unsigned f;
  FASTCOVER_accel_t accelParams;
} FASTCOVER_ctx_t;


/*-*************************************
*  Helper functions
***************************************/
/**
 * Selects the best segment in an epoch.
 * Segments of are scored according to the function:
 *
 * Let F(d) be the frequency of all dmers with hash value d.
 * Let S_i be hash value of the dmer at position i of segment S which has length k.
 *
 *     Score(S) = F(S_1) + F(S_2) + ... + F(S_{k-d+1})
 *
 * Once the dmer with hash value d is in the dictionary we set F(d) = 0.
 */
static COVER_segment_t FASTCOVER_selectSegment(const FASTCOVER_ctx_t *ctx,
                                              U32 *freqs, U32 begin, U32 end,
                                              ZDICT_cover_params_t parameters,
                                              U16* segmentFreqs) {
  /* Constants */
  const U32 k = parameters.k;
  const U32 d = parameters.d;
  const U32 f = ctx->f;
  const U32 dmersInK = k - d + 1;

  /* Try each segment (activeSegment) and save the best (bestSegment) */
  COVER_segment_t bestSegment = {0, 0, 0};
  COVER_segment_t activeSegment;

  /* Reset the activeDmers in the segment */
  /* The activeSegment starts at the beginning of the epoch. */
  activeSegment.begin = begin;
  activeSegment.end = begin;
  activeSegment.score = 0;

  /* Slide the activeSegment through the whole epoch.
   * Save the best segment in bestSegment.
   */
  while (activeSegment.end < end) {
    /* Get hash value of current dmer */
    const size_t idx = FASTCOVER_hashPtrToIndex(ctx->samples + activeSegment.end, f, d);

    /* Add frequency of this index to score if this is the first occurrence of index in active segment */
    if (segmentFreqs[idx] == 0) {
      activeSegment.score += freqs[idx];
    }
    /* Increment end of segment and segmentFreqs*/
    activeSegment.end += 1;
    segmentFreqs[idx] += 1;
    /* If the window is now too large, drop the first position */
    if (activeSegment.end - activeSegment.begin == dmersInK + 1) {
      /* Get hash value of the dmer to be eliminated from active segment */
      const size_t delIndex = FASTCOVER_hashPtrToIndex(ctx->samples + activeSegment.begin, f, d);
      segmentFreqs[delIndex] -= 1;
      /* Subtract frequency of this index from score if this is the last occurrence of this index in active segment */
      if (segmentFreqs[delIndex] == 0) {
        activeSegment.score -= freqs[delIndex];
      }
      /* Increment start of segment */
      activeSegment.begin += 1;
    }

    /* If this segment is the best so far save it */
    if (activeSegment.score > bestSegment.score) {
      bestSegment = activeSegment;
    }
  }

  /* Zero out rest of segmentFreqs array */
  while (activeSegment.begin < end) {
    const size_t delIndex = FASTCOVER_hashPtrToIndex(ctx->samples + activeSegment.begin, f, d);
    segmentFreqs[delIndex] -= 1;
    activeSegment.begin += 1;
  }

  {
    /*  Zero the frequency of hash value of each dmer covered by the chosen segment. */
    U32 pos;
    for (pos = bestSegment.begin; pos != bestSegment.end; ++pos) {
      const size_t i = FASTCOVER_hashPtrToIndex(ctx->samples + pos, f, d);
      freqs[i] = 0;
    }
  }

  return bestSegment;
}


static int FASTCOVER_checkParameters(ZDICT_cover_params_t parameters,
                                     size_t maxDictSize, unsigned f,
                                     unsigned accel) {
  /* k, d, and f are required parameters */
  if (parameters.d == 0 || parameters.k == 0) {
    return 0;
  }
  /* d has to be 6 or 8 */
  if (parameters.d != 6 && parameters.d != 8) {
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
  /* 0 < f <= FASTCOVER_MAX_F*/
  if (f > FASTCOVER_MAX_F || f == 0) {
    return 0;
  }
  /* 0 < splitPoint <= 1 */
  if (parameters.splitPoint <= 0 || parameters.splitPoint > 1) {
    return 0;
  }
  /* 0 < accel <= 10 */
  if (accel > 10 || accel == 0) {
    return 0;
  }
  return 1;
}


/**
 * Clean up a context initialized with `FASTCOVER_ctx_init()`.
 */
static void
FASTCOVER_ctx_destroy(FASTCOVER_ctx_t* ctx)
{
    if (!ctx) return;

    free(ctx->freqs);
    ctx->freqs = NULL;

    free(ctx->offsets);
    ctx->offsets = NULL;
}


/**
 * Calculate for frequency of hash value of each dmer in ctx->samples
 */
static void
FASTCOVER_computeFrequency(U32* freqs, const FASTCOVER_ctx_t* ctx)
{
    const unsigned f = ctx->f;
    const unsigned d = ctx->d;
    const unsigned skip = ctx->accelParams.skip;
    const unsigned readLength = MAX(d, 8);
    size_t i;
    assert(ctx->nbTrainSamples >= 5);
    assert(ctx->nbTrainSamples <= ctx->nbSamples);
    for (i = 0; i < ctx->nbTrainSamples; i++) {
        size_t start = ctx->offsets[i];  /* start of current dmer */
        size_t const currSampleEnd = ctx->offsets[i+1];
        while (start + readLength <= currSampleEnd) {
            const size_t dmerIndex = FASTCOVER_hashPtrToIndex(ctx->samples + start, f, d);
            freqs[dmerIndex]++;
            start = start + skip + 1;
        }
    }
}


/**
 * Prepare a context for dictionary building.
 * The context is only dependent on the parameter `d` and can be used multiple
 * times.
 * Returns 0 on success or error code on error.
 * The context must be destroyed with `FASTCOVER_ctx_destroy()`.
 */
static size_t
FASTCOVER_ctx_init(FASTCOVER_ctx_t* ctx,
                   const void* samplesBuffer,
                   const size_t* samplesSizes, unsigned nbSamples,
                   unsigned d, double splitPoint, unsigned f,
                   FASTCOVER_accel_t accelParams)
{
    const BYTE* const samples = (const BYTE*)samplesBuffer;
    const size_t totalSamplesSize = COVER_sum(samplesSizes, nbSamples);
    /* Split samples into testing and training sets */
    const unsigned nbTrainSamples = splitPoint < 1.0 ? (unsigned)((double)nbSamples * splitPoint) : nbSamples;
    const unsigned nbTestSamples = splitPoint < 1.0 ? nbSamples - nbTrainSamples : nbSamples;
    const size_t trainingSamplesSize = splitPoint < 1.0 ? COVER_sum(samplesSizes, nbTrainSamples) : totalSamplesSize;
    const size_t testSamplesSize = splitPoint < 1.0 ? COVER_sum(samplesSizes + nbTrainSamples, nbTestSamples) : totalSamplesSize;

    /* Checks */
    if (totalSamplesSize < MAX(d, sizeof(U64)) ||
        totalSamplesSize >= (size_t)FASTCOVER_MAX_SAMPLES_SIZE) {
        DISPLAYLEVEL(1, "Total samples size is too large (%u MB), maximum size is %u MB\n",
                    (unsigned)(totalSamplesSize >> 20), (FASTCOVER_MAX_SAMPLES_SIZE >> 20));
        return ERROR(srcSize_wrong);
    }

    /* Check if there are at least 5 training samples */
    if (nbTrainSamples < 5) {
        DISPLAYLEVEL(1, "Total number of training samples is %u and is invalid\n", nbTrainSamples);
        return ERROR(srcSize_wrong);
    }

    /* Check if there's testing sample */
    if (nbTestSamples < 1) {
        DISPLAYLEVEL(1, "Total number of testing samples is %u and is invalid.\n", nbTestSamples);
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
    ctx->nbDmers = trainingSamplesSize - MAX(d, sizeof(U64)) + 1;
    ctx->d = d;
    ctx->f = f;
    ctx->accelParams = accelParams;

    /* The offsets of each file */
    ctx->offsets = (size_t*)calloc((nbSamples + 1), sizeof(size_t));
    if (ctx->offsets == NULL) {
        DISPLAYLEVEL(1, "Failed to allocate scratch buffers \n");
        FASTCOVER_ctx_destroy(ctx);
        return ERROR(memory_allocation);
    }

    /* Fill offsets from the samplesSizes */
    {   U32 i;
        ctx->offsets[0] = 0;
        assert(nbSamples >= 5);
        for (i = 1; i <= nbSamples; ++i) {
            ctx->offsets[i] = ctx->offsets[i - 1] + samplesSizes[i - 1];
        }
    }

    /* Initialize frequency array of size 2^f */
    ctx->freqs = (U32*)calloc(((U64)1 << f), sizeof(U32));
    if (ctx->freqs == NULL) {
        DISPLAYLEVEL(1, "Failed to allocate frequency table \n");
        FASTCOVER_ctx_destroy(ctx);
        return ERROR(memory_allocation);
    }

    DISPLAYLEVEL(2, "Computing frequencies\n");
    FASTCOVER_computeFrequency(ctx->freqs, ctx);

    return 0;
}


/**
 * Given the prepared context build the dictionary.
 */
static size_t
FASTCOVER_buildDictionary(const FASTCOVER_ctx_t* ctx,
                          U32* freqs,
                          void* dictBuffer, size_t dictBufferCapacity,
                          ZDICT_cover_params_t parameters,
                          U16* segmentFreqs)
{
  BYTE *const dict = (BYTE *)dictBuffer;
  size_t tail = dictBufferCapacity;
  /* Divide the data into epochs. We will select one segment from each epoch. */
  const COVER_epoch_info_t epochs = COVER_computeEpochs(
      (U32)dictBufferCapacity, (U32)ctx->nbDmers, parameters.k, 1);
  const size_t maxZeroScoreRun = 10;
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
    COVER_segment_t segment = FASTCOVER_selectSegment(
        ctx, freqs, epochBegin, epochEnd, parameters, segmentFreqs);

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

/**
 * Parameters for FASTCOVER_tryParameters().
 */
typedef struct FASTCOVER_tryParameters_data_s {
    const FASTCOVER_ctx_t* ctx;
    COVER_best_t* best;
    size_t dictBufferCapacity;
    ZDICT_cover_params_t parameters;
} FASTCOVER_tryParameters_data_t;


/**
 * Tries a set of parameters and updates the COVER_best_t with the results.
 * This function is thread safe if zstd is compiled with multithreaded support.
 * It takes its parameters as an *OWNING* opaque pointer to support threading.
 */
static void FASTCOVER_tryParameters(void* opaque)
{
  /* Save parameters as local variables */
  FASTCOVER_tryParameters_data_t *const data = (FASTCOVER_tryParameters_data_t*)opaque;
  const FASTCOVER_ctx_t *const ctx = data->ctx;
  const ZDICT_cover_params_t parameters = data->parameters;
  size_t dictBufferCapacity = data->dictBufferCapacity;
  size_t totalCompressedSize = ERROR(GENERIC);
  /* Initialize array to keep track of frequency of dmer within activeSegment */
  U16* segmentFreqs = (U16*)calloc(((U64)1 << ctx->f), sizeof(U16));
  /* Allocate space for hash table, dict, and freqs */
  BYTE *const dict = (BYTE*)malloc(dictBufferCapacity);
  COVER_dictSelection_t selection = COVER_dictSelectionError(ERROR(GENERIC));
  U32* freqs = (U32*) malloc(((U64)1 << ctx->f) * sizeof(U32));
  if (!segmentFreqs || !dict || !freqs) {
    DISPLAYLEVEL(1, "Failed to allocate buffers: out of memory\n");
    goto _cleanup;
  }
  /* Copy the frequencies because we need to modify them */
  memcpy(freqs, ctx->freqs, ((U64)1 << ctx->f) * sizeof(U32));
  /* Build the dictionary */
  { const size_t tail = FASTCOVER_buildDictionary(ctx, freqs, dict, dictBufferCapacity,
                                                    parameters, segmentFreqs);

    const unsigned nbFinalizeSamples = (unsigned)(ctx->nbTrainSamples * ctx->accelParams.finalize / 100);
    selection = COVER_selectDict(dict + tail, dictBufferCapacity, dictBufferCapacity - tail,
         ctx->samples, ctx->samplesSizes, nbFinalizeSamples, ctx->nbTrainSamples, ctx->nbSamples, parameters, ctx->offsets,
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
  free(segmentFreqs);
  COVER_dictSelectionFree(selection);
  free(freqs);
}


static void
FASTCOVER_convertToCoverParams(ZDICT_fastCover_params_t fastCoverParams,
                               ZDICT_cover_params_t* coverParams)
{
    coverParams->k = fastCoverParams.k;
    coverParams->d = fastCoverParams.d;
    coverParams->steps = fastCoverParams.steps;
    coverParams->nbThreads = fastCoverParams.nbThreads;
    coverParams->splitPoint = fastCoverParams.splitPoint;
    coverParams->zParams = fastCoverParams.zParams;
    coverParams->shrinkDict = fastCoverParams.shrinkDict;
}


static void
FASTCOVER_convertToFastCoverParams(ZDICT_cover_params_t coverParams,
                                   ZDICT_fastCover_params_t* fastCoverParams,
                                   unsigned f, unsigned accel)
{
    fastCoverParams->k = coverParams.k;
    fastCoverParams->d = coverParams.d;
    fastCoverParams->steps = coverParams.steps;
    fastCoverParams->nbThreads = coverParams.nbThreads;
    fastCoverParams->splitPoint = coverParams.splitPoint;
    fastCoverParams->f = f;
    fastCoverParams->accel = accel;
    fastCoverParams->zParams = coverParams.zParams;
    fastCoverParams->shrinkDict = coverParams.shrinkDict;
}


ZDICTLIB_API size_t
ZDICT_trainFromBuffer_fastCover(void* dictBuffer, size_t dictBufferCapacity,
                                const void* samplesBuffer,
                                const size_t* samplesSizes, unsigned nbSamples,
                                ZDICT_fastCover_params_t parameters)
{
    BYTE* const dict = (BYTE*)dictBuffer;
    FASTCOVER_ctx_t ctx;
    ZDICT_cover_params_t coverParams;
    FASTCOVER_accel_t accelParams;
    /* Initialize global data */
    g_displayLevel = (int)parameters.zParams.notificationLevel;
    /* Assign splitPoint and f if not provided */
    parameters.splitPoint = 1.0;
    parameters.f = parameters.f == 0 ? DEFAULT_F : parameters.f;
    parameters.accel = parameters.accel == 0 ? DEFAULT_ACCEL : parameters.accel;
    /* Convert to cover parameter */
    memset(&coverParams, 0 , sizeof(coverParams));
    FASTCOVER_convertToCoverParams(parameters, &coverParams);
    /* Checks */
    if (!FASTCOVER_checkParameters(coverParams, dictBufferCapacity, parameters.f,
                                   parameters.accel)) {
      DISPLAYLEVEL(1, "FASTCOVER parameters incorrect\n");
      return ERROR(parameter_outOfBound);
    }
    if (nbSamples == 0) {
      DISPLAYLEVEL(1, "FASTCOVER must have at least one input file\n");
      return ERROR(srcSize_wrong);
    }
    if (dictBufferCapacity < ZDICT_DICTSIZE_MIN) {
      DISPLAYLEVEL(1, "dictBufferCapacity must be at least %u\n",
                   ZDICT_DICTSIZE_MIN);
      return ERROR(dstSize_tooSmall);
    }
    /* Assign corresponding FASTCOVER_accel_t to accelParams*/
    accelParams = FASTCOVER_defaultAccelParameters[parameters.accel];
    /* Initialize context */
    {
      size_t const initVal = FASTCOVER_ctx_init(&ctx, samplesBuffer, samplesSizes, nbSamples,
                            coverParams.d, parameters.splitPoint, parameters.f,
                            accelParams);
      if (ZSTD_isError(initVal)) {
        DISPLAYLEVEL(1, "Failed to initialize context\n");
        return initVal;
      }
    }
    COVER_warnOnSmallCorpus(dictBufferCapacity, ctx.nbDmers, g_displayLevel);
    /* Build the dictionary */
    DISPLAYLEVEL(2, "Building dictionary\n");
    {
      /* Initialize array to keep track of frequency of dmer within activeSegment */
      U16* segmentFreqs = (U16 *)calloc(((U64)1 << parameters.f), sizeof(U16));
      const size_t tail = FASTCOVER_buildDictionary(&ctx, ctx.freqs, dictBuffer,
                                                dictBufferCapacity, coverParams, segmentFreqs);
      const unsigned nbFinalizeSamples = (unsigned)(ctx.nbTrainSamples * ctx.accelParams.finalize / 100);
      const size_t dictionarySize = ZDICT_finalizeDictionary(
          dict, dictBufferCapacity, dict + tail, dictBufferCapacity - tail,
          samplesBuffer, samplesSizes, nbFinalizeSamples, coverParams.zParams);
      if (!ZSTD_isError(dictionarySize)) {
          DISPLAYLEVEL(2, "Constructed dictionary of size %u\n",
                      (unsigned)dictionarySize);
      }
      FASTCOVER_ctx_destroy(&ctx);
      free(segmentFreqs);
      return dictionarySize;
    }
}


ZDICTLIB_API size_t
ZDICT_optimizeTrainFromBuffer_fastCover(
                    void* dictBuffer, size_t dictBufferCapacity,
                    const void* samplesBuffer,
                    const size_t* samplesSizes, unsigned nbSamples,
                    ZDICT_fastCover_params_t* parameters)
{
    ZDICT_cover_params_t coverParams;
    FASTCOVER_accel_t accelParams;
    /* constants */
    const unsigned nbThreads = parameters->nbThreads;
    const double splitPoint =
        parameters->splitPoint <= 0.0 ? FASTCOVER_DEFAULT_SPLITPOINT : parameters->splitPoint;
    const unsigned kMinD = parameters->d == 0 ? 6 : parameters->d;
    const unsigned kMaxD = parameters->d == 0 ? 8 : parameters->d;
    const unsigned kMinK = parameters->k == 0 ? 50 : parameters->k;
    const unsigned kMaxK = parameters->k == 0 ? 2000 : parameters->k;
    const unsigned kSteps = parameters->steps == 0 ? 40 : parameters->steps;
    const unsigned kStepSize = MAX((kMaxK - kMinK) / kSteps, 1);
    const unsigned kIterations =
        (1 + (kMaxD - kMinD) / 2) * (1 + (kMaxK - kMinK) / kStepSize);
    const unsigned f = parameters->f == 0 ? DEFAULT_F : parameters->f;
    const unsigned accel = parameters->accel == 0 ? DEFAULT_ACCEL : parameters->accel;
    const unsigned shrinkDict = 0;
    /* Local variables */
    const int displayLevel = (int)parameters->zParams.notificationLevel;
    unsigned iteration = 1;
    unsigned d;
    unsigned k;
    COVER_best_t best;
    POOL_ctx *pool = NULL;
    int warned = 0;
    /* Checks */
    if (splitPoint <= 0 || splitPoint > 1) {
      LOCALDISPLAYLEVEL(displayLevel, 1, "Incorrect splitPoint\n");
      return ERROR(parameter_outOfBound);
    }
    if (accel == 0 || accel > FASTCOVER_MAX_ACCEL) {
      LOCALDISPLAYLEVEL(displayLevel, 1, "Incorrect accel\n");
      return ERROR(parameter_outOfBound);
    }
    if (kMinK < kMaxD || kMaxK < kMinK) {
      LOCALDISPLAYLEVEL(displayLevel, 1, "Incorrect k\n");
      return ERROR(parameter_outOfBound);
    }
    if (nbSamples == 0) {
      LOCALDISPLAYLEVEL(displayLevel, 1, "FASTCOVER must have at least one input file\n");
      return ERROR(srcSize_wrong);
    }
    if (dictBufferCapacity < ZDICT_DICTSIZE_MIN) {
      LOCALDISPLAYLEVEL(displayLevel, 1, "dictBufferCapacity must be at least %u\n",
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
    memset(&coverParams, 0 , sizeof(coverParams));
    FASTCOVER_convertToCoverParams(*parameters, &coverParams);
    accelParams = FASTCOVER_defaultAccelParameters[accel];
    /* Turn down global display level to clean up display at level 2 and below */
    g_displayLevel = displayLevel == 0 ? 0 : displayLevel - 1;
    /* Loop through d first because each new value needs a new context */
    LOCALDISPLAYLEVEL(displayLevel, 2, "Trying %u different sets of parameters\n",
                      kIterations);
    for (d = kMinD; d <= kMaxD; d += 2) {
      /* Initialize the context for this value of d */
      FASTCOVER_ctx_t ctx;
      LOCALDISPLAYLEVEL(displayLevel, 3, "d=%u\n", d);
      {
        size_t const initVal = FASTCOVER_ctx_init(&ctx, samplesBuffer, samplesSizes, nbSamples, d, splitPoint, f, accelParams);
        if (ZSTD_isError(initVal)) {
          LOCALDISPLAYLEVEL(displayLevel, 1, "Failed to initialize context\n");
          COVER_best_destroy(&best);
          POOL_free(pool);
          return initVal;
        }
      }
      if (!warned) {
        COVER_warnOnSmallCorpus(dictBufferCapacity, ctx.nbDmers, displayLevel);
        warned = 1;
      }
      /* Loop through k reusing the same context */
      for (k = kMinK; k <= kMaxK; k += kStepSize) {
        /* Prepare the arguments */
        FASTCOVER_tryParameters_data_t *data = (FASTCOVER_tryParameters_data_t *)malloc(
            sizeof(FASTCOVER_tryParameters_data_t));
        LOCALDISPLAYLEVEL(displayLevel, 3, "k=%u\n", k);
        if (!data) {
          LOCALDISPLAYLEVEL(displayLevel, 1, "Failed to allocate parameters\n");
          COVER_best_destroy(&best);
          FASTCOVER_ctx_destroy(&ctx);
          POOL_free(pool);
          return ERROR(memory_allocation);
        }
        data->ctx = &ctx;
        data->best = &best;
        data->dictBufferCapacity = dictBufferCapacity;
        data->parameters = coverParams;
        data->parameters.k = k;
        data->parameters.d = d;
        data->parameters.splitPoint = splitPoint;
        data->parameters.steps = kSteps;
        data->parameters.shrinkDict = shrinkDict;
        data->parameters.zParams.notificationLevel = (unsigned)g_displayLevel;
        /* Check the parameters */
        if (!FASTCOVER_checkParameters(data->parameters, dictBufferCapacity,
                                       data->ctx->f, accel)) {
          DISPLAYLEVEL(1, "FASTCOVER parameters incorrect\n");
          free(data);
          continue;
        }
        /* Call the function and pass ownership of data to it */
        COVER_best_start(&best);
        if (pool) {
          POOL_add(pool, &FASTCOVER_tryParameters, data);
        } else {
          FASTCOVER_tryParameters(data);
        }
        /* Print status */
        LOCALDISPLAYUPDATE(displayLevel, 2, "\r%u%%       ",
                           (unsigned)((iteration * 100) / kIterations));
        ++iteration;
      }
      COVER_best_wait(&best);
      FASTCOVER_ctx_destroy(&ctx);
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
      FASTCOVER_convertToFastCoverParams(best.parameters, parameters, f, accel);
      memcpy(dictBuffer, best.dict, dictSize);
      COVER_best_destroy(&best);
      POOL_free(pool);
      return dictSize;
    }

}

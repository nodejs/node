/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZDICT_STATIC_LINKING_ONLY
#  define ZDICT_STATIC_LINKING_ONLY
#endif

#include "../common/threading.h" /* ZSTD_pthread_mutex_t */
#include "../common/mem.h"   /* U32, BYTE */
#include "../zdict.h"

/**
 * COVER_best_t is used for two purposes:
 * 1. Synchronizing threads.
 * 2. Saving the best parameters and dictionary.
 *
 * All of the methods except COVER_best_init() are thread safe if zstd is
 * compiled with multithreaded support.
 */
typedef struct COVER_best_s {
  ZSTD_pthread_mutex_t mutex;
  ZSTD_pthread_cond_t cond;
  size_t liveJobs;
  void *dict;
  size_t dictSize;
  ZDICT_cover_params_t parameters;
  size_t compressedSize;
} COVER_best_t;

/**
 * A segment is a range in the source as well as the score of the segment.
 */
typedef struct {
  U32 begin;
  U32 end;
  U32 score;
} COVER_segment_t;

/**
 *Number of epochs and size of each epoch.
 */
typedef struct {
  U32 num;
  U32 size;
} COVER_epoch_info_t;

/**
 * Struct used for the dictionary selection function.
 */
typedef struct COVER_dictSelection {
  BYTE* dictContent;
  size_t dictSize;
  size_t totalCompressedSize;
} COVER_dictSelection_t;

/**
 * Computes the number of epochs and the size of each epoch.
 * We will make sure that each epoch gets at least 10 * k bytes.
 *
 * The COVER algorithms divide the data up into epochs of equal size and
 * select one segment from each epoch.
 *
 * @param maxDictSize The maximum allowed dictionary size.
 * @param nbDmers     The number of dmers we are training on.
 * @param k           The parameter k (segment size).
 * @param passes      The target number of passes over the dmer corpus.
 *                    More passes means a better dictionary.
 */
COVER_epoch_info_t COVER_computeEpochs(U32 maxDictSize, U32 nbDmers,
                                       U32 k, U32 passes);

/**
 * Warns the user when their corpus is too small.
 */
void COVER_warnOnSmallCorpus(size_t maxDictSize, size_t nbDmers, int displayLevel);

/**
 *  Checks total compressed size of a dictionary
 */
size_t COVER_checkTotalCompressedSize(const ZDICT_cover_params_t parameters,
                                      const size_t *samplesSizes, const BYTE *samples,
                                      size_t *offsets,
                                      size_t nbTrainSamples, size_t nbSamples,
                                      BYTE *const dict, size_t dictBufferCapacity);

/**
 * Returns the sum of the sample sizes.
 */
size_t COVER_sum(const size_t *samplesSizes, unsigned nbSamples) ;

/**
 * Initialize the `COVER_best_t`.
 */
void COVER_best_init(COVER_best_t *best);

/**
 * Wait until liveJobs == 0.
 */
void COVER_best_wait(COVER_best_t *best);

/**
 * Call COVER_best_wait() and then destroy the COVER_best_t.
 */
void COVER_best_destroy(COVER_best_t *best);

/**
 * Called when a thread is about to be launched.
 * Increments liveJobs.
 */
void COVER_best_start(COVER_best_t *best);

/**
 * Called when a thread finishes executing, both on error or success.
 * Decrements liveJobs and signals any waiting threads if liveJobs == 0.
 * If this dictionary is the best so far save it and its parameters.
 */
void COVER_best_finish(COVER_best_t *best, ZDICT_cover_params_t parameters,
                       COVER_dictSelection_t selection);
/**
 * Error function for COVER_selectDict function. Checks if the return
 * value is an error.
 */
unsigned COVER_dictSelectionIsError(COVER_dictSelection_t selection);

 /**
  * Error function for COVER_selectDict function. Returns a struct where
  * return.totalCompressedSize is a ZSTD error.
  */
COVER_dictSelection_t COVER_dictSelectionError(size_t error);

/**
 * Always call after selectDict is called to free up used memory from
 * newly created dictionary.
 */
void COVER_dictSelectionFree(COVER_dictSelection_t selection);

/**
 * Called to finalize the dictionary and select one based on whether or not
 * the shrink-dict flag was enabled. If enabled the dictionary used is the
 * smallest dictionary within a specified regression of the compressed size
 * from the largest dictionary.
 */
 COVER_dictSelection_t COVER_selectDict(BYTE* customDictContent, size_t dictBufferCapacity,
                       size_t dictContentSize, const BYTE* samplesBuffer, const size_t* samplesSizes, unsigned nbFinalizeSamples,
                       size_t nbCheckSamples, size_t nbSamples, ZDICT_cover_params_t params, size_t* offsets, size_t totalCompressedSize);

// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// file: rbbi_cache.h
//
#ifndef RBBI_CACHE_H
#define RBBI_CACHE_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/rbbi.h"
#include "unicode/uobject.h"

#include "uvectr32.h"

U_NAMESPACE_BEGIN

/* DictionaryCache  stores the boundaries obtained from a run of dictionary characters.
 *                 Dictionary boundaries are moved first to this cache, then from here
 *                 to the main BreakCache, where they may inter-leave with non-dictionary
 *                 boundaries. The public BreakIterator API always fetches directly
 *                 from the main BreakCache, not from here.
 *
 *                 In common situations, the number of boundaries in a single dictionary run
 *                 should be quite small, it will be terminated by punctuation, spaces,
 *                 or any other non-dictionary characters. The main BreakCache may end
 *                 up with boundaries from multiple dictionary based runs.
 *
 *                 The boundaries are stored in a simple ArrayList (vector), with the
 *                 assumption that they will be accessed sequentially.
 */
class RuleBasedBreakIterator::DictionaryCache: public UMemory {
  public:
     DictionaryCache(RuleBasedBreakIterator *bi, UErrorCode &status);
     ~DictionaryCache();

     void reset();

     UBool following(int32_t fromPos, int32_t *pos, int32_t *statusIndex);
     UBool preceding(int32_t fromPos, int32_t *pos, int32_t *statusIndex);

    /**
     * Populate the cache with the dictionary based boundaries within a region of text.
     * @param startPos  The start position of a range of text
     * @param endPos    The end position of a range of text
     * @param firstRuleStatus The rule status index that applies to the break at startPos
     * @param otherRuleStatus The rule status index that applies to boundaries other than startPos
     * @internal
     */
    void populateDictionary(int32_t startPos, int32_t endPos,
                         int32_t firstRuleStatus, int32_t otherRuleStatus);



    RuleBasedBreakIterator *fBI;

    UVector32          *fBreaks;                // A vector containing the boundaries.
    int32_t             fPositionInCache;       // Index in fBreaks of last boundary returned by following()
                                                //    or preceding(). Optimizes sequential access.
    int32_t             fStart;                 // Text position of first boundary in cache.
    int32_t             fLimit;                 // Last boundary in cache. Which is the limit of the
                                                //    text segment being handled by the dictionary.
    int32_t             fFirstRuleStatusIndex;  // Rule status info for first boundary.
    int32_t             fOtherRuleStatusIndex;  // Rule status info for 2nd through last boundaries.
};


/*
 * class BreakCache
 *
 * Cache of break boundary positions and rule status values.
 * Break iterator API functions, next(), previous(), etc., will use cached results
 * when possible, and otherwise cache new results as they are obtained.
 *
 * Uniformly caches both dictionary and rule based (non-dictionary) boundaries.
 *
 * The cache is implemented as a single circular buffer.
 */

/*
 * size of the circular cache buffer.
 */

class RuleBasedBreakIterator::BreakCache: public UMemory {
  public:
                BreakCache(RuleBasedBreakIterator *bi, UErrorCode &status);
    virtual     ~BreakCache();
    void        reset(int32_t pos = 0, int32_t ruleStatus = 0);
    void        next() {    if (fBufIdx == fEndBufIdx) {
                                nextOL();
                            } else {
                                fBufIdx = modChunkSize(fBufIdx + 1);
                                fTextIdx = fBI->fPosition = fBoundaries[fBufIdx];
                                fBI->fRuleStatusIndex = fStatuses[fBufIdx];
                            }
                };


    void        nextOL();
    void        previous(UErrorCode &status);

    // Move the iteration state to the position following the startPosition.
    // Input position must be pinned to the input length.
    void        following(int32_t startPosition, UErrorCode &status);

    void        preceding(int32_t startPosition, UErrorCode &status);

    /*
     * Update the state of the public BreakIterator (fBI) to reflect the
     * current state of the break iterator cache (this).
     */
    int32_t     current();

    /**
     * Add boundaries to the cache near the specified position.
     * The given position need not be a boundary itself.
     * The input position must be within the range of the text, and
     * on a code point boundary.
     * If the requested position is a break boundary, leave the iteration
     * position on it.
     * If the requested position is not a boundary, leave the iteration
     * position on the preceding boundary and include both the
     * preceding and following boundaries in the cache.
     * Additional boundaries, either preceding or following, may be added
     * to the cache as a side effect.
     *
     * Return FALSE if the operation failed.
     */
    UBool populateNear(int32_t position, UErrorCode &status);

    /**
     *  Add boundary(s) to the cache following the current last boundary.
     *  Return FALSE if at the end of the text, and no more boundaries can be added.
     *  Leave iteration position at the first newly added boundary, or unchanged if no boundary was added.
     */
    UBool populateFollowing();

    /**
     *  Add one or more boundaries to the cache preceding the first currently cached boundary.
     *  Leave the iteration position on the first added boundary.
     *  Return false if no boundaries could be added (if at the start of the text.)
     */
    UBool populatePreceding(UErrorCode &status);

    enum UpdatePositionValues {
        RetainCachePosition = 0,
        UpdateCachePosition = 1
    };

    /*
     * Add the boundary following the current position.
     * The current position can be left as it was, or changed to the newly added boundary,
     * as specified by the update parameter.
     */
    void addFollowing(int32_t position, int32_t ruleStatusIdx, UpdatePositionValues update);


    /*
     * Add the boundary preceding the current position.
     * The current position can be left as it was, or changed to the newly added boundary,
     * as specified by the update parameter.
     */
    bool addPreceding(int32_t position, int32_t ruleStatusIdx, UpdatePositionValues update);

    /**
     *  Set the cache position to the specified position, or, if the position
     *  falls between to cached boundaries, to the preceding boundary.
     *  Fails if the requested position is outside of the range of boundaries currently held by the cache.
     *  The startPosition must be on a code point boundary.
     *
     *  Return TRUE if successful, FALSE if the specified position is after
     *  the last cached boundary or before the first.
     */
    UBool                   seek(int32_t startPosition);

    void dumpCache();

  private:
    static inline int32_t   modChunkSize(int index) { return index & (CACHE_SIZE - 1); };

    static constexpr int32_t CACHE_SIZE = 128;
    static_assert((CACHE_SIZE & (CACHE_SIZE-1)) == 0, "CACHE_SIZE must be power of two.");

    RuleBasedBreakIterator *fBI;
    int32_t                 fStartBufIdx;
    int32_t                 fEndBufIdx;    // inclusive

    int32_t                 fTextIdx;
    int32_t                 fBufIdx;

    int32_t                 fBoundaries[CACHE_SIZE];
    uint16_t                fStatuses[CACHE_SIZE];

    UVector32               fSideBuffer;
};

U_NAMESPACE_END

#endif // #if !UCONFIG_NO_BREAK_ITERATION

#endif // RBBI_CACHE_H

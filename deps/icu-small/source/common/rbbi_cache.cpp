// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// file: rbbi_cache.cpp

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/ubrk.h"
#include "unicode/rbbi.h"

#include "rbbi_cache.h"

#include "brkeng.h"
#include "cmemory.h"
#include "rbbidata.h"
#include "rbbirb.h"
#include "uassert.h"
#include "uvectr32.h"

U_NAMESPACE_BEGIN

/*
 * DictionaryCache implementation
 */

RuleBasedBreakIterator::DictionaryCache::DictionaryCache(RuleBasedBreakIterator *bi, UErrorCode &status) :
        fBI(bi), fBreaks(status), fPositionInCache(-1),
        fStart(0), fLimit(0), fFirstRuleStatusIndex(0), fOtherRuleStatusIndex(0) {
}

RuleBasedBreakIterator::DictionaryCache::~DictionaryCache() {
}

void RuleBasedBreakIterator::DictionaryCache::reset() {
    fPositionInCache = -1;
    fStart = 0;
    fLimit = 0;
    fFirstRuleStatusIndex = 0;
    fOtherRuleStatusIndex = 0;
    fBreaks.removeAllElements();
}

UBool RuleBasedBreakIterator::DictionaryCache::following(int32_t fromPos, int32_t *result, int32_t *statusIndex) {
    if (fromPos >= fLimit || fromPos < fStart) {
        fPositionInCache = -1;
        return FALSE;
    }

    // Sequential iteration, move from previous boundary to the following

    int32_t r = 0;
    if (fPositionInCache >= 0 && fPositionInCache < fBreaks.size() && fBreaks.elementAti(fPositionInCache) == fromPos) {
        ++fPositionInCache;
        if (fPositionInCache >= fBreaks.size()) {
            fPositionInCache = -1;
            return FALSE;
        }
        r = fBreaks.elementAti(fPositionInCache);
        U_ASSERT(r > fromPos);
        *result = r;
        *statusIndex = fOtherRuleStatusIndex;
        return TRUE;
    }

    // Random indexing. Linear search for the boundary following the given position.

    for (fPositionInCache = 0; fPositionInCache < fBreaks.size(); ++fPositionInCache) {
        r= fBreaks.elementAti(fPositionInCache);
        if (r > fromPos) {
            *result = r;
            *statusIndex = fOtherRuleStatusIndex;
            return TRUE;
        }
    }
    U_ASSERT(FALSE);
    fPositionInCache = -1;
    return FALSE;
}


UBool RuleBasedBreakIterator::DictionaryCache::preceding(int32_t fromPos, int32_t *result, int32_t *statusIndex) {
    if (fromPos <= fStart || fromPos > fLimit) {
        fPositionInCache = -1;
        return FALSE;
    }

    if (fromPos == fLimit) {
        fPositionInCache = fBreaks.size() - 1;
        if (fPositionInCache >= 0) {
            U_ASSERT(fBreaks.elementAti(fPositionInCache) == fromPos);
        }
    }

    int32_t r;
    if (fPositionInCache > 0 && fPositionInCache < fBreaks.size() && fBreaks.elementAti(fPositionInCache) == fromPos) {
        --fPositionInCache;
        r = fBreaks.elementAti(fPositionInCache);
        U_ASSERT(r < fromPos);
        *result = r;
        *statusIndex = ( r== fStart) ? fFirstRuleStatusIndex : fOtherRuleStatusIndex;
        return TRUE;
    }

    if (fPositionInCache == 0) {
        fPositionInCache = -1;
        return FALSE;
    }

    for (fPositionInCache = fBreaks.size()-1; fPositionInCache >= 0; --fPositionInCache) {
        r = fBreaks.elementAti(fPositionInCache);
        if (r < fromPos) {
            *result = r;
            *statusIndex = ( r == fStart) ? fFirstRuleStatusIndex : fOtherRuleStatusIndex;
            return TRUE;
        }
    }
    U_ASSERT(FALSE);
    fPositionInCache = -1;
    return FALSE;
}

void RuleBasedBreakIterator::DictionaryCache::populateDictionary(int32_t startPos, int32_t endPos,
                                       int32_t firstRuleStatus, int32_t otherRuleStatus) {
    if ((endPos - startPos) <= 1) {
        return;
    }

    reset();
    fFirstRuleStatusIndex = firstRuleStatus;
    fOtherRuleStatusIndex = otherRuleStatus;

    int32_t rangeStart = startPos;
    int32_t rangeEnd = endPos;

    uint16_t    category;
    int32_t     current;
    UErrorCode  status = U_ZERO_ERROR;
    int32_t     foundBreakCount = 0;
    UText      *text = &fBI->fText;

    // Loop through the text, looking for ranges of dictionary characters.
    // For each span, find the appropriate break engine, and ask it to find
    // any breaks within the span.

    utext_setNativeIndex(text, rangeStart);
    UChar32     c = utext_current32(text);
    category = UTRIE2_GET16(fBI->fData->fTrie, c);

    while(U_SUCCESS(status)) {
        while((current = (int32_t)UTEXT_GETNATIVEINDEX(text)) < rangeEnd && (category & 0x4000) == 0) {
            utext_next32(text);           // TODO: cleaner loop structure.
            c = utext_current32(text);
            category = UTRIE2_GET16(fBI->fData->fTrie, c);
        }
        if (current >= rangeEnd) {
            break;
        }

        // We now have a dictionary character. Get the appropriate language object
        // to deal with it.
        const LanguageBreakEngine *lbe = fBI->getLanguageBreakEngine(c);

        // Ask the language object if there are any breaks. It will add them to the cache and
        // leave the text pointer on the other side of its range, ready to search for the next one.
        if (lbe != NULL) {
            foundBreakCount += lbe->findBreaks(text, rangeStart, rangeEnd, fBreaks);
        }

        // Reload the loop variables for the next go-round
        c = utext_current32(text);
        category = UTRIE2_GET16(fBI->fData->fTrie, c);
    }

    // If we found breaks, ensure that the first and last entries are
    // the original starting and ending position. And initialize the
    // cache iteration position to the first entry.

    // printf("foundBreakCount = %d\n", foundBreakCount);
    if (foundBreakCount > 0) {
        U_ASSERT(foundBreakCount == fBreaks.size());
        if (startPos < fBreaks.elementAti(0)) {
            // The dictionary did not place a boundary at the start of the segment of text.
            // Add one now. This should not commonly happen, but it would be easy for interactions
            // of the rules for dictionary segments and the break engine implementations to
            // inadvertently cause it. Cover it here, just in case.
            fBreaks.insertElementAt(startPos, 0, status);
        }
        if (endPos > fBreaks.peeki()) {
            fBreaks.push(endPos, status);
        }
        fPositionInCache = 0;
        // Note: Dictionary matching may extend beyond the original limit.
        fStart = fBreaks.elementAti(0);
        fLimit = fBreaks.peeki();
    } else {
        // there were no language-based breaks, even though the segment contained
        // dictionary characters. Subsequent attempts to fetch boundaries from the dictionary cache
        // for this range will fail, and the calling code will fall back to the rule based boundaries.
    }
}


/*
 *   BreakCache implemetation
 */

RuleBasedBreakIterator::BreakCache::BreakCache(RuleBasedBreakIterator *bi, UErrorCode &status) :
        fBI(bi), fSideBuffer(status) {
    reset();
}


RuleBasedBreakIterator::BreakCache::~BreakCache() {
}


void RuleBasedBreakIterator::BreakCache::reset(int32_t pos, int32_t ruleStatus) {
    fStartBufIdx = 0;
    fEndBufIdx = 0;
    fTextIdx = pos;
    fBufIdx = 0;
    fBoundaries[0] = pos;
    fStatuses[0] = (uint16_t)ruleStatus;
}


int32_t  RuleBasedBreakIterator::BreakCache::current() {
    fBI->fPosition = fTextIdx;
    fBI->fRuleStatusIndex = fStatuses[fBufIdx];
    fBI->fDone = FALSE;
    return fTextIdx;
}


void RuleBasedBreakIterator::BreakCache::following(int32_t startPos, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if (startPos == fTextIdx || seek(startPos) || populateNear(startPos, status)) {
        // startPos is in the cache. Do a next() from that position.
        // TODO: an awkward set of interactions with bi->fDone
        //       seek() does not clear it; it can't because of interactions with populateNear().
        //       next() does not clear it in the fast-path case, where everything matters. Maybe it should.
        //       So clear it here, for the case where seek() succeeded on an iterator that had previously run off the end.
        fBI->fDone = false;
        next();
    }
    return;
}


void RuleBasedBreakIterator::BreakCache::preceding(int32_t startPos, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if (startPos == fTextIdx || seek(startPos) || populateNear(startPos, status)) {
        if (startPos == fTextIdx) {
            previous(status);
        } else {
            // seek() leaves the BreakCache positioned at the preceding boundary
            //        if the requested position is between two bounaries.
            // current() pushes the BreakCache position out to the BreakIterator itself.
            U_ASSERT(startPos > fTextIdx);
            current();
        }
    }
    return;
}


/*
 * Out-of-line code for BreakCache::next().
 * Cache does not already contain the boundary
 */
void RuleBasedBreakIterator::BreakCache::nextOL() {
    fBI->fDone = !populateFollowing();
    fBI->fPosition = fTextIdx;
    fBI->fRuleStatusIndex = fStatuses[fBufIdx];
    return;
}


void RuleBasedBreakIterator::BreakCache::previous(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    int32_t initialBufIdx = fBufIdx;
    if (fBufIdx == fStartBufIdx) {
        // At start of cache. Prepend to it.
        populatePreceding(status);
    } else {
        // Cache already holds the next boundary
        fBufIdx = modChunkSize(fBufIdx - 1);
        fTextIdx = fBoundaries[fBufIdx];
    }
    fBI->fDone = (fBufIdx == initialBufIdx);
    fBI->fPosition = fTextIdx;
    fBI->fRuleStatusIndex = fStatuses[fBufIdx];
    return;
}


UBool RuleBasedBreakIterator::BreakCache::seek(int32_t pos) {
    if (pos < fBoundaries[fStartBufIdx] || pos > fBoundaries[fEndBufIdx]) {
        return FALSE;
    }
    if (pos == fBoundaries[fStartBufIdx]) {
        // Common case: seek(0), from BreakIterator::first()
        fBufIdx = fStartBufIdx;
        fTextIdx = fBoundaries[fBufIdx];
        return TRUE;
    }
    if (pos == fBoundaries[fEndBufIdx]) {
        fBufIdx = fEndBufIdx;
        fTextIdx = fBoundaries[fBufIdx];
        return TRUE;
    }

    int32_t min = fStartBufIdx;
    int32_t max = fEndBufIdx;
    while (min != max) {
        int32_t probe = (min + max + (min>max ? CACHE_SIZE : 0)) / 2;
        probe = modChunkSize(probe);
        if (fBoundaries[probe] > pos) {
            max = probe;
        } else {
            min = modChunkSize(probe + 1);
        }
    }
    U_ASSERT(fBoundaries[max] > pos);
    fBufIdx = modChunkSize(max - 1);
    fTextIdx = fBoundaries[fBufIdx];
    U_ASSERT(fTextIdx <= pos);
    return TRUE;
}


UBool RuleBasedBreakIterator::BreakCache::populateNear(int32_t position, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    U_ASSERT(position < fBoundaries[fStartBufIdx] || position > fBoundaries[fEndBufIdx]);

    // Find a boundary somewhere in the vicinity of the requested position.
    // Depending on the safe rules and the text data, it could be either before, at, or after
    // the requested position.


    // If the requested position is not near already cached positions, clear the existing cache,
    // find a near-by boundary and begin new cache contents there.

    if ((position < fBoundaries[fStartBufIdx] - 15) || position > (fBoundaries[fEndBufIdx] + 15)) {
        int32_t aBoundary = 0;
        int32_t ruleStatusIndex = 0;
        if (position > 20) {
            int32_t backupPos = fBI->handleSafePrevious(position);

            if (backupPos > 0) {
                // Advance to the boundary following the backup position.
                // There is a complication: the safe reverse rules identify pairs of code points
                // that are safe. If advancing from the safe point moves forwards by less than
                // two code points, we need to advance one more time to ensure that the boundary
                // is good, including a correct rules status value.
                //
                fBI->fPosition = backupPos;
                aBoundary = fBI->handleNext();
                if (aBoundary <= backupPos + 4) {
                    // +4 is a quick test for possibly having advanced only one codepoint.
                    // Four being the length of the longest potential code point, a supplementary in UTF-8
                    utext_setNativeIndex(&fBI->fText, aBoundary);
                    if (backupPos == utext_getPreviousNativeIndex(&fBI->fText)) {
                        // The initial handleNext() only advanced by a single code point. Go again.
                        aBoundary = fBI->handleNext();   // Safe rules identify safe pairs.
                    }
                }
                ruleStatusIndex = fBI->fRuleStatusIndex;
            }
        }
        reset(aBoundary, ruleStatusIndex);        // Reset cache to hold aBoundary as a single starting point.
    }

    // Fill in boundaries between existing cache content and the new requested position.

    if (fBoundaries[fEndBufIdx] < position) {
        // The last position in the cache precedes the requested position.
        // Add following position(s) to the cache.
        while (fBoundaries[fEndBufIdx] < position) {
            if (!populateFollowing()) {
                U_ASSERT(false);
                return false;
            }
        }
        fBufIdx = fEndBufIdx;                      // Set iterator position to the end of the buffer.
        fTextIdx = fBoundaries[fBufIdx];           // Required because populateFollowing may add extra boundaries.
        while (fTextIdx > position) {              // Move backwards to a position at or preceding the requested pos.
            previous(status);
        }
        return true;
    }

    if (fBoundaries[fStartBufIdx] > position) {
        // The first position in the cache is beyond the requested position.
        // back up more until we get a boundary <= the requested position.
        while (fBoundaries[fStartBufIdx] > position) {
            populatePreceding(status);
        }
        fBufIdx = fStartBufIdx;                    // Set iterator position to the start of the buffer.
        fTextIdx = fBoundaries[fBufIdx];           // Required because populatePreceding may add extra boundaries.
        while (fTextIdx < position) {              // Move forwards to a position at or following the requested pos.
            next();
        }
        if (fTextIdx > position) {
            // If position is not itself a boundary, the next() loop above will overshoot.
            // Back up one, leaving cache position at the boundary preceding the requested position.
            previous(status);
        }
        return true;
    }

    U_ASSERT(fTextIdx == position);
    return true;
}



UBool RuleBasedBreakIterator::BreakCache::populateFollowing() {
    int32_t fromPosition = fBoundaries[fEndBufIdx];
    int32_t fromRuleStatusIdx = fStatuses[fEndBufIdx];
    int32_t pos = 0;
    int32_t ruleStatusIdx = 0;

    if (fBI->fDictionaryCache->following(fromPosition, &pos, &ruleStatusIdx)) {
        addFollowing(pos, ruleStatusIdx, UpdateCachePosition);
        return TRUE;
    }

    fBI->fPosition = fromPosition;
    pos = fBI->handleNext();
    if (pos == UBRK_DONE) {
        return FALSE;
    }

    ruleStatusIdx = fBI->fRuleStatusIndex;
    if (fBI->fDictionaryCharCount > 0) {
        // The text segment obtained from the rules includes dictionary characters.
        // Subdivide it, with subdivided results going into the dictionary cache.
        fBI->fDictionaryCache->populateDictionary(fromPosition, pos, fromRuleStatusIdx, ruleStatusIdx);
        if (fBI->fDictionaryCache->following(fromPosition, &pos, &ruleStatusIdx)) {
            addFollowing(pos, ruleStatusIdx, UpdateCachePosition);
            return TRUE;
            // TODO: may want to move a sizable chunk of dictionary cache to break cache at this point.
            //       But be careful with interactions with populateNear().
        }
    }

    // Rule based segment did not include dictionary characters.
    // Or, it did contain dictionary chars, but the dictionary segmenter didn't handle them,
    //    meaning that we didn't take the return, above.
    // Add its end point to the cache.
    addFollowing(pos, ruleStatusIdx, UpdateCachePosition);

    // Add several non-dictionary boundaries at this point, to optimize straight forward iteration.
    //    (subsequent calls to BreakIterator::next() will take the fast path, getting cached results.
    //
    for (int count=0; count<6; ++count) {
        pos = fBI->handleNext();
        if (pos == UBRK_DONE || fBI->fDictionaryCharCount > 0) {
            break;
        }
        addFollowing(pos, fBI->fRuleStatusIndex, RetainCachePosition);
    }

    return TRUE;
}


UBool RuleBasedBreakIterator::BreakCache::populatePreceding(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }

    int32_t fromPosition = fBoundaries[fStartBufIdx];
    if (fromPosition == 0) {
        return FALSE;
    }

    int32_t position = 0;
    int32_t positionStatusIdx = 0;

    if (fBI->fDictionaryCache->preceding(fromPosition, &position, &positionStatusIdx)) {
        addPreceding(position, positionStatusIdx, UpdateCachePosition);
        return TRUE;
    }

    int32_t backupPosition = fromPosition;

    // Find a boundary somewhere preceding the first already-cached boundary
    do {
        backupPosition = backupPosition - 30;
        if (backupPosition <= 0) {
            backupPosition = 0;
        } else {
            backupPosition = fBI->handleSafePrevious(backupPosition);
        }
        if (backupPosition == UBRK_DONE || backupPosition == 0) {
            position = 0;
            positionStatusIdx = 0;
        } else {
            // Advance to the boundary following the backup position.
            // There is a complication: the safe reverse rules identify pairs of code points
            // that are safe. If advancing from the safe point moves forwards by less than
            // two code points, we need to advance one more time to ensure that the boundary
            // is good, including a correct rules status value.
            //
            fBI->fPosition = backupPosition;
            position = fBI->handleNext();
            if (position <= backupPosition + 4) {
                // +4 is a quick test for possibly having advanced only one codepoint.
                // Four being the length of the longest potential code point, a supplementary in UTF-8
                utext_setNativeIndex(&fBI->fText, position);
                if (backupPosition == utext_getPreviousNativeIndex(&fBI->fText)) {
                    // The initial handleNext() only advanced by a single code point. Go again.
                    position = fBI->handleNext();   // Safe rules identify safe pairs.
                }
            };
            positionStatusIdx = fBI->fRuleStatusIndex;
        }
    } while (position >= fromPosition);

    // Find boundaries between the one we just located and the first already-cached boundary
    // Put them in a side buffer, because we don't yet know where they will fall in the circular cache buffer..

    fSideBuffer.removeAllElements();
    fSideBuffer.addElement(position, status);
    fSideBuffer.addElement(positionStatusIdx, status);

    do {
        int32_t prevPosition = fBI->fPosition = position;
        int32_t prevStatusIdx = positionStatusIdx;
        position = fBI->handleNext();
        positionStatusIdx = fBI->fRuleStatusIndex;
        if (position == UBRK_DONE) {
            break;
        }

        UBool segmentHandledByDictionary = FALSE;
        if (fBI->fDictionaryCharCount != 0) {
            // Segment from the rules includes dictionary characters.
            // Subdivide it, with subdivided results going into the dictionary cache.
            int32_t dictSegEndPosition = position;
            fBI->fDictionaryCache->populateDictionary(prevPosition, dictSegEndPosition, prevStatusIdx, positionStatusIdx);
            while (fBI->fDictionaryCache->following(prevPosition, &position, &positionStatusIdx)) {
                segmentHandledByDictionary = true;
                U_ASSERT(position > prevPosition);
                if (position >= fromPosition) {
                    break;
                }
                U_ASSERT(position <= dictSegEndPosition);
                fSideBuffer.addElement(position, status);
                fSideBuffer.addElement(positionStatusIdx, status);
                prevPosition = position;
            }
            U_ASSERT(position==dictSegEndPosition || position>=fromPosition);
        }

        if (!segmentHandledByDictionary && position < fromPosition) {
            fSideBuffer.addElement(position, status);
            fSideBuffer.addElement(positionStatusIdx, status);
        }
    } while (position < fromPosition);

    // Move boundaries from the side buffer to the main circular buffer.
    UBool success = FALSE;
    if (!fSideBuffer.isEmpty()) {
        positionStatusIdx = fSideBuffer.popi();
        position = fSideBuffer.popi();
        addPreceding(position, positionStatusIdx, UpdateCachePosition);
        success = TRUE;
    }

    while (!fSideBuffer.isEmpty()) {
        positionStatusIdx = fSideBuffer.popi();
        position = fSideBuffer.popi();
        if (!addPreceding(position, positionStatusIdx, RetainCachePosition)) {
            // No space in circular buffer to hold a new preceding result while
            // also retaining the current cache (iteration) position.
            // Bailing out is safe; the cache will refill again if needed.
            break;
        }
    }

    return success;
}


void RuleBasedBreakIterator::BreakCache::addFollowing(int32_t position, int32_t ruleStatusIdx, UpdatePositionValues update) {
    U_ASSERT(position > fBoundaries[fEndBufIdx]);
    U_ASSERT(ruleStatusIdx <= UINT16_MAX);
    int32_t nextIdx = modChunkSize(fEndBufIdx + 1);
    if (nextIdx == fStartBufIdx) {
        fStartBufIdx = modChunkSize(fStartBufIdx + 6);    // TODO: experiment. Probably revert to 1.
    }
    fBoundaries[nextIdx] = position;
    fStatuses[nextIdx] = ruleStatusIdx;
    fEndBufIdx = nextIdx;
    if (update == UpdateCachePosition) {
        // Set current position to the newly added boundary.
        fBufIdx = nextIdx;
        fTextIdx = position;
    } else {
        // Retaining the original cache position.
        // Check if the added boundary wraps around the buffer, and would over-write the original position.
        // It's the responsibility of callers of this function to not add too many.
        U_ASSERT(nextIdx != fBufIdx);
    }
}

bool RuleBasedBreakIterator::BreakCache::addPreceding(int32_t position, int32_t ruleStatusIdx, UpdatePositionValues update) {
    U_ASSERT(position < fBoundaries[fStartBufIdx]);
    U_ASSERT(ruleStatusIdx <= UINT16_MAX);
    int32_t nextIdx = modChunkSize(fStartBufIdx - 1);
    if (nextIdx == fEndBufIdx) {
        if (fBufIdx == fEndBufIdx && update == RetainCachePosition) {
            // Failure. The insertion of the new boundary would claim the buffer position that is the
            // current iteration position. And we also want to retain the current iteration position.
            // (The buffer is already completely full of entries that precede the iteration position.)
            return false;
        }
        fEndBufIdx = modChunkSize(fEndBufIdx - 1);
    }
    fBoundaries[nextIdx] = position;
    fStatuses[nextIdx] = ruleStatusIdx;
    fStartBufIdx = nextIdx;
    if (update == UpdateCachePosition) {
        fBufIdx = nextIdx;
        fTextIdx = position;
    }
    return true;
}


void RuleBasedBreakIterator::BreakCache::dumpCache() {
#ifdef RBBI_DEBUG
    RBBIDebugPrintf("fTextIdx:%d   fBufIdx:%d\n", fTextIdx, fBufIdx);
    for (int32_t i=fStartBufIdx; ; i=modChunkSize(i+1)) {
        RBBIDebugPrintf("%d  %d\n", i, fBoundaries[i]);
        if (i == fEndBufIdx) {
            break;
        }
    }
#endif
}

U_NAMESPACE_END

#endif // #if !UCONFIG_NO_BREAK_ITERATION

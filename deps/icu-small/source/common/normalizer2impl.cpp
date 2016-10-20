// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2009-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  normalizer2impl.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009nov22
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/normalizer2.h"
#include "unicode/udata.h"
#include "unicode/ustring.h"
#include "unicode/utf16.h"
#include "cmemory.h"
#include "mutex.h"
#include "normalizer2impl.h"
#include "putilimp.h"
#include "uassert.h"
#include "uset_imp.h"
#include "utrie2.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

// ReorderingBuffer -------------------------------------------------------- ***

UBool ReorderingBuffer::init(int32_t destCapacity, UErrorCode &errorCode) {
    int32_t length=str.length();
    start=str.getBuffer(destCapacity);
    if(start==NULL) {
        // getBuffer() already did str.setToBogus()
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return FALSE;
    }
    limit=start+length;
    remainingCapacity=str.getCapacity()-length;
    reorderStart=start;
    if(start==limit) {
        lastCC=0;
    } else {
        setIterator();
        lastCC=previousCC();
        // Set reorderStart after the last code point with cc<=1 if there is one.
        if(lastCC>1) {
            while(previousCC()>1) {}
        }
        reorderStart=codePointLimit;
    }
    return TRUE;
}

UBool ReorderingBuffer::equals(const UChar *otherStart, const UChar *otherLimit) const {
    int32_t length=(int32_t)(limit-start);
    return
        length==(int32_t)(otherLimit-otherStart) &&
        0==u_memcmp(start, otherStart, length);
}

UBool ReorderingBuffer::appendSupplementary(UChar32 c, uint8_t cc, UErrorCode &errorCode) {
    if(remainingCapacity<2 && !resize(2, errorCode)) {
        return FALSE;
    }
    if(lastCC<=cc || cc==0) {
        limit[0]=U16_LEAD(c);
        limit[1]=U16_TRAIL(c);
        limit+=2;
        lastCC=cc;
        if(cc<=1) {
            reorderStart=limit;
        }
    } else {
        insert(c, cc);
    }
    remainingCapacity-=2;
    return TRUE;
}

UBool ReorderingBuffer::append(const UChar *s, int32_t length,
                               uint8_t leadCC, uint8_t trailCC,
                               UErrorCode &errorCode) {
    if(length==0) {
        return TRUE;
    }
    if(remainingCapacity<length && !resize(length, errorCode)) {
        return FALSE;
    }
    remainingCapacity-=length;
    if(lastCC<=leadCC || leadCC==0) {
        if(trailCC<=1) {
            reorderStart=limit+length;
        } else if(leadCC<=1) {
            reorderStart=limit+1;  // Ok if not a code point boundary.
        }
        const UChar *sLimit=s+length;
        do { *limit++=*s++; } while(s!=sLimit);
        lastCC=trailCC;
    } else {
        int32_t i=0;
        UChar32 c;
        U16_NEXT(s, i, length, c);
        insert(c, leadCC);  // insert first code point
        while(i<length) {
            U16_NEXT(s, i, length, c);
            if(i<length) {
                // s must be in NFD, otherwise we need to use getCC().
                leadCC=Normalizer2Impl::getCCFromYesOrMaybe(impl.getNorm16(c));
            } else {
                leadCC=trailCC;
            }
            append(c, leadCC, errorCode);
        }
    }
    return TRUE;
}

UBool ReorderingBuffer::appendZeroCC(UChar32 c, UErrorCode &errorCode) {
    int32_t cpLength=U16_LENGTH(c);
    if(remainingCapacity<cpLength && !resize(cpLength, errorCode)) {
        return FALSE;
    }
    remainingCapacity-=cpLength;
    if(cpLength==1) {
        *limit++=(UChar)c;
    } else {
        limit[0]=U16_LEAD(c);
        limit[1]=U16_TRAIL(c);
        limit+=2;
    }
    lastCC=0;
    reorderStart=limit;
    return TRUE;
}

UBool ReorderingBuffer::appendZeroCC(const UChar *s, const UChar *sLimit, UErrorCode &errorCode) {
    if(s==sLimit) {
        return TRUE;
    }
    int32_t length=(int32_t)(sLimit-s);
    if(remainingCapacity<length && !resize(length, errorCode)) {
        return FALSE;
    }
    u_memcpy(limit, s, length);
    limit+=length;
    remainingCapacity-=length;
    lastCC=0;
    reorderStart=limit;
    return TRUE;
}

void ReorderingBuffer::remove() {
    reorderStart=limit=start;
    remainingCapacity=str.getCapacity();
    lastCC=0;
}

void ReorderingBuffer::removeSuffix(int32_t suffixLength) {
    if(suffixLength<(limit-start)) {
        limit-=suffixLength;
        remainingCapacity+=suffixLength;
    } else {
        limit=start;
        remainingCapacity=str.getCapacity();
    }
    lastCC=0;
    reorderStart=limit;
}

UBool ReorderingBuffer::resize(int32_t appendLength, UErrorCode &errorCode) {
    int32_t reorderStartIndex=(int32_t)(reorderStart-start);
    int32_t length=(int32_t)(limit-start);
    str.releaseBuffer(length);
    int32_t newCapacity=length+appendLength;
    int32_t doubleCapacity=2*str.getCapacity();
    if(newCapacity<doubleCapacity) {
        newCapacity=doubleCapacity;
    }
    if(newCapacity<256) {
        newCapacity=256;
    }
    start=str.getBuffer(newCapacity);
    if(start==NULL) {
        // getBuffer() already did str.setToBogus()
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return FALSE;
    }
    reorderStart=start+reorderStartIndex;
    limit=start+length;
    remainingCapacity=str.getCapacity()-length;
    return TRUE;
}

void ReorderingBuffer::skipPrevious() {
    codePointLimit=codePointStart;
    UChar c=*--codePointStart;
    if(U16_IS_TRAIL(c) && start<codePointStart && U16_IS_LEAD(*(codePointStart-1))) {
        --codePointStart;
    }
}

uint8_t ReorderingBuffer::previousCC() {
    codePointLimit=codePointStart;
    if(reorderStart>=codePointStart) {
        return 0;
    }
    UChar32 c=*--codePointStart;
    if(c<Normalizer2Impl::MIN_CCC_LCCC_CP) {
        return 0;
    }

    UChar c2;
    if(U16_IS_TRAIL(c) && start<codePointStart && U16_IS_LEAD(c2=*(codePointStart-1))) {
        --codePointStart;
        c=U16_GET_SUPPLEMENTARY(c2, c);
    }
    return Normalizer2Impl::getCCFromYesOrMaybe(impl.getNorm16(c));
}

// Inserts c somewhere before the last character.
// Requires 0<cc<lastCC which implies reorderStart<limit.
void ReorderingBuffer::insert(UChar32 c, uint8_t cc) {
    for(setIterator(), skipPrevious(); previousCC()>cc;) {}
    // insert c at codePointLimit, after the character with prevCC<=cc
    UChar *q=limit;
    UChar *r=limit+=U16_LENGTH(c);
    do {
        *--r=*--q;
    } while(codePointLimit!=q);
    writeCodePoint(q, c);
    if(cc<=1) {
        reorderStart=r;
    }
}

// Normalizer2Impl --------------------------------------------------------- ***

struct CanonIterData : public UMemory {
    CanonIterData(UErrorCode &errorCode);
    ~CanonIterData();
    void addToStartSet(UChar32 origin, UChar32 decompLead, UErrorCode &errorCode);
    UTrie2 *trie;
    UVector canonStartSets;  // contains UnicodeSet *
};

Normalizer2Impl::~Normalizer2Impl() {
    delete fCanonIterData;
}

void
Normalizer2Impl::init(const int32_t *inIndexes, const UTrie2 *inTrie,
                      const uint16_t *inExtraData, const uint8_t *inSmallFCD) {
    minDecompNoCP=inIndexes[IX_MIN_DECOMP_NO_CP];
    minCompNoMaybeCP=inIndexes[IX_MIN_COMP_NO_MAYBE_CP];

    minYesNo=inIndexes[IX_MIN_YES_NO];
    minYesNoMappingsOnly=inIndexes[IX_MIN_YES_NO_MAPPINGS_ONLY];
    minNoNo=inIndexes[IX_MIN_NO_NO];
    limitNoNo=inIndexes[IX_LIMIT_NO_NO];
    minMaybeYes=inIndexes[IX_MIN_MAYBE_YES];

    normTrie=inTrie;

    maybeYesCompositions=inExtraData;
    extraData=maybeYesCompositions+(MIN_NORMAL_MAYBE_YES-minMaybeYes);

    smallFCD=inSmallFCD;

    // Build tccc180[].
    // gennorm2 enforces lccc=0 for c<MIN_CCC_LCCC_CP=U+0300.
    uint8_t bits=0;
    for(UChar c=0; c<0x180; bits>>=1) {
        if((c&0xff)==0) {
            bits=smallFCD[c>>8];  // one byte per 0x100 code points
        }
        if(bits&1) {
            for(int i=0; i<0x20; ++i, ++c) {
                tccc180[c]=(uint8_t)getFCD16FromNormData(c);
            }
        } else {
            uprv_memset(tccc180+c, 0, 0x20);
            c+=0x20;
        }
    }
}

uint8_t Normalizer2Impl::getTrailCCFromCompYesAndZeroCC(const UChar *cpStart, const UChar *cpLimit) const {
    UChar32 c;
    if(cpStart==(cpLimit-1)) {
        c=*cpStart;
    } else {
        c=U16_GET_SUPPLEMENTARY(cpStart[0], cpStart[1]);
    }
    uint16_t prevNorm16=getNorm16(c);
    if(prevNorm16<=minYesNo) {
        return 0;  // yesYes and Hangul LV/LVT have ccc=tccc=0
    } else {
        return (uint8_t)(*getMapping(prevNorm16)>>8);  // tccc from yesNo
    }
}

namespace {

class LcccContext {
public:
    LcccContext(const Normalizer2Impl &ni, UnicodeSet &s) : impl(ni), set(s) {}

    void handleRange(UChar32 start, UChar32 end, uint16_t norm16) {
        if(impl.isAlgorithmicNoNo(norm16)) {
            // Range of code points with same-norm16-value algorithmic decompositions.
            // They might have different non-zero FCD16 values.
            do {
                uint16_t fcd16=impl.getFCD16(start);
                if(fcd16>0xff) { set.add(start); }
            } while(++start<=end);
        } else {
            uint16_t fcd16=impl.getFCD16(start);
            if(fcd16>0xff) { set.add(start, end); }
        }
    }

private:
    const Normalizer2Impl &impl;
    UnicodeSet &set;
};

struct PropertyStartsContext {
    PropertyStartsContext(const Normalizer2Impl &ni, const USetAdder *adder)
            : impl(ni), sa(adder) {}

    const Normalizer2Impl &impl;
    const USetAdder *sa;
};

}  // namespace

U_CDECL_BEGIN

static UBool U_CALLCONV
enumLcccRange(const void *context, UChar32 start, UChar32 end, uint32_t value) {
    ((LcccContext *)context)->handleRange(start, end, (uint16_t)value);
    return TRUE;
}

static UBool U_CALLCONV
enumNorm16PropertyStartsRange(const void *context, UChar32 start, UChar32 end, uint32_t value) {
    /* add the start code point to the USet */
    const PropertyStartsContext *ctx=(const PropertyStartsContext *)context;
    const USetAdder *sa=ctx->sa;
    sa->add(sa->set, start);
    if(start!=end && ctx->impl.isAlgorithmicNoNo((uint16_t)value)) {
        // Range of code points with same-norm16-value algorithmic decompositions.
        // They might have different non-zero FCD16 values.
        uint16_t prevFCD16=ctx->impl.getFCD16(start);
        while(++start<=end) {
            uint16_t fcd16=ctx->impl.getFCD16(start);
            if(fcd16!=prevFCD16) {
                sa->add(sa->set, start);
                prevFCD16=fcd16;
            }
        }
    }
    return TRUE;
}

static UBool U_CALLCONV
enumPropertyStartsRange(const void *context, UChar32 start, UChar32 /*end*/, uint32_t /*value*/) {
    /* add the start code point to the USet */
    const USetAdder *sa=(const USetAdder *)context;
    sa->add(sa->set, start);
    return TRUE;
}

static uint32_t U_CALLCONV
segmentStarterMapper(const void * /*context*/, uint32_t value) {
    return value&CANON_NOT_SEGMENT_STARTER;
}

U_CDECL_END

void
Normalizer2Impl::addLcccChars(UnicodeSet &set) const {
    /* add the start code point of each same-value range of each trie */
    LcccContext context(*this, set);
    utrie2_enum(normTrie, NULL, enumLcccRange, &context);
}

void
Normalizer2Impl::addPropertyStarts(const USetAdder *sa, UErrorCode & /*errorCode*/) const {
    /* add the start code point of each same-value range of each trie */
    PropertyStartsContext context(*this, sa);
    utrie2_enum(normTrie, NULL, enumNorm16PropertyStartsRange, &context);

    /* add Hangul LV syllables and LV+1 because of skippables */
    for(UChar c=Hangul::HANGUL_BASE; c<Hangul::HANGUL_LIMIT; c+=Hangul::JAMO_T_COUNT) {
        sa->add(sa->set, c);
        sa->add(sa->set, c+1);
    }
    sa->add(sa->set, Hangul::HANGUL_LIMIT); /* add Hangul+1 to continue with other properties */
}

void
Normalizer2Impl::addCanonIterPropertyStarts(const USetAdder *sa, UErrorCode &errorCode) const {
    /* add the start code point of each same-value range of the canonical iterator data trie */
    if(ensureCanonIterData(errorCode)) {
        // currently only used for the SEGMENT_STARTER property
        utrie2_enum(fCanonIterData->trie, segmentStarterMapper, enumPropertyStartsRange, sa);
    }
}

const UChar *
Normalizer2Impl::copyLowPrefixFromNulTerminated(const UChar *src,
                                                UChar32 minNeedDataCP,
                                                ReorderingBuffer *buffer,
                                                UErrorCode &errorCode) const {
    // Make some effort to support NUL-terminated strings reasonably.
    // Take the part of the fast quick check loop that does not look up
    // data and check the first part of the string.
    // After this prefix, determine the string length to simplify the rest
    // of the code.
    const UChar *prevSrc=src;
    UChar c;
    while((c=*src++)<minNeedDataCP && c!=0) {}
    // Back out the last character for full processing.
    // Copy this prefix.
    if(--src!=prevSrc) {
        if(buffer!=NULL) {
            buffer->appendZeroCC(prevSrc, src, errorCode);
        }
    }
    return src;
}

UnicodeString &
Normalizer2Impl::decompose(const UnicodeString &src, UnicodeString &dest,
                           UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) {
        dest.setToBogus();
        return dest;
    }
    const UChar *sArray=src.getBuffer();
    if(&dest==&src || sArray==NULL) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        dest.setToBogus();
        return dest;
    }
    decompose(sArray, sArray+src.length(), dest, src.length(), errorCode);
    return dest;
}

void
Normalizer2Impl::decompose(const UChar *src, const UChar *limit,
                           UnicodeString &dest,
                           int32_t destLengthEstimate,
                           UErrorCode &errorCode) const {
    if(destLengthEstimate<0 && limit!=NULL) {
        destLengthEstimate=(int32_t)(limit-src);
    }
    dest.remove();
    ReorderingBuffer buffer(*this, dest);
    if(buffer.init(destLengthEstimate, errorCode)) {
        decompose(src, limit, &buffer, errorCode);
    }
}

// Dual functionality:
// buffer!=NULL: normalize
// buffer==NULL: isNormalized/spanQuickCheckYes
const UChar *
Normalizer2Impl::decompose(const UChar *src, const UChar *limit,
                           ReorderingBuffer *buffer,
                           UErrorCode &errorCode) const {
    UChar32 minNoCP=minDecompNoCP;
    if(limit==NULL) {
        src=copyLowPrefixFromNulTerminated(src, minNoCP, buffer, errorCode);
        if(U_FAILURE(errorCode)) {
            return src;
        }
        limit=u_strchr(src, 0);
    }

    const UChar *prevSrc;
    UChar32 c=0;
    uint16_t norm16=0;

    // only for quick check
    const UChar *prevBoundary=src;
    uint8_t prevCC=0;

    for(;;) {
        // count code units below the minimum or with irrelevant data for the quick check
        for(prevSrc=src; src!=limit;) {
            if( (c=*src)<minNoCP ||
                isMostDecompYesAndZeroCC(norm16=UTRIE2_GET16_FROM_U16_SINGLE_LEAD(normTrie, c))
            ) {
                ++src;
            } else if(!U16_IS_SURROGATE(c)) {
                break;
            } else {
                UChar c2;
                if(U16_IS_SURROGATE_LEAD(c)) {
                    if((src+1)!=limit && U16_IS_TRAIL(c2=src[1])) {
                        c=U16_GET_SUPPLEMENTARY(c, c2);
                    }
                } else /* trail surrogate */ {
                    if(prevSrc<src && U16_IS_LEAD(c2=*(src-1))) {
                        --src;
                        c=U16_GET_SUPPLEMENTARY(c2, c);
                    }
                }
                if(isMostDecompYesAndZeroCC(norm16=getNorm16(c))) {
                    src+=U16_LENGTH(c);
                } else {
                    break;
                }
            }
        }
        // copy these code units all at once
        if(src!=prevSrc) {
            if(buffer!=NULL) {
                if(!buffer->appendZeroCC(prevSrc, src, errorCode)) {
                    break;
                }
            } else {
                prevCC=0;
                prevBoundary=src;
            }
        }
        if(src==limit) {
            break;
        }

        // Check one above-minimum, relevant code point.
        src+=U16_LENGTH(c);
        if(buffer!=NULL) {
            if(!decompose(c, norm16, *buffer, errorCode)) {
                break;
            }
        } else {
            if(isDecompYes(norm16)) {
                uint8_t cc=getCCFromYesOrMaybe(norm16);
                if(prevCC<=cc || cc==0) {
                    prevCC=cc;
                    if(cc<=1) {
                        prevBoundary=src;
                    }
                    continue;
                }
            }
            return prevBoundary;  // "no" or cc out of order
        }
    }
    return src;
}

// Decompose a short piece of text which is likely to contain characters that
// fail the quick check loop and/or where the quick check loop's overhead
// is unlikely to be amortized.
// Called by the compose() and makeFCD() implementations.
UBool Normalizer2Impl::decomposeShort(const UChar *src, const UChar *limit,
                                      ReorderingBuffer &buffer,
                                      UErrorCode &errorCode) const {
    while(src<limit) {
        UChar32 c;
        uint16_t norm16;
        UTRIE2_U16_NEXT16(normTrie, src, limit, c, norm16);
        if(!decompose(c, norm16, buffer, errorCode)) {
            return FALSE;
        }
    }
    return TRUE;
}

UBool Normalizer2Impl::decompose(UChar32 c, uint16_t norm16,
                                 ReorderingBuffer &buffer,
                                 UErrorCode &errorCode) const {
    // Only loops for 1:1 algorithmic mappings.
    for(;;) {
        // get the decomposition and the lead and trail cc's
        if(isDecompYes(norm16)) {
            // c does not decompose
            return buffer.append(c, getCCFromYesOrMaybe(norm16), errorCode);
        } else if(isHangul(norm16)) {
            // Hangul syllable: decompose algorithmically
            UChar jamos[3];
            return buffer.appendZeroCC(jamos, jamos+Hangul::decompose(c, jamos), errorCode);
        } else if(isDecompNoAlgorithmic(norm16)) {
            c=mapAlgorithmic(c, norm16);
            norm16=getNorm16(c);
        } else {
            // c decomposes, get everything from the variable-length extra data
            const uint16_t *mapping=getMapping(norm16);
            uint16_t firstUnit=*mapping;
            int32_t length=firstUnit&MAPPING_LENGTH_MASK;
            uint8_t leadCC, trailCC;
            trailCC=(uint8_t)(firstUnit>>8);
            if(firstUnit&MAPPING_HAS_CCC_LCCC_WORD) {
                leadCC=(uint8_t)(*(mapping-1)>>8);
            } else {
                leadCC=0;
            }
            return buffer.append((const UChar *)mapping+1, length, leadCC, trailCC, errorCode);
        }
    }
}

const UChar *
Normalizer2Impl::getDecomposition(UChar32 c, UChar buffer[4], int32_t &length) const {
    const UChar *decomp=NULL;
    uint16_t norm16;
    for(;;) {
        if(c<minDecompNoCP || isDecompYes(norm16=getNorm16(c))) {
            // c does not decompose
            return decomp;
        } else if(isHangul(norm16)) {
            // Hangul syllable: decompose algorithmically
            length=Hangul::decompose(c, buffer);
            return buffer;
        } else if(isDecompNoAlgorithmic(norm16)) {
            c=mapAlgorithmic(c, norm16);
            decomp=buffer;
            length=0;
            U16_APPEND_UNSAFE(buffer, length, c);
        } else {
            // c decomposes, get everything from the variable-length extra data
            const uint16_t *mapping=getMapping(norm16);
            length=*mapping&MAPPING_LENGTH_MASK;
            return (const UChar *)mapping+1;
        }
    }
}

// The capacity of the buffer must be 30=MAPPING_LENGTH_MASK-1
// so that a raw mapping fits that consists of one unit ("rm0")
// plus all but the first two code units of the normal mapping.
// The maximum length of a normal mapping is 31=MAPPING_LENGTH_MASK.
const UChar *
Normalizer2Impl::getRawDecomposition(UChar32 c, UChar buffer[30], int32_t &length) const {
    // We do not loop in this method because an algorithmic mapping itself
    // becomes a final result rather than having to be decomposed recursively.
    uint16_t norm16;
    if(c<minDecompNoCP || isDecompYes(norm16=getNorm16(c))) {
        // c does not decompose
        return NULL;
    } else if(isHangul(norm16)) {
        // Hangul syllable: decompose algorithmically
        Hangul::getRawDecomposition(c, buffer);
        length=2;
        return buffer;
    } else if(isDecompNoAlgorithmic(norm16)) {
        c=mapAlgorithmic(c, norm16);
        length=0;
        U16_APPEND_UNSAFE(buffer, length, c);
        return buffer;
    } else {
        // c decomposes, get everything from the variable-length extra data
        const uint16_t *mapping=getMapping(norm16);
        uint16_t firstUnit=*mapping;
        int32_t mLength=firstUnit&MAPPING_LENGTH_MASK;  // length of normal mapping
        if(firstUnit&MAPPING_HAS_RAW_MAPPING) {
            // Read the raw mapping from before the firstUnit and before the optional ccc/lccc word.
            // Bit 7=MAPPING_HAS_CCC_LCCC_WORD
            const uint16_t *rawMapping=mapping-((firstUnit>>7)&1)-1;
            uint16_t rm0=*rawMapping;
            if(rm0<=MAPPING_LENGTH_MASK) {
                length=rm0;
                return (const UChar *)rawMapping-rm0;
            } else {
                // Copy the normal mapping and replace its first two code units with rm0.
                buffer[0]=(UChar)rm0;
                u_memcpy(buffer+1, (const UChar *)mapping+1+2, mLength-2);
                length=mLength-1;
                return buffer;
            }
        } else {
            length=mLength;
            return (const UChar *)mapping+1;
        }
    }
}

void Normalizer2Impl::decomposeAndAppend(const UChar *src, const UChar *limit,
                                         UBool doDecompose,
                                         UnicodeString &safeMiddle,
                                         ReorderingBuffer &buffer,
                                         UErrorCode &errorCode) const {
    buffer.copyReorderableSuffixTo(safeMiddle);
    if(doDecompose) {
        decompose(src, limit, &buffer, errorCode);
        return;
    }
    // Just merge the strings at the boundary.
    ForwardUTrie2StringIterator iter(normTrie, src, limit);
    uint8_t firstCC, prevCC, cc;
    firstCC=prevCC=cc=getCC(iter.next16());
    while(cc!=0) {
        prevCC=cc;
        cc=getCC(iter.next16());
    };
    if(limit==NULL) {  // appendZeroCC() needs limit!=NULL
        limit=u_strchr(iter.codePointStart, 0);
    }

    if (buffer.append(src, (int32_t)(iter.codePointStart-src), firstCC, prevCC, errorCode)) {
        buffer.appendZeroCC(iter.codePointStart, limit, errorCode);
    }
}

// Note: hasDecompBoundary() could be implemented as aliases to
// hasFCDBoundaryBefore() and hasFCDBoundaryAfter()
// at the cost of building the FCD trie for a decomposition normalizer.
UBool Normalizer2Impl::hasDecompBoundary(UChar32 c, UBool before) const {
    for(;;) {
        if(c<minDecompNoCP) {
            return TRUE;
        }
        uint16_t norm16=getNorm16(c);
        if(isHangul(norm16) || isDecompYesAndZeroCC(norm16)) {
            return TRUE;
        } else if(norm16>MIN_NORMAL_MAYBE_YES) {
            return FALSE;  // ccc!=0
        } else if(isDecompNoAlgorithmic(norm16)) {
            c=mapAlgorithmic(c, norm16);
        } else {
            // c decomposes, get everything from the variable-length extra data
            const uint16_t *mapping=getMapping(norm16);
            uint16_t firstUnit=*mapping;
            if((firstUnit&MAPPING_LENGTH_MASK)==0) {
                return FALSE;
            }
            if(!before) {
                // decomp after-boundary: same as hasFCDBoundaryAfter(),
                // fcd16<=1 || trailCC==0
                if(firstUnit>0x1ff) {
                    return FALSE;  // trailCC>1
                }
                if(firstUnit<=0xff) {
                    return TRUE;  // trailCC==0
                }
                // if(trailCC==1) test leadCC==0, same as checking for before-boundary
            }
            // TRUE if leadCC==0 (hasFCDBoundaryBefore())
            return (firstUnit&MAPPING_HAS_CCC_LCCC_WORD)==0 || (*(mapping-1)&0xff00)==0;
        }
    }
}

/*
 * Finds the recomposition result for
 * a forward-combining "lead" character,
 * specified with a pointer to its compositions list,
 * and a backward-combining "trail" character.
 *
 * If the lead and trail characters combine, then this function returns
 * the following "compositeAndFwd" value:
 * Bits 21..1  composite character
 * Bit      0  set if the composite is a forward-combining starter
 * otherwise it returns -1.
 *
 * The compositions list has (trail, compositeAndFwd) pair entries,
 * encoded as either pairs or triples of 16-bit units.
 * The last entry has the high bit of its first unit set.
 *
 * The list is sorted by ascending trail characters (there are no duplicates).
 * A linear search is used.
 *
 * See normalizer2impl.h for a more detailed description
 * of the compositions list format.
 */
int32_t Normalizer2Impl::combine(const uint16_t *list, UChar32 trail) {
    uint16_t key1, firstUnit;
    if(trail<COMP_1_TRAIL_LIMIT) {
        // trail character is 0..33FF
        // result entry may have 2 or 3 units
        key1=(uint16_t)(trail<<1);
        while(key1>(firstUnit=*list)) {
            list+=2+(firstUnit&COMP_1_TRIPLE);
        }
        if(key1==(firstUnit&COMP_1_TRAIL_MASK)) {
            if(firstUnit&COMP_1_TRIPLE) {
                return ((int32_t)list[1]<<16)|list[2];
            } else {
                return list[1];
            }
        }
    } else {
        // trail character is 3400..10FFFF
        // result entry has 3 units
        key1=(uint16_t)(COMP_1_TRAIL_LIMIT+
                        (((trail>>COMP_1_TRAIL_SHIFT))&
                          ~COMP_1_TRIPLE));
        uint16_t key2=(uint16_t)(trail<<COMP_2_TRAIL_SHIFT);
        uint16_t secondUnit;
        for(;;) {
            if(key1>(firstUnit=*list)) {
                list+=2+(firstUnit&COMP_1_TRIPLE);
            } else if(key1==(firstUnit&COMP_1_TRAIL_MASK)) {
                if(key2>(secondUnit=list[1])) {
                    if(firstUnit&COMP_1_LAST_TUPLE) {
                        break;
                    } else {
                        list+=3;
                    }
                } else if(key2==(secondUnit&COMP_2_TRAIL_MASK)) {
                    return ((int32_t)(secondUnit&~COMP_2_TRAIL_MASK)<<16)|list[2];
                } else {
                    break;
                }
            } else {
                break;
            }
        }
    }
    return -1;
}

/**
  * @param list some character's compositions list
  * @param set recursively receives the composites from these compositions
  */
void Normalizer2Impl::addComposites(const uint16_t *list, UnicodeSet &set) const {
    uint16_t firstUnit;
    int32_t compositeAndFwd;
    do {
        firstUnit=*list;
        if((firstUnit&COMP_1_TRIPLE)==0) {
            compositeAndFwd=list[1];
            list+=2;
        } else {
            compositeAndFwd=(((int32_t)list[1]&~COMP_2_TRAIL_MASK)<<16)|list[2];
            list+=3;
        }
        UChar32 composite=compositeAndFwd>>1;
        if((compositeAndFwd&1)!=0) {
            addComposites(getCompositionsListForComposite(getNorm16(composite)), set);
        }
        set.add(composite);
    } while((firstUnit&COMP_1_LAST_TUPLE)==0);
}

/*
 * Recomposes the buffer text starting at recomposeStartIndex
 * (which is in NFD - decomposed and canonically ordered),
 * and truncates the buffer contents.
 *
 * Note that recomposition never lengthens the text:
 * Any character consists of either one or two code units;
 * a composition may contain at most one more code unit than the original starter,
 * while the combining mark that is removed has at least one code unit.
 */
void Normalizer2Impl::recompose(ReorderingBuffer &buffer, int32_t recomposeStartIndex,
                                UBool onlyContiguous) const {
    UChar *p=buffer.getStart()+recomposeStartIndex;
    UChar *limit=buffer.getLimit();
    if(p==limit) {
        return;
    }

    UChar *starter, *pRemove, *q, *r;
    const uint16_t *compositionsList;
    UChar32 c, compositeAndFwd;
    uint16_t norm16;
    uint8_t cc, prevCC;
    UBool starterIsSupplementary;

    // Some of the following variables are not used until we have a forward-combining starter
    // and are only initialized now to avoid compiler warnings.
    compositionsList=NULL;  // used as indicator for whether we have a forward-combining starter
    starter=NULL;
    starterIsSupplementary=FALSE;
    prevCC=0;

    for(;;) {
        UTRIE2_U16_NEXT16(normTrie, p, limit, c, norm16);
        cc=getCCFromYesOrMaybe(norm16);
        if( // this character combines backward and
            isMaybe(norm16) &&
            // we have seen a starter that combines forward and
            compositionsList!=NULL &&
            // the backward-combining character is not blocked
            (prevCC<cc || prevCC==0)
        ) {
            if(isJamoVT(norm16)) {
                // c is a Jamo V/T, see if we can compose it with the previous character.
                if(c<Hangul::JAMO_T_BASE) {
                    // c is a Jamo Vowel, compose with previous Jamo L and following Jamo T.
                    UChar prev=(UChar)(*starter-Hangul::JAMO_L_BASE);
                    if(prev<Hangul::JAMO_L_COUNT) {
                        pRemove=p-1;
                        UChar syllable=(UChar)
                            (Hangul::HANGUL_BASE+
                             (prev*Hangul::JAMO_V_COUNT+(c-Hangul::JAMO_V_BASE))*
                             Hangul::JAMO_T_COUNT);
                        UChar t;
                        if(p!=limit && (t=(UChar)(*p-Hangul::JAMO_T_BASE))<Hangul::JAMO_T_COUNT) {
                            ++p;
                            syllable+=t;  // The next character was a Jamo T.
                        }
                        *starter=syllable;
                        // remove the Jamo V/T
                        q=pRemove;
                        r=p;
                        while(r<limit) {
                            *q++=*r++;
                        }
                        limit=q;
                        p=pRemove;
                    }
                }
                /*
                 * No "else" for Jamo T:
                 * Since the input is in NFD, there are no Hangul LV syllables that
                 * a Jamo T could combine with.
                 * All Jamo Ts are combined above when handling Jamo Vs.
                 */
                if(p==limit) {
                    break;
                }
                compositionsList=NULL;
                continue;
            } else if((compositeAndFwd=combine(compositionsList, c))>=0) {
                // The starter and the combining mark (c) do combine.
                UChar32 composite=compositeAndFwd>>1;

                // Replace the starter with the composite, remove the combining mark.
                pRemove=p-U16_LENGTH(c);  // pRemove & p: start & limit of the combining mark
                if(starterIsSupplementary) {
                    if(U_IS_SUPPLEMENTARY(composite)) {
                        // both are supplementary
                        starter[0]=U16_LEAD(composite);
                        starter[1]=U16_TRAIL(composite);
                    } else {
                        *starter=(UChar)composite;
                        // The composite is shorter than the starter,
                        // move the intermediate characters forward one.
                        starterIsSupplementary=FALSE;
                        q=starter+1;
                        r=q+1;
                        while(r<pRemove) {
                            *q++=*r++;
                        }
                        --pRemove;
                    }
                } else if(U_IS_SUPPLEMENTARY(composite)) {
                    // The composite is longer than the starter,
                    // move the intermediate characters back one.
                    starterIsSupplementary=TRUE;
                    ++starter;  // temporarily increment for the loop boundary
                    q=pRemove;
                    r=++pRemove;
                    while(starter<q) {
                        *--r=*--q;
                    }
                    *starter=U16_TRAIL(composite);
                    *--starter=U16_LEAD(composite);  // undo the temporary increment
                } else {
                    // both are on the BMP
                    *starter=(UChar)composite;
                }

                /* remove the combining mark by moving the following text over it */
                if(pRemove<p) {
                    q=pRemove;
                    r=p;
                    while(r<limit) {
                        *q++=*r++;
                    }
                    limit=q;
                    p=pRemove;
                }
                // Keep prevCC because we removed the combining mark.

                if(p==limit) {
                    break;
                }
                // Is the composite a starter that combines forward?
                if(compositeAndFwd&1) {
                    compositionsList=
                        getCompositionsListForComposite(getNorm16(composite));
                } else {
                    compositionsList=NULL;
                }

                // We combined; continue with looking for compositions.
                continue;
            }
        }

        // no combination this time
        prevCC=cc;
        if(p==limit) {
            break;
        }

        // If c did not combine, then check if it is a starter.
        if(cc==0) {
            // Found a new starter.
            if((compositionsList=getCompositionsListForDecompYes(norm16))!=NULL) {
                // It may combine with something, prepare for it.
                if(U_IS_BMP(c)) {
                    starterIsSupplementary=FALSE;
                    starter=p-1;
                } else {
                    starterIsSupplementary=TRUE;
                    starter=p-2;
                }
            }
        } else if(onlyContiguous) {
            // FCC: no discontiguous compositions; any intervening character blocks.
            compositionsList=NULL;
        }
    }
    buffer.setReorderingLimit(limit);
}

UChar32
Normalizer2Impl::composePair(UChar32 a, UChar32 b) const {
    uint16_t norm16=getNorm16(a);  // maps an out-of-range 'a' to inert norm16=0
    const uint16_t *list;
    if(isInert(norm16)) {
        return U_SENTINEL;
    } else if(norm16<minYesNoMappingsOnly) {
        if(isJamoL(norm16)) {
            b-=Hangul::JAMO_V_BASE;
            if(0<=b && b<Hangul::JAMO_V_COUNT) {
                return
                    (Hangul::HANGUL_BASE+
                     ((a-Hangul::JAMO_L_BASE)*Hangul::JAMO_V_COUNT+b)*
                     Hangul::JAMO_T_COUNT);
            } else {
                return U_SENTINEL;
            }
        } else if(isHangul(norm16)) {
            b-=Hangul::JAMO_T_BASE;
            if(Hangul::isHangulWithoutJamoT(a) && 0<b && b<Hangul::JAMO_T_COUNT) {  // not b==0!
                return a+b;
            } else {
                return U_SENTINEL;
            }
        } else {
            // 'a' has a compositions list in extraData
            list=extraData+norm16;
            if(norm16>minYesNo) {  // composite 'a' has both mapping & compositions list
                list+=  // mapping pointer
                    1+  // +1 to skip the first unit with the mapping lenth
                    (*list&MAPPING_LENGTH_MASK);  // + mapping length
            }
        }
    } else if(norm16<minMaybeYes || MIN_NORMAL_MAYBE_YES<=norm16) {
        return U_SENTINEL;
    } else {
        list=maybeYesCompositions+norm16-minMaybeYes;
    }
    if(b<0 || 0x10ffff<b) {  // combine(list, b) requires a valid code point b
        return U_SENTINEL;
    }
#if U_SIGNED_RIGHT_SHIFT_IS_ARITHMETIC
    return combine(list, b)>>1;
#else
    int32_t compositeAndFwd=combine(list, b);
    return compositeAndFwd>=0 ? compositeAndFwd>>1 : U_SENTINEL;
#endif
}

// Very similar to composeQuickCheck(): Make the same changes in both places if relevant.
// doCompose: normalize
// !doCompose: isNormalized (buffer must be empty and initialized)
UBool
Normalizer2Impl::compose(const UChar *src, const UChar *limit,
                         UBool onlyContiguous,
                         UBool doCompose,
                         ReorderingBuffer &buffer,
                         UErrorCode &errorCode) const {
    /*
     * prevBoundary points to the last character before the current one
     * that has a composition boundary before it with ccc==0 and quick check "yes".
     * Keeping track of prevBoundary saves us looking for a composition boundary
     * when we find a "no" or "maybe".
     *
     * When we back out from prevSrc back to prevBoundary,
     * then we also remove those same characters (which had been simply copied
     * or canonically-order-inserted) from the ReorderingBuffer.
     * Therefore, at all times, the [prevBoundary..prevSrc[ source units
     * must correspond 1:1 to destination units at the end of the destination buffer.
     */
    const UChar *prevBoundary=src;
    UChar32 minNoMaybeCP=minCompNoMaybeCP;
    if(limit==NULL) {
        src=copyLowPrefixFromNulTerminated(src, minNoMaybeCP,
                                           doCompose ? &buffer : NULL,
                                           errorCode);
        if(U_FAILURE(errorCode)) {
            return FALSE;
        }
        if(prevBoundary<src) {
            // Set prevBoundary to the last character in the prefix.
            prevBoundary=src-1;
        }
        limit=u_strchr(src, 0);
    }

    const UChar *prevSrc;
    UChar32 c=0;
    uint16_t norm16=0;

    // only for isNormalized
    uint8_t prevCC=0;

    for(;;) {
        // count code units below the minimum or with irrelevant data for the quick check
        for(prevSrc=src; src!=limit;) {
            if( (c=*src)<minNoMaybeCP ||
                isCompYesAndZeroCC(norm16=UTRIE2_GET16_FROM_U16_SINGLE_LEAD(normTrie, c))
            ) {
                ++src;
            } else if(!U16_IS_SURROGATE(c)) {
                break;
            } else {
                UChar c2;
                if(U16_IS_SURROGATE_LEAD(c)) {
                    if((src+1)!=limit && U16_IS_TRAIL(c2=src[1])) {
                        c=U16_GET_SUPPLEMENTARY(c, c2);
                    }
                } else /* trail surrogate */ {
                    if(prevSrc<src && U16_IS_LEAD(c2=*(src-1))) {
                        --src;
                        c=U16_GET_SUPPLEMENTARY(c2, c);
                    }
                }
                if(isCompYesAndZeroCC(norm16=getNorm16(c))) {
                    src+=U16_LENGTH(c);
                } else {
                    break;
                }
            }
        }
        // copy these code units all at once
        if(src!=prevSrc) {
            if(doCompose) {
                if(!buffer.appendZeroCC(prevSrc, src, errorCode)) {
                    break;
                }
            } else {
                prevCC=0;
            }
            if(src==limit) {
                break;
            }
            // Set prevBoundary to the last character in the quick check loop.
            prevBoundary=src-1;
            if( U16_IS_TRAIL(*prevBoundary) && prevSrc<prevBoundary &&
                U16_IS_LEAD(*(prevBoundary-1))
            ) {
                --prevBoundary;
            }
            // The start of the current character (c).
            prevSrc=src;
        } else if(src==limit) {
            break;
        }

        src+=U16_LENGTH(c);
        /*
         * isCompYesAndZeroCC(norm16) is false, that is, norm16>=minNoNo.
         * c is either a "noNo" (has a mapping) or a "maybeYes" (combines backward)
         * or has ccc!=0.
         * Check for Jamo V/T, then for regular characters.
         * c is not a Hangul syllable or Jamo L because those have "yes" properties.
         */
        if(isJamoVT(norm16) && prevBoundary!=prevSrc) {
            UChar prev=*(prevSrc-1);
            UBool needToDecompose=FALSE;
            if(c<Hangul::JAMO_T_BASE) {
                // c is a Jamo Vowel, compose with previous Jamo L and following Jamo T.
                prev=(UChar)(prev-Hangul::JAMO_L_BASE);
                if(prev<Hangul::JAMO_L_COUNT) {
                    if(!doCompose) {
                        return FALSE;
                    }
                    UChar syllable=(UChar)
                        (Hangul::HANGUL_BASE+
                         (prev*Hangul::JAMO_V_COUNT+(c-Hangul::JAMO_V_BASE))*
                         Hangul::JAMO_T_COUNT);
                    UChar t;
                    if(src!=limit && (t=(UChar)(*src-Hangul::JAMO_T_BASE))<Hangul::JAMO_T_COUNT) {
                        ++src;
                        syllable+=t;  // The next character was a Jamo T.
                        prevBoundary=src;
                        buffer.setLastChar(syllable);
                        continue;
                    }
                    // If we see L+V+x where x!=T then we drop to the slow path,
                    // decompose and recompose.
                    // This is to deal with NFKC finding normal L and V but a
                    // compatibility variant of a T. We need to either fully compose that
                    // combination here (which would complicate the code and may not work
                    // with strange custom data) or use the slow path -- or else our replacing
                    // two input characters (L+V) with one output character (LV syllable)
                    // would violate the invariant that [prevBoundary..prevSrc[ has the same
                    // length as what we appended to the buffer since prevBoundary.
                    needToDecompose=TRUE;
                }
            } else if(Hangul::isHangulWithoutJamoT(prev)) {
                // c is a Jamo Trailing consonant,
                // compose with previous Hangul LV that does not contain a Jamo T.
                if(!doCompose) {
                    return FALSE;
                }
                buffer.setLastChar((UChar)(prev+c-Hangul::JAMO_T_BASE));
                prevBoundary=src;
                continue;
            }
            if(!needToDecompose) {
                // The Jamo V/T did not compose into a Hangul syllable.
                if(doCompose) {
                    if(!buffer.appendBMP((UChar)c, 0, errorCode)) {
                        break;
                    }
                } else {
                    prevCC=0;
                }
                continue;
            }
        }
        /*
         * Source buffer pointers:
         *
         *  all done      quick check   current char  not yet
         *                "yes" but     (c)           processed
         *                may combine
         *                forward
         * [-------------[-------------[-------------[-------------[
         * |             |             |             |             |
         * orig. src     prevBoundary  prevSrc       src           limit
         *
         *
         * Destination buffer pointers inside the ReorderingBuffer:
         *
         *  all done      might take    not filled yet
         *                characters for
         *                reordering
         * [-------------[-------------[-------------[
         * |             |             |             |
         * start         reorderStart  limit         |
         *                             +remainingCap.+
         */
        if(norm16>=MIN_YES_YES_WITH_CC) {
            uint8_t cc=(uint8_t)norm16;  // cc!=0
            if( onlyContiguous &&  // FCC
                (doCompose ? buffer.getLastCC() : prevCC)==0 &&
                prevBoundary<prevSrc &&
                // buffer.getLastCC()==0 && prevBoundary<prevSrc tell us that
                // [prevBoundary..prevSrc[ (which is exactly one character under these conditions)
                // passed the quick check "yes && ccc==0" test.
                // Check whether the last character was a "yesYes" or a "yesNo".
                // If a "yesNo", then we get its trailing ccc from its
                // mapping and check for canonical order.
                // All other cases are ok.
                getTrailCCFromCompYesAndZeroCC(prevBoundary, prevSrc)>cc
            ) {
                // Fails FCD test, need to decompose and contiguously recompose.
                if(!doCompose) {
                    return FALSE;
                }
            } else if(doCompose) {
                if(!buffer.append(c, cc, errorCode)) {
                    break;
                }
                continue;
            } else if(prevCC<=cc) {
                prevCC=cc;
                continue;
            } else {
                return FALSE;
            }
        } else if(!doCompose && !isMaybeOrNonZeroCC(norm16)) {
            return FALSE;
        }

        /*
         * Find appropriate boundaries around this character,
         * decompose the source text from between the boundaries,
         * and recompose it.
         *
         * We may need to remove the last few characters from the ReorderingBuffer
         * to account for source text that was copied or appended
         * but needs to take part in the recomposition.
         */

        /*
         * Find the last composition boundary in [prevBoundary..src[.
         * It is either the decomposition of the current character (at prevSrc),
         * or prevBoundary.
         */
        if(hasCompBoundaryBefore(c, norm16)) {
            prevBoundary=prevSrc;
        } else if(doCompose) {
            buffer.removeSuffix((int32_t)(prevSrc-prevBoundary));
        }

        // Find the next composition boundary in [src..limit[ -
        // modifies src to point to the next starter.
        src=(UChar *)findNextCompBoundary(src, limit);

        // Decompose [prevBoundary..src[ into the buffer and then recompose that part of it.
        int32_t recomposeStartIndex=buffer.length();
        if(!decomposeShort(prevBoundary, src, buffer, errorCode)) {
            break;
        }
        recompose(buffer, recomposeStartIndex, onlyContiguous);
        if(!doCompose) {
            if(!buffer.equals(prevBoundary, src)) {
                return FALSE;
            }
            buffer.remove();
            prevCC=0;
        }

        // Move to the next starter. We never need to look back before this point again.
        prevBoundary=src;
    }
    return TRUE;
}

// Very similar to compose(): Make the same changes in both places if relevant.
// pQCResult==NULL: spanQuickCheckYes
// pQCResult!=NULL: quickCheck (*pQCResult must be UNORM_YES)
const UChar *
Normalizer2Impl::composeQuickCheck(const UChar *src, const UChar *limit,
                                   UBool onlyContiguous,
                                   UNormalizationCheckResult *pQCResult) const {
    /*
     * prevBoundary points to the last character before the current one
     * that has a composition boundary before it with ccc==0 and quick check "yes".
     */
    const UChar *prevBoundary=src;
    UChar32 minNoMaybeCP=minCompNoMaybeCP;
    if(limit==NULL) {
        UErrorCode errorCode=U_ZERO_ERROR;
        src=copyLowPrefixFromNulTerminated(src, minNoMaybeCP, NULL, errorCode);
        if(prevBoundary<src) {
            // Set prevBoundary to the last character in the prefix.
            prevBoundary=src-1;
        }
        limit=u_strchr(src, 0);
    }

    const UChar *prevSrc;
    UChar32 c=0;
    uint16_t norm16=0;
    uint8_t prevCC=0;

    for(;;) {
        // count code units below the minimum or with irrelevant data for the quick check
        for(prevSrc=src;;) {
            if(src==limit) {
                return src;
            }
            if( (c=*src)<minNoMaybeCP ||
                isCompYesAndZeroCC(norm16=UTRIE2_GET16_FROM_U16_SINGLE_LEAD(normTrie, c))
            ) {
                ++src;
            } else if(!U16_IS_SURROGATE(c)) {
                break;
            } else {
                UChar c2;
                if(U16_IS_SURROGATE_LEAD(c)) {
                    if((src+1)!=limit && U16_IS_TRAIL(c2=src[1])) {
                        c=U16_GET_SUPPLEMENTARY(c, c2);
                    }
                } else /* trail surrogate */ {
                    if(prevSrc<src && U16_IS_LEAD(c2=*(src-1))) {
                        --src;
                        c=U16_GET_SUPPLEMENTARY(c2, c);
                    }
                }
                if(isCompYesAndZeroCC(norm16=getNorm16(c))) {
                    src+=U16_LENGTH(c);
                } else {
                    break;
                }
            }
        }
        if(src!=prevSrc) {
            // Set prevBoundary to the last character in the quick check loop.
            prevBoundary=src-1;
            if( U16_IS_TRAIL(*prevBoundary) && prevSrc<prevBoundary &&
                U16_IS_LEAD(*(prevBoundary-1))
            ) {
                --prevBoundary;
            }
            prevCC=0;
            // The start of the current character (c).
            prevSrc=src;
        }

        src+=U16_LENGTH(c);
        /*
         * isCompYesAndZeroCC(norm16) is false, that is, norm16>=minNoNo.
         * c is either a "noNo" (has a mapping) or a "maybeYes" (combines backward)
         * or has ccc!=0.
         */
        if(isMaybeOrNonZeroCC(norm16)) {
            uint8_t cc=getCCFromYesOrMaybe(norm16);
            if( onlyContiguous &&  // FCC
                cc!=0 &&
                prevCC==0 &&
                prevBoundary<prevSrc &&
                // prevCC==0 && prevBoundary<prevSrc tell us that
                // [prevBoundary..prevSrc[ (which is exactly one character under these conditions)
                // passed the quick check "yes && ccc==0" test.
                // Check whether the last character was a "yesYes" or a "yesNo".
                // If a "yesNo", then we get its trailing ccc from its
                // mapping and check for canonical order.
                // All other cases are ok.
                getTrailCCFromCompYesAndZeroCC(prevBoundary, prevSrc)>cc
            ) {
                // Fails FCD test.
            } else if(prevCC<=cc || cc==0) {
                prevCC=cc;
                if(norm16<MIN_YES_YES_WITH_CC) {
                    if(pQCResult!=NULL) {
                        *pQCResult=UNORM_MAYBE;
                    } else {
                        return prevBoundary;
                    }
                }
                continue;
            }
        }
        if(pQCResult!=NULL) {
            *pQCResult=UNORM_NO;
        }
        return prevBoundary;
    }
}

void Normalizer2Impl::composeAndAppend(const UChar *src, const UChar *limit,
                                       UBool doCompose,
                                       UBool onlyContiguous,
                                       UnicodeString &safeMiddle,
                                       ReorderingBuffer &buffer,
                                       UErrorCode &errorCode) const {
    if(!buffer.isEmpty()) {
        const UChar *firstStarterInSrc=findNextCompBoundary(src, limit);
        if(src!=firstStarterInSrc) {
            const UChar *lastStarterInDest=findPreviousCompBoundary(buffer.getStart(),
                                                                    buffer.getLimit());
            int32_t destSuffixLength=(int32_t)(buffer.getLimit()-lastStarterInDest);
            UnicodeString middle(lastStarterInDest, destSuffixLength);
            buffer.removeSuffix(destSuffixLength);
            safeMiddle=middle;
            middle.append(src, (int32_t)(firstStarterInSrc-src));
            const UChar *middleStart=middle.getBuffer();
            compose(middleStart, middleStart+middle.length(), onlyContiguous,
                    TRUE, buffer, errorCode);
            if(U_FAILURE(errorCode)) {
                return;
            }
            src=firstStarterInSrc;
        }
    }
    if(doCompose) {
        compose(src, limit, onlyContiguous, TRUE, buffer, errorCode);
    } else {
        if(limit==NULL) {  // appendZeroCC() needs limit!=NULL
            limit=u_strchr(src, 0);
        }
        buffer.appendZeroCC(src, limit, errorCode);
    }
}

/**
 * Does c have a composition boundary before it?
 * True if its decomposition begins with a character that has
 * ccc=0 && NFC_QC=Yes (isCompYesAndZeroCC()).
 * As a shortcut, this is true if c itself has ccc=0 && NFC_QC=Yes
 * (isCompYesAndZeroCC()) so we need not decompose.
 */
UBool Normalizer2Impl::hasCompBoundaryBefore(UChar32 c, uint16_t norm16) const {
    for(;;) {
        if(isCompYesAndZeroCC(norm16)) {
            return TRUE;
        } else if(isMaybeOrNonZeroCC(norm16)) {
            return FALSE;
        } else if(isDecompNoAlgorithmic(norm16)) {
            c=mapAlgorithmic(c, norm16);
            norm16=getNorm16(c);
        } else {
            // c decomposes, get everything from the variable-length extra data
            const uint16_t *mapping=getMapping(norm16);
            uint16_t firstUnit=*mapping;
            if((firstUnit&MAPPING_LENGTH_MASK)==0) {
                return FALSE;
            }
            if((firstUnit&MAPPING_HAS_CCC_LCCC_WORD) && (*(mapping-1)&0xff00)) {
                return FALSE;  // non-zero leadCC
            }
            int32_t i=1;  // skip over the firstUnit
            UChar32 c;
            U16_NEXT_UNSAFE(mapping, i, c);
            return isCompYesAndZeroCC(getNorm16(c));
        }
    }
}

UBool Normalizer2Impl::hasCompBoundaryAfter(UChar32 c, UBool onlyContiguous, UBool testInert) const {
    for(;;) {
        uint16_t norm16=getNorm16(c);
        if(isInert(norm16)) {
            return TRUE;
        } else if(norm16<=minYesNo) {
            // Hangul: norm16==minYesNo
            // Hangul LVT has a boundary after it.
            // Hangul LV and non-inert yesYes characters combine forward.
            return isHangul(norm16) && !Hangul::isHangulWithoutJamoT((UChar)c);
        } else if(norm16>= (testInert ? minNoNo : minMaybeYes)) {
            return FALSE;
        } else if(isDecompNoAlgorithmic(norm16)) {
            c=mapAlgorithmic(c, norm16);
        } else {
            // c decomposes, get everything from the variable-length extra data.
            // If testInert, then c must be a yesNo character which has lccc=0,
            // otherwise it could be a noNo.
            const uint16_t *mapping=getMapping(norm16);
            uint16_t firstUnit=*mapping;
            // TRUE if
            //   not MAPPING_NO_COMP_BOUNDARY_AFTER
            //     (which is set if
            //       c is not deleted, and
            //       it and its decomposition do not combine forward, and it has a starter)
            //   and if FCC then trailCC<=1
            return
                (firstUnit&MAPPING_NO_COMP_BOUNDARY_AFTER)==0 &&
                (!onlyContiguous || firstUnit<=0x1ff);
        }
    }
}

const UChar *Normalizer2Impl::findPreviousCompBoundary(const UChar *start, const UChar *p) const {
    BackwardUTrie2StringIterator iter(normTrie, start, p);
    uint16_t norm16;
    do {
        norm16=iter.previous16();
    } while(!hasCompBoundaryBefore(iter.codePoint, norm16));
    // We could also test hasCompBoundaryAfter() and return iter.codePointLimit,
    // but that's probably not worth the extra cost.
    return iter.codePointStart;
}

const UChar *Normalizer2Impl::findNextCompBoundary(const UChar *p, const UChar *limit) const {
    ForwardUTrie2StringIterator iter(normTrie, p, limit);
    uint16_t norm16;
    do {
        norm16=iter.next16();
    } while(!hasCompBoundaryBefore(iter.codePoint, norm16));
    return iter.codePointStart;
}

// Note: normalizer2impl.cpp r30982 (2011-nov-27)
// still had getFCDTrie() which built and cached an FCD trie.
// That provided faster access to FCD data than getFCD16FromNormData()
// but required synchronization and consumed some 10kB of heap memory
// in any process that uses FCD (e.g., via collation).
// tccc180[] and smallFCD[] are intended to help with any loss of performance,
// at least for Latin & CJK.

// Gets the FCD value from the regular normalization data.
uint16_t Normalizer2Impl::getFCD16FromNormData(UChar32 c) const {
    // Only loops for 1:1 algorithmic mappings.
    for(;;) {
        uint16_t norm16=getNorm16(c);
        if(norm16<=minYesNo) {
            // no decomposition or Hangul syllable, all zeros
            return 0;
        } else if(norm16>=MIN_NORMAL_MAYBE_YES) {
            // combining mark
            norm16&=0xff;
            return norm16|(norm16<<8);
        } else if(norm16>=minMaybeYes) {
            return 0;
        } else if(isDecompNoAlgorithmic(norm16)) {
            c=mapAlgorithmic(c, norm16);
        } else {
            // c decomposes, get everything from the variable-length extra data
            const uint16_t *mapping=getMapping(norm16);
            uint16_t firstUnit=*mapping;
            if((firstUnit&MAPPING_LENGTH_MASK)==0) {
                // A character that is deleted (maps to an empty string) must
                // get the worst-case lccc and tccc values because arbitrary
                // characters on both sides will become adjacent.
                return 0x1ff;
            } else {
                norm16=firstUnit>>8;  // tccc
                if(firstUnit&MAPPING_HAS_CCC_LCCC_WORD) {
                    norm16|=*(mapping-1)&0xff00;  // lccc
                }
                return norm16;
            }
        }
    }
}

// Dual functionality:
// buffer!=NULL: normalize
// buffer==NULL: isNormalized/quickCheck/spanQuickCheckYes
const UChar *
Normalizer2Impl::makeFCD(const UChar *src, const UChar *limit,
                         ReorderingBuffer *buffer,
                         UErrorCode &errorCode) const {
    // Tracks the last FCD-safe boundary, before lccc=0 or after properly-ordered tccc<=1.
    // Similar to the prevBoundary in the compose() implementation.
    const UChar *prevBoundary=src;
    int32_t prevFCD16=0;
    if(limit==NULL) {
        src=copyLowPrefixFromNulTerminated(src, MIN_CCC_LCCC_CP, buffer, errorCode);
        if(U_FAILURE(errorCode)) {
            return src;
        }
        if(prevBoundary<src) {
            prevBoundary=src;
            // We know that the previous character's lccc==0.
            // Fetching the fcd16 value was deferred for this below-U+0300 code point.
            prevFCD16=getFCD16(*(src-1));
            if(prevFCD16>1) {
                --prevBoundary;
            }
        }
        limit=u_strchr(src, 0);
    }

    // Note: In this function we use buffer->appendZeroCC() because we track
    // the lead and trail combining classes here, rather than leaving it to
    // the ReorderingBuffer.
    // The exception is the call to decomposeShort() which uses the buffer
    // in the normal way.

    const UChar *prevSrc;
    UChar32 c=0;
    uint16_t fcd16=0;

    for(;;) {
        // count code units with lccc==0
        for(prevSrc=src; src!=limit;) {
            if((c=*src)<MIN_CCC_LCCC_CP) {
                prevFCD16=~c;
                ++src;
            } else if(!singleLeadMightHaveNonZeroFCD16(c)) {
                prevFCD16=0;
                ++src;
            } else {
                if(U16_IS_SURROGATE(c)) {
                    UChar c2;
                    if(U16_IS_SURROGATE_LEAD(c)) {
                        if((src+1)!=limit && U16_IS_TRAIL(c2=src[1])) {
                            c=U16_GET_SUPPLEMENTARY(c, c2);
                        }
                    } else /* trail surrogate */ {
                        if(prevSrc<src && U16_IS_LEAD(c2=*(src-1))) {
                            --src;
                            c=U16_GET_SUPPLEMENTARY(c2, c);
                        }
                    }
                }
                if((fcd16=getFCD16FromNormData(c))<=0xff) {
                    prevFCD16=fcd16;
                    src+=U16_LENGTH(c);
                } else {
                    break;
                }
            }
        }
        // copy these code units all at once
        if(src!=prevSrc) {
            if(buffer!=NULL && !buffer->appendZeroCC(prevSrc, src, errorCode)) {
                break;
            }
            if(src==limit) {
                break;
            }
            prevBoundary=src;
            // We know that the previous character's lccc==0.
            if(prevFCD16<0) {
                // Fetching the fcd16 value was deferred for this below-U+0300 code point.
                UChar32 prev=~prevFCD16;
                prevFCD16= prev<0x180 ? tccc180[prev] : getFCD16FromNormData(prev);
                if(prevFCD16>1) {
                    --prevBoundary;
                }
            } else {
                const UChar *p=src-1;
                if(U16_IS_TRAIL(*p) && prevSrc<p && U16_IS_LEAD(*(p-1))) {
                    --p;
                    // Need to fetch the previous character's FCD value because
                    // prevFCD16 was just for the trail surrogate code point.
                    prevFCD16=getFCD16FromNormData(U16_GET_SUPPLEMENTARY(p[0], p[1]));
                    // Still known to have lccc==0 because its lead surrogate unit had lccc==0.
                }
                if(prevFCD16>1) {
                    prevBoundary=p;
                }
            }
            // The start of the current character (c).
            prevSrc=src;
        } else if(src==limit) {
            break;
        }

        src+=U16_LENGTH(c);
        // The current character (c) at [prevSrc..src[ has a non-zero lead combining class.
        // Check for proper order, and decompose locally if necessary.
        if((prevFCD16&0xff)<=(fcd16>>8)) {
            // proper order: prev tccc <= current lccc
            if((fcd16&0xff)<=1) {
                prevBoundary=src;
            }
            if(buffer!=NULL && !buffer->appendZeroCC(c, errorCode)) {
                break;
            }
            prevFCD16=fcd16;
            continue;
        } else if(buffer==NULL) {
            return prevBoundary;  // quick check "no"
        } else {
            /*
             * Back out the part of the source that we copied or appended
             * already but is now going to be decomposed.
             * prevSrc is set to after what was copied/appended.
             */
            buffer->removeSuffix((int32_t)(prevSrc-prevBoundary));
            /*
             * Find the part of the source that needs to be decomposed,
             * up to the next safe boundary.
             */
            src=findNextFCDBoundary(src, limit);
            /*
             * The source text does not fulfill the conditions for FCD.
             * Decompose and reorder a limited piece of the text.
             */
            if(!decomposeShort(prevBoundary, src, *buffer, errorCode)) {
                break;
            }
            prevBoundary=src;
            prevFCD16=0;
        }
    }
    return src;
}

void Normalizer2Impl::makeFCDAndAppend(const UChar *src, const UChar *limit,
                                       UBool doMakeFCD,
                                       UnicodeString &safeMiddle,
                                       ReorderingBuffer &buffer,
                                       UErrorCode &errorCode) const {
    if(!buffer.isEmpty()) {
        const UChar *firstBoundaryInSrc=findNextFCDBoundary(src, limit);
        if(src!=firstBoundaryInSrc) {
            const UChar *lastBoundaryInDest=findPreviousFCDBoundary(buffer.getStart(),
                                                                    buffer.getLimit());
            int32_t destSuffixLength=(int32_t)(buffer.getLimit()-lastBoundaryInDest);
            UnicodeString middle(lastBoundaryInDest, destSuffixLength);
            buffer.removeSuffix(destSuffixLength);
            safeMiddle=middle;
            middle.append(src, (int32_t)(firstBoundaryInSrc-src));
            const UChar *middleStart=middle.getBuffer();
            makeFCD(middleStart, middleStart+middle.length(), &buffer, errorCode);
            if(U_FAILURE(errorCode)) {
                return;
            }
            src=firstBoundaryInSrc;
        }
    }
    if(doMakeFCD) {
        makeFCD(src, limit, &buffer, errorCode);
    } else {
        if(limit==NULL) {  // appendZeroCC() needs limit!=NULL
            limit=u_strchr(src, 0);
        }
        buffer.appendZeroCC(src, limit, errorCode);
    }
}

const UChar *Normalizer2Impl::findPreviousFCDBoundary(const UChar *start, const UChar *p) const {
    while(start<p && previousFCD16(start, p)>0xff) {}
    return p;
}

const UChar *Normalizer2Impl::findNextFCDBoundary(const UChar *p, const UChar *limit) const {
    while(p<limit) {
        const UChar *codePointStart=p;
        if(nextFCD16(p, limit)<=0xff) {
            return codePointStart;
        }
    }
    return p;
}

// CanonicalIterator data -------------------------------------------------- ***

CanonIterData::CanonIterData(UErrorCode &errorCode) :
        trie(utrie2_open(0, 0, &errorCode)),
        canonStartSets(uprv_deleteUObject, NULL, errorCode) {}

CanonIterData::~CanonIterData() {
    utrie2_close(trie);
}

void CanonIterData::addToStartSet(UChar32 origin, UChar32 decompLead, UErrorCode &errorCode) {
    uint32_t canonValue=utrie2_get32(trie, decompLead);
    if((canonValue&(CANON_HAS_SET|CANON_VALUE_MASK))==0 && origin!=0) {
        // origin is the first character whose decomposition starts with
        // the character for which we are setting the value.
        utrie2_set32(trie, decompLead, canonValue|origin, &errorCode);
    } else {
        // origin is not the first character, or it is U+0000.
        UnicodeSet *set;
        if((canonValue&CANON_HAS_SET)==0) {
            set=new UnicodeSet;
            if(set==NULL) {
                errorCode=U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            UChar32 firstOrigin=(UChar32)(canonValue&CANON_VALUE_MASK);
            canonValue=(canonValue&~CANON_VALUE_MASK)|CANON_HAS_SET|(uint32_t)canonStartSets.size();
            utrie2_set32(trie, decompLead, canonValue, &errorCode);
            canonStartSets.addElement(set, errorCode);
            if(firstOrigin!=0) {
                set->add(firstOrigin);
            }
        } else {
            set=(UnicodeSet *)canonStartSets[(int32_t)(canonValue&CANON_VALUE_MASK)];
        }
        set->add(origin);
    }
}

U_CDECL_BEGIN

// Call Normalizer2Impl::makeCanonIterDataFromNorm16() for a range of same-norm16 characters.
//     context: the Normalizer2Impl
static UBool U_CALLCONV
enumCIDRangeHandler(const void *context, UChar32 start, UChar32 end, uint32_t value) {
    UErrorCode errorCode = U_ZERO_ERROR;
    if (value != 0) {
        Normalizer2Impl *impl = (Normalizer2Impl *)context;
        impl->makeCanonIterDataFromNorm16(
            start, end, (uint16_t)value, *impl->fCanonIterData, errorCode);
    }
    return U_SUCCESS(errorCode);
}



// UInitOnce instantiation function for CanonIterData

static void U_CALLCONV
initCanonIterData(Normalizer2Impl *impl, UErrorCode &errorCode) {
    U_ASSERT(impl->fCanonIterData == NULL);
    impl->fCanonIterData = new CanonIterData(errorCode);
    if (impl->fCanonIterData == NULL) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_SUCCESS(errorCode)) {
        utrie2_enum(impl->getNormTrie(), NULL, enumCIDRangeHandler, impl);
        utrie2_freeze(impl->fCanonIterData->trie, UTRIE2_32_VALUE_BITS, &errorCode);
    }
    if (U_FAILURE(errorCode)) {
        delete impl->fCanonIterData;
        impl->fCanonIterData = NULL;
    }
}

U_CDECL_END

void Normalizer2Impl::makeCanonIterDataFromNorm16(UChar32 start, UChar32 end, uint16_t norm16,
                                                  CanonIterData &newData,
                                                  UErrorCode &errorCode) const {
    if(norm16==0 || (minYesNo<=norm16 && norm16<minNoNo)) {
        // Inert, or 2-way mapping (including Hangul syllable).
        // We do not write a canonStartSet for any yesNo character.
        // Composites from 2-way mappings are added at runtime from the
        // starter's compositions list, and the other characters in
        // 2-way mappings get CANON_NOT_SEGMENT_STARTER set because they are
        // "maybe" characters.
        return;
    }
    for(UChar32 c=start; c<=end; ++c) {
        uint32_t oldValue=utrie2_get32(newData.trie, c);
        uint32_t newValue=oldValue;
        if(norm16>=minMaybeYes) {
            // not a segment starter if it occurs in a decomposition or has cc!=0
            newValue|=CANON_NOT_SEGMENT_STARTER;
            if(norm16<MIN_NORMAL_MAYBE_YES) {
                newValue|=CANON_HAS_COMPOSITIONS;
            }
        } else if(norm16<minYesNo) {
            newValue|=CANON_HAS_COMPOSITIONS;
        } else {
            // c has a one-way decomposition
            UChar32 c2=c;
            uint16_t norm16_2=norm16;
            while(limitNoNo<=norm16_2 && norm16_2<minMaybeYes) {
                c2=mapAlgorithmic(c2, norm16_2);
                norm16_2=getNorm16(c2);
            }
            if(minYesNo<=norm16_2 && norm16_2<limitNoNo) {
                // c decomposes, get everything from the variable-length extra data
                const uint16_t *mapping=getMapping(norm16_2);
                uint16_t firstUnit=*mapping;
                int32_t length=firstUnit&MAPPING_LENGTH_MASK;
                if((firstUnit&MAPPING_HAS_CCC_LCCC_WORD)!=0) {
                    if(c==c2 && (*(mapping-1)&0xff)!=0) {
                        newValue|=CANON_NOT_SEGMENT_STARTER;  // original c has cc!=0
                    }
                }
                // Skip empty mappings (no characters in the decomposition).
                if(length!=0) {
                    ++mapping;  // skip over the firstUnit
                    // add c to first code point's start set
                    int32_t i=0;
                    U16_NEXT_UNSAFE(mapping, i, c2);
                    newData.addToStartSet(c, c2, errorCode);
                    // Set CANON_NOT_SEGMENT_STARTER for each remaining code point of a
                    // one-way mapping. A 2-way mapping is possible here after
                    // intermediate algorithmic mapping.
                    if(norm16_2>=minNoNo) {
                        while(i<length) {
                            U16_NEXT_UNSAFE(mapping, i, c2);
                            uint32_t c2Value=utrie2_get32(newData.trie, c2);
                            if((c2Value&CANON_NOT_SEGMENT_STARTER)==0) {
                                utrie2_set32(newData.trie, c2, c2Value|CANON_NOT_SEGMENT_STARTER,
                                             &errorCode);
                            }
                        }
                    }
                }
            } else {
                // c decomposed to c2 algorithmically; c has cc==0
                newData.addToStartSet(c, c2, errorCode);
            }
        }
        if(newValue!=oldValue) {
            utrie2_set32(newData.trie, c, newValue, &errorCode);
        }
    }
}

UBool Normalizer2Impl::ensureCanonIterData(UErrorCode &errorCode) const {
    // Logically const: Synchronized instantiation.
    Normalizer2Impl *me=const_cast<Normalizer2Impl *>(this);
    umtx_initOnce(me->fCanonIterDataInitOnce, &initCanonIterData, me, errorCode);
    return U_SUCCESS(errorCode);
}

int32_t Normalizer2Impl::getCanonValue(UChar32 c) const {
    return (int32_t)utrie2_get32(fCanonIterData->trie, c);
}

const UnicodeSet &Normalizer2Impl::getCanonStartSet(int32_t n) const {
    return *(const UnicodeSet *)fCanonIterData->canonStartSets[n];
}

UBool Normalizer2Impl::isCanonSegmentStarter(UChar32 c) const {
    return getCanonValue(c)>=0;
}

UBool Normalizer2Impl::getCanonStartSet(UChar32 c, UnicodeSet &set) const {
    int32_t canonValue=getCanonValue(c)&~CANON_NOT_SEGMENT_STARTER;
    if(canonValue==0) {
        return FALSE;
    }
    set.clear();
    int32_t value=canonValue&CANON_VALUE_MASK;
    if((canonValue&CANON_HAS_SET)!=0) {
        set.addAll(getCanonStartSet(value));
    } else if(value!=0) {
        set.add(value);
    }
    if((canonValue&CANON_HAS_COMPOSITIONS)!=0) {
        uint16_t norm16=getNorm16(c);
        if(norm16==JAMO_L) {
            UChar32 syllable=
                (UChar32)(Hangul::HANGUL_BASE+(c-Hangul::JAMO_L_BASE)*Hangul::JAMO_VT_COUNT);
            set.add(syllable, syllable+Hangul::JAMO_VT_COUNT-1);
        } else {
            addComposites(getCompositionsList(norm16), set);
        }
    }
    return TRUE;
}

U_NAMESPACE_END

// Normalizer2 data swapping ----------------------------------------------- ***

U_NAMESPACE_USE

U_CAPI int32_t U_EXPORT2
unorm2_swap(const UDataSwapper *ds,
            const void *inData, int32_t length, void *outData,
            UErrorCode *pErrorCode) {
    const UDataInfo *pInfo;
    int32_t headerSize;

    const uint8_t *inBytes;
    uint8_t *outBytes;

    const int32_t *inIndexes;
    int32_t indexes[Normalizer2Impl::IX_MIN_MAYBE_YES+1];

    int32_t i, offset, nextOffset, size;

    /* udata_swapDataHeader checks the arguments */
    headerSize=udata_swapDataHeader(ds, inData, length, outData, pErrorCode);
    if(pErrorCode==NULL || U_FAILURE(*pErrorCode)) {
        return 0;
    }

    /* check data format and format version */
    pInfo=(const UDataInfo *)((const char *)inData+4);
    if(!(
        pInfo->dataFormat[0]==0x4e &&   /* dataFormat="Nrm2" */
        pInfo->dataFormat[1]==0x72 &&
        pInfo->dataFormat[2]==0x6d &&
        pInfo->dataFormat[3]==0x32 &&
        (pInfo->formatVersion[0]==1 || pInfo->formatVersion[0]==2)
    )) {
        udata_printError(ds, "unorm2_swap(): data format %02x.%02x.%02x.%02x (format version %02x) is not recognized as Normalizer2 data\n",
                         pInfo->dataFormat[0], pInfo->dataFormat[1],
                         pInfo->dataFormat[2], pInfo->dataFormat[3],
                         pInfo->formatVersion[0]);
        *pErrorCode=U_UNSUPPORTED_ERROR;
        return 0;
    }

    inBytes=(const uint8_t *)inData+headerSize;
    outBytes=(uint8_t *)outData+headerSize;

    inIndexes=(const int32_t *)inBytes;

    if(length>=0) {
        length-=headerSize;
        if(length<(int32_t)sizeof(indexes)) {
            udata_printError(ds, "unorm2_swap(): too few bytes (%d after header) for Normalizer2 data\n",
                             length);
            *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
            return 0;
        }
    }

    /* read the first few indexes */
    for(i=0; i<=Normalizer2Impl::IX_MIN_MAYBE_YES; ++i) {
        indexes[i]=udata_readInt32(ds, inIndexes[i]);
    }

    /* get the total length of the data */
    size=indexes[Normalizer2Impl::IX_TOTAL_SIZE];

    if(length>=0) {
        if(length<size) {
            udata_printError(ds, "unorm2_swap(): too few bytes (%d after header) for all of Normalizer2 data\n",
                             length);
            *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
            return 0;
        }

        /* copy the data for inaccessible bytes */
        if(inBytes!=outBytes) {
            uprv_memcpy(outBytes, inBytes, size);
        }

        offset=0;

        /* swap the int32_t indexes[] */
        nextOffset=indexes[Normalizer2Impl::IX_NORM_TRIE_OFFSET];
        ds->swapArray32(ds, inBytes, nextOffset-offset, outBytes, pErrorCode);
        offset=nextOffset;

        /* swap the UTrie2 */
        nextOffset=indexes[Normalizer2Impl::IX_EXTRA_DATA_OFFSET];
        utrie2_swap(ds, inBytes+offset, nextOffset-offset, outBytes+offset, pErrorCode);
        offset=nextOffset;

        /* swap the uint16_t extraData[] */
        nextOffset=indexes[Normalizer2Impl::IX_SMALL_FCD_OFFSET];
        ds->swapArray16(ds, inBytes+offset, nextOffset-offset, outBytes+offset, pErrorCode);
        offset=nextOffset;

        /* no need to swap the uint8_t smallFCD[] (new in formatVersion 2) */
        nextOffset=indexes[Normalizer2Impl::IX_SMALL_FCD_OFFSET+1];
        offset=nextOffset;

        U_ASSERT(offset==size);
    }

    return headerSize+size;
}

#endif  // !UCONFIG_NO_NORMALIZATION

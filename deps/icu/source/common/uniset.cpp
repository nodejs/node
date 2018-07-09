// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 1999-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   10/20/99    alan        Creation.
**********************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/parsepos.h"
#include "unicode/symtable.h"
#include "unicode/uniset.h"
#include "unicode/utf8.h"
#include "unicode/utf16.h"
#include "ruleiter.h"
#include "cmemory.h"
#include "cstring.h"
#include "patternprops.h"
#include "uelement.h"
#include "util.h"
#include "uvector.h"
#include "charstr.h"
#include "ustrfmt.h"
#include "uassert.h"
#include "bmpset.h"
#include "unisetspan.h"

// Define UChar constants using hex for EBCDIC compatibility
// Used #define to reduce private static exports and memory access time.
#define SET_OPEN        ((UChar)0x005B) /*[*/
#define SET_CLOSE       ((UChar)0x005D) /*]*/
#define HYPHEN          ((UChar)0x002D) /*-*/
#define COMPLEMENT      ((UChar)0x005E) /*^*/
#define COLON           ((UChar)0x003A) /*:*/
#define BACKSLASH       ((UChar)0x005C) /*\*/
#define INTERSECTION    ((UChar)0x0026) /*&*/
#define UPPER_U         ((UChar)0x0055) /*U*/
#define LOWER_U         ((UChar)0x0075) /*u*/
#define OPEN_BRACE      ((UChar)123)    /*{*/
#define CLOSE_BRACE     ((UChar)125)    /*}*/
#define UPPER_P         ((UChar)0x0050) /*P*/
#define LOWER_P         ((UChar)0x0070) /*p*/
#define UPPER_N         ((UChar)78)     /*N*/
#define EQUALS          ((UChar)0x003D) /*=*/

// HIGH_VALUE > all valid values. 110000 for codepoints
#define UNICODESET_HIGH 0x0110000

// LOW <= all valid values. ZERO for codepoints
#define UNICODESET_LOW 0x000000

// initial storage. Must be >= 0
#define START_EXTRA 16

// extra amount for growth. Must be >= 0
#define GROW_EXTRA START_EXTRA

U_NAMESPACE_BEGIN

SymbolTable::~SymbolTable() {}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(UnicodeSet)

/**
 * Modify the given UChar32 variable so that it is in range, by
 * pinning values < UNICODESET_LOW to UNICODESET_LOW, and
 * pinning values > UNICODESET_HIGH-1 to UNICODESET_HIGH-1.
 * It modifies its argument in-place and also returns it.
 */
static inline UChar32 pinCodePoint(UChar32& c) {
    if (c < UNICODESET_LOW) {
        c = UNICODESET_LOW;
    } else if (c > (UNICODESET_HIGH-1)) {
        c = (UNICODESET_HIGH-1);
    }
    return c;
}

//----------------------------------------------------------------
// Debugging
//----------------------------------------------------------------

// DO NOT DELETE THIS CODE.  This code is used to debug memory leaks.
// To enable the debugging, define the symbol DEBUG_MEM in the line
// below.  This will result in text being sent to stdout that looks
// like this:
//   DEBUG UnicodeSet: ct 0x00A39B20; 397 [\u0A81-\u0A83\u0A85-
//   DEBUG UnicodeSet: dt 0x00A39B20; 396 [\u0A81-\u0A83\u0A85-
// Each line lists a construction (ct) or destruction (dt) event, the
// object address, the number of outstanding objects after the event,
// and the pattern of the object in question.

// #define DEBUG_MEM

#ifdef DEBUG_MEM
#include <stdio.h>
static int32_t _dbgCount = 0;

static inline void _dbgct(UnicodeSet* set) {
    UnicodeString str;
    set->toPattern(str, TRUE);
    char buf[40];
    str.extract(0, 39, buf, "");
    printf("DEBUG UnicodeSet: ct 0x%08X; %d %s\n", set, ++_dbgCount, buf);
}

static inline void _dbgdt(UnicodeSet* set) {
    UnicodeString str;
    set->toPattern(str, TRUE);
    char buf[40];
    str.extract(0, 39, buf, "");
    printf("DEBUG UnicodeSet: dt 0x%08X; %d %s\n", set, --_dbgCount, buf);
}

#else

#define _dbgct(set)
#define _dbgdt(set)

#endif

//----------------------------------------------------------------
// UnicodeString in UVector support
//----------------------------------------------------------------

static void U_CALLCONV cloneUnicodeString(UElement *dst, UElement *src) {
    dst->pointer = new UnicodeString(*(UnicodeString*)src->pointer);
}

static int8_t U_CALLCONV compareUnicodeString(UElement t1, UElement t2) {
    const UnicodeString &a = *(const UnicodeString*)t1.pointer;
    const UnicodeString &b = *(const UnicodeString*)t2.pointer;
    return a.compare(b);
}

//----------------------------------------------------------------
// Constructors &c
//----------------------------------------------------------------

/**
 * Constructs an empty set.
 */
UnicodeSet::UnicodeSet() :
    len(1), capacity(1 + START_EXTRA), list(0), bmpSet(0), buffer(0),
    bufferCapacity(0), patLen(0), pat(NULL), strings(NULL), stringSpan(NULL),
    fFlags(0)
{
    UErrorCode status = U_ZERO_ERROR;
    allocateStrings(status);
    if (U_FAILURE(status)) {
        return;
    }
    list = (UChar32*) uprv_malloc(sizeof(UChar32) * capacity);
    if(list!=NULL){
        list[0] = UNICODESET_HIGH;
    } else { // If memory allocation failed, set to bogus state.
        setToBogus();
        return;
    }
    _dbgct(this);
}

/**
 * Constructs a set containing the given range. If <code>end >
 * start</code> then an empty set is created.
 *
 * @param start first character, inclusive, of range
 * @param end last character, inclusive, of range
 */
UnicodeSet::UnicodeSet(UChar32 start, UChar32 end) :
    len(1), capacity(1 + START_EXTRA), list(0), bmpSet(0), buffer(0),
    bufferCapacity(0), patLen(0), pat(NULL), strings(NULL), stringSpan(NULL),
    fFlags(0)
{
    UErrorCode status = U_ZERO_ERROR;
    allocateStrings(status);
    if (U_FAILURE(status)) {
        return;
    }
    list = (UChar32*) uprv_malloc(sizeof(UChar32) * capacity);
    if(list!=NULL){
        list[0] = UNICODESET_HIGH;
        complement(start, end);
    } else { // If memory allocation failed, set to bogus state.
        setToBogus();
        return;
    }
    _dbgct(this);
}

/**
 * Constructs a set that is identical to the given UnicodeSet.
 */
UnicodeSet::UnicodeSet(const UnicodeSet& o) :
    UnicodeFilter(o),
    len(0), capacity(o.isFrozen() ? o.len : o.len + GROW_EXTRA), list(0),
    bmpSet(0),
    buffer(0), bufferCapacity(0),
    patLen(0), pat(NULL), strings(NULL), stringSpan(NULL),
    fFlags(0)
{
    UErrorCode status = U_ZERO_ERROR;
    allocateStrings(status);
    if (U_FAILURE(status)) {
        return;
    }
    list = (UChar32*) uprv_malloc(sizeof(UChar32) * capacity);
    if(list!=NULL){
        *this = o;
    } else { // If memory allocation failed, set to bogus state.
        setToBogus();
        return;
    }
    _dbgct(this);
}

// Copy-construct as thawed.
UnicodeSet::UnicodeSet(const UnicodeSet& o, UBool /* asThawed */) :
    UnicodeFilter(o),
    len(0), capacity(o.len + GROW_EXTRA), list(0),
    bmpSet(0),
    buffer(0), bufferCapacity(0),
    patLen(0), pat(NULL), strings(NULL), stringSpan(NULL),
    fFlags(0)
{
    UErrorCode status = U_ZERO_ERROR;
    allocateStrings(status);
    if (U_FAILURE(status)) {
        return;
    }
    list = (UChar32*) uprv_malloc(sizeof(UChar32) * capacity);
    if(list!=NULL){
        // *this = o except for bmpSet and stringSpan
        len = o.len;
        uprv_memcpy(list, o.list, (size_t)len*sizeof(UChar32));
        if (strings != NULL && o.strings != NULL) {
            strings->assign(*o.strings, cloneUnicodeString, status);
        } else { // Invalid strings.
            setToBogus();
            return;
        }
        if (o.pat) {
            setPattern(UnicodeString(o.pat, o.patLen));
        }
    } else { // If memory allocation failed, set to bogus state.
        setToBogus();
        return;
    }
    _dbgct(this);
}

/**
 * Destructs the set.
 */
UnicodeSet::~UnicodeSet() {
    _dbgdt(this); // first!
    uprv_free(list);
    delete bmpSet;
    if (buffer) {
        uprv_free(buffer);
    }
    delete strings;
    delete stringSpan;
    releasePattern();
}

/**
 * Assigns this object to be a copy of another.
 */
UnicodeSet& UnicodeSet::operator=(const UnicodeSet& o) {
    if (this == &o) {
        return *this;
    }
    if (isFrozen()) {
        return *this;
    }
    if (o.isBogus()) {
        setToBogus();
        return *this;
    }
    UErrorCode ec = U_ZERO_ERROR;
    ensureCapacity(o.len, ec);
    if (U_FAILURE(ec)) {
        return *this; // There is no way to report this error :-(
    }
    len = o.len;
    uprv_memcpy(list, o.list, (size_t)len*sizeof(UChar32));
    if (o.bmpSet == NULL) {
        bmpSet = NULL;
    } else {
        bmpSet = new BMPSet(*o.bmpSet, list, len);
        if (bmpSet == NULL) { // Check for memory allocation error.
            setToBogus();
            return *this;
        }
    }
    if (strings != NULL && o.strings != NULL) {
        strings->assign(*o.strings, cloneUnicodeString, ec);
    } else { // Invalid strings.
        setToBogus();
        return *this;
    }
    if (o.stringSpan == NULL) {
        stringSpan = NULL;
    } else {
        stringSpan = new UnicodeSetStringSpan(*o.stringSpan, *strings);
        if (stringSpan == NULL) { // Check for memory allocation error.
            setToBogus();
            return *this;
        }
    }
    releasePattern();
    if (o.pat) {
        setPattern(UnicodeString(o.pat, o.patLen));
    }
    return *this;
}

/**
 * Returns a copy of this object.  All UnicodeMatcher objects have
 * to support cloning in order to allow classes using
 * UnicodeMatchers, such as Transliterator, to implement cloning.
 */
UnicodeFunctor* UnicodeSet::clone() const {
    return new UnicodeSet(*this);
}

UnicodeFunctor *UnicodeSet::cloneAsThawed() const {
    return new UnicodeSet(*this, TRUE);
}

/**
 * Compares the specified object with this set for equality.  Returns
 * <tt>true</tt> if the two sets
 * have the same size, and every member of the specified set is
 * contained in this set (or equivalently, every member of this set is
 * contained in the specified set).
 *
 * @param o set to be compared for equality with this set.
 * @return <tt>true</tt> if the specified set is equal to this set.
 */
UBool UnicodeSet::operator==(const UnicodeSet& o) const {
    if (len != o.len) return FALSE;
    for (int32_t i = 0; i < len; ++i) {
        if (list[i] != o.list[i]) return FALSE;
    }
    if (*strings != *o.strings) return FALSE;
    return TRUE;
}

/**
 * Returns the hash code value for this set.
 *
 * @return the hash code value for this set.
 * @see Object#hashCode()
 */
int32_t UnicodeSet::hashCode(void) const {
    int32_t result = len;
    for (int32_t i = 0; i < len; ++i) {
        result *= 1000003;
        result += list[i];
    }
    return result;
}

//----------------------------------------------------------------
// Public API
//----------------------------------------------------------------

/**
 * Returns the number of elements in this set (its cardinality),
 * Note than the elements of a set may include both individual
 * codepoints and strings.
 *
 * @return the number of elements in this set (its cardinality).
 */
int32_t UnicodeSet::size(void) const {
    int32_t n = 0;
    int32_t count = getRangeCount();
    for (int32_t i = 0; i < count; ++i) {
        n += getRangeEnd(i) - getRangeStart(i) + 1;
    }
    return n + strings->size();
}

/**
 * Returns <tt>true</tt> if this set contains no elements.
 *
 * @return <tt>true</tt> if this set contains no elements.
 */
UBool UnicodeSet::isEmpty(void) const {
    return len == 1 && strings->size() == 0;
}

/**
 * Returns true if this set contains the given character.
 * @param c character to be checked for containment
 * @return true if the test condition is met
 */
UBool UnicodeSet::contains(UChar32 c) const {
    // Set i to the index of the start item greater than ch
    // We know we will terminate without length test!
    // LATER: for large sets, add binary search
    //int32_t i = -1;
    //for (;;) {
    //    if (c < list[++i]) break;
    //}
    if (bmpSet != NULL) {
        return bmpSet->contains(c);
    }
    if (stringSpan != NULL) {
        return stringSpan->contains(c);
    }
    if (c >= UNICODESET_HIGH) { // Don't need to check LOW bound
        return FALSE;
    }
    int32_t i = findCodePoint(c);
    return (UBool)(i & 1); // return true if odd
}

/**
 * Returns the smallest value i such that c < list[i].  Caller
 * must ensure that c is a legal value or this method will enter
 * an infinite loop.  This method performs a binary search.
 * @param c a character in the range MIN_VALUE..MAX_VALUE
 * inclusive
 * @return the smallest integer i in the range 0..len-1,
 * inclusive, such that c < list[i]
 */
int32_t UnicodeSet::findCodePoint(UChar32 c) const {
    /* Examples:
                                       findCodePoint(c)
       set              list[]         c=0 1 3 4 7 8
       ===              ==============   ===========
       []               [110000]         0 0 0 0 0 0
       [\u0000-\u0003]  [0, 4, 110000]   1 1 1 2 2 2
       [\u0004-\u0007]  [4, 8, 110000]   0 0 0 1 1 2
       [:Any:]          [0, 110000]      1 1 1 1 1 1
     */

    // Return the smallest i such that c < list[i].  Assume
    // list[len - 1] == HIGH and that c is legal (0..HIGH-1).
    if (c < list[0])
        return 0;
    // High runner test.  c is often after the last range, so an
    // initial check for this condition pays off.
    int32_t lo = 0;
    int32_t hi = len - 1;
    if (lo >= hi || c >= list[hi-1])
        return hi;
    // invariant: c >= list[lo]
    // invariant: c < list[hi]
    for (;;) {
        int32_t i = (lo + hi) >> 1;
        if (i == lo) {
            break; // Found!
        } else if (c < list[i]) {
            hi = i;
        } else {
            lo = i;
        }
    }
    return hi;
}

/**
 * Returns true if this set contains every character
 * of the given range.
 * @param start first character, inclusive, of the range
 * @param end last character, inclusive, of the range
 * @return true if the test condition is met
 */
UBool UnicodeSet::contains(UChar32 start, UChar32 end) const {
    //int32_t i = -1;
    //for (;;) {
    //    if (start < list[++i]) break;
    //}
    int32_t i = findCodePoint(start);
    return ((i & 1) != 0 && end < list[i]);
}

/**
 * Returns <tt>true</tt> if this set contains the given
 * multicharacter string.
 * @param s string to be checked for containment
 * @return <tt>true</tt> if this set contains the specified string
 */
UBool UnicodeSet::contains(const UnicodeString& s) const {
    if (s.length() == 0) return FALSE;
    int32_t cp = getSingleCP(s);
    if (cp < 0) {
        return strings->contains((void*) &s);
    } else {
        return contains((UChar32) cp);
    }
}

/**
 * Returns true if this set contains all the characters and strings
 * of the given set.
 * @param c set to be checked for containment
 * @return true if the test condition is met
 */
UBool UnicodeSet::containsAll(const UnicodeSet& c) const {
    // The specified set is a subset if all of its pairs are contained in
    // this set.  It's possible to code this more efficiently in terms of
    // direct manipulation of the inversion lists if the need arises.
    int32_t n = c.getRangeCount();
    for (int i=0; i<n; ++i) {
        if (!contains(c.getRangeStart(i), c.getRangeEnd(i))) {
            return FALSE;
        }
    }
    if (!strings->containsAll(*c.strings)) return FALSE;
    return TRUE;
}

/**
 * Returns true if this set contains all the characters
 * of the given string.
 * @param s string containing characters to be checked for containment
 * @return true if the test condition is met
 */
UBool UnicodeSet::containsAll(const UnicodeString& s) const {
    return (UBool)(span(s.getBuffer(), s.length(), USET_SPAN_CONTAINED) ==
                   s.length());
}

/**
 * Returns true if this set contains none of the characters
 * of the given range.
 * @param start first character, inclusive, of the range
 * @param end last character, inclusive, of the range
 * @return true if the test condition is met
 */
UBool UnicodeSet::containsNone(UChar32 start, UChar32 end) const {
    //int32_t i = -1;
    //for (;;) {
    //    if (start < list[++i]) break;
    //}
    int32_t i = findCodePoint(start);
    return ((i & 1) == 0 && end < list[i]);
}

/**
 * Returns true if this set contains none of the characters and strings
 * of the given set.
 * @param c set to be checked for containment
 * @return true if the test condition is met
 */
UBool UnicodeSet::containsNone(const UnicodeSet& c) const {
    // The specified set is a subset if all of its pairs are contained in
    // this set.  It's possible to code this more efficiently in terms of
    // direct manipulation of the inversion lists if the need arises.
    int32_t n = c.getRangeCount();
    for (int32_t i=0; i<n; ++i) {
        if (!containsNone(c.getRangeStart(i), c.getRangeEnd(i))) {
            return FALSE;
        }
    }
    if (!strings->containsNone(*c.strings)) return FALSE;
    return TRUE;
}

/**
 * Returns true if this set contains none of the characters
 * of the given string.
 * @param s string containing characters to be checked for containment
 * @return true if the test condition is met
 */
UBool UnicodeSet::containsNone(const UnicodeString& s) const {
    return (UBool)(span(s.getBuffer(), s.length(), USET_SPAN_NOT_CONTAINED) ==
                   s.length());
}

/**
 * Returns <tt>true</tt> if this set contains any character whose low byte
 * is the given value.  This is used by <tt>RuleBasedTransliterator</tt> for
 * indexing.
 */
UBool UnicodeSet::matchesIndexValue(uint8_t v) const {
    /* The index value v, in the range [0,255], is contained in this set if
     * it is contained in any pair of this set.  Pairs either have the high
     * bytes equal, or unequal.  If the high bytes are equal, then we have
     * aaxx..aayy, where aa is the high byte.  Then v is contained if xx <=
     * v <= yy.  If the high bytes are unequal we have aaxx..bbyy, bb>aa.
     * Then v is contained if xx <= v || v <= yy.  (This is identical to the
     * time zone month containment logic.)
     */
    int32_t i;
    int32_t rangeCount=getRangeCount();
    for (i=0; i<rangeCount; ++i) {
        UChar32 low = getRangeStart(i);
        UChar32 high = getRangeEnd(i);
        if ((low & ~0xFF) == (high & ~0xFF)) {
            if ((low & 0xFF) <= v && v <= (high & 0xFF)) {
                return TRUE;
            }
        } else if ((low & 0xFF) <= v || v <= (high & 0xFF)) {
            return TRUE;
        }
    }
    if (strings->size() != 0) {
        for (i=0; i<strings->size(); ++i) {
            const UnicodeString& s = *(const UnicodeString*)strings->elementAt(i);
            //if (s.length() == 0) {
            //    // Empty strings match everything
            //    return TRUE;
            //}
            // assert(s.length() != 0); // We enforce this elsewhere
            UChar32 c = s.char32At(0);
            if ((c & 0xFF) == v) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/**
 * Implementation of UnicodeMatcher::matches().  Always matches the
 * longest possible multichar string.
 */
UMatchDegree UnicodeSet::matches(const Replaceable& text,
                                 int32_t& offset,
                                 int32_t limit,
                                 UBool incremental) {
    if (offset == limit) {
        // Strings, if any, have length != 0, so we don't worry
        // about them here.  If we ever allow zero-length strings
        // we much check for them here.
        if (contains(U_ETHER)) {
            return incremental ? U_PARTIAL_MATCH : U_MATCH;
        } else {
            return U_MISMATCH;
        }
    } else {
        if (strings->size() != 0) { // try strings first

            // might separate forward and backward loops later
            // for now they are combined

            // TODO Improve efficiency of this, at least in the forward
            // direction, if not in both.  In the forward direction we
            // can assume the strings are sorted.

            int32_t i;
            UBool forward = offset < limit;

            // firstChar is the leftmost char to match in the
            // forward direction or the rightmost char to match in
            // the reverse direction.
            UChar firstChar = text.charAt(offset);

            // If there are multiple strings that can match we
            // return the longest match.
            int32_t highWaterLength = 0;

            for (i=0; i<strings->size(); ++i) {
                const UnicodeString& trial = *(const UnicodeString*)strings->elementAt(i);

                //if (trial.length() == 0) {
                //    return U_MATCH; // null-string always matches
                //}
                // assert(trial.length() != 0); // We ensure this elsewhere

                UChar c = trial.charAt(forward ? 0 : trial.length() - 1);

                // Strings are sorted, so we can optimize in the
                // forward direction.
                if (forward && c > firstChar) break;
                if (c != firstChar) continue;

                int32_t matchLen = matchRest(text, offset, limit, trial);

                if (incremental) {
                    int32_t maxLen = forward ? limit-offset : offset-limit;
                    if (matchLen == maxLen) {
                        // We have successfully matched but only up to limit.
                        return U_PARTIAL_MATCH;
                    }
                }

                if (matchLen == trial.length()) {
                    // We have successfully matched the whole string.
                    if (matchLen > highWaterLength) {
                        highWaterLength = matchLen;
                    }
                    // In the forward direction we know strings
                    // are sorted so we can bail early.
                    if (forward && matchLen < highWaterLength) {
                        break;
                    }
                    continue;
                }
            }

            // We've checked all strings without a partial match.
            // If we have full matches, return the longest one.
            if (highWaterLength != 0) {
                offset += forward ? highWaterLength : -highWaterLength;
                return U_MATCH;
            }
        }
        return UnicodeFilter::matches(text, offset, limit, incremental);
    }
}

/**
 * Returns the longest match for s in text at the given position.
 * If limit > start then match forward from start+1 to limit
 * matching all characters except s.charAt(0).  If limit < start,
 * go backward starting from start-1 matching all characters
 * except s.charAt(s.length()-1).  This method assumes that the
 * first character, text.charAt(start), matches s, so it does not
 * check it.
 * @param text the text to match
 * @param start the first character to match.  In the forward
 * direction, text.charAt(start) is matched against s.charAt(0).
 * In the reverse direction, it is matched against
 * s.charAt(s.length()-1).
 * @param limit the limit offset for matching, either last+1 in
 * the forward direction, or last-1 in the reverse direction,
 * where last is the index of the last character to match.
 * @return If part of s matches up to the limit, return |limit -
 * start|.  If all of s matches before reaching the limit, return
 * s.length().  If there is a mismatch between s and text, return
 * 0
 */
int32_t UnicodeSet::matchRest(const Replaceable& text,
                              int32_t start, int32_t limit,
                              const UnicodeString& s) {
    int32_t i;
    int32_t maxLen;
    int32_t slen = s.length();
    if (start < limit) {
        maxLen = limit - start;
        if (maxLen > slen) maxLen = slen;
        for (i = 1; i < maxLen; ++i) {
            if (text.charAt(start + i) != s.charAt(i)) return 0;
        }
    } else {
        maxLen = start - limit;
        if (maxLen > slen) maxLen = slen;
        --slen; // <=> slen = s.length() - 1;
        for (i = 1; i < maxLen; ++i) {
            if (text.charAt(start - i) != s.charAt(slen - i)) return 0;
        }
    }
    return maxLen;
}

/**
 * Implement of UnicodeMatcher
 */
void UnicodeSet::addMatchSetTo(UnicodeSet& toUnionTo) const {
    toUnionTo.addAll(*this);
}

/**
 * Returns the index of the given character within this set, where
 * the set is ordered by ascending code point.  If the character
 * is not in this set, return -1.  The inverse of this method is
 * <code>charAt()</code>.
 * @return an index from 0..size()-1, or -1
 */
int32_t UnicodeSet::indexOf(UChar32 c) const {
    if (c < MIN_VALUE || c > MAX_VALUE) {
        return -1;
    }
    int32_t i = 0;
    int32_t n = 0;
    for (;;) {
        UChar32 start = list[i++];
        if (c < start) {
            return -1;
        }
        UChar32 limit = list[i++];
        if (c < limit) {
            return n + c - start;
        }
        n += limit - start;
    }
}

/**
 * Returns the character at the given index within this set, where
 * the set is ordered by ascending code point.  If the index is
 * out of range, return (UChar32)-1.  The inverse of this method is
 * <code>indexOf()</code>.
 * @param index an index from 0..size()-1
 * @return the character at the given index, or (UChar32)-1.
 */
UChar32 UnicodeSet::charAt(int32_t index) const {
    if (index >= 0) {
        // len2 is the largest even integer <= len, that is, it is len
        // for even values and len-1 for odd values.  With odd values
        // the last entry is UNICODESET_HIGH.
        int32_t len2 = len & ~1;
        for (int32_t i=0; i < len2;) {
            UChar32 start = list[i++];
            int32_t count = list[i++] - start;
            if (index < count) {
                return (UChar32)(start + index);
            }
            index -= count;
        }
    }
    return (UChar32)-1;
}

/**
 * Make this object represent the range <code>start - end</code>.
 * If <code>end > start</code> then this object is set to an
 * an empty range.
 *
 * @param start first character in the set, inclusive
 * @rparam end last character in the set, inclusive
 */
UnicodeSet& UnicodeSet::set(UChar32 start, UChar32 end) {
    clear();
    complement(start, end);
    return *this;
}

/**
 * Adds the specified range to this set if it is not already
 * present.  If this set already contains the specified range,
 * the call leaves this set unchanged.  If <code>end > start</code>
 * then an empty range is added, leaving the set unchanged.
 *
 * @param start first character, inclusive, of range to be added
 * to this set.
 * @param end last character, inclusive, of range to be added
 * to this set.
 */
UnicodeSet& UnicodeSet::add(UChar32 start, UChar32 end) {
    if (pinCodePoint(start) < pinCodePoint(end)) {
        UChar32 range[3] = { start, end+1, UNICODESET_HIGH };
        add(range, 2, 0);
    } else if (start == end) {
        add(start);
    }
    return *this;
}

// #define DEBUG_US_ADD

#ifdef DEBUG_US_ADD
#include <stdio.h>
void dump(UChar32 c) {
    if (c <= 0xFF) {
        printf("%c", (char)c);
    } else {
        printf("U+%04X", c);
    }
}
void dump(const UChar32* list, int32_t len) {
    printf("[");
    for (int32_t i=0; i<len; ++i) {
        if (i != 0) printf(", ");
        dump(list[i]);
    }
    printf("]");
}
#endif

/**
 * Adds the specified character to this set if it is not already
 * present.  If this set already contains the specified character,
 * the call leaves this set unchanged.
 */
UnicodeSet& UnicodeSet::add(UChar32 c) {
    // find smallest i such that c < list[i]
    // if odd, then it is IN the set
    // if even, then it is OUT of the set
    int32_t i = findCodePoint(pinCodePoint(c));

    // already in set?
    if ((i & 1) != 0  || isFrozen() || isBogus()) return *this;

    // HIGH is 0x110000
    // assert(list[len-1] == HIGH);

    // empty = [HIGH]
    // [start_0, limit_0, start_1, limit_1, HIGH]

    // [..., start_k-1, limit_k-1, start_k, limit_k, ..., HIGH]
    //                             ^
    //                             list[i]

    // i == 0 means c is before the first range

#ifdef DEBUG_US_ADD
    printf("Add of ");
    dump(c);
    printf(" found at %d", i);
    printf(": ");
    dump(list, len);
    printf(" => ");
#endif

    if (c == list[i]-1) {
        // c is before start of next range
        list[i] = c;
        // if we touched the HIGH mark, then add a new one
        if (c == (UNICODESET_HIGH - 1)) {
            UErrorCode status = U_ZERO_ERROR;
            ensureCapacity(len+1, status);
            if (U_FAILURE(status)) {
                return *this; // There is no way to report this error :-(
            }
            list[len++] = UNICODESET_HIGH;
        }
        if (i > 0 && c == list[i-1]) {
            // collapse adjacent ranges

            // [..., start_k-1, c, c, limit_k, ..., HIGH]
            //                     ^
            //                     list[i]

            //for (int32_t k=i-1; k<len-2; ++k) {
            //    list[k] = list[k+2];
            //}
            UChar32* dst = list + i - 1;
            UChar32* src = dst + 2;
            UChar32* srclimit = list + len;
            while (src < srclimit) *(dst++) = *(src++);

            len -= 2;
        }
    }

    else if (i > 0 && c == list[i-1]) {
        // c is after end of prior range
        list[i-1]++;
        // no need to check for collapse here
    }

    else {
        // At this point we know the new char is not adjacent to
        // any existing ranges, and it is not 10FFFF.


        // [..., start_k-1, limit_k-1, start_k, limit_k, ..., HIGH]
        //                             ^
        //                             list[i]

        // [..., start_k-1, limit_k-1, c, c+1, start_k, limit_k, ..., HIGH]
        //                             ^
        //                             list[i]

        UErrorCode status = U_ZERO_ERROR;
        ensureCapacity(len+2, status);
        if (U_FAILURE(status)) {
            return *this; // There is no way to report this error :-(
        }

        //for (int32_t k=len-1; k>=i; --k) {
        //    list[k+2] = list[k];
        //}
        UChar32* src = list + len;
        UChar32* dst = src + 2;
        UChar32* srclimit = list + i;
        while (src > srclimit) *(--dst) = *(--src);

        list[i] = c;
        list[i+1] = c+1;
        len += 2;
    }

#ifdef DEBUG_US_ADD
    dump(list, len);
    printf("\n");

    for (i=1; i<len; ++i) {
        if (list[i] <= list[i-1]) {
            // Corrupt array!
            printf("ERROR: list has been corrupted\n");
            exit(1);
        }
    }
#endif

    releasePattern();
    return *this;
}

/**
 * Adds the specified multicharacter to this set if it is not already
 * present.  If this set already contains the multicharacter,
 * the call leaves this set unchanged.
 * Thus "ch" => {"ch"}
 * <br><b>Warning: you cannot add an empty string ("") to a UnicodeSet.</b>
 * @param s the source string
 * @return the modified set, for chaining
 */
UnicodeSet& UnicodeSet::add(const UnicodeString& s) {
    if (s.length() == 0 || isFrozen() || isBogus()) return *this;
    int32_t cp = getSingleCP(s);
    if (cp < 0) {
        if (!strings->contains((void*) &s)) {
            _add(s);
            releasePattern();
        }
    } else {
        add((UChar32)cp);
    }
    return *this;
}

/**
 * Adds the given string, in order, to 'strings'.  The given string
 * must have been checked by the caller to not be empty and to not
 * already be in 'strings'.
 */
void UnicodeSet::_add(const UnicodeString& s) {
    if (isFrozen() || isBogus()) {
        return;
    }
    UnicodeString* t = new UnicodeString(s);
    if (t == NULL) { // Check for memory allocation error.
        setToBogus();
        return;
    }
    UErrorCode ec = U_ZERO_ERROR;
    strings->sortedInsert(t, compareUnicodeString, ec);
    if (U_FAILURE(ec)) {
        setToBogus();
        delete t;
    }
}

/**
 * @return a code point IF the string consists of a single one.
 * otherwise returns -1.
 * @param string to test
 */
int32_t UnicodeSet::getSingleCP(const UnicodeString& s) {
    //if (s.length() < 1) {
    //    throw new IllegalArgumentException("Can't use zero-length strings in UnicodeSet");
    //}
    if (s.length() > 2) return -1;
    if (s.length() == 1) return s.charAt(0);

    // at this point, len = 2
    UChar32 cp = s.char32At(0);
    if (cp > 0xFFFF) { // is surrogate pair
        return cp;
    }
    return -1;
}

/**
 * Adds each of the characters in this string to the set. Thus "ch" => {"c", "h"}
 * If this set already any particular character, it has no effect on that character.
 * @param the source string
 * @return the modified set, for chaining
 */
UnicodeSet& UnicodeSet::addAll(const UnicodeString& s) {
    UChar32 cp;
    for (int32_t i = 0; i < s.length(); i += U16_LENGTH(cp)) {
        cp = s.char32At(i);
        add(cp);
    }
    return *this;
}

/**
 * Retains EACH of the characters in this string. Note: "ch" == {"c", "h"}
 * If this set already any particular character, it has no effect on that character.
 * @param the source string
 * @return the modified set, for chaining
 */
UnicodeSet& UnicodeSet::retainAll(const UnicodeString& s) {
    UnicodeSet set;
    set.addAll(s);
    retainAll(set);
    return *this;
}

/**
 * Complement EACH of the characters in this string. Note: "ch" == {"c", "h"}
 * If this set already any particular character, it has no effect on that character.
 * @param the source string
 * @return the modified set, for chaining
 */
UnicodeSet& UnicodeSet::complementAll(const UnicodeString& s) {
    UnicodeSet set;
    set.addAll(s);
    complementAll(set);
    return *this;
}

/**
 * Remove EACH of the characters in this string. Note: "ch" == {"c", "h"}
 * If this set already any particular character, it has no effect on that character.
 * @param the source string
 * @return the modified set, for chaining
 */
UnicodeSet& UnicodeSet::removeAll(const UnicodeString& s) {
    UnicodeSet set;
    set.addAll(s);
    removeAll(set);
    return *this;
}

UnicodeSet& UnicodeSet::removeAllStrings() {
    strings->removeAllElements();
    return *this;
}


/**
 * Makes a set from a multicharacter string. Thus "ch" => {"ch"}
 * <br><b>Warning: you cannot add an empty string ("") to a UnicodeSet.</b>
 * @param the source string
 * @return a newly created set containing the given string
 */
UnicodeSet* U_EXPORT2 UnicodeSet::createFrom(const UnicodeString& s) {
    UnicodeSet *set = new UnicodeSet();
    if (set != NULL) { // Check for memory allocation error.
        set->add(s);
    }
    return set;
}


/**
 * Makes a set from each of the characters in the string. Thus "ch" => {"c", "h"}
 * @param the source string
 * @return a newly created set containing the given characters
 */
UnicodeSet* U_EXPORT2 UnicodeSet::createFromAll(const UnicodeString& s) {
    UnicodeSet *set = new UnicodeSet();
    if (set != NULL) { // Check for memory allocation error.
        set->addAll(s);
    }
    return set;
}

/**
 * Retain only the elements in this set that are contained in the
 * specified range.  If <code>end > start</code> then an empty range is
 * retained, leaving the set empty.
 *
 * @param start first character, inclusive, of range to be retained
 * to this set.
 * @param end last character, inclusive, of range to be retained
 * to this set.
 */
UnicodeSet& UnicodeSet::retain(UChar32 start, UChar32 end) {
    if (pinCodePoint(start) <= pinCodePoint(end)) {
        UChar32 range[3] = { start, end+1, UNICODESET_HIGH };
        retain(range, 2, 0);
    } else {
        clear();
    }
    return *this;
}

UnicodeSet& UnicodeSet::retain(UChar32 c) {
    return retain(c, c);
}

/**
 * Removes the specified range from this set if it is present.
 * The set will not contain the specified range once the call
 * returns.  If <code>end > start</code> then an empty range is
 * removed, leaving the set unchanged.
 *
 * @param start first character, inclusive, of range to be removed
 * from this set.
 * @param end last character, inclusive, of range to be removed
 * from this set.
 */
UnicodeSet& UnicodeSet::remove(UChar32 start, UChar32 end) {
    if (pinCodePoint(start) <= pinCodePoint(end)) {
        UChar32 range[3] = { start, end+1, UNICODESET_HIGH };
        retain(range, 2, 2);
    }
    return *this;
}

/**
 * Removes the specified character from this set if it is present.
 * The set will not contain the specified range once the call
 * returns.
 */
UnicodeSet& UnicodeSet::remove(UChar32 c) {
    return remove(c, c);
}

/**
 * Removes the specified string from this set if it is present.
 * The set will not contain the specified character once the call
 * returns.
 * @param the source string
 * @return the modified set, for chaining
 */
UnicodeSet& UnicodeSet::remove(const UnicodeString& s) {
    if (s.length() == 0 || isFrozen() || isBogus()) return *this;
    int32_t cp = getSingleCP(s);
    if (cp < 0) {
        strings->removeElement((void*) &s);
        releasePattern();
    } else {
        remove((UChar32)cp, (UChar32)cp);
    }
    return *this;
}

/**
 * Complements the specified range in this set.  Any character in
 * the range will be removed if it is in this set, or will be
 * added if it is not in this set.  If <code>end > start</code>
 * then an empty range is xor'ed, leaving the set unchanged.
 *
 * @param start first character, inclusive, of range to be removed
 * from this set.
 * @param end last character, inclusive, of range to be removed
 * from this set.
 */
UnicodeSet& UnicodeSet::complement(UChar32 start, UChar32 end) {
    if (isFrozen() || isBogus()) {
        return *this;
    }
    if (pinCodePoint(start) <= pinCodePoint(end)) {
        UChar32 range[3] = { start, end+1, UNICODESET_HIGH };
        exclusiveOr(range, 2, 0);
    }
    releasePattern();
    return *this;
}

UnicodeSet& UnicodeSet::complement(UChar32 c) {
    return complement(c, c);
}

/**
 * This is equivalent to
 * <code>complement(MIN_VALUE, MAX_VALUE)</code>.
 */
UnicodeSet& UnicodeSet::complement(void) {
    if (isFrozen() || isBogus()) {
        return *this;
    }
    UErrorCode status = U_ZERO_ERROR;
    if (list[0] == UNICODESET_LOW) {
        ensureBufferCapacity(len-1, status);
        if (U_FAILURE(status)) {
            return *this;
        }
        uprv_memcpy(buffer, list + 1, (size_t)(len-1)*sizeof(UChar32));
        --len;
    } else {
        ensureBufferCapacity(len+1, status);
        if (U_FAILURE(status)) {
            return *this;
        }
        uprv_memcpy(buffer + 1, list, (size_t)len*sizeof(UChar32));
        buffer[0] = UNICODESET_LOW;
        ++len;
    }
    swapBuffers();
    releasePattern();
    return *this;
}

/**
 * Complement the specified string in this set.
 * The set will not contain the specified string once the call
 * returns.
 * <br><b>Warning: you cannot add an empty string ("") to a UnicodeSet.</b>
 * @param s the string to complement
 * @return this object, for chaining
 */
UnicodeSet& UnicodeSet::complement(const UnicodeString& s) {
    if (s.length() == 0 || isFrozen() || isBogus()) return *this;
    int32_t cp = getSingleCP(s);
    if (cp < 0) {
        if (strings->contains((void*) &s)) {
            strings->removeElement((void*) &s);
        } else {
            _add(s);
        }
        releasePattern();
    } else {
        complement((UChar32)cp, (UChar32)cp);
    }
    return *this;
}

/**
 * Adds all of the elements in the specified set to this set if
 * they're not already present.  This operation effectively
 * modifies this set so that its value is the <i>union</i> of the two
 * sets.  The behavior of this operation is unspecified if the specified
 * collection is modified while the operation is in progress.
 *
 * @param c set whose elements are to be added to this set.
 * @see #add(char, char)
 */
UnicodeSet& UnicodeSet::addAll(const UnicodeSet& c) {
    if ( c.len>0 && c.list!=NULL ) {
        add(c.list, c.len, 0);
    }

    // Add strings in order
    if ( c.strings!=NULL ) {
        for (int32_t i=0; i<c.strings->size(); ++i) {
            const UnicodeString* s = (const UnicodeString*)c.strings->elementAt(i);
            if (!strings->contains((void*) s)) {
                _add(*s);
            }
        }
    }
    return *this;
}

/**
 * Retains only the elements in this set that are contained in the
 * specified set.  In other words, removes from this set all of
 * its elements that are not contained in the specified set.  This
 * operation effectively modifies this set so that its value is
 * the <i>intersection</i> of the two sets.
 *
 * @param c set that defines which elements this set will retain.
 */
UnicodeSet& UnicodeSet::retainAll(const UnicodeSet& c) {
    if (isFrozen() || isBogus()) {
        return *this;
    }
    retain(c.list, c.len, 0);
    strings->retainAll(*c.strings);
    return *this;
}

/**
 * Removes from this set all of its elements that are contained in the
 * specified set.  This operation effectively modifies this
 * set so that its value is the <i>asymmetric set difference</i> of
 * the two sets.
 *
 * @param c set that defines which elements will be removed from
 *          this set.
 */
UnicodeSet& UnicodeSet::removeAll(const UnicodeSet& c) {
    if (isFrozen() || isBogus()) {
        return *this;
    }
    retain(c.list, c.len, 2);
    strings->removeAll(*c.strings);
    return *this;
}

/**
 * Complements in this set all elements contained in the specified
 * set.  Any character in the other set will be removed if it is
 * in this set, or will be added if it is not in this set.
 *
 * @param c set that defines which elements will be xor'ed from
 *          this set.
 */
UnicodeSet& UnicodeSet::complementAll(const UnicodeSet& c) {
    if (isFrozen() || isBogus()) {
        return *this;
    }
    exclusiveOr(c.list, c.len, 0);

    for (int32_t i=0; i<c.strings->size(); ++i) {
        void* e = c.strings->elementAt(i);
        if (!strings->removeElement(e)) {
            _add(*(const UnicodeString*)e);
        }
    }
    return *this;
}

/**
 * Removes all of the elements from this set.  This set will be
 * empty after this call returns.
 */
UnicodeSet& UnicodeSet::clear(void) {
    if (isFrozen()) {
        return *this;
    }
    if (list != NULL) {
        list[0] = UNICODESET_HIGH;
    }
    len = 1;
    releasePattern();
    if (strings != NULL) {
        strings->removeAllElements();
    }
    if (list != NULL && strings != NULL) {
        // Remove bogus
        fFlags = 0;
    }
    return *this;
}

/**
 * Iteration method that returns the number of ranges contained in
 * this set.
 * @see #getRangeStart
 * @see #getRangeEnd
 */
int32_t UnicodeSet::getRangeCount() const {
    return len/2;
}

/**
 * Iteration method that returns the first character in the
 * specified range of this set.
 * @see #getRangeCount
 * @see #getRangeEnd
 */
UChar32 UnicodeSet::getRangeStart(int32_t index) const {
    return list[index*2];
}

/**
 * Iteration method that returns the last character in the
 * specified range of this set.
 * @see #getRangeStart
 * @see #getRangeEnd
 */
UChar32 UnicodeSet::getRangeEnd(int32_t index) const {
    return list[index*2 + 1] - 1;
}

int32_t UnicodeSet::getStringCount() const {
    return strings->size();
}

const UnicodeString* UnicodeSet::getString(int32_t index) const {
    return (const UnicodeString*) strings->elementAt(index);
}

/**
 * Reallocate this objects internal structures to take up the least
 * possible space, without changing this object's value.
 */
UnicodeSet& UnicodeSet::compact() {
    if (isFrozen() || isBogus()) {
        return *this;
    }
    // Delete buffer first to defragment memory less.
    if (buffer != NULL) {
        uprv_free(buffer);
        buffer = NULL;
    }
    if (len < capacity) {
        // Make the capacity equal to len or 1.
        // We don't want to realloc of 0 size.
        int32_t newCapacity = len + (len == 0);
        UChar32* temp = (UChar32*) uprv_realloc(list, sizeof(UChar32) * newCapacity);
        if (temp) {
            list = temp;
            capacity = newCapacity;
        }
        // else what the heck happened?! We allocated less memory!
        // Oh well. We'll keep our original array.
    }
    return *this;
}

#ifdef DEBUG_SERIALIZE
#include <stdio.h>
#endif

/**
 * Deserialize constructor.
 */
UnicodeSet::UnicodeSet(const uint16_t data[], int32_t dataLen, ESerialization serialization, UErrorCode &ec)
  : len(1), capacity(1+START_EXTRA), list(0), bmpSet(0), buffer(0),
    bufferCapacity(0), patLen(0), pat(NULL), strings(NULL), stringSpan(NULL),
    fFlags(0) {

  if(U_FAILURE(ec)) {
    setToBogus();
    return;
  }

  if( (serialization != kSerialized)
      || (data==NULL)
      || (dataLen < 1)) {
    ec = U_ILLEGAL_ARGUMENT_ERROR;
    setToBogus();
    return;
  }

  allocateStrings(ec);
  if (U_FAILURE(ec)) {
    setToBogus();
    return;
  }

  // bmp?
  int32_t headerSize = ((data[0]&0x8000)) ?2:1;
  int32_t bmpLength = (headerSize==1)?data[0]:data[1];

  len = (((data[0]&0x7FFF)-bmpLength)/2)+bmpLength;
#ifdef DEBUG_SERIALIZE
  printf("dataLen %d headerSize %d bmpLen %d len %d. data[0]=%X/%X/%X/%X\n", dataLen,headerSize,bmpLength,len, data[0],data[1],data[2],data[3]);
#endif
  capacity = len+1;
  list = (UChar32*) uprv_malloc(sizeof(UChar32) * capacity);
  if(!list || U_FAILURE(ec)) {
    setToBogus();
    return;
  }
  // copy bmp
  int32_t i;
  for(i = 0; i< bmpLength;i++) {
    list[i] = data[i+headerSize];
#ifdef DEBUG_SERIALIZE
    printf("<<16@%d[%d] %X\n", i+headerSize, i, list[i]);
#endif
  }
  // copy smp
  for(i=bmpLength;i<len;i++) {
    list[i] = ((UChar32)data[headerSize+bmpLength+(i-bmpLength)*2+0] << 16) +
              ((UChar32)data[headerSize+bmpLength+(i-bmpLength)*2+1]);
#ifdef DEBUG_SERIALIZE
    printf("<<32@%d+[%d] %lX\n", headerSize+bmpLength+i, i, list[i]);
#endif
  }
  // terminator
  list[len++]=UNICODESET_HIGH;
}


int32_t UnicodeSet::serialize(uint16_t *dest, int32_t destCapacity, UErrorCode& ec) const {
    int32_t bmpLength, length, destLength;

    if (U_FAILURE(ec)) {
        return 0;
    }

    if (destCapacity<0 || (destCapacity>0 && dest==NULL)) {
        ec=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    /* count necessary 16-bit units */
    length=this->len-1; // Subtract 1 to ignore final UNICODESET_HIGH
    // assert(length>=0);
    if (length==0) {
        /* empty set */
        if (destCapacity>0) {
            *dest=0;
        } else {
            ec=U_BUFFER_OVERFLOW_ERROR;
        }
        return 1;
    }
    /* now length>0 */

    if (this->list[length-1]<=0xffff) {
        /* all BMP */
        bmpLength=length;
    } else if (this->list[0]>=0x10000) {
        /* all supplementary */
        bmpLength=0;
        length*=2;
    } else {
        /* some BMP, some supplementary */
        for (bmpLength=0; bmpLength<length && this->list[bmpLength]<=0xffff; ++bmpLength) {}
        length=bmpLength+2*(length-bmpLength);
    }
#ifdef DEBUG_SERIALIZE
    printf(">> bmpLength%d length%d len%d\n", bmpLength, length, len);
#endif
    /* length: number of 16-bit array units */
    if (length>0x7fff) {
        /* there are only 15 bits for the length in the first serialized word */
        ec=U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
    }

    /*
     * total serialized length:
     * number of 16-bit array units (length) +
     * 1 length unit (always) +
     * 1 bmpLength unit (if there are supplementary values)
     */
    destLength=length+((length>bmpLength)?2:1);
    if (destLength<=destCapacity) {
        const UChar32 *p;
        int32_t i;

#ifdef DEBUG_SERIALIZE
        printf("writeHdr\n");
#endif
        *dest=(uint16_t)length;
        if (length>bmpLength) {
            *dest|=0x8000;
            *++dest=(uint16_t)bmpLength;
        }
        ++dest;

        /* write the BMP part of the array */
        p=this->list;
        for (i=0; i<bmpLength; ++i) {
#ifdef DEBUG_SERIALIZE
          printf("writebmp: %x\n", (int)*p);
#endif
            *dest++=(uint16_t)*p++;
        }

        /* write the supplementary part of the array */
        for (; i<length; i+=2) {
#ifdef DEBUG_SERIALIZE
          printf("write32: %x\n", (int)*p);
#endif
            *dest++=(uint16_t)(*p>>16);
            *dest++=(uint16_t)*p++;
        }
    } else {
        ec=U_BUFFER_OVERFLOW_ERROR;
    }
    return destLength;
}

//----------------------------------------------------------------
// Implementation: Utility methods
//----------------------------------------------------------------

/**
 * Allocate our strings vector and return TRUE if successful.
 */
UBool UnicodeSet::allocateStrings(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    strings = new UVector(uprv_deleteUObject,
                          uhash_compareUnicodeString, 1, status);
    if (strings == NULL) { // Check for memory allocation error.
        status = U_MEMORY_ALLOCATION_ERROR;
        return FALSE;
    }
    if (U_FAILURE(status)) {
        delete strings;
        strings = NULL;
        return FALSE;
    } 
    return TRUE;
}

void UnicodeSet::ensureCapacity(int32_t newLen, UErrorCode& ec) {
    if (newLen <= capacity)
        return;
    UChar32* temp = (UChar32*) uprv_realloc(list, sizeof(UChar32) * (newLen + GROW_EXTRA));
    if (temp == NULL) {
        ec = U_MEMORY_ALLOCATION_ERROR;
        setToBogus();
        return;
    }
    list = temp;
    capacity = newLen + GROW_EXTRA;
    // else we keep the original contents on the memory failure.
}

void UnicodeSet::ensureBufferCapacity(int32_t newLen, UErrorCode& ec) {
    if (buffer != NULL && newLen <= bufferCapacity)
        return;
    UChar32* temp = (UChar32*) uprv_realloc(buffer, sizeof(UChar32) * (newLen + GROW_EXTRA));
    if (temp == NULL) {
        ec = U_MEMORY_ALLOCATION_ERROR;
        setToBogus();
        return;
    }
    buffer = temp;
    bufferCapacity = newLen + GROW_EXTRA;
    // else we keep the original contents on the memory failure.
}

/**
 * Swap list and buffer.
 */
void UnicodeSet::swapBuffers(void) {
    // swap list and buffer
    UChar32* temp = list;
    list = buffer;
    buffer = temp;

    int32_t c = capacity;
    capacity = bufferCapacity;
    bufferCapacity = c;
}

void UnicodeSet::setToBogus() {
    clear(); // Remove everything in the set.
    fFlags = kIsBogus;
}

//----------------------------------------------------------------
// Implementation: Fundamental operators
//----------------------------------------------------------------

static inline UChar32 max(UChar32 a, UChar32 b) {
    return (a > b) ? a : b;
}

// polarity = 0, 3 is normal: x xor y
// polarity = 1, 2: x xor ~y == x === y

void UnicodeSet::exclusiveOr(const UChar32* other, int32_t otherLen, int8_t polarity) {
    if (isFrozen() || isBogus()) {
        return;
    }
    UErrorCode status = U_ZERO_ERROR;
    ensureBufferCapacity(len + otherLen, status);
    if (U_FAILURE(status)) {
        return;
    }

    int32_t i = 0, j = 0, k = 0;
    UChar32 a = list[i++];
    UChar32 b;
    if (polarity == 1 || polarity == 2) {
        b = UNICODESET_LOW;
        if (other[j] == UNICODESET_LOW) { // skip base if already LOW
            ++j;
            b = other[j];
        }
    } else {
        b = other[j++];
    }
    // simplest of all the routines
    // sort the values, discarding identicals!
    for (;;) {
        if (a < b) {
            buffer[k++] = a;
            a = list[i++];
        } else if (b < a) {
            buffer[k++] = b;
            b = other[j++];
        } else if (a != UNICODESET_HIGH) { // at this point, a == b
            // discard both values!
            a = list[i++];
            b = other[j++];
        } else { // DONE!
            buffer[k++] = UNICODESET_HIGH;
            len = k;
            break;
        }
    }
    swapBuffers();
    releasePattern();
}

// polarity = 0 is normal: x union y
// polarity = 2: x union ~y
// polarity = 1: ~x union y
// polarity = 3: ~x union ~y

void UnicodeSet::add(const UChar32* other, int32_t otherLen, int8_t polarity) {
    if (isFrozen() || isBogus() || other==NULL) {
        return;
    }
    UErrorCode status = U_ZERO_ERROR;
    ensureBufferCapacity(len + otherLen, status);
    if (U_FAILURE(status)) {
        return;
    }

    int32_t i = 0, j = 0, k = 0;
    UChar32 a = list[i++];
    UChar32 b = other[j++];
    // change from xor is that we have to check overlapping pairs
    // polarity bit 1 means a is second, bit 2 means b is.
    for (;;) {
        switch (polarity) {
          case 0: // both first; take lower if unequal
            if (a < b) { // take a
                // Back up over overlapping ranges in buffer[]
                if (k > 0 && a <= buffer[k-1]) {
                    // Pick latter end value in buffer[] vs. list[]
                    a = max(list[i], buffer[--k]);
                } else {
                    // No overlap
                    buffer[k++] = a;
                    a = list[i];
                }
                i++; // Common if/else code factored out
                polarity ^= 1;
            } else if (b < a) { // take b
                if (k > 0 && b <= buffer[k-1]) {
                    b = max(other[j], buffer[--k]);
                } else {
                    buffer[k++] = b;
                    b = other[j];
                }
                j++;
                polarity ^= 2;
            } else { // a == b, take a, drop b
                if (a == UNICODESET_HIGH) goto loop_end;
                // This is symmetrical; it doesn't matter if
                // we backtrack with a or b. - liu
                if (k > 0 && a <= buffer[k-1]) {
                    a = max(list[i], buffer[--k]);
                } else {
                    // No overlap
                    buffer[k++] = a;
                    a = list[i];
                }
                i++;
                polarity ^= 1;
                b = other[j++];
                polarity ^= 2;
            }
            break;
          case 3: // both second; take higher if unequal, and drop other
            if (b <= a) { // take a
                if (a == UNICODESET_HIGH) goto loop_end;
                buffer[k++] = a;
            } else { // take b
                if (b == UNICODESET_HIGH) goto loop_end;
                buffer[k++] = b;
            }
            a = list[i++];
            polarity ^= 1;   // factored common code
            b = other[j++];
            polarity ^= 2;
            break;
          case 1: // a second, b first; if b < a, overlap
            if (a < b) { // no overlap, take a
                buffer[k++] = a; a = list[i++]; polarity ^= 1;
            } else if (b < a) { // OVERLAP, drop b
                b = other[j++];
                polarity ^= 2;
            } else { // a == b, drop both!
                if (a == UNICODESET_HIGH) goto loop_end;
                a = list[i++];
                polarity ^= 1;
                b = other[j++];
                polarity ^= 2;
            }
            break;
          case 2: // a first, b second; if a < b, overlap
            if (b < a) { // no overlap, take b
                buffer[k++] = b;
                b = other[j++];
                polarity ^= 2;
            } else  if (a < b) { // OVERLAP, drop a
                a = list[i++];
                polarity ^= 1;
            } else { // a == b, drop both!
                if (a == UNICODESET_HIGH) goto loop_end;
                a = list[i++];
                polarity ^= 1;
                b = other[j++];
                polarity ^= 2;
            }
            break;
        }
    }
 loop_end:
    buffer[k++] = UNICODESET_HIGH;    // terminate
    len = k;
    swapBuffers();
    releasePattern();
}

// polarity = 0 is normal: x intersect y
// polarity = 2: x intersect ~y == set-minus
// polarity = 1: ~x intersect y
// polarity = 3: ~x intersect ~y

void UnicodeSet::retain(const UChar32* other, int32_t otherLen, int8_t polarity) {
    if (isFrozen() || isBogus()) {
        return;
    }
    UErrorCode status = U_ZERO_ERROR;
    ensureBufferCapacity(len + otherLen, status);
    if (U_FAILURE(status)) {
        return;
    }

    int32_t i = 0, j = 0, k = 0;
    UChar32 a = list[i++];
    UChar32 b = other[j++];
    // change from xor is that we have to check overlapping pairs
    // polarity bit 1 means a is second, bit 2 means b is.
    for (;;) {
        switch (polarity) {
          case 0: // both first; drop the smaller
            if (a < b) { // drop a
                a = list[i++];
                polarity ^= 1;
            } else if (b < a) { // drop b
                b = other[j++];
                polarity ^= 2;
            } else { // a == b, take one, drop other
                if (a == UNICODESET_HIGH) goto loop_end;
                buffer[k++] = a;
                a = list[i++];
                polarity ^= 1;
                b = other[j++];
                polarity ^= 2;
            }
            break;
          case 3: // both second; take lower if unequal
            if (a < b) { // take a
                buffer[k++] = a;
                a = list[i++];
                polarity ^= 1;
            } else if (b < a) { // take b
                buffer[k++] = b;
                b = other[j++];
                polarity ^= 2;
            } else { // a == b, take one, drop other
                if (a == UNICODESET_HIGH) goto loop_end;
                buffer[k++] = a;
                a = list[i++];
                polarity ^= 1;
                b = other[j++];
                polarity ^= 2;
            }
            break;
          case 1: // a second, b first;
            if (a < b) { // NO OVERLAP, drop a
                a = list[i++];
                polarity ^= 1;
            } else if (b < a) { // OVERLAP, take b
                buffer[k++] = b;
                b = other[j++];
                polarity ^= 2;
            } else { // a == b, drop both!
                if (a == UNICODESET_HIGH) goto loop_end;
                a = list[i++];
                polarity ^= 1;
                b = other[j++];
                polarity ^= 2;
            }
            break;
          case 2: // a first, b second; if a < b, overlap
            if (b < a) { // no overlap, drop b
                b = other[j++];
                polarity ^= 2;
            } else  if (a < b) { // OVERLAP, take a
                buffer[k++] = a;
                a = list[i++];
                polarity ^= 1;
            } else { // a == b, drop both!
                if (a == UNICODESET_HIGH) goto loop_end;
                a = list[i++];
                polarity ^= 1;
                b = other[j++];
                polarity ^= 2;
            }
            break;
        }
    }
 loop_end:
    buffer[k++] = UNICODESET_HIGH;    // terminate
    len = k;
    swapBuffers();
    releasePattern();
}

/**
 * Append the <code>toPattern()</code> representation of a
 * string to the given <code>StringBuffer</code>.
 */
void UnicodeSet::_appendToPat(UnicodeString& buf, const UnicodeString& s, UBool
escapeUnprintable) {
    UChar32 cp;
    for (int32_t i = 0; i < s.length(); i += U16_LENGTH(cp)) {
        _appendToPat(buf, cp = s.char32At(i), escapeUnprintable);
    }
}

/**
 * Append the <code>toPattern()</code> representation of a
 * character to the given <code>StringBuffer</code>.
 */
void UnicodeSet::_appendToPat(UnicodeString& buf, UChar32 c, UBool
escapeUnprintable) {
    if (escapeUnprintable && ICU_Utility::isUnprintable(c)) {
        // Use hex escape notation (\uxxxx or \Uxxxxxxxx) for anything
        // unprintable
        if (ICU_Utility::escapeUnprintable(buf, c)) {
            return;
        }
    }
    // Okay to let ':' pass through
    switch (c) {
    case SET_OPEN:
    case SET_CLOSE:
    case HYPHEN:
    case COMPLEMENT:
    case INTERSECTION:
    case BACKSLASH:
    case OPEN_BRACE:
    case CLOSE_BRACE:
    case COLON:
    case SymbolTable::SYMBOL_REF:
        buf.append(BACKSLASH);
        break;
    default:
        // Escape whitespace
        if (PatternProps::isWhiteSpace(c)) {
            buf.append(BACKSLASH);
        }
        break;
    }
    buf.append(c);
}

/**
 * Append a string representation of this set to result.  This will be
 * a cleaned version of the string passed to applyPattern(), if there
 * is one.  Otherwise it will be generated.
 */
UnicodeString& UnicodeSet::_toPattern(UnicodeString& result,
                                      UBool escapeUnprintable) const
{
    if (pat != NULL) {
        int32_t i;
        int32_t backslashCount = 0;
        for (i=0; i<patLen; ) {
            UChar32 c;
            U16_NEXT(pat, i, patLen, c);
            if (escapeUnprintable && ICU_Utility::isUnprintable(c)) {
                // If the unprintable character is preceded by an odd
                // number of backslashes, then it has been escaped.
                // Before unescaping it, we delete the final
                // backslash.
                if ((backslashCount % 2) == 1) {
                    result.truncate(result.length() - 1);
                }
                ICU_Utility::escapeUnprintable(result, c);
                backslashCount = 0;
            } else {
                result.append(c);
                if (c == BACKSLASH) {
                    ++backslashCount;
                } else {
                    backslashCount = 0;
                }
            }
        }
        return result;
    }

    return _generatePattern(result, escapeUnprintable);
}

/**
 * Returns a string representation of this set.  If the result of
 * calling this function is passed to a UnicodeSet constructor, it
 * will produce another set that is equal to this one.
 */
UnicodeString& UnicodeSet::toPattern(UnicodeString& result,
                                     UBool escapeUnprintable) const
{
    result.truncate(0);
    return _toPattern(result, escapeUnprintable);
}

/**
 * Generate and append a string representation of this set to result.
 * This does not use this.pat, the cleaned up copy of the string
 * passed to applyPattern().
 */
UnicodeString& UnicodeSet::_generatePattern(UnicodeString& result,
                                            UBool escapeUnprintable) const
{
    result.append(SET_OPEN);

//  // Check against the predefined categories.  We implicitly build
//  // up ALL category sets the first time toPattern() is called.
//  for (int8_t cat=0; cat<Unicode::GENERAL_TYPES_COUNT; ++cat) {
//      if (*this == getCategorySet(cat)) {
//          result.append(COLON);
//          result.append(CATEGORY_NAMES, cat*2, 2);
//          return result.append(CATEGORY_CLOSE);
//      }
//  }

    int32_t count = getRangeCount();

    // If the set contains at least 2 intervals and includes both
    // MIN_VALUE and MAX_VALUE, then the inverse representation will
    // be more economical.
    if (count > 1 &&
        getRangeStart(0) == MIN_VALUE &&
        getRangeEnd(count-1) == MAX_VALUE) {

        // Emit the inverse
        result.append(COMPLEMENT);

        for (int32_t i = 1; i < count; ++i) {
            UChar32 start = getRangeEnd(i-1)+1;
            UChar32 end = getRangeStart(i)-1;
            _appendToPat(result, start, escapeUnprintable);
            if (start != end) {
                if ((start+1) != end) {
                    result.append(HYPHEN);
                }
                _appendToPat(result, end, escapeUnprintable);
            }
        }
    }

    // Default; emit the ranges as pairs
    else {
        for (int32_t i = 0; i < count; ++i) {
            UChar32 start = getRangeStart(i);
            UChar32 end = getRangeEnd(i);
            _appendToPat(result, start, escapeUnprintable);
            if (start != end) {
                if ((start+1) != end) {
                    result.append(HYPHEN);
                }
                _appendToPat(result, end, escapeUnprintable);
            }
        }
    }

    for (int32_t i = 0; i<strings->size(); ++i) {
        result.append(OPEN_BRACE);
        _appendToPat(result,
                     *(const UnicodeString*) strings->elementAt(i),
                     escapeUnprintable);
        result.append(CLOSE_BRACE);
    }
    return result.append(SET_CLOSE);
}

/**
* Release existing cached pattern
*/
void UnicodeSet::releasePattern() {
    if (pat) {
        uprv_free(pat);
        pat = NULL;
        patLen = 0;
    }
}

/**
* Set the new pattern to cache.
*/
void UnicodeSet::setPattern(const UnicodeString& newPat) {
    releasePattern();
    int32_t newPatLen = newPat.length();
    pat = (UChar *)uprv_malloc((newPatLen + 1) * sizeof(UChar));
    if (pat) {
        patLen = newPatLen;
        newPat.extractBetween(0, patLen, pat);
        pat[patLen] = 0;
    }
    // else we don't care if malloc failed. This was just a nice cache.
    // We can regenerate an equivalent pattern later when requested.
}

UnicodeFunctor *UnicodeSet::freeze() {
    if(!isFrozen() && !isBogus()) {
        // Do most of what compact() does before freezing because
        // compact() will not work when the set is frozen.
        // Small modification: Don't shrink if the savings would be tiny (<=GROW_EXTRA).

        // Delete buffer first to defragment memory less.
        if (buffer != NULL) {
            uprv_free(buffer);
            buffer = NULL;
        }
        if (capacity > (len + GROW_EXTRA)) {
            // Make the capacity equal to len or 1.
            // We don't want to realloc of 0 size.
            capacity = len + (len == 0);
            list = (UChar32*) uprv_realloc(list, sizeof(UChar32) * capacity);
            if (list == NULL) { // Check for memory allocation error.
                setToBogus();
                return this;
            }
        }

        // Optimize contains() and span() and similar functions.
        if (!strings->isEmpty()) {
            stringSpan = new UnicodeSetStringSpan(*this, *strings, UnicodeSetStringSpan::ALL);
            if (stringSpan != NULL && !stringSpan->needsStringSpanUTF16()) {
                // All strings are irrelevant for span() etc. because
                // all of each string's code points are contained in this set.
                // Do not check needsStringSpanUTF8() because UTF-8 has at most as
                // many relevant strings as UTF-16.
                // (Thus needsStringSpanUTF8() implies needsStringSpanUTF16().)
                delete stringSpan;
                stringSpan = NULL;
            }
        }
        if (stringSpan == NULL) {
            // No span-relevant strings: Optimize for code point spans.
            bmpSet=new BMPSet(list, len);
            if (bmpSet == NULL) { // Check for memory allocation error.
                setToBogus();
            }
        }
    }
    return this;
}

int32_t UnicodeSet::span(const UChar *s, int32_t length, USetSpanCondition spanCondition) const {
    if(length>0 && bmpSet!=NULL) {
        return (int32_t)(bmpSet->span(s, s+length, spanCondition)-s);
    }
    if(length<0) {
        length=u_strlen(s);
    }
    if(length==0) {
        return 0;
    }
    if(stringSpan!=NULL) {
        return stringSpan->span(s, length, spanCondition);
    } else if(!strings->isEmpty()) {
        uint32_t which= spanCondition==USET_SPAN_NOT_CONTAINED ?
                            UnicodeSetStringSpan::FWD_UTF16_NOT_CONTAINED :
                            UnicodeSetStringSpan::FWD_UTF16_CONTAINED;
        UnicodeSetStringSpan strSpan(*this, *strings, which);
        if(strSpan.needsStringSpanUTF16()) {
            return strSpan.span(s, length, spanCondition);
        }
    }

    if(spanCondition!=USET_SPAN_NOT_CONTAINED) {
        spanCondition=USET_SPAN_CONTAINED;  // Pin to 0/1 values.
    }

    UChar32 c;
    int32_t start=0, prev=0;
    do {
        U16_NEXT(s, start, length, c);
        if(spanCondition!=contains(c)) {
            break;
        }
    } while((prev=start)<length);
    return prev;
}

int32_t UnicodeSet::spanBack(const UChar *s, int32_t length, USetSpanCondition spanCondition) const {
    if(length>0 && bmpSet!=NULL) {
        return (int32_t)(bmpSet->spanBack(s, s+length, spanCondition)-s);
    }
    if(length<0) {
        length=u_strlen(s);
    }
    if(length==0) {
        return 0;
    }
    if(stringSpan!=NULL) {
        return stringSpan->spanBack(s, length, spanCondition);
    } else if(!strings->isEmpty()) {
        uint32_t which= spanCondition==USET_SPAN_NOT_CONTAINED ?
                            UnicodeSetStringSpan::BACK_UTF16_NOT_CONTAINED :
                            UnicodeSetStringSpan::BACK_UTF16_CONTAINED;
        UnicodeSetStringSpan strSpan(*this, *strings, which);
        if(strSpan.needsStringSpanUTF16()) {
            return strSpan.spanBack(s, length, spanCondition);
        }
    }

    if(spanCondition!=USET_SPAN_NOT_CONTAINED) {
        spanCondition=USET_SPAN_CONTAINED;  // Pin to 0/1 values.
    }

    UChar32 c;
    int32_t prev=length;
    do {
        U16_PREV(s, 0, length, c);
        if(spanCondition!=contains(c)) {
            break;
        }
    } while((prev=length)>0);
    return prev;
}

int32_t UnicodeSet::spanUTF8(const char *s, int32_t length, USetSpanCondition spanCondition) const {
    if(length>0 && bmpSet!=NULL) {
        const uint8_t *s0=(const uint8_t *)s;
        return (int32_t)(bmpSet->spanUTF8(s0, length, spanCondition)-s0);
    }
    if(length<0) {
        length=(int32_t)uprv_strlen(s);
    }
    if(length==0) {
        return 0;
    }
    if(stringSpan!=NULL) {
        return stringSpan->spanUTF8((const uint8_t *)s, length, spanCondition);
    } else if(!strings->isEmpty()) {
        uint32_t which= spanCondition==USET_SPAN_NOT_CONTAINED ?
                            UnicodeSetStringSpan::FWD_UTF8_NOT_CONTAINED :
                            UnicodeSetStringSpan::FWD_UTF8_CONTAINED;
        UnicodeSetStringSpan strSpan(*this, *strings, which);
        if(strSpan.needsStringSpanUTF8()) {
            return strSpan.spanUTF8((const uint8_t *)s, length, spanCondition);
        }
    }

    if(spanCondition!=USET_SPAN_NOT_CONTAINED) {
        spanCondition=USET_SPAN_CONTAINED;  // Pin to 0/1 values.
    }

    UChar32 c;
    int32_t start=0, prev=0;
    do {
        U8_NEXT_OR_FFFD(s, start, length, c);
        if(spanCondition!=contains(c)) {
            break;
        }
    } while((prev=start)<length);
    return prev;
}

int32_t UnicodeSet::spanBackUTF8(const char *s, int32_t length, USetSpanCondition spanCondition) const {
    if(length>0 && bmpSet!=NULL) {
        const uint8_t *s0=(const uint8_t *)s;
        return bmpSet->spanBackUTF8(s0, length, spanCondition);
    }
    if(length<0) {
        length=(int32_t)uprv_strlen(s);
    }
    if(length==0) {
        return 0;
    }
    if(stringSpan!=NULL) {
        return stringSpan->spanBackUTF8((const uint8_t *)s, length, spanCondition);
    } else if(!strings->isEmpty()) {
        uint32_t which= spanCondition==USET_SPAN_NOT_CONTAINED ?
                            UnicodeSetStringSpan::BACK_UTF8_NOT_CONTAINED :
                            UnicodeSetStringSpan::BACK_UTF8_CONTAINED;
        UnicodeSetStringSpan strSpan(*this, *strings, which);
        if(strSpan.needsStringSpanUTF8()) {
            return strSpan.spanBackUTF8((const uint8_t *)s, length, spanCondition);
        }
    }

    if(spanCondition!=USET_SPAN_NOT_CONTAINED) {
        spanCondition=USET_SPAN_CONTAINED;  // Pin to 0/1 values.
    }

    UChar32 c;
    int32_t prev=length;
    do {
        U8_PREV_OR_FFFD(s, 0, length, c);
        if(spanCondition!=contains(c)) {
            break;
        }
    } while((prev=length)>0);
    return prev;
}

U_NAMESPACE_END

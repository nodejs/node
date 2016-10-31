// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
* Copyright (C) {1999-2001}, International Business Machines Corporation and others. All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   11/17/99    aliu        Creation.
**********************************************************************
*/
#ifndef RBT_RULE_H
#define RBT_RULE_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/uobject.h"
#include "unicode/unistr.h"
#include "unicode/utrans.h"
#include "unicode/unimatch.h"

U_NAMESPACE_BEGIN

class Replaceable;
class TransliterationRuleData;
class StringMatcher;
class UnicodeFunctor;

/**
 * A transliteration rule used by
 * <code>RuleBasedTransliterator</code>.
 * <code>TransliterationRule</code> is an immutable object.
 *
 * <p>A rule consists of an input pattern and an output string.  When
 * the input pattern is matched, the output string is emitted.  The
 * input pattern consists of zero or more characters which are matched
 * exactly (the key) and optional context.  Context must match if it
 * is specified.  Context may be specified before the key, after the
 * key, or both.  The key, preceding context, and following context
 * may contain variables.  Variables represent a set of Unicode
 * characters, such as the letters <i>a</i> through <i>z</i>.
 * Variables are detected by looking up each character in a supplied
 * variable list to see if it has been so defined.
 *
 * <p>A rule may contain segments in its input string and segment
 * references in its output string.  A segment is a substring of the
 * input pattern, indicated by an offset and limit.  The segment may
 * be in the preceding or following context.  It may not span a
 * context boundary.  A segment reference is a special character in
 * the output string that causes a segment of the input string (not
 * the input pattern) to be copied to the output string.  The range of
 * special characters that represent segment references is defined by
 * RuleBasedTransliterator.Data.
 *
 * @author Alan Liu
 */
class TransliterationRule : public UMemory {

private:

    // TODO Eliminate the pattern and keyLength data members.  They
    // are used only by masks() and getIndexValue() which are called
    // only during build time, not during run-time.  Perhaps these
    // methods and pattern/keyLength can be isolated into a separate
    // object.

    /**
     * The match that must occur before the key, or null if there is no
     * preceding context.
     */
    StringMatcher *anteContext;

    /**
     * The matcher object for the key.  If null, then the key is empty.
     */
    StringMatcher *key;

    /**
     * The match that must occur after the key, or null if there is no
     * following context.
     */
    StringMatcher *postContext;

    /**
     * The object that performs the replacement if the key,
     * anteContext, and postContext are matched.  Never null.
     */
    UnicodeFunctor* output;

    /**
     * The string that must be matched, consisting of the anteContext, key,
     * and postContext, concatenated together, in that order.  Some components
     * may be empty (zero length).
     * @see anteContextLength
     * @see keyLength
     */
    UnicodeString pattern;

    /**
     * An array of matcher objects corresponding to the input pattern
     * segments.  If there are no segments this is null.  N.B. This is
     * a UnicodeMatcher for generality, but in practice it is always a
     * StringMatcher.  In the future we may generalize this, but for
     * now we sometimes cast down to StringMatcher.
     *
     * The array is owned, but the pointers within it are not.
     */
    UnicodeFunctor** segments;

    /**
     * The number of elements in segments[] or zero if segments is NULL.
     */
    int32_t segmentsCount;

    /**
     * The length of the string that must match before the key.  If
     * zero, then there is no matching requirement before the key.
     * Substring [0,anteContextLength) of pattern is the anteContext.
     */
    int32_t anteContextLength;

    /**
     * The length of the key.  Substring [anteContextLength,
     * anteContextLength + keyLength) is the key.

     */
    int32_t keyLength;

    /**
     * Miscellaneous attributes.
     */
    int8_t flags;

    /**
     * Flag attributes.
     */
    enum {
        ANCHOR_START = 1,
        ANCHOR_END   = 2
    };

    /**
     * An alias pointer to the data for this rule.  The data provides
     * lookup services for matchers and segments.
     */
    const TransliterationRuleData* data;

public:

    /**
     * Construct a new rule with the given input, output text, and other
     * attributes.  A cursor position may be specified for the output text.
     * @param input          input string, including key and optional ante and
     *                       post context.
     * @param anteContextPos offset into input to end of ante context, or -1 if
     *                       none.  Must be <= input.length() if not -1.
     * @param postContextPos offset into input to start of post context, or -1
     *                       if none.  Must be <= input.length() if not -1, and must be >=
     *                       anteContextPos.
     * @param outputStr      output string.
     * @param cursorPosition offset into output at which cursor is located, or -1 if
     *                       none.  If less than zero, then the cursor is placed after the
     *                       <code>output</code>; that is, -1 is equivalent to
     *                       <code>output.length()</code>.  If greater than
     *                       <code>output.length()</code> then an exception is thrown.
     * @param cursorOffset   an offset to be added to cursorPos to position the
     *                       cursor either in the ante context, if < 0, or in the post context, if >
     *                       0.  For example, the rule "abc{def} > | @@@ xyz;" changes "def" to
     *                       "xyz" and moves the cursor to before "a".  It would have a cursorOffset
     *                       of -3.
     * @param segs           array of UnicodeMatcher corresponding to input pattern
     *                       segments, or null if there are none.  The array itself is adopted,
     *                       but the pointers within it are not.
     * @param segsCount      number of elements in segs[].
     * @param anchorStart    TRUE if the the rule is anchored on the left to
     *                       the context start.
     * @param anchorEnd      TRUE if the rule is anchored on the right to the
     *                       context limit.
     * @param data           the rule data.
     * @param status         Output parameter filled in with success or failure status.
     */
    TransliterationRule(const UnicodeString& input,
                        int32_t anteContextPos, int32_t postContextPos,
                        const UnicodeString& outputStr,
                        int32_t cursorPosition, int32_t cursorOffset,
                        UnicodeFunctor** segs,
                        int32_t segsCount,
                        UBool anchorStart, UBool anchorEnd,
                        const TransliterationRuleData* data,
                        UErrorCode& status);

    /**
     * Copy constructor.
     * @param other    the object to be copied.
     */
    TransliterationRule(TransliterationRule& other);

    /**
     * Destructor.
     */
    virtual ~TransliterationRule();

    /**
     * Change the data object that this rule belongs to.  Used
     * internally by the TransliterationRuleData copy constructor.
     * @param data    the new data value to be set.
     */
    void setData(const TransliterationRuleData* data);

    /**
     * Return the preceding context length.  This method is needed to
     * support the <code>Transliterator</code> method
     * <code>getMaximumContextLength()</code>.  Internally, this is
     * implemented as the anteContextLength, optionally plus one if
     * there is a start anchor.  The one character anchor gap is
     * needed to make repeated incremental transliteration with
     * anchors work.
     * @return    the preceding context length.
     */
    virtual int32_t getContextLength(void) const;

    /**
     * Internal method.  Returns 8-bit index value for this rule.
     * This is the low byte of the first character of the key,
     * unless the first character of the key is a set.  If it's a
     * set, or otherwise can match multiple keys, the index value is -1.
     * @return    8-bit index value for this rule.
     */
    int16_t getIndexValue() const;

    /**
     * Internal method.  Returns true if this rule matches the given
     * index value.  The index value is an 8-bit integer, 0..255,
     * representing the low byte of the first character of the key.
     * It matches this rule if it matches the first character of the
     * key, or if the first character of the key is a set, and the set
     * contains any character with a low byte equal to the index
     * value.  If the rule contains only ante context, as in foo)>bar,
     * then it will match any key.
     * @param v    the given index value.
     * @return     true if this rule matches the given index value.
     */
    UBool matchesIndexValue(uint8_t v) const;

    /**
     * Return true if this rule masks another rule.  If r1 masks r2 then
     * r1 matches any input string that r2 matches.  If r1 masks r2 and r2 masks
     * r1 then r1 == r2.  Examples: "a>x" masks "ab>y".  "a>x" masks "a[b]>y".
     * "[c]a>x" masks "[dc]a>y".
     * @param r2  the given rule to be compared with.
     * @return    true if this rule masks 'r2'
     */
    virtual UBool masks(const TransliterationRule& r2) const;

    /**
     * Attempt a match and replacement at the given position.  Return
     * the degree of match between this rule and the given text.  The
     * degree of match may be mismatch, a partial match, or a full
     * match.  A mismatch means at least one character of the text
     * does not match the context or key.  A partial match means some
     * context and key characters match, but the text is not long
     * enough to match all of them.  A full match means all context
     * and key characters match.
     * 
     * If a full match is obtained, perform a replacement, update pos,
     * and return U_MATCH.  Otherwise both text and pos are unchanged.
     * 
     * @param text the text
     * @param pos the position indices
     * @param incremental if TRUE, test for partial matches that may
     * be completed by additional text inserted at pos.limit.
     * @return one of <code>U_MISMATCH</code>,
     * <code>U_PARTIAL_MATCH</code>, or <code>U_MATCH</code>.  If
     * incremental is FALSE then U_PARTIAL_MATCH will not be returned.
     */
    UMatchDegree matchAndReplace(Replaceable& text,
                                 UTransPosition& pos,
                                 UBool incremental) const;

    /**
     * Create a rule string that represents this rule object.  Append
     * it to the given string.
     */
    virtual UnicodeString& toRule(UnicodeString& pat,
                                  UBool escapeUnprintable) const;

    /**
     * Union the set of all characters that may be modified by this rule
     * into the given set.
     */
    void addSourceSetTo(UnicodeSet& toUnionTo) const;

    /**
     * Union the set of all characters that may be emitted by this rule
     * into the given set.
     */
    void addTargetSetTo(UnicodeSet& toUnionTo) const;

 private:

    friend class StringMatcher;

    TransliterationRule &operator=(const TransliterationRule &other); // forbid copying of this class
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

#endif

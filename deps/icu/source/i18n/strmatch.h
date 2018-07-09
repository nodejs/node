// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 * Copyright (C) 2001-2011, International Business Machines Corporation
 * and others. All Rights Reserved.
 **********************************************************************
 *   Date        Name        Description
 *   07/23/01    aliu        Creation.
 **********************************************************************
 */
#ifndef STRMATCH_H
#define STRMATCH_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/unistr.h"
#include "unicode/unifunct.h"
#include "unicode/unimatch.h"
#include "unicode/unirepl.h"

U_NAMESPACE_BEGIN

class TransliterationRuleData;

/**
 * An object that matches a fixed input string, implementing the
 * UnicodeMatcher API.  This object also implements the
 * UnicodeReplacer API, allowing it to emit the matched text as
 * output.  Since the match text may contain flexible match elements,
 * such as UnicodeSets, the emitted text is not the match pattern, but
 * instead a substring of the actual matched text.  Following
 * convention, the output text is the leftmost match seen up to this
 * point.
 *
 * A StringMatcher may represent a segment, in which case it has a
 * positive segment number.  This affects how the matcher converts
 * itself to a pattern but does not otherwise affect its function.
 *
 * A StringMatcher that is not a segment should not be used as a
 * UnicodeReplacer.
 */
class StringMatcher : public UnicodeFunctor, public UnicodeMatcher, public UnicodeReplacer {

 public:

    /**
     * Construct a matcher that matches the given pattern string.
     * @param string the pattern to be matched, possibly containing
     * stand-ins that represent nested UnicodeMatcher objects.
     * @param start inclusive start index of text to be replaced
     * @param limit exclusive end index of text to be replaced;
     * must be greater than or equal to start
     * @param segmentNum the segment number from 1..n, or 0 if this is
     * not a segment.
     * @param data context object mapping stand-ins to
     * UnicodeMatcher objects.
     */
    StringMatcher(const UnicodeString& string,
                  int32_t start,
                  int32_t limit,
                  int32_t segmentNum,
                  const TransliterationRuleData& data);

    /**
     * Copy constructor
     * @param o  the object to be copied.
     */
    StringMatcher(const StringMatcher& o);
        
    /**
     * Destructor
     */
    virtual ~StringMatcher();

    /**
     * Implement UnicodeFunctor
     * @return a copy of the object.
     */
    virtual UnicodeFunctor* clone() const;

    /**
     * UnicodeFunctor API.  Cast 'this' to a UnicodeMatcher* pointer
     * and return the pointer.
     * @return the UnicodeMatcher point.
     */
    virtual UnicodeMatcher* toMatcher() const;

    /**
     * UnicodeFunctor API.  Cast 'this' to a UnicodeReplacer* pointer
     * and return the pointer.
     * @return the UnicodeReplacer pointer.
     */
    virtual UnicodeReplacer* toReplacer() const;

    /**
     * Implement UnicodeMatcher
     * @param text the text to be matched
     * @param offset on input, the index into text at which to begin
     * matching.  On output, the limit of the matched text.  The
     * number of matched characters is the output value of offset
     * minus the input value.  Offset should always point to the
     * HIGH SURROGATE (leading code unit) of a pair of surrogates,
     * both on entry and upon return.
     * @param limit the limit index of text to be matched.  Greater
     * than offset for a forward direction match, less than offset for
     * a backward direction match.  The last character to be
     * considered for matching will be text.charAt(limit-1) in the
     * forward direction or text.charAt(limit+1) in the backward
     * direction.
     * @param incremental  if TRUE, then assume further characters may
     * be inserted at limit and check for partial matching.  Otherwise
     * assume the text as given is complete.
     * @return a match degree value indicating a full match, a partial
     * match, or a mismatch.  If incremental is FALSE then
     * U_PARTIAL_MATCH should never be returned.
     */
    virtual UMatchDegree matches(const Replaceable& text,
                                 int32_t& offset,
                                 int32_t limit,
                                 UBool incremental);

    /**
     * Implement UnicodeMatcher
     * @param result            Output param to receive the pattern.
     * @param escapeUnprintable if True then escape the unprintable characters.
     * @return                  A reference to 'result'.
     */
    virtual UnicodeString& toPattern(UnicodeString& result,
                                     UBool escapeUnprintable = FALSE) const;

    /**
     * Implement UnicodeMatcher
     * Returns TRUE if this matcher will match a character c, where c
     * & 0xFF == v, at offset, in the forward direction (with limit >
     * offset).  This is used by <tt>RuleBasedTransliterator</tt> for
     * indexing.
     * @param v    the given value
     * @return     TRUE if this matcher will match a character c, 
     *             where c & 0xFF == v
     */
    virtual UBool matchesIndexValue(uint8_t v) const;

    /**
     * Implement UnicodeMatcher
     */
    virtual void addMatchSetTo(UnicodeSet& toUnionTo) const;

    /**
     * Implement UnicodeFunctor
     */
    virtual void setData(const TransliterationRuleData*);

    /**
     * Replace characters in 'text' from 'start' to 'limit' with the
     * output text of this object.  Update the 'cursor' parameter to
     * give the cursor position and return the length of the
     * replacement text.
     *
     * @param text the text to be matched
     * @param start inclusive start index of text to be replaced
     * @param limit exclusive end index of text to be replaced;
     * must be greater than or equal to start
     * @param cursor output parameter for the cursor position.
     * Not all replacer objects will update this, but in a complete
     * tree of replacer objects, representing the entire output side
     * of a transliteration rule, at least one must update it.
     * @return the number of 16-bit code units in the text replacing
     * the characters at offsets start..(limit-1) in text
     */
    virtual int32_t replace(Replaceable& text,
                            int32_t start,
                            int32_t limit,
                            int32_t& cursor);

    /**
     * Returns a string representation of this replacer.  If the
     * result of calling this function is passed to the appropriate
     * parser, typically TransliteratorParser, it will produce another
     * replacer that is equal to this one.
     * @param result the string to receive the pattern.  Previous
     * contents will be deleted.
     * @param escapeUnprintable if TRUE then convert unprintable
     * character to their hex escape representations, \\uxxxx or
     * \\Uxxxxxxxx.  Unprintable characters are defined by
     * Utility.isUnprintable().
     * @return a reference to 'result'.
     */
    virtual UnicodeString& toReplacerPattern(UnicodeString& result,
                                             UBool escapeUnprintable) const;

    /**
     * Remove any match data.  This must be called before performing a
     * set of matches with this segment.
     */
    void resetMatch();

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     */
    virtual UClassID getDynamicClassID() const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     */
    static UClassID U_EXPORT2 getStaticClassID();

    /**
     * Union the set of all characters that may output by this object
     * into the given set.
     * @param toUnionTo the set into which to union the output characters
     */
    virtual void addReplacementSetTo(UnicodeSet& toUnionTo) const;

 private:

    /**
     * The text to be matched.
     */
    UnicodeString pattern;

    /**
     * Context object that maps stand-ins to matcher and replacer
     * objects.
     */
    const TransliterationRuleData* data;

    /**
     * The segment number, 1-based, or 0 if not a segment.
     */
    int32_t segmentNumber;

    /**
     * Start offset, in the match text, of the <em>rightmost</em>
     * match.
     */
    int32_t matchStart;

    /**
     * Limit offset, in the match text, of the <em>rightmost</em>
     * match.
     */
    int32_t matchLimit;

};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

#endif

// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 * Copyright (C) 2001-2011, International Business Machines Corporation
 * and others. All Rights Reserved.
 **********************************************************************
 *   Date        Name        Description
 *   07/26/01    aliu        Creation.
 **********************************************************************
 */
#ifndef QUANT_H
#define QUANT_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/unifunct.h"
#include "unicode/unimatch.h"

U_NAMESPACE_BEGIN

class Quantifier : public UnicodeFunctor, public UnicodeMatcher {

 public:

    enum { MAX = 0x7FFFFFFF };

    Quantifier(UnicodeFunctor *adoptedMatcher,
               uint32_t minCount, uint32_t maxCount);

    Quantifier(const Quantifier& o);

    virtual ~Quantifier();

    /**
     * UnicodeFunctor API.  Cast 'this' to a UnicodeMatcher* pointer
     * and return the pointer.
     * @return the UnicodeMatcher pointer.
     */
    virtual UnicodeMatcher* toMatcher() const;

    /**
     * Implement UnicodeFunctor
     * @return a copy of the object.
     */
    virtual Quantifier* clone() const;

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
     * @param v    the given index value.
     * @return     true if this rule matches the given index value.
     */
    virtual UBool matchesIndexValue(uint8_t v) const;

    /**
     * Implement UnicodeMatcher
     */
    virtual void addMatchSetTo(UnicodeSet& toUnionTo) const;

    /**
     * UnicodeFunctor API
     */
    virtual void setData(const TransliterationRuleData*);

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     */
    virtual UClassID getDynamicClassID() const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     */
    static UClassID U_EXPORT2 getStaticClassID();

 private:

    UnicodeFunctor* matcher; // owned

    uint32_t minCount;

    uint32_t maxCount;
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

#endif

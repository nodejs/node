// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (c) 2002-2012, International Business Machines Corporation
*   and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   02/04/2002  aliu        Creation.
**********************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/translit.h"
#include "unicode/uniset.h"
#include "funcrepl.h"

static const UChar AMPERSAND = 38; // '&'
static const UChar OPEN[]    = {40,32,0}; // "( "
static const UChar CLOSE[]   = {32,41,0}; // " )"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(FunctionReplacer)

/**
 * Construct a replacer that takes the output of the given
 * replacer, passes it through the given transliterator, and emits
 * the result as output.
 */
FunctionReplacer::FunctionReplacer(Transliterator* adoptedTranslit,
                                   UnicodeFunctor* adoptedReplacer) {
    translit = adoptedTranslit;
    replacer = adoptedReplacer;
}

/**
 * Copy constructor.
 */
FunctionReplacer::FunctionReplacer(const FunctionReplacer& other) :
    UnicodeFunctor(other),
    UnicodeReplacer(other)
{
    translit = other.translit->clone();
    replacer = other.replacer->clone();
}

/**
 * Destructor
 */
FunctionReplacer::~FunctionReplacer() {
    delete translit;
    delete replacer;
}

/**
 * Implement UnicodeFunctor
 */
FunctionReplacer* FunctionReplacer::clone() const {
    return new FunctionReplacer(*this);
}

/**
 * UnicodeFunctor API.  Cast 'this' to a UnicodeReplacer* pointer
 * and return the pointer.
 */
UnicodeReplacer* FunctionReplacer::toReplacer() const {
  FunctionReplacer  *nonconst_this = const_cast<FunctionReplacer *>(this);
  UnicodeReplacer *nonconst_base = static_cast<UnicodeReplacer *>(nonconst_this);

  return nonconst_base;
}

/**
 * UnicodeReplacer API
 */
int32_t FunctionReplacer::replace(Replaceable& text,
                                  int32_t start,
                                  int32_t limit,
                                  int32_t& cursor)
{

    // First delegate to subordinate replacer
    int32_t len = replacer->toReplacer()->replace(text, start, limit, cursor);
    limit = start + len;

    // Now transliterate
    limit = translit->transliterate(text, start, limit);

    return limit - start;
}

/**
 * UnicodeReplacer API
 */
UnicodeString& FunctionReplacer::toReplacerPattern(UnicodeString& rule,
                                                   UBool escapeUnprintable) const {
    UnicodeString str;
    rule.truncate(0);
    rule.append(AMPERSAND);
    rule.append(translit->getID());
    rule.append(OPEN, 2);
    rule.append(replacer->toReplacer()->toReplacerPattern(str, escapeUnprintable));
    rule.append(CLOSE, 2);
    return rule;
}

/**
 * Implement UnicodeReplacer
 */
void FunctionReplacer::addReplacementSetTo(UnicodeSet& toUnionTo) const {
    UnicodeSet set;
    toUnionTo.addAll(translit->getTargetSet(set));
}

/**
 * UnicodeFunctor API
 */
void FunctionReplacer::setData(const TransliterationRuleData* d) {
    replacer->setData(d);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

//eof

/*
**********************************************************************
*   Copyright (c) 2002-2011, International Business Machines Corporation
*   and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   02/04/2002  aliu        Creation.
**********************************************************************
*/

#ifndef FUNCREPL_H
#define FUNCREPL_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/unifunct.h"
#include "unicode/unirepl.h"

U_NAMESPACE_BEGIN

class Transliterator;

/**
 * A replacer that calls a transliterator to generate its output text.
 * The input text to the transliterator is the output of another
 * UnicodeReplacer object.  That is, this replacer wraps another
 * replacer with a transliterator.
 *
 * @author Alan Liu
 */
class FunctionReplacer : public UnicodeFunctor, public UnicodeReplacer {

 private:

    /**
     * The transliterator.  Must not be null.  OWNED.
     */
    Transliterator* translit;

    /**
     * The replacer object.  This generates text that is then
     * processed by 'translit'.  Must not be null.  OWNED.
     */
    UnicodeFunctor* replacer;

 public:

    /**
     * Construct a replacer that takes the output of the given
     * replacer, passes it through the given transliterator, and emits
     * the result as output.
     */
    FunctionReplacer(Transliterator* adoptedTranslit,
                     UnicodeFunctor* adoptedReplacer);

    /**
     * Copy constructor.
     */
    FunctionReplacer(const FunctionReplacer& other);

    /**
     * Destructor
     */
    virtual ~FunctionReplacer();

    /**
     * Implement UnicodeFunctor
     */
    virtual UnicodeFunctor* clone() const;

    /**
     * UnicodeFunctor API.  Cast 'this' to a UnicodeReplacer* pointer
     * and return the pointer.
     */
    virtual UnicodeReplacer* toReplacer() const;

    /**
     * UnicodeReplacer API
     */
    virtual int32_t replace(Replaceable& text,
                            int32_t start,
                            int32_t limit,
                            int32_t& cursor);

    /**
     * UnicodeReplacer API
     */
    virtual UnicodeString& toReplacerPattern(UnicodeString& rule,
                                             UBool escapeUnprintable) const;

    /**
     * Implement UnicodeReplacer
     */
    virtual void addReplacementSetTo(UnicodeSet& toUnionTo) const;

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
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */
#endif

//eof

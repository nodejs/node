// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (c) 2002-2011, International Business Machines Corporation
*   and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   01/21/2002  aliu        Creation.
**********************************************************************
*/

#ifndef STRREPL_H
#define STRREPL_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/unifunct.h"
#include "unicode/unirepl.h"
#include "unicode/unistr.h"

U_NAMESPACE_BEGIN

class TransliterationRuleData;

/**
 * A replacer that produces static text as its output.  The text may
 * contain transliterator stand-in characters that represent nested
 * UnicodeReplacer objects, making it possible to encode a tree of
 * replacers in a StringReplacer.  A StringReplacer that contains such
 * stand-ins is called a <em>complex</em> StringReplacer.  A complex
 * StringReplacer has a slower processing loop than a non-complex one.
 * @author Alan Liu
 */
class StringReplacer : public UnicodeFunctor, public UnicodeReplacer {

 private:

    /**
     * Output text, possibly containing stand-in characters that
     * represent nested UnicodeReplacers.
     */
    UnicodeString output;

    /**
     * Cursor position.  Value is ignored if hasCursor is false.
     */
    int32_t cursorPos;

    /**
     * True if this object outputs a cursor position.
     */
    UBool hasCursor;

    /**
     * A complex object contains nested replacers and requires more
     * complex processing.  StringReplacers are initially assumed to
     * be complex.  If no nested replacers are seen during processing,
     * then isComplex is set to false, and future replacements are
     * short circuited for better performance.
     */
    UBool isComplex;

    /**
     * Object that translates stand-in characters in 'output' to
     * UnicodeReplacer objects.
     */
    const TransliterationRuleData* data;

 public:

    /**
     * Construct a StringReplacer that sets the emits the given output
     * text and sets the cursor to the given position.
     * @param theOutput text that will replace input text when the
     * replace() method is called.  May contain stand-in characters
     * that represent nested replacers.
     * @param theCursorPos cursor position that will be returned by
     * the replace() method
     * @param theData transliterator context object that translates
     * stand-in characters to UnicodeReplacer objects
     */
    StringReplacer(const UnicodeString& theOutput,
                   int32_t theCursorPos,
                   const TransliterationRuleData* theData);

    /**
     * Construct a StringReplacer that sets the emits the given output
     * text and does not modify the cursor.
     * @param theOutput text that will replace input text when the
     * replace() method is called.  May contain stand-in characters
     * that represent nested replacers.
     * @param theData transliterator context object that translates
     * stand-in characters to UnicodeReplacer objects
     */
    StringReplacer(const UnicodeString& theOutput,
                   const TransliterationRuleData* theData);

    /**
     * Copy constructor.
     */
    StringReplacer(const StringReplacer& other);

    /**
     * Destructor
     */
    virtual ~StringReplacer();

    /**
     * Implement UnicodeFunctor
     */
    virtual StringReplacer* clone() const;

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
    virtual UnicodeString& toReplacerPattern(UnicodeString& result,
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
     * ICU "poor man's RTTI", returns a UClassID for this class.
     */
    static UClassID U_EXPORT2 getStaticClassID();

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     */
    virtual UClassID getDynamicClassID() const;
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

#endif

//eof

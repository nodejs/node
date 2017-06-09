// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2001-2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   05/24/01    aliu        Creation.
**********************************************************************
*/
#ifndef TITLETRN_H
#define TITLETRN_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/translit.h"
#include "ucase.h"
#include "casetrn.h"

U_NAMESPACE_BEGIN

/**
 * A transliterator that converts all letters (as defined by
 * <code>UCharacter.isLetter()</code>) to lower case, except for those
 * letters preceded by non-letters.  The latter are converted to title
 * case using <code>u_totitle()</code>.
 * @author Alan Liu
 */
class TitlecaseTransliterator : public CaseMapTransliterator {
 public:

    /**
     * Constructs a transliterator.
     * @param loc the given locale.
     */
    TitlecaseTransliterator();

    /**
     * Destructor.
     */
    virtual ~TitlecaseTransliterator();

    /**
     * Copy constructor.
     */
    TitlecaseTransliterator(const TitlecaseTransliterator&);

    /**
     * Transliterator API.
     * @return a copy of the object.
     */
    virtual Transliterator* clone(void) const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     */
    virtual UClassID getDynamicClassID() const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     */
    U_I18N_API static UClassID U_EXPORT2 getStaticClassID();

protected:

    /**
     * Implements {@link Transliterator#handleTransliterate}.
     * @param text        the buffer holding transliterated and
     *                    untransliterated text
     * @param offset      the start and limit of the text, the position
     *                    of the cursor, and the start and limit of transliteration.
     * @param incremental if true, assume more text may be coming after
     *                    pos.contextLimit.  Otherwise, assume the text is complete.
     */
    virtual void handleTransliterate(Replaceable& text, UTransPosition& offset,
                             UBool isIncremental) const;

private:
    /**
     * Assignment operator.
     */
    TitlecaseTransliterator& operator=(const TitlecaseTransliterator&);
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

#endif

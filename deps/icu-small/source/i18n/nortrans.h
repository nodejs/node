// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2001-2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   07/03/01    aliu        Creation.
**********************************************************************
*/
#ifndef NORTRANS_H
#define NORTRANS_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/translit.h"
#include "unicode/normalizer2.h"

U_NAMESPACE_BEGIN

/**
 * A transliterator that performs normalization.
 * @author Alan Liu
 */
class NormalizationTransliterator : public Transliterator {
    const Normalizer2 &fNorm2;

 public:

    /**
     * Destructor.
     */
    virtual ~NormalizationTransliterator();

    /**
     * Copy constructor.
     */
    NormalizationTransliterator(const NormalizationTransliterator&);

    /**
     * Transliterator API.
     * @return    A copy of the object.
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
     * @param text          the buffer holding transliterated and
     *                      untransliterated text
     * @param offset        the start and limit of the text, the position
     *                      of the cursor, and the start and limit of transliteration.
     * @param incremental   if true, assume more text may be coming after
     *                      pos.contextLimit. Otherwise, assume the text is complete.
     */
    virtual void handleTransliterate(Replaceable& text, UTransPosition& offset,
                             UBool isIncremental) const;
 public:

    /**
     * System registration hook.  Public to Transliterator only.
     */
    static void registerIDs();

 private:

    // Transliterator::Factory methods
    static Transliterator* _create(const UnicodeString& ID,
                                   Token context);

    /**
     * Constructs a transliterator.  This method is private.
     * Public users must use the factory method createInstance().
     */
    NormalizationTransliterator(const UnicodeString& id, const Normalizer2 &norm2);

private:
    /**
     * Assignment operator.
     */
    NormalizationTransliterator& operator=(const NormalizationTransliterator&);
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

#endif

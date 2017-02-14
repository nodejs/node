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
#ifndef TOLOWTRN_H
#define TOLOWTRN_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/translit.h"
#include "casetrn.h"

U_NAMESPACE_BEGIN

/**
 * A transliterator that performs locale-sensitive toLower()
 * case mapping.
 * @author Alan Liu
 */
class LowercaseTransliterator : public CaseMapTransliterator {

 public:

    /**
     * Constructs a transliterator.
     * @param loc the given locale.
     */
    LowercaseTransliterator();

    /**
     * Destructor.
     */
    virtual ~LowercaseTransliterator();

    /**
     * Copy constructor.
     */
    LowercaseTransliterator(const LowercaseTransliterator&);

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
private:

    /**
     * Assignment operator.
     */
    LowercaseTransliterator& operator=(const LowercaseTransliterator&);
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

#endif

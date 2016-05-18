/*
**********************************************************************
*   Copyright (C) 2001-2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   05/24/01    aliu        Creation.
**********************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include "unicode/uchar.h"
#include "unicode/ustring.h"
#include "tolowtrn.h"
#include "ustr_imp.h"
#include "cpputils.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(LowercaseTransliterator)

/**
 * Constructs a transliterator.
 */
LowercaseTransliterator::LowercaseTransliterator() :
    CaseMapTransliterator(UNICODE_STRING("Any-Lower", 9), ucase_toFullLower)
{
}

/**
 * Destructor.
 */
LowercaseTransliterator::~LowercaseTransliterator() {
}

/**
 * Copy constructor.
 */
LowercaseTransliterator::LowercaseTransliterator(const LowercaseTransliterator& o) :
    CaseMapTransliterator(o)
{
}

/**
 * Assignment operator.
 */
/*LowercaseTransliterator& LowercaseTransliterator::operator=(
                             const LowercaseTransliterator& o) {
    CaseMapTransliterator::operator=(o);
    return *this;
}*/

/**
 * Transliterator API.
 */
Transliterator* LowercaseTransliterator::clone(void) const {
    return new LowercaseTransliterator(*this);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_TRANSLITERATION */

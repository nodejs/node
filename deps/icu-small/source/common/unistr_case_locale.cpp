// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  unistr_case_locale.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011may31
*   created by: Markus W. Scherer
*
*   Locale-sensitive case mapping functions (ones that call uloc_getDefault())
*   were moved here to break dependency cycles among parts of the common library.
*/

#include "unicode/utypes.h"
#include "unicode/locid.h"
#include "unicode/unistr.h"
#include "cmemory.h"
#include "ustr_imp.h"

U_NAMESPACE_BEGIN

//========================================
// Write implementation
//========================================

/*
 * Set parameters on an empty UCaseMap, for UCaseMap-less API functions.
 * Do this fast because it is called with every function call.
 */
static inline void
setTempCaseMap(UCaseMap *csm, const char *locale) {
    if(csm->csp==NULL) {
        csm->csp=ucase_getSingleton();
    }
    if(locale!=NULL && locale[0]==0) {
        csm->locale[0]=0;
    } else {
        ustrcase_setTempCaseMapLocale(csm, locale);
    }
}

UnicodeString &
UnicodeString::toLower() {
  return toLower(Locale::getDefault());
}

UnicodeString &
UnicodeString::toLower(const Locale &locale) {
  UCaseMap csm=UCASEMAP_INITIALIZER;
  setTempCaseMap(&csm, locale.getName());
  return caseMap(&csm, ustrcase_internalToLower);
}

UnicodeString &
UnicodeString::toUpper() {
  return toUpper(Locale::getDefault());
}

UnicodeString &
UnicodeString::toUpper(const Locale &locale) {
  UCaseMap csm=UCASEMAP_INITIALIZER;
  setTempCaseMap(&csm, locale.getName());
  return caseMap(&csm, ustrcase_internalToUpper);
}

U_NAMESPACE_END

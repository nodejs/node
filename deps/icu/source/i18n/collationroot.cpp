// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2012-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationroot.cpp
*
* created on: 2012dec17
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/coll.h"
#include "unicode/udata.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatareader.h"
#include "collationroot.h"
#include "collationsettings.h"
#include "collationtailoring.h"
#include "normalizer2impl.h"
#include "ucln_in.h"
#include "udatamem.h"
#include "umutex.h"

U_NAMESPACE_BEGIN

namespace {

static const CollationCacheEntry *rootSingleton = NULL;
static UInitOnce initOnce = U_INITONCE_INITIALIZER;

}  // namespace

U_CDECL_BEGIN

static UBool U_CALLCONV uprv_collation_root_cleanup() {
    SharedObject::clearPtr(rootSingleton);
    initOnce.reset();
    return TRUE;
}

U_CDECL_END

void U_CALLCONV
CollationRoot::load(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    LocalPointer<CollationTailoring> t(new CollationTailoring(NULL));
    if(t.isNull() || t->isBogus()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    t->memory = udata_openChoice(U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "coll",
                                 "icu", "ucadata",
                                 CollationDataReader::isAcceptable, t->version, &errorCode);
    if(U_FAILURE(errorCode)) { return; }
    const uint8_t *inBytes = static_cast<const uint8_t *>(udata_getMemory(t->memory));
    CollationDataReader::read(NULL, inBytes, udata_getLength(t->memory), *t, errorCode);
    if(U_FAILURE(errorCode)) { return; }
    ucln_i18n_registerCleanup(UCLN_I18N_COLLATION_ROOT, uprv_collation_root_cleanup);
    CollationCacheEntry *entry = new CollationCacheEntry(Locale::getRoot(), t.getAlias());
    if(entry != NULL) {
        t.orphan();  // The rootSingleton took ownership of the tailoring.
        entry->addRef();
        rootSingleton = entry;
    }
}

const CollationCacheEntry *
CollationRoot::getRootCacheEntry(UErrorCode &errorCode) {
    umtx_initOnce(initOnce, CollationRoot::load, errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    return rootSingleton;
}

const CollationTailoring *
CollationRoot::getRoot(UErrorCode &errorCode) {
    umtx_initOnce(initOnce, CollationRoot::load, errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    return rootSingleton->tailoring;
}

const CollationData *
CollationRoot::getData(UErrorCode &errorCode) {
    const CollationTailoring *root = getRoot(errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    return root->data;
}

const CollationSettings *
CollationRoot::getSettings(UErrorCode &errorCode) {
    const CollationTailoring *root = getRoot(errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    return root->settings;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION

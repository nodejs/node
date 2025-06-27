// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 ***************************************************************************
 * Copyright (C) 2008-2015, International Business Machines Corporation
 * and others. All Rights Reserved.
 ***************************************************************************
 *   file name:  uspoof_build.cpp
 *   encoding:   UTF-8
 *   tab size:   8 (not used)
 *   indentation:4
 *
 *   created on: 2008 Dec 8
 *   created by: Andy Heninger
 *
 *   Unicode Spoof Detection Data Builder
 *   Builder-related functions are kept in separate files so that applications not needing
 *   the builder can more easily exclude them, typically by means of static linking.
 *
 *   There are three relatively independent sets of Spoof data,
 *      Confusables,
 *      Whole Script Confusables
 *      ID character extensions.
 *
 *   The data tables for each are built separately, each from its own definitions
 */

#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "unicode/unorm.h"
#include "unicode/uregex.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "uspoof_impl.h"
#include "uhash.h"
#include "uvector.h"
#include "uassert.h"
#include "uarrsort.h"
#include "uspoof_conf.h"

#if !UCONFIG_NO_NORMALIZATION

U_NAMESPACE_USE

// Defined in uspoof.cpp, initializes file-static variables.
U_CFUNC void uspoof_internalInitStatics(UErrorCode *status);

// The main data building function

U_CAPI USpoofChecker * U_EXPORT2
uspoof_openFromSource(const char *confusables,  int32_t confusablesLen,
                      const char* /*confusablesWholeScript*/, int32_t /*confusablesWholeScriptLen*/,
                      int32_t *errorType, UParseError *pe, UErrorCode *status) {
    uspoof_internalInitStatics(status);
    if (U_FAILURE(*status)) {
        return nullptr;
    }
#if UCONFIG_NO_REGULAR_EXPRESSIONS 
    *status = U_UNSUPPORTED_ERROR;      
    return nullptr;
#else
    if (errorType!=nullptr) {
        *errorType = 0;
    }
    if (pe != nullptr) {
        pe->line = 0;
        pe->offset = 0;
        pe->preContext[0] = 0;
        pe->postContext[0] = 0;
    }

    // Set up a shell of a spoof detector, with empty data.
    SpoofData *newSpoofData = new SpoofData(*status);

    if (newSpoofData == nullptr) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }

    if (U_FAILURE(*status)) {
        delete newSpoofData;
        return nullptr;
    }
    SpoofImpl *This = new SpoofImpl(newSpoofData, *status);

    if (This == nullptr) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        delete newSpoofData; // explicit delete as the destructor for SpoofImpl won't be called.
        return nullptr;
    }

    if (U_FAILURE(*status)) {
        delete This; // no delete for newSpoofData, as the SpoofImpl destructor will delete it.
        return nullptr;
    }

    // Compile the binary data from the source (text) format.
    ConfusabledataBuilder::buildConfusableData(This, confusables, confusablesLen, errorType, pe, *status);
    
    if (U_FAILURE(*status)) {
        delete This;
        This = nullptr;
    }
    return (USpoofChecker *)This;
#endif // UCONFIG_NO_REGULAR_EXPRESSIONS 
}

#endif

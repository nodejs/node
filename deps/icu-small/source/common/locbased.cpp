// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (c) 2004-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: January 16 2004
* Since: ICU 2.8
**********************************************************************
*/
#include "locbased.h"
#include "cstring.h"
#include "charstr.h"

U_NAMESPACE_BEGIN

Locale LocaleBased::getLocale(const CharString* valid, const CharString* actual,
                              ULocDataLocaleType type, UErrorCode& status) {
    const char* id = getLocaleID(valid, actual, type, status);
    return Locale(id != nullptr ? id : "");
}

const char* LocaleBased::getLocaleID(const CharString* valid, const CharString* actual,
                                     ULocDataLocaleType type, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return nullptr;
    }

    switch(type) {
    case ULOC_VALID_LOCALE:
        return valid == nullptr ? "" : valid->data();
    case ULOC_ACTUAL_LOCALE:
        return actual == nullptr ? "" : actual->data();
    default:
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return nullptr;
    }
}

void LocaleBased::setLocaleIDs(const CharString* validID, const CharString* actualID, UErrorCode& status) {
    setValidLocaleID(validID, status);
    setActualLocaleID(actualID,status);
}
void LocaleBased::setLocaleIDs(const char* validID, const char* actualID, UErrorCode& status) {
    setValidLocaleID(validID, status);
    setActualLocaleID(actualID,status);
}

void LocaleBased::setLocaleID(const char* id, CharString*& dest, UErrorCode& status) {
    if (U_FAILURE(status)) { return; }
    if (id == nullptr || *id == 0) {
        delete dest;
        dest = nullptr;
    } else {
        if (dest == nullptr) {
            dest = new CharString(id, status);
            if (dest == nullptr) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
        } else {
            dest->copyFrom(id, status);
        }
    }
}

void LocaleBased::setLocaleID(const CharString* id, CharString*& dest, UErrorCode& status) {
    if (U_FAILURE(status)) { return; }
    if (id == nullptr || id->isEmpty()) {
        delete dest;
        dest = nullptr;
    } else {
        if (dest == nullptr) {
            dest = new CharString(*id, status);
            if (dest == nullptr) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
        } else {
            dest->copyFrom(*id, status);
        }
    }
}

bool LocaleBased::equalIDs(const CharString* left, const CharString* right) {
    // true if both are nullptr
    if (left == nullptr && right == nullptr) return true;
    // false if only one is nullptr
    if (left == nullptr || right == nullptr) return false;
    return *left == *right;
}

U_NAMESPACE_END

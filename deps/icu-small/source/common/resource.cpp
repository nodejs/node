/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* resource.cpp
*
* created on: 2015nov04
* created by: Markus W. Scherer
*/

#include "resource.h"

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/ures.h"

U_NAMESPACE_BEGIN

ResourceValue::~ResourceValue() {}


ResourceArraySink::~ResourceArraySink() {}

void ResourceArraySink::put(
        int32_t /*index*/, const ResourceValue & /*value*/, UErrorCode & /*errorCode*/) {}

ResourceArraySink *ResourceArraySink::getOrCreateArraySink(
        int32_t /*index*/, int32_t /*size*/, UErrorCode & /*errorCode*/) {
    return NULL;
}

ResourceTableSink *ResourceArraySink::getOrCreateTableSink(
        int32_t /*index*/, int32_t /*initialSize*/, UErrorCode & /*errorCode*/) {
    return NULL;
}

void ResourceArraySink::leave(UErrorCode & /*errorCode*/) {}


ResourceTableSink::~ResourceTableSink() {}

void ResourceTableSink::put(
        const char * /*key*/, const ResourceValue & /*value*/, UErrorCode & /*errorCode*/) {}

void ResourceTableSink::putNoFallback(const char * /*key*/, UErrorCode & /*errorCode*/) {}

ResourceArraySink *ResourceTableSink::getOrCreateArraySink(
        const char * /*key*/, int32_t /*size*/, UErrorCode & /*errorCode*/) {
    return NULL;
}

ResourceTableSink *ResourceTableSink::getOrCreateTableSink(
        const char * /*key*/, int32_t /*initialSize*/, UErrorCode & /*errorCode*/) {
    return NULL;
}

void ResourceTableSink::leave(UErrorCode & /*errorCode*/) {}

U_NAMESPACE_END

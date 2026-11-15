// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (c) 2002-2004, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "unicode/unifunct.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_ABSTRACT_RTTI_IMPLEMENTATION(UnicodeFunctor)

UnicodeFunctor::~UnicodeFunctor() {}

UnicodeMatcher* UnicodeFunctor::toMatcher() const U_LIFETIME_BOUND {
    return nullptr;
}

UnicodeReplacer* UnicodeFunctor::toReplacer() const U_LIFETIME_BOUND {
    return nullptr;
}

U_NAMESPACE_END

//eof

// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1997-2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  resbund_cnv.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2004aug25
*   created by: Markus W. Scherer
*
*   Character conversion functions moved here from resbund.cpp
*/

#include "unicode/utypes.h"
#include "unicode/resbund.h"
#include "uinvchar.h"

U_NAMESPACE_BEGIN

ResourceBundle::ResourceBundle( const UnicodeString&    path,
                                const Locale&           locale,
                                UErrorCode&              error)
                                :UObject(), fLocale(NULL)
{
    constructForLocale(path, locale, error);
}

ResourceBundle::ResourceBundle( const UnicodeString&    path,
                                UErrorCode&              error)
                                :UObject(), fLocale(NULL)
{
    constructForLocale(path, Locale::getDefault(), error);
}

void 
ResourceBundle::constructForLocale(const UnicodeString& path,
                                   const Locale& locale,
                                   UErrorCode& error)
{
    if (path.isEmpty()) {
        fResource = ures_open(NULL, locale.getName(), &error);
    }
    else {
        UnicodeString nullTerminatedPath(path);
        nullTerminatedPath.append((UChar)0);
        fResource = ures_openU(nullTerminatedPath.getBuffer(), locale.getName(), &error);
    }
}

U_NAMESPACE_END

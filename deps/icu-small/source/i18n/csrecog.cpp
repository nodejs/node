// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 *   Copyright (C) 2005-2006, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "csrecog.h"

U_NAMESPACE_BEGIN

CharsetRecognizer::~CharsetRecognizer()
{
    // nothing to do.
}

const char *CharsetRecognizer::getLanguage() const
{
    return "";
}

U_NAMESPACE_END    

#endif

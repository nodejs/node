// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 *   Copyright (C) 2005-2012, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#ifndef __CSRUTF8_H
#define __CSRUTF8_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "csrecog.h"

U_NAMESPACE_BEGIN

/**
 * Charset recognizer for UTF-8
 *
 * @internal
 */
class CharsetRecog_UTF8: public CharsetRecognizer {

 public:

    virtual ~CharsetRecog_UTF8();

    const char *getName() const;

    /* (non-Javadoc)
     * @see com.ibm.icu.text.CharsetRecognizer#match(com.ibm.icu.text.CharsetDetector)
     */
    UBool match(InputText *input, CharsetMatch *results) const;

};

U_NAMESPACE_END

#endif
#endif /* __CSRUTF8_H */

// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 *   Copyright (C) 2005-2012, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#ifndef __CSRECOG_H
#define __CSRECOG_H

#include "unicode/uobject.h"

#if !UCONFIG_NO_CONVERSION

#include "inputext.h"

U_NAMESPACE_BEGIN

class CharsetMatch;

class CharsetRecognizer : public UMemory
{
 public:
    /**
     * Get the IANA name of this charset.
     * Note that some recognizers can recognize more than one charset, but that this API
     * assumes just one name per recognizer.
     * TODO: need to account for multiple names in public API that enumerates over the
     *       known detectable charsets.
     * @return the charset name.
     */
    virtual const char *getName() const = 0;
    
    /**
     * Get the ISO language code for this charset.
     * @return the language code, or <code>null</code> if the language cannot be determined.
     */
    virtual const char *getLanguage() const;
        
    /*
     * Try the given input text against this Charset, and fill in the results object
     * with the quality of the match plus other information related to the match.
     *
     * Return true if the input bytes are a potential match, and
     * false if the input data is not compatible with, or illegal in this charset.
     */
    virtual UBool match(InputText *textIn, CharsetMatch *results) const = 0;

    virtual ~CharsetRecognizer();
};

U_NAMESPACE_END

#endif
#endif /* __CSRECOG_H */

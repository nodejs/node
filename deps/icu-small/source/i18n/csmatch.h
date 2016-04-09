/*
 **********************************************************************
 *   Copyright (C) 2005-2012, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#ifndef __CSMATCH_H
#define __CSMATCH_H

#include "unicode/uobject.h"

#if !UCONFIG_NO_CONVERSION

U_NAMESPACE_BEGIN

class InputText;
class CharsetRecognizer;

/*
 * CharsetMatch represents the results produced by one Charset Recognizer for one input text
 *              Any confidence > 0 indicates a possible match, meaning that the input bytes
 *              are at least legal.
 *
 *              The full results of a detect are represented by an array of these
 *              CharsetMatch objects, each representing a possible matching charset.
 *
 *              Note that a single charset recognizer may detect multiple closely related
 *              charsets, and set different names depending on the exact input bytes seen.
 */
class CharsetMatch : public UMemory
{
 private:
    InputText               *textIn;
    int32_t                  confidence;
    const char              *fCharsetName;
    const char              *fLang;

 public:
    CharsetMatch();

    /**
      * fully set the state of this CharsetMatch.
      * Called by the CharsetRecognizers to record match results.
      * Default (NULL) parameters for names will be filled by calling the
      *   corresponding getters on the recognizer.
      */
    void set(InputText               *input,
             const CharsetRecognizer *cr,
             int32_t                  conf,
             const char              *csName=NULL,
             const char              *lang=NULL);

    /**
      * Return the name of the charset for this Match
      */
    const char *getName() const;

    const char *getLanguage()const;

    int32_t getConfidence()const;

    int32_t getUChars(UChar *buf, int32_t cap, UErrorCode *status) const;
};

U_NAMESPACE_END

#endif
#endif /* __CSMATCH_H */

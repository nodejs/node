// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
********************************************************************************
*   Copyright (C) 2012-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************/

#ifndef DCFMTIMP_H
#define DCFMTIMP_H

#include "unicode/utypes.h"


#if UCONFIG_FORMAT_FASTPATHS_49

U_NAMESPACE_BEGIN

enum EDecimalFormatFastpathStatus {
  kFastpathNO = 0,
  kFastpathYES = 1,
  kFastpathUNKNOWN = 2, /* not yet set */
  kFastpathMAYBE = 3 /* depends on value being formatted. */
};

/**
 * Must be smaller than DecimalFormat::fReserved
 */
struct DecimalFormatInternal {
  uint8_t    fFastFormatStatus;
  uint8_t    fFastParseStatus;

  DecimalFormatInternal &operator=(const DecimalFormatInternal& rhs) {
    fFastParseStatus = rhs.fFastParseStatus;
    fFastFormatStatus = rhs.fFastFormatStatus;
    return *this;
  }
#ifdef FMT_DEBUG
  void dump() const {
    printf("DecimalFormatInternal: fFastFormatStatus=%c, fFastParseStatus=%c\n",
           "NY?"[(int)fFastFormatStatus&3],
           "NY?"[(int)fFastParseStatus&3]
           );
  }
#endif
};



U_NAMESPACE_END

#endif

#endif

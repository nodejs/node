// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2000-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  ucol_data.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011jul02
*   created by: Markus Scherer
*
* Private implementation header for C/C++ collation.
* Some file data structure definitions were moved here from i18n/ucol_imp.h
* so that the common library (via ucol_swp.cpp) need not depend on the i18n library at all.
*
* We do not want to move the collation swapper to the i18n library because
* a) the resource bundle swapper depends on it and would have to move too, and
* b) we might want to eventually implement runtime data swapping,
*    which might (or might not) be easier if all swappers are in the common library.
*/

#ifndef __UCOL_DATA_H__
#define __UCOL_DATA_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

/* let us know whether reserved fields are reset to zero or junked */
#define UCOL_HEADER_MAGIC 0x20030618

typedef struct {
      int32_t size;
      /* all the offsets are in bytes */
      /* to get the address add to the header address and cast properly */
      uint32_t options; /* these are the default options for the collator */
      uint32_t UCAConsts; /* structure which holds values for indirect positioning and implicit ranges */
      uint32_t contractionUCACombos;        /* this one is needed only for UCA, to copy the appropriate contractions */
      uint32_t magic;            /* magic number - lets us know whether reserved data is reset or junked */
      uint32_t mappingPosition;  /* const uint8_t *mappingPosition; */
      uint32_t expansion;        /* uint32_t *expansion;            */
      uint32_t contractionIndex; /* UChar *contractionIndex;        */
      uint32_t contractionCEs;   /* uint32_t *contractionCEs;       */
      uint32_t contractionSize;  /* needed for various closures */
      /*int32_t latinOneMapping;*/ /* this is now handled in the trie itself *//* fast track to latin1 chars      */

      uint32_t endExpansionCE;      /* array of last collation element in
                                       expansion */
      uint32_t expansionCESize;     /* array of maximum expansion size
                                       corresponding to the expansion
                                       collation elements with last element
                                       in endExpansionCE*/
      int32_t  endExpansionCECount; /* size of endExpansionCE */
      uint32_t unsafeCP;            /* hash table of unsafe code points */
      uint32_t contrEndCP;          /* hash table of final code points  */
                                    /*   in contractions.               */

      int32_t contractionUCACombosSize;     /* number of UCA contraction items. */
                                            /*Length is contractionUCACombosSize*contractionUCACombosWidth*sizeof(UChar) */
      UBool jamoSpecial;                    /* is jamoSpecial */
      UBool isBigEndian;                    /* is this data big endian? from the UDataInfo header*/
      uint8_t charSetFamily;                /* what is the charset family of this data from the UDataInfo header*/
      uint8_t contractionUCACombosWidth;    /* width of UCA combos field */
      UVersionInfo version;
      UVersionInfo UCAVersion;              /* version of the UCA, read from file */
      UVersionInfo UCDVersion;              /* UCD version, obtained by u_getUnicodeVersion */
      UVersionInfo formatVersion;           /* format version from the UDataInfo header */
      uint32_t scriptToLeadByte;            /* offset to script to lead collation byte mapping data */
      uint32_t leadByteToScript;            /* offset to lead collation byte to script mapping data */
      uint8_t reserved[76];                 /* for future use */
} UCATableHeader;

typedef struct {
  uint32_t byteSize;
  uint32_t tableSize;
  uint32_t contsSize;
  uint32_t table;
  uint32_t conts;
  UVersionInfo UCAVersion;              /* version of the UCA, read from file */
  uint8_t padding[8];
} InverseUCATableHeader;

#endif  /* !UCONFIG_NO_COLLATION */

#endif  /* __UCOL_DATA_H__ */

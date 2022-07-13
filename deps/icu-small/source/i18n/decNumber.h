// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/* ------------------------------------------------------------------ */
/* Decimal Number arithmetic module header                            */
/* ------------------------------------------------------------------ */
/* Copyright (c) IBM Corporation, 2000-2010.   All rights reserved.   */
/*                                                                    */
/* This software is made available under the terms of the             */
/* ICU License -- ICU 1.8.1 and later.                                */
/*                                                                    */
/* The description and User's Guide ("The decNumber C Library") for   */
/* this software is called decNumber.pdf.  This document is           */
/* available, together with arithmetic and format specifications,     */
/* testcases, and Web links, on the General Decimal Arithmetic page.  */
/*                                                                    */
/* Please send comments, suggestions, and corrections to the author:  */
/*   mfc@uk.ibm.com                                                   */
/*   Mike Cowlishaw, IBM Fellow                                       */
/*   IBM UK, PO Box 31, Birmingham Road, Warwick CV34 5JL, UK         */
/* ------------------------------------------------------------------ */

/* Modified version, for use from within ICU.
 *    Renamed public functions, to avoid an unwanted export of the 
 *    standard names from the ICU library.
 *
 *    Use ICU's uprv_malloc() and uprv_free()
 *
 *    Revert comment syntax to plain C
 *
 *    Remove a few compiler warnings.
 */

#if !defined(DECNUMBER)
  #define DECNUMBER
  #define DECNAME     "decNumber"                       /* Short name */
  #define DECFULLNAME "Decimal Number Module"         /* Verbose name */
  #define DECAUTHOR   "Mike Cowlishaw"                /* Who to blame */

  #if !defined(DECCONTEXT)
    #include "decContext.h"
  #endif

  /* Bit settings for decNumber.bits                                  */
  #define DECNEG    0x80      /* Sign; 1=negative, 0=positive or zero */
  #define DECINF    0x40      /* 1=Infinity                           */
  #define DECNAN    0x20      /* 1=NaN                                */
  #define DECSNAN   0x10      /* 1=sNaN                               */
  /* The remaining bits are reserved; they must be 0                  */
  #define DECSPECIAL (DECINF|DECNAN|DECSNAN) /* any special value     */

  /* Define the decNumber data structure.  The size and shape of the  */
  /* units array in the structure is determined by the following      */
  /* constant.  This must not be changed without recompiling the      */
  /* decNumber library modules. */

  /* For ICU, use one digit per byte, to make it easier to emulate the
   * old DigitList interface on top of a decNumber
   */
  #define DECDPUN 1           /* DECimal Digits Per UNit [must be >0  */
                              /* and <10; 3 or powers of 2 are best]. */

  /* DECNUMDIGITS is the default number of digits that can be held in */
  /* the structure.  If undefined, 1 is assumed and it is assumed     */
  /* that the structure will be immediately followed by extra space,  */
  /* as required.  DECNUMDIGITS is always >0.                         */
  #if !defined(DECNUMDIGITS)
    #define DECNUMDIGITS 1
  #endif

  /* The size (integer data type) of each unit is determined by the   */
  /* number of digits it will hold.                                   */
  #if   DECDPUN<=2
    #define decNumberUnit uint8_t
  #elif DECDPUN<=4
    #define decNumberUnit uint16_t
  #else
    #define decNumberUnit uint32_t
  #endif
  /* The number of units needed is ceil(DECNUMDIGITS/DECDPUN)         */
  #define DECNUMUNITS ((DECNUMDIGITS+DECDPUN-1)/DECDPUN)

  /* The data structure... */
  typedef struct {
    int32_t digits;      /* Count of digits in the coefficient; >0    */
    int32_t exponent;    /* Unadjusted exponent, unbiased, in         */
                         /* range: -1999999997 through 999999999      */
    uint8_t bits;        /* Indicator bits (see above)                */
                         /* Coefficient, from least significant unit  */
    decNumberUnit lsu[DECNUMUNITS];
    } decNumber;

  /* Notes:                                                           */
  /* 1. If digits is > DECDPUN then there will one or more            */
  /*    decNumberUnits immediately following the first element of lsu.*/
  /*    These contain the remaining (more significant) digits of the  */
  /*    number, and may be in the lsu array, or may be guaranteed by  */
  /*    some other mechanism (such as being contained in another      */
  /*    structure, or being overlaid on dynamically allocated         */
  /*    storage).                                                     */
  /*                                                                  */
  /*    Each integer of the coefficient (except potentially the last) */
  /*    contains DECDPUN digits (e.g., a value in the range 0 through */
  /*    99999999 if DECDPUN is 8, or 0 through 999 if DECDPUN is 3).  */
  /*                                                                  */
  /* 2. A decNumber converted to a string may need up to digits+14    */
  /*    characters.  The worst cases (non-exponential and exponential */
  /*    formats) are -0.00000{9...}# and -9.{9...}E+999999999#        */
  /*    (where # is '\0')                                             */


  /* ---------------------------------------------------------------- */
  /* decNumber public functions and macros                            */
  /* ---------------------------------------------------------------- */
  /* Conversions                                                      */
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberFromInt32(decNumber *, int32_t);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberFromUInt32(decNumber *, uint32_t);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberFromString(decNumber *, const char *, decContext *);
  U_CAPI char      * U_EXPORT2 uprv_decNumberToString(const decNumber *, char *);
  U_CAPI char      * U_EXPORT2 uprv_decNumberToEngString(const decNumber *, char *);
  U_CAPI uint32_t    U_EXPORT2 uprv_decNumberToUInt32(const decNumber *, decContext *);
  U_CAPI int32_t     U_EXPORT2 uprv_decNumberToInt32(const decNumber *, decContext *);
  U_CAPI uint8_t   * U_EXPORT2 uprv_decNumberGetBCD(const decNumber *, uint8_t *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberSetBCD(decNumber *, const uint8_t *, uint32_t);

  /* Operators and elementary functions                               */
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberAbs(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberAdd(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberAnd(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberCompare(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberCompareSignal(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberCompareTotal(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberCompareTotalMag(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberDivide(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberDivideInteger(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberExp(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberFMA(decNumber *, const decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberInvert(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberLn(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberLogB(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberLog10(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberMax(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberMaxMag(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberMin(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberMinMag(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberMinus(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberMultiply(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberNormalize(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberOr(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberPlus(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberPower(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberQuantize(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberReduce(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberRemainder(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberRemainderNear(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberRescale(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberRotate(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberSameQuantum(decNumber *, const decNumber *, const decNumber *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberScaleB(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberShift(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberSquareRoot(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberSubtract(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberToIntegralExact(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberToIntegralValue(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber * U_EXPORT2 uprv_decNumberXor(decNumber *, const decNumber *, const decNumber *, decContext *);

  /* Utilities                                                        */
  enum decClass uprv_decNumberClass(const decNumber *, decContext *);
  U_CAPI const char * U_EXPORT2 uprv_decNumberClassToString(enum decClass);
  U_CAPI decNumber  * U_EXPORT2 uprv_decNumberCopy(decNumber *, const decNumber *);
  U_CAPI decNumber  * U_EXPORT2 uprv_decNumberCopyAbs(decNumber *, const decNumber *);
  U_CAPI decNumber  * U_EXPORT2 uprv_decNumberCopyNegate(decNumber *, const decNumber *);
  U_CAPI decNumber  * U_EXPORT2 uprv_decNumberCopySign(decNumber *, const decNumber *, const decNumber *);
  U_CAPI decNumber  * U_EXPORT2 uprv_decNumberNextMinus(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber  * U_EXPORT2 uprv_decNumberNextPlus(decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber  * U_EXPORT2 uprv_decNumberNextToward(decNumber *, const decNumber *, const decNumber *, decContext *);
  U_CAPI decNumber  * U_EXPORT2 uprv_decNumberTrim(decNumber *);
  U_CAPI const char * U_EXPORT2 uprv_decNumberVersion(void);
  U_CAPI decNumber  * U_EXPORT2 uprv_decNumberZero(decNumber *);

  /* Functions for testing decNumbers (normality depends on context)  */
  U_CAPI int32_t U_EXPORT2 uprv_decNumberIsNormal(const decNumber *, decContext *);
  U_CAPI int32_t U_EXPORT2 uprv_decNumberIsSubnormal(const decNumber *, decContext *);

  /* Macros for testing decNumber *dn                                 */
  #define decNumberIsCanonical(dn) (1)  /* All decNumbers are saintly */
  #define decNumberIsFinite(dn)    (((dn)->bits&DECSPECIAL)==0)
  #define decNumberIsInfinite(dn)  (((dn)->bits&DECINF)!=0)
  #define decNumberIsNaN(dn)       (((dn)->bits&(DECNAN|DECSNAN))!=0)
  #define decNumberIsNegative(dn)  (((dn)->bits&DECNEG)!=0)
  #define decNumberIsQNaN(dn)      (((dn)->bits&(DECNAN))!=0)
  #define decNumberIsSNaN(dn)      (((dn)->bits&(DECSNAN))!=0)
  #define decNumberIsSpecial(dn)   (((dn)->bits&DECSPECIAL)!=0)
  #define decNumberIsZero(dn)      (*(dn)->lsu==0 \
                                    && (dn)->digits==1 \
                                    && (((dn)->bits&DECSPECIAL)==0))
  #define decNumberRadix(dn)       (10)

#endif

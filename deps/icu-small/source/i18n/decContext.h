// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/* ------------------------------------------------------------------ */
/* Decimal Context module header                                      */
/* ------------------------------------------------------------------ */
/* Copyright (c) IBM Corporation, 2000-2011.   All rights reserved.   */
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
#include "unicode/utypes.h"
#include "putilimp.h"

/*                                                                    */
/* Context variables must always have valid values:                   */
/*                                                                    */
/*  status   -- [any bits may be cleared, but not set, by user]       */
/*  round    -- must be one of the enumerated rounding modes          */
/*                                                                    */
/* The following variables are implied for fixed size formats (i.e.,  */
/* they are ignored) but should still be set correctly in case used   */
/* with decNumber functions:                                          */
/*                                                                    */
/*  clamp    -- must be either 0 or 1                                 */
/*  digits   -- must be in the range 1 through 999999999              */
/*  emax     -- must be in the range 0 through 999999999              */
/*  emin     -- must be in the range 0 through -999999999             */
/*  extended -- must be either 0 or 1 [present only if DECSUBSET]     */
/*  traps    -- only defined bits may be set                          */
/*                                                                    */
/* ------------------------------------------------------------------ */

#if !defined(DECCONTEXT)
  #define DECCONTEXT
  #define DECCNAME     "decContext"                     /* Short name */
  #define DECCFULLNAME "Decimal Context Descriptor"   /* Verbose name */
  #define DECCAUTHOR   "Mike Cowlishaw"               /* Who to blame */

  #if !defined(int32_t)
/* #include <stdint.h>   */         /* C99 standard integers           */
  #endif
  #include <stdio.h>               /* for printf, etc.                */
  #include <signal.h>              /* for traps                       */

  /* Extended flags setting -- set this to 0 to use only IEEE flags   */
  #if !defined(DECEXTFLAG)
  #define DECEXTFLAG 1             /* 1=enable extended flags         */
  #endif

  /* Conditional code flag -- set this to 0 for best performance      */
  #if !defined(DECSUBSET)
  #define DECSUBSET  0             /* 1=enable subset arithmetic      */
  #endif

  /* Context for operations, with associated constants                */
  enum rounding {
    DEC_ROUND_CEILING,             /* round towards +infinity         */
    DEC_ROUND_UP,                  /* round away from 0               */
    DEC_ROUND_HALF_UP,             /* 0.5 rounds up                   */
    DEC_ROUND_HALF_EVEN,           /* 0.5 rounds to nearest even      */
    DEC_ROUND_HALF_DOWN,           /* 0.5 rounds down                 */
    DEC_ROUND_DOWN,                /* round towards 0 (truncate)      */
    DEC_ROUND_FLOOR,               /* round towards -infinity         */
    DEC_ROUND_05UP,                /* round for reround               */
    DEC_ROUND_MAX                  /* enum must be less than this     */
    };
  #define DEC_ROUND_DEFAULT DEC_ROUND_HALF_EVEN;

  typedef struct {
    int32_t  digits;               /* working precision               */
    int32_t  emax;                 /* maximum positive exponent       */
    int32_t  emin;                 /* minimum negative exponent       */
    enum     rounding round;       /* rounding mode                   */
    uint32_t traps;                /* trap-enabler flags              */
    uint32_t status;               /* status flags                    */
    uint8_t  clamp;                /* flag: apply IEEE exponent clamp */
    #if DECSUBSET
    uint8_t  extended;             /* flag: special-values allowed    */
    #endif
    } decContext;

  /* Maxima and Minima for context settings                           */
  #define DEC_MAX_DIGITS 999999999
  #define DEC_MIN_DIGITS         1
  #define DEC_MAX_EMAX   999999999
  #define DEC_MIN_EMAX           0
  #define DEC_MAX_EMIN           0
  #define DEC_MIN_EMIN  -999999999
  #define DEC_MAX_MATH      999999 /* max emax, etc., for math funcs. */

  /* Classifications for decimal numbers, aligned with 754 (note that */
  /* 'normal' and 'subnormal' are meaningful only with a decContext   */
  /* or a fixed size format).                                         */
  enum decClass {
    DEC_CLASS_SNAN,
    DEC_CLASS_QNAN,
    DEC_CLASS_NEG_INF,
    DEC_CLASS_NEG_NORMAL,
    DEC_CLASS_NEG_SUBNORMAL,
    DEC_CLASS_NEG_ZERO,
    DEC_CLASS_POS_ZERO,
    DEC_CLASS_POS_SUBNORMAL,
    DEC_CLASS_POS_NORMAL,
    DEC_CLASS_POS_INF
    };
  /* Strings for the decClasses */
  #define DEC_ClassString_SN  "sNaN"
  #define DEC_ClassString_QN  "NaN"
  #define DEC_ClassString_NI  "-Infinity"
  #define DEC_ClassString_NN  "-Normal"
  #define DEC_ClassString_NS  "-Subnormal"
  #define DEC_ClassString_NZ  "-Zero"
  #define DEC_ClassString_PZ  "+Zero"
  #define DEC_ClassString_PS  "+Subnormal"
  #define DEC_ClassString_PN  "+Normal"
  #define DEC_ClassString_PI  "+Infinity"
  #define DEC_ClassString_UN  "Invalid"

  /* Trap-enabler and Status flags (exceptional conditions), and      */
  /* their names.  The top byte is reserved for internal use          */
  #if DECEXTFLAG
    /* Extended flags */
    #define DEC_Conversion_syntax    0x00000001
    #define DEC_Division_by_zero     0x00000002
    #define DEC_Division_impossible  0x00000004
    #define DEC_Division_undefined   0x00000008
    #define DEC_Insufficient_storage 0x00000010 /* [when malloc fails]  */
    #define DEC_Inexact              0x00000020
    #define DEC_Invalid_context      0x00000040
    #define DEC_Invalid_operation    0x00000080
    #if DECSUBSET
    #define DEC_Lost_digits          0x00000100
    #endif
    #define DEC_Overflow             0x00000200
    #define DEC_Clamped              0x00000400
    #define DEC_Rounded              0x00000800
    #define DEC_Subnormal            0x00001000
    #define DEC_Underflow            0x00002000
  #else
    /* IEEE flags only */
    #define DEC_Conversion_syntax    0x00000010
    #define DEC_Division_by_zero     0x00000002
    #define DEC_Division_impossible  0x00000010
    #define DEC_Division_undefined   0x00000010
    #define DEC_Insufficient_storage 0x00000010 /* [when malloc fails]  */
    #define DEC_Inexact              0x00000001
    #define DEC_Invalid_context      0x00000010
    #define DEC_Invalid_operation    0x00000010
    #if DECSUBSET
    #define DEC_Lost_digits          0x00000000
    #endif
    #define DEC_Overflow             0x00000008
    #define DEC_Clamped              0x00000000
    #define DEC_Rounded              0x00000000
    #define DEC_Subnormal            0x00000000
    #define DEC_Underflow            0x00000004
  #endif

  /* IEEE 754 groupings for the flags                                 */
  /* [DEC_Clamped, DEC_Lost_digits, DEC_Rounded, and DEC_Subnormal    */
  /* are not in IEEE 754]                                             */
  #define DEC_IEEE_754_Division_by_zero  (DEC_Division_by_zero)
  #if DECSUBSET
  #define DEC_IEEE_754_Inexact           (DEC_Inexact | DEC_Lost_digits)
  #else
  #define DEC_IEEE_754_Inexact           (DEC_Inexact)
  #endif
  #define DEC_IEEE_754_Invalid_operation (DEC_Conversion_syntax |     \
                                          DEC_Division_impossible |   \
                                          DEC_Division_undefined |    \
                                          DEC_Insufficient_storage |  \
                                          DEC_Invalid_context |       \
                                          DEC_Invalid_operation)
  #define DEC_IEEE_754_Overflow          (DEC_Overflow)
  #define DEC_IEEE_754_Underflow         (DEC_Underflow)

  /* flags which are normally errors (result is qNaN, infinite, or 0) */
  #define DEC_Errors (DEC_IEEE_754_Division_by_zero |                 \
                      DEC_IEEE_754_Invalid_operation |                \
                      DEC_IEEE_754_Overflow | DEC_IEEE_754_Underflow)
  /* flags which cause a result to become qNaN                        */
  #define DEC_NaNs    DEC_IEEE_754_Invalid_operation

  /* flags which are normally for information only (finite results)   */
  #if DECSUBSET
  #define DEC_Information (DEC_Clamped | DEC_Rounded | DEC_Inexact    \
                          | DEC_Lost_digits)
  #else
  #define DEC_Information (DEC_Clamped | DEC_Rounded | DEC_Inexact)
  #endif

  /* IEEE 854 names (for compatibility with older decNumber versions) */
  #define DEC_IEEE_854_Division_by_zero  DEC_IEEE_754_Division_by_zero
  #define DEC_IEEE_854_Inexact           DEC_IEEE_754_Inexact
  #define DEC_IEEE_854_Invalid_operation DEC_IEEE_754_Invalid_operation
  #define DEC_IEEE_854_Overflow          DEC_IEEE_754_Overflow
  #define DEC_IEEE_854_Underflow         DEC_IEEE_754_Underflow

  /* Name strings for the exceptional conditions                      */
  #define DEC_Condition_CS "Conversion syntax"
  #define DEC_Condition_DZ "Division by zero"
  #define DEC_Condition_DI "Division impossible"
  #define DEC_Condition_DU "Division undefined"
  #define DEC_Condition_IE "Inexact"
  #define DEC_Condition_IS "Insufficient storage"
  #define DEC_Condition_IC "Invalid context"
  #define DEC_Condition_IO "Invalid operation"
  #if DECSUBSET
  #define DEC_Condition_LD "Lost digits"
  #endif
  #define DEC_Condition_OV "Overflow"
  #define DEC_Condition_PA "Clamped"
  #define DEC_Condition_RO "Rounded"
  #define DEC_Condition_SU "Subnormal"
  #define DEC_Condition_UN "Underflow"
  #define DEC_Condition_ZE "No status"
  #define DEC_Condition_MU "Multiple status"
  #define DEC_Condition_Length 21  /* length of the longest string,   */
                                   /* including terminator            */

  /* Initialization descriptors, used by decContextDefault            */
  #define DEC_INIT_BASE         0
  #define DEC_INIT_DECIMAL32   32
  #define DEC_INIT_DECIMAL64   64
  #define DEC_INIT_DECIMAL128 128
  /* Synonyms */
  #define DEC_INIT_DECSINGLE  DEC_INIT_DECIMAL32
  #define DEC_INIT_DECDOUBLE  DEC_INIT_DECIMAL64
  #define DEC_INIT_DECQUAD    DEC_INIT_DECIMAL128

  /* decContext routines                                              */
  U_CAPI decContext  * U_EXPORT2 uprv_decContextClearStatus(decContext *, uint32_t);
  U_CAPI decContext  * U_EXPORT2 uprv_decContextDefault(decContext *, int32_t);
  U_CAPI enum rounding U_EXPORT2 uprv_decContextGetRounding(decContext *);
  U_CAPI uint32_t      U_EXPORT2 uprv_decContextGetStatus(decContext *);
  U_CAPI decContext  * U_EXPORT2 uprv_decContextRestoreStatus(decContext *, uint32_t, uint32_t);
  U_CAPI uint32_t      U_EXPORT2 uprv_decContextSaveStatus(decContext *, uint32_t);
  U_CAPI decContext  * U_EXPORT2 uprv_decContextSetRounding(decContext *, enum rounding);
  U_CAPI decContext  * U_EXPORT2 uprv_decContextSetStatus(decContext *, uint32_t);
  U_CAPI decContext  * U_EXPORT2 uprv_decContextSetStatusFromString(decContext *, const char *);
  U_CAPI decContext  * U_EXPORT2 uprv_decContextSetStatusFromStringQuiet(decContext *, const char *);
  U_CAPI decContext  * U_EXPORT2 uprv_decContextSetStatusQuiet(decContext *, uint32_t);
  U_CAPI const char  * U_EXPORT2 uprv_decContextStatusToString(const decContext *);
  U_CAPI int32_t       U_EXPORT2 uprv_decContextTestEndian(uint8_t);
  U_CAPI uint32_t      U_EXPORT2 uprv_decContextTestSavedStatus(uint32_t, uint32_t);
  U_CAPI uint32_t      U_EXPORT2 uprv_decContextTestStatus(decContext *, uint32_t);
  U_CAPI decContext  * U_EXPORT2 uprv_decContextZeroStatus(decContext *);

#endif

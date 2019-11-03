// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/* ------------------------------------------------------------------ */
/* Decimal Context module                                             */
/* ------------------------------------------------------------------ */
/* Copyright (c) IBM Corporation, 2000-2012.  All rights reserved.    */
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
/* This module comprises the routines for handling arithmetic         */
/* context structures.                                                */
/* ------------------------------------------------------------------ */

#include <string.h>           /* for strcmp  */
#include <stdio.h>            /* for printf if DECCHECK  */
#include "decContext.h"       /* context and base types  */
#include "decNumberLocal.h"   /* decNumber local types, etc.  */

#if 0  /* ICU: No need to test endianness at runtime. */
/* compile-time endian tester [assumes sizeof(Int)>1] */
static  const  Int mfcone=1;                 /* constant 1  */
static  const  Flag *mfctop=(Flag *)&mfcone; /* -> top byte  */
#define LITEND *mfctop             /* named flag; 1=little-endian  */
#endif

/* ------------------------------------------------------------------ */
/* decContextClearStatus -- clear bits in current status              */
/*                                                                    */
/*  context is the context structure to be queried                    */
/*  mask indicates the bits to be cleared (the status bit that        */
/*    corresponds to each 1 bit in the mask is cleared)               */
/*  returns context                                                   */
/*                                                                    */
/* No error is possible.                                              */
/* ------------------------------------------------------------------ */
U_CAPI decContext * U_EXPORT2 uprv_decContextClearStatus(decContext *context, uInt mask) {
  context->status&=~mask;
  return context;
  } /* decContextClearStatus  */

/* ------------------------------------------------------------------ */
/* decContextDefault -- initialize a context structure                */
/*                                                                    */
/*  context is the structure to be initialized                        */
/*  kind selects the required set of default values, one of:          */
/*      DEC_INIT_BASE       -- select ANSI X3-274 defaults            */
/*      DEC_INIT_DECIMAL32  -- select IEEE 754 defaults, 32-bit       */
/*      DEC_INIT_DECIMAL64  -- select IEEE 754 defaults, 64-bit       */
/*      DEC_INIT_DECIMAL128 -- select IEEE 754 defaults, 128-bit      */
/*      For any other value a valid context is returned, but with     */
/*      Invalid_operation set in the status field.                    */
/*  returns a context structure with the appropriate initial values.  */
/* ------------------------------------------------------------------ */
U_CAPI decContext *  U_EXPORT2 uprv_decContextDefault(decContext *context, Int kind) {
  /* set defaults...  */
  context->digits=9;                         /* 9 digits  */
  context->emax=DEC_MAX_EMAX;                /* 9-digit exponents  */
  context->emin=DEC_MIN_EMIN;                /* .. balanced  */
  context->round=DEC_ROUND_HALF_UP;          /* 0.5 rises  */
  context->traps=DEC_Errors;                 /* all but informational  */
  context->status=0;                         /* cleared  */
  context->clamp=0;                          /* no clamping  */
  #if DECSUBSET
  context->extended=0;                       /* cleared  */
  #endif
  switch (kind) {
    case DEC_INIT_BASE:
      /* [use defaults]  */
      break;
    case DEC_INIT_DECIMAL32:
      context->digits=7;                     /* digits  */
      context->emax=96;                      /* Emax  */
      context->emin=-95;                     /* Emin  */
      context->round=DEC_ROUND_HALF_EVEN;    /* 0.5 to nearest even  */
      context->traps=0;                      /* no traps set  */
      context->clamp=1;                      /* clamp exponents  */
      #if DECSUBSET
      context->extended=1;                   /* set  */
      #endif
      break;
    case DEC_INIT_DECIMAL64:
      context->digits=16;                    /* digits  */
      context->emax=384;                     /* Emax  */
      context->emin=-383;                    /* Emin  */
      context->round=DEC_ROUND_HALF_EVEN;    /* 0.5 to nearest even  */
      context->traps=0;                      /* no traps set  */
      context->clamp=1;                      /* clamp exponents  */
      #if DECSUBSET
      context->extended=1;                   /* set  */
      #endif
      break;
    case DEC_INIT_DECIMAL128:
      context->digits=34;                    /* digits  */
      context->emax=6144;                    /* Emax  */
      context->emin=-6143;                   /* Emin  */
      context->round=DEC_ROUND_HALF_EVEN;    /* 0.5 to nearest even  */
      context->traps=0;                      /* no traps set  */
      context->clamp=1;                      /* clamp exponents  */
      #if DECSUBSET
      context->extended=1;                   /* set  */
      #endif
      break;

    default:                                 /* invalid Kind  */
      /* use defaults, and ..  */
      uprv_decContextSetStatus(context, DEC_Invalid_operation); /* trap  */
    }

  return context;} /* decContextDefault  */

/* ------------------------------------------------------------------ */
/* decContextGetRounding -- return current rounding mode              */
/*                                                                    */
/*  context is the context structure to be queried                    */
/*  returns the rounding mode                                         */
/*                                                                    */
/* No error is possible.                                              */
/* ------------------------------------------------------------------ */
U_CAPI enum rounding  U_EXPORT2 uprv_decContextGetRounding(decContext *context) {
  return context->round;
  } /* decContextGetRounding  */

/* ------------------------------------------------------------------ */
/* decContextGetStatus -- return current status                       */
/*                                                                    */
/*  context is the context structure to be queried                    */
/*  returns status                                                    */
/*                                                                    */
/* No error is possible.                                              */
/* ------------------------------------------------------------------ */
U_CAPI uInt  U_EXPORT2 uprv_decContextGetStatus(decContext *context) {
  return context->status;
  } /* decContextGetStatus  */

/* ------------------------------------------------------------------ */
/* decContextRestoreStatus -- restore bits in current status          */
/*                                                                    */
/*  context is the context structure to be updated                    */
/*  newstatus is the source for the bits to be restored               */
/*  mask indicates the bits to be restored (the status bit that       */
/*    corresponds to each 1 bit in the mask is set to the value of    */
/*    the correspnding bit in newstatus)                              */
/*  returns context                                                   */
/*                                                                    */
/* No error is possible.                                              */
/* ------------------------------------------------------------------ */
U_CAPI decContext * U_EXPORT2 uprv_decContextRestoreStatus(decContext *context,
                                    uInt newstatus, uInt mask) {
  context->status&=~mask;               /* clear the selected bits  */
  context->status|=(mask&newstatus);    /* or in the new bits  */
  return context;
  } /* decContextRestoreStatus  */

/* ------------------------------------------------------------------ */
/* decContextSaveStatus -- save bits in current status                */
/*                                                                    */
/*  context is the context structure to be queried                    */
/*  mask indicates the bits to be saved (the status bits that         */
/*    correspond to each 1 bit in the mask are saved)                 */
/*  returns the AND of the mask and the current status                */
/*                                                                    */
/* No error is possible.                                              */
/* ------------------------------------------------------------------ */
U_CAPI uInt  U_EXPORT2 uprv_decContextSaveStatus(decContext *context, uInt mask) {
  return context->status&mask;
  } /* decContextSaveStatus  */

/* ------------------------------------------------------------------ */
/* decContextSetRounding -- set current rounding mode                 */
/*                                                                    */
/*  context is the context structure to be updated                    */
/*  newround is the value which will replace the current mode         */
/*  returns context                                                   */
/*                                                                    */
/* No error is possible.                                              */
/* ------------------------------------------------------------------ */
U_CAPI decContext * U_EXPORT2 uprv_decContextSetRounding(decContext *context,
                                  enum rounding newround) {
  context->round=newround;
  return context;
  } /* decContextSetRounding  */

/* ------------------------------------------------------------------ */
/* decContextSetStatus -- set status and raise trap if appropriate    */
/*                                                                    */
/*  context is the context structure to be updated                    */
/*  status  is the DEC_ exception code                                */
/*  returns the context structure                                     */
/*                                                                    */
/* Control may never return from this routine, if there is a signal   */
/* handler and it takes a long jump.                                  */
/* ------------------------------------------------------------------ */
U_CAPI decContext *  U_EXPORT2 uprv_decContextSetStatus(decContext *context, uInt status) {
  context->status|=status;
#if 0  /* ICU: Do not raise signals. */
  if (status & context->traps) raise(SIGFPE);
#endif
  return context;} /* decContextSetStatus  */

/* ------------------------------------------------------------------ */
/* decContextSetStatusFromString -- set status from a string + trap   */
/*                                                                    */
/*  context is the context structure to be updated                    */
/*  string is a string exactly equal to one that might be returned    */
/*            by decContextStatusToString                             */
/*                                                                    */
/*  The status bit corresponding to the string is set, and a trap     */
/*  is raised if appropriate.                                         */
/*                                                                    */
/*  returns the context structure, unless the string is equal to      */
/*    DEC_Condition_MU or is not recognized.  In these cases NULL is  */
/*    returned.                                                       */
/* ------------------------------------------------------------------ */
U_CAPI decContext *  U_EXPORT2 uprv_decContextSetStatusFromString(decContext *context,
                                           const char *string) {
  if (strcmp(string, DEC_Condition_CS)==0)
    return uprv_decContextSetStatus(context, DEC_Conversion_syntax);
  if (strcmp(string, DEC_Condition_DZ)==0)
    return uprv_decContextSetStatus(context, DEC_Division_by_zero);
  if (strcmp(string, DEC_Condition_DI)==0)
    return uprv_decContextSetStatus(context, DEC_Division_impossible);
  if (strcmp(string, DEC_Condition_DU)==0)
    return uprv_decContextSetStatus(context, DEC_Division_undefined);
  if (strcmp(string, DEC_Condition_IE)==0)
    return uprv_decContextSetStatus(context, DEC_Inexact);
  if (strcmp(string, DEC_Condition_IS)==0)
    return uprv_decContextSetStatus(context, DEC_Insufficient_storage);
  if (strcmp(string, DEC_Condition_IC)==0)
    return uprv_decContextSetStatus(context, DEC_Invalid_context);
  if (strcmp(string, DEC_Condition_IO)==0)
    return uprv_decContextSetStatus(context, DEC_Invalid_operation);
  #if DECSUBSET
  if (strcmp(string, DEC_Condition_LD)==0)
    return uprv_decContextSetStatus(context, DEC_Lost_digits);
  #endif
  if (strcmp(string, DEC_Condition_OV)==0)
    return uprv_decContextSetStatus(context, DEC_Overflow);
  if (strcmp(string, DEC_Condition_PA)==0)
    return uprv_decContextSetStatus(context, DEC_Clamped);
  if (strcmp(string, DEC_Condition_RO)==0)
    return uprv_decContextSetStatus(context, DEC_Rounded);
  if (strcmp(string, DEC_Condition_SU)==0)
    return uprv_decContextSetStatus(context, DEC_Subnormal);
  if (strcmp(string, DEC_Condition_UN)==0)
    return uprv_decContextSetStatus(context, DEC_Underflow);
  if (strcmp(string, DEC_Condition_ZE)==0)
    return context;
  return NULL;  /* Multiple status, or unknown  */
  } /* decContextSetStatusFromString  */

/* ------------------------------------------------------------------ */
/* decContextSetStatusFromStringQuiet -- set status from a string     */
/*                                                                    */
/*  context is the context structure to be updated                    */
/*  string is a string exactly equal to one that might be returned    */
/*            by decContextStatusToString                             */
/*                                                                    */
/*  The status bit corresponding to the string is set; no trap is     */
/*  raised.                                                           */
/*                                                                    */
/*  returns the context structure, unless the string is equal to      */
/*    DEC_Condition_MU or is not recognized.  In these cases NULL is  */
/*    returned.                                                       */
/* ------------------------------------------------------------------ */
U_CAPI decContext *  U_EXPORT2 uprv_decContextSetStatusFromStringQuiet(decContext *context,
                                                const char *string) {
  if (strcmp(string, DEC_Condition_CS)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Conversion_syntax);
  if (strcmp(string, DEC_Condition_DZ)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Division_by_zero);
  if (strcmp(string, DEC_Condition_DI)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Division_impossible);
  if (strcmp(string, DEC_Condition_DU)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Division_undefined);
  if (strcmp(string, DEC_Condition_IE)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Inexact);
  if (strcmp(string, DEC_Condition_IS)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Insufficient_storage);
  if (strcmp(string, DEC_Condition_IC)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Invalid_context);
  if (strcmp(string, DEC_Condition_IO)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Invalid_operation);
  #if DECSUBSET
  if (strcmp(string, DEC_Condition_LD)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Lost_digits);
  #endif
  if (strcmp(string, DEC_Condition_OV)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Overflow);
  if (strcmp(string, DEC_Condition_PA)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Clamped);
  if (strcmp(string, DEC_Condition_RO)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Rounded);
  if (strcmp(string, DEC_Condition_SU)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Subnormal);
  if (strcmp(string, DEC_Condition_UN)==0)
    return uprv_decContextSetStatusQuiet(context, DEC_Underflow);
  if (strcmp(string, DEC_Condition_ZE)==0)
    return context;
  return NULL;  /* Multiple status, or unknown  */
  } /* decContextSetStatusFromStringQuiet  */

/* ------------------------------------------------------------------ */
/* decContextSetStatusQuiet -- set status without trap                */
/*                                                                    */
/*  context is the context structure to be updated                    */
/*  status  is the DEC_ exception code                                */
/*  returns the context structure                                     */
/*                                                                    */
/* No error is possible.                                              */
/* ------------------------------------------------------------------ */
U_CAPI decContext *  U_EXPORT2 uprv_decContextSetStatusQuiet(decContext *context, uInt status) {
  context->status|=status;
  return context;} /* decContextSetStatusQuiet  */

/* ------------------------------------------------------------------ */
/* decContextStatusToString -- convert status flags to a string       */
/*                                                                    */
/*  context is a context with valid status field                      */
/*                                                                    */
/*  returns a constant string describing the condition.  If multiple  */
/*    (or no) flags are set, a generic constant message is returned.  */
/* ------------------------------------------------------------------ */
U_CAPI const char * U_EXPORT2 uprv_decContextStatusToString(const decContext *context) {
  Int status=context->status;

  /* test the five IEEE first, as some of the others are ambiguous when  */
  /* DECEXTFLAG=0  */
  if (status==DEC_Invalid_operation    ) return DEC_Condition_IO;
  if (status==DEC_Division_by_zero     ) return DEC_Condition_DZ;
  if (status==DEC_Overflow             ) return DEC_Condition_OV;
  if (status==DEC_Underflow            ) return DEC_Condition_UN;
  if (status==DEC_Inexact              ) return DEC_Condition_IE;

  if (status==DEC_Division_impossible  ) return DEC_Condition_DI;
  if (status==DEC_Division_undefined   ) return DEC_Condition_DU;
  if (status==DEC_Rounded              ) return DEC_Condition_RO;
  if (status==DEC_Clamped              ) return DEC_Condition_PA;
  if (status==DEC_Subnormal            ) return DEC_Condition_SU;
  if (status==DEC_Conversion_syntax    ) return DEC_Condition_CS;
  if (status==DEC_Insufficient_storage ) return DEC_Condition_IS;
  if (status==DEC_Invalid_context      ) return DEC_Condition_IC;
  #if DECSUBSET
  if (status==DEC_Lost_digits          ) return DEC_Condition_LD;
  #endif
  if (status==0                        ) return DEC_Condition_ZE;
  return DEC_Condition_MU;  /* Multiple errors  */
  } /* decContextStatusToString  */

/* ------------------------------------------------------------------ */
/* decContextTestEndian -- test whether DECLITEND is set correctly    */
/*                                                                    */
/*  quiet is 1 to suppress message; 0 otherwise                       */
/*  returns 0 if DECLITEND is correct                                 */
/*          1 if DECLITEND is incorrect and should be 1               */
/*         -1 if DECLITEND is incorrect and should be 0               */
/*                                                                    */
/* A message is displayed if the return value is not 0 and quiet==0.  */
/*                                                                    */
/* No error is possible.                                              */
/* ------------------------------------------------------------------ */
#if 0  /* ICU: Unused function. Anyway, do not call printf(). */
U_CAPI Int  U_EXPORT2 uprv_decContextTestEndian(Flag quiet) {
  Int res=0;                  /* optimist  */
  uInt dle=(uInt)DECLITEND;   /* unsign  */
  if (dle>1) dle=1;           /* ensure 0 or 1  */

  if (LITEND!=DECLITEND) {
    const char *adj;
    if (!quiet) {
      if (LITEND) adj="little";
             else adj="big";
      printf("Warning: DECLITEND is set to %d, but this computer appears to be %s-endian\n",
             DECLITEND, adj);
      }
    res=(Int)LITEND-dle;
    }
  return res;
  } /* decContextTestEndian  */
#endif

/* ------------------------------------------------------------------ */
/* decContextTestSavedStatus -- test bits in saved status             */
/*                                                                    */
/*  oldstatus is the status word to be tested                         */
/*  mask indicates the bits to be tested (the oldstatus bits that     */
/*    correspond to each 1 bit in the mask are tested)                */
/*  returns 1 if any of the tested bits are 1, or 0 otherwise         */
/*                                                                    */
/* No error is possible.                                              */
/* ------------------------------------------------------------------ */
U_CAPI  uInt U_EXPORT2 uprv_decContextTestSavedStatus(uInt oldstatus, uInt mask) {
  return (oldstatus&mask)!=0;
  } /* decContextTestSavedStatus  */

/* ------------------------------------------------------------------ */
/* decContextTestStatus -- test bits in current status                */
/*                                                                    */
/*  context is the context structure to be updated                    */
/*  mask indicates the bits to be tested (the status bits that        */
/*    correspond to each 1 bit in the mask are tested)                */
/*  returns 1 if any of the tested bits are 1, or 0 otherwise         */
/*                                                                    */
/* No error is possible.                                              */
/* ------------------------------------------------------------------ */
U_CAPI uInt  U_EXPORT2 uprv_decContextTestStatus(decContext *context, uInt mask) {
  return (context->status&mask)!=0;
  } /* decContextTestStatus  */

/* ------------------------------------------------------------------ */
/* decContextZeroStatus -- clear all status bits                      */
/*                                                                    */
/*  context is the context structure to be updated                    */
/*  returns context                                                   */
/*                                                                    */
/* No error is possible.                                              */
/* ------------------------------------------------------------------ */
U_CAPI decContext * U_EXPORT2 uprv_decContextZeroStatus(decContext *context) {
  context->status=0;
  return context;
  } /* decContextZeroStatus  */


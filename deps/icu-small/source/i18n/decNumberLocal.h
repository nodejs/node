// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/* ------------------------------------------------------------------ */
/* decNumber package local type, tuning, and macro definitions        */
/* ------------------------------------------------------------------ */
/* Copyright (c) IBM Corporation, 2000-2016.   All rights reserved.   */
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
/* This header file is included by all modules in the decNumber       */
/* library, and contains local type definitions, tuning parameters,   */
/* etc.  It should not need to be used by application programs.       */
/* decNumber.h or one of decDouble (etc.) must be included first.     */
/* ------------------------------------------------------------------ */

#if !defined(DECNUMBERLOC)
  #define DECNUMBERLOC
  #define DECVERSION    "decNumber 3.61" /* Package Version [16 max.] */
  #define DECNLAUTHOR   "Mike Cowlishaw"              /* Who to blame */

  #include <stdlib.h>         /* for abs                              */
  #include <string.h>         /* for memset, strcpy                   */
  #include "decContext.h"

  /* Conditional code flag -- set this to match hardware platform     */
  #if !defined(DECLITEND)
  #define DECLITEND 1         /* 1=little-endian, 0=big-endian        */
  #endif

  /* Conditional code flag -- set this to 1 for best performance      */
  #if !defined(DECUSE64)
  #define DECUSE64  1         /* 1=use int64s, 0=int32 & smaller only */
  #endif

  /* Conditional check flags -- set these to 0 for best performance   */
  #if !defined(DECCHECK)
  #define DECCHECK  0         /* 1 to enable robust checking          */
  #endif
  #if !defined(DECALLOC)
  #define DECALLOC  0         /* 1 to enable memory accounting        */
  #endif
  #if !defined(DECTRACE)
  #define DECTRACE  0         /* 1 to trace certain internals, etc.   */
  #endif

  /* Tuning parameter for decNumber (arbitrary precision) module      */
  #if !defined(DECBUFFER)
  #define DECBUFFER 36        /* Size basis for local buffers.  This  */
                              /* should be a common maximum precision */
                              /* rounded up to a multiple of 4; must  */
                              /* be zero or positive.                 */
  #endif

  /* ---------------------------------------------------------------- */
  /* Definitions for all modules (general-purpose)                    */
  /* ---------------------------------------------------------------- */

  /* Local names for common types -- for safety, decNumber modules do */
  /* not use int or long directly.                                    */
  #define Flag   uint8_t
  #define Byte   int8_t
  #define uByte  uint8_t
  #define Short  int16_t
  #define uShort uint16_t
  #define Int    int32_t
  #define uInt   uint32_t
  #define Unit   decNumberUnit
  #if DECUSE64
  #define Long   int64_t
  #define uLong  uint64_t
  #endif

  /* Development-use definitions                                      */
  typedef long int LI;        /* for printf arguments only            */
  #define DECNOINT  0         /* 1 to check no internal use of 'int'  */
                              /*   or stdint types                    */
  #if DECNOINT
    /* if these interfere with your C includes, do not set DECNOINT   */
    #define int     ?         /* enable to ensure that plain C 'int'  */
    #define long    ??        /* .. or 'long' types are not used      */
  #endif

  /* LONGMUL32HI -- set w=(u*v)>>32, where w, u, and v are uInts      */
  /* (that is, sets w to be the high-order word of the 64-bit result; */
  /* the low-order word is simply u*v.)                               */
  /* This version is derived from Knuth via Hacker's Delight;         */
  /* it seems to optimize better than some others tried               */
  #define LONGMUL32HI(w, u, v) {             \
    uInt u0, u1, v0, v1, w0, w1, w2, t;      \
    u0=u & 0xffff; u1=u>>16;                 \
    v0=v & 0xffff; v1=v>>16;                 \
    w0=u0*v0;                                \
    t=u1*v0 + (w0>>16);                      \
    w1=t & 0xffff; w2=t>>16;                 \
    w1=u0*v1 + w1;                           \
    (w)=u1*v1 + w2 + (w1>>16);}

  /* ROUNDUP -- round an integer up to a multiple of n                */
  #define ROUNDUP(i, n) ((((i)+(n)-1)/n)*n)
  #define ROUNDUP4(i)   (((i)+3)&~3)    /* special for n=4            */

  /* ROUNDDOWN -- round an integer down to a multiple of n            */
  #define ROUNDDOWN(i, n) (((i)/n)*n)
  #define ROUNDDOWN4(i)   ((i)&~3)      /* special for n=4            */

  /* References to multi-byte sequences under different sizes; these  */
  /* require locally declared variables, but do not violate strict    */
  /* aliasing or alignment (as did the UINTAT simple cast to uInt).   */
  /* Variables needed are uswork, uiwork, etc. [so do not use at same */
  /* level in an expression, e.g., UBTOUI(x)==UBTOUI(y) may fail].    */

  /* Return a uInt, etc., from bytes starting at a char* or uByte*    */
  #define UBTOUS(b)  (memcpy((void *)&uswork, b, 2), uswork)
  #define UBTOUI(b)  (memcpy((void *)&uiwork, b, 4), uiwork)

  /* Store a uInt, etc., into bytes starting at a char* or uByte*.    */
  /* Returns i, evaluated, for convenience; has to use uiwork because */
  /* i may be an expression.                                          */
  #define UBFROMUS(b, i)  (uswork=(i), memcpy(b, (void *)&uswork, 2), uswork)
  #define UBFROMUI(b, i)  (uiwork=(i), memcpy(b, (void *)&uiwork, 4), uiwork)

  /* X10 and X100 -- multiply integer i by 10 or 100                  */
  /* [shifts are usually faster than multiply; could be conditional]  */
  #define X10(i)  (((i)<<1)+((i)<<3))
  #define X100(i) (((i)<<2)+((i)<<5)+((i)<<6))

  /* MAXI and MINI -- general max & min (not in ANSI) for integers    */
  #define MAXI(x,y) ((x)<(y)?(y):(x))
  #define MINI(x,y) ((x)>(y)?(y):(x))

  /* Useful constants                                                 */
  #define BILLION      1000000000            /* 10**9                 */
  /* CHARMASK: 0x30303030 for ASCII/UTF8; 0xF0F0F0F0 for EBCDIC       */
  #define CHARMASK ((((((((uInt)'0')<<8)+'0')<<8)+'0')<<8)+'0')


  /* ---------------------------------------------------------------- */
  /* Definitions for arbitary-precision modules (only valid after     */
  /* decNumber.h has been included)                                   */
  /* ---------------------------------------------------------------- */

  /* Limits and constants                                             */
  #define DECNUMMAXP 999999999  /* maximum precision code can handle  */
  #define DECNUMMAXE 999999999  /* maximum adjusted exponent ditto    */
  #define DECNUMMINE -999999999 /* minimum adjusted exponent ditto    */
  #if (DECNUMMAXP != DEC_MAX_DIGITS)
    #error Maximum digits mismatch
  #endif
  #if (DECNUMMAXE != DEC_MAX_EMAX)
    #error Maximum exponent mismatch
  #endif
  #if (DECNUMMINE != DEC_MIN_EMIN)
    #error Minimum exponent mismatch
  #endif

  /* Set DECDPUNMAX -- the maximum integer that fits in DECDPUN       */
  /* digits, and D2UTABLE -- the initializer for the D2U table        */
  #ifndef DECDPUN
    // no-op
  #elif   DECDPUN==1
    #define DECDPUNMAX 9
    #define D2UTABLE {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,  \
                      18,19,20,21,22,23,24,25,26,27,28,29,30,31,32, \
                      33,34,35,36,37,38,39,40,41,42,43,44,45,46,47, \
                      48,49}
  #elif DECDPUN==2
    #define DECDPUNMAX 99
    #define D2UTABLE {0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,  \
                      11,11,12,12,13,13,14,14,15,15,16,16,17,17,18, \
                      18,19,19,20,20,21,21,22,22,23,23,24,24,25}
  #elif DECDPUN==3
    #define DECDPUNMAX 999
    #define D2UTABLE {0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7,  \
                      8,8,8,9,9,9,10,10,10,11,11,11,12,12,12,13,13, \
                      13,14,14,14,15,15,15,16,16,16,17}
  #elif DECDPUN==4
    #define DECDPUNMAX 9999
    #define D2UTABLE {0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,6,  \
                      6,6,6,7,7,7,7,8,8,8,8,9,9,9,9,10,10,10,10,11, \
                      11,11,11,12,12,12,12,13}
  #elif DECDPUN==5
    #define DECDPUNMAX 99999
    #define D2UTABLE {0,1,1,1,1,1,2,2,2,2,2,3,3,3,3,3,4,4,4,4,4,5,  \
                      5,5,5,5,6,6,6,6,6,7,7,7,7,7,8,8,8,8,8,9,9,9,  \
                      9,9,10,10,10,10}
  #elif DECDPUN==6
    #define DECDPUNMAX 999999
    #define D2UTABLE {0,1,1,1,1,1,1,2,2,2,2,2,2,3,3,3,3,3,3,4,4,4,  \
                      4,4,4,5,5,5,5,5,5,6,6,6,6,6,6,7,7,7,7,7,7,8,  \
                      8,8,8,8,8,9}
  #elif DECDPUN==7
    #define DECDPUNMAX 9999999
    #define D2UTABLE {0,1,1,1,1,1,1,1,2,2,2,2,2,2,2,3,3,3,3,3,3,3,  \
                      4,4,4,4,4,4,4,5,5,5,5,5,5,5,6,6,6,6,6,6,6,7,  \
                      7,7,7,7,7,7}
  #elif DECDPUN==8
    #define DECDPUNMAX 99999999
    #define D2UTABLE {0,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,3,3,3,3,3,  \
                      3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,  \
                      6,6,6,6,6,7}
  #elif DECDPUN==9
    #define DECDPUNMAX 999999999
    #define D2UTABLE {0,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,3,3,3,  \
                      3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,  \
                      5,5,6,6,6,6}
  #else
    #error DECDPUN must be in the range 1-9
  #endif

  /* ----- Shared data (in decNumber.c) ----- */
  /* Public lookup table used by the D2U macro (see below)            */
  #define DECMAXD2U 49
  /*extern const uByte d2utable[DECMAXD2U+1];*/

  /* ----- Macros ----- */
  /* ISZERO -- return true if decNumber dn is a zero                  */
  /* [performance-critical in some situations]                        */
  #define ISZERO(dn) decNumberIsZero(dn)     /* now just a local name */

  /* D2U -- return the number of Units needed to hold d digits        */
  /* (runtime version, with table lookaside for small d)              */
  #if defined(DECDPUN) && DECDPUN==8
    #define D2U(d) ((unsigned)((d)<=DECMAXD2U?d2utable[d]:((d)+7)>>3))
  #elif defined(DECDPUN) && DECDPUN==4
    #define D2U(d) ((unsigned)((d)<=DECMAXD2U?d2utable[d]:((d)+3)>>2))
  #else
    #define D2U(d) ((d)<=DECMAXD2U?d2utable[d]:((d)+DECDPUN-1)/DECDPUN)
  #endif
  /* SD2U -- static D2U macro (for compile-time calculation)          */
  #define SD2U(d) (((d)+DECDPUN-1)/DECDPUN)

  /* MSUDIGITS -- returns digits in msu, from digits, calculated      */
  /* using D2U                                                        */
  #define MSUDIGITS(d) ((d)-(D2U(d)-1)*DECDPUN)

  /* D2N -- return the number of decNumber structs that would be      */
  /* needed to contain that number of digits (and the initial         */
  /* decNumber struct) safely.  Note that one Unit is included in the */
  /* initial structure.  Used for allocating space that is aligned on */
  /* a decNumber struct boundary. */
  #define D2N(d) \
    ((((SD2U(d)-1)*sizeof(Unit))+sizeof(decNumber)*2-1)/sizeof(decNumber))

  /* TODIGIT -- macro to remove the leading digit from the unsigned   */
  /* integer u at column cut (counting from the right, LSD=0) and     */
  /* place it as an ASCII character into the character pointed to by  */
  /* c.  Note that cut must be <= 9, and the maximum value for u is   */
  /* 2,000,000,000 (as is needed for negative exponents of            */
  /* subnormals).  The unsigned integer pow is used as a temporary    */
  /* variable. */
  #define TODIGIT(u, cut, c, pow) {       \
    *(c)='0';                             \
    pow=DECPOWERS[cut]*2;                 \
    if ((u)>pow) {                        \
      pow*=4;                             \
      if ((u)>=pow) {(u)-=pow; *(c)+=8;}  \
      pow/=2;                             \
      if ((u)>=pow) {(u)-=pow; *(c)+=4;}  \
      pow/=2;                             \
      }                                   \
    if ((u)>=pow) {(u)-=pow; *(c)+=2;}    \
    pow/=2;                               \
    if ((u)>=pow) {(u)-=pow; *(c)+=1;}    \
    }

  /* ---------------------------------------------------------------- */
  /* Definitions for fixed-precision modules (only valid after        */
  /* decSingle.h, decDouble.h, or decQuad.h has been included)        */
  /* ---------------------------------------------------------------- */

  /* bcdnum -- a structure describing a format-independent finite     */
  /* number, whose coefficient is a string of bcd8 uBytes             */
  typedef struct {
    uByte   *msd;             /* -> most significant digit            */
    uByte   *lsd;             /* -> least ditto                       */
    uInt     sign;            /* 0=positive, DECFLOAT_Sign=negative   */
    Int      exponent;        /* Unadjusted signed exponent (q), or   */
                              /* DECFLOAT_NaN etc. for a special      */
    } bcdnum;

  /* Test if exponent or bcdnum exponent must be a special, etc.      */
  #define EXPISSPECIAL(exp) ((exp)>=DECFLOAT_MinSp)
  #define EXPISINF(exp) (exp==DECFLOAT_Inf)
  #define EXPISNAN(exp) (exp==DECFLOAT_qNaN || exp==DECFLOAT_sNaN)
  #define NUMISSPECIAL(num) (EXPISSPECIAL((num)->exponent))

  /* Refer to a 32-bit word or byte in a decFloat (df) by big-endian  */
  /* (array) notation (the 0 word or byte contains the sign bit),     */
  /* automatically adjusting for endianness; similarly address a word */
  /* in the next-wider format (decFloatWider, or dfw)                 */
  #define DECWORDS  (DECBYTES/4)
  #define DECWWORDS (DECWBYTES/4)
  #if DECLITEND
    #define DFBYTE(df, off)   ((df)->bytes[DECBYTES-1-(off)])
    #define DFWORD(df, off)   ((df)->words[DECWORDS-1-(off)])
    #define DFWWORD(dfw, off) ((dfw)->words[DECWWORDS-1-(off)])
  #else
    #define DFBYTE(df, off)   ((df)->bytes[off])
    #define DFWORD(df, off)   ((df)->words[off])
    #define DFWWORD(dfw, off) ((dfw)->words[off])
  #endif

  /* Tests for sign or specials, directly on DECFLOATs                */
  #define DFISSIGNED(df)   (DFWORD(df, 0)&0x80000000)
  #define DFISSPECIAL(df) ((DFWORD(df, 0)&0x78000000)==0x78000000)
  #define DFISINF(df)     ((DFWORD(df, 0)&0x7c000000)==0x78000000)
  #define DFISNAN(df)     ((DFWORD(df, 0)&0x7c000000)==0x7c000000)
  #define DFISQNAN(df)    ((DFWORD(df, 0)&0x7e000000)==0x7c000000)
  #define DFISSNAN(df)    ((DFWORD(df, 0)&0x7e000000)==0x7e000000)

  /* Shared lookup tables                                             */
  extern const uInt   DECCOMBMSD[64];   /* Combination field -> MSD   */
  extern const uInt   DECCOMBFROM[48];  /* exp+msd -> Combination     */

  /* Private generic (utility) routine                                */
  #if DECCHECK || DECTRACE
    extern void decShowNum(const bcdnum *, const char *);
  #endif

  /* Format-dependent macros and constants                            */
  #if defined(DECPMAX)

    /* Useful constants                                               */
    #define DECPMAX9  (ROUNDUP(DECPMAX, 9)/9)  /* 'Pmax' in 10**9s    */
    /* Top words for a zero                                           */
    #define SINGLEZERO   0x22500000
    #define DOUBLEZERO   0x22380000
    #define QUADZERO     0x22080000
    /* [ZEROWORD is defined to be one of these in the DFISZERO macro] */

    /* Format-dependent common tests:                                 */
    /*   DFISZERO   -- test for (any) zero                            */
    /*   DFISCCZERO -- test for coefficient continuation being zero   */
    /*   DFISCC01   -- test for coefficient contains only 0s and 1s   */
    /*   DFISINT    -- test for finite and exponent q=0               */
    /*   DFISUINT01 -- test for sign=0, finite, exponent q=0, and     */
    /*                 MSD=0 or 1                                     */
    /*   ZEROWORD is also defined here.                               */
    /* In DFISZERO the first test checks the least-significant word   */
    /* (most likely to be non-zero); the penultimate tests MSD and    */
    /* DPDs in the signword, and the final test excludes specials and */
    /* MSD>7.  DFISINT similarly has to allow for the two forms of    */
    /* MSD codes.  DFISUINT01 only has to allow for one form of MSD   */
    /* code.                                                          */
    #if DECPMAX==7
      #define ZEROWORD SINGLEZERO
      /* [test macros not needed except for Zero]                     */
      #define DFISZERO(df)  ((DFWORD(df, 0)&0x1c0fffff)==0         \
                          && (DFWORD(df, 0)&0x60000000)!=0x60000000)
    #elif DECPMAX==16
      #define ZEROWORD DOUBLEZERO
      #define DFISZERO(df)  ((DFWORD(df, 1)==0                     \
                          && (DFWORD(df, 0)&0x1c03ffff)==0         \
                          && (DFWORD(df, 0)&0x60000000)!=0x60000000))
      #define DFISINT(df) ((DFWORD(df, 0)&0x63fc0000)==0x22380000  \
                         ||(DFWORD(df, 0)&0x7bfc0000)==0x6a380000)
      #define DFISUINT01(df) ((DFWORD(df, 0)&0xfbfc0000)==0x22380000)
      #define DFISCCZERO(df) (DFWORD(df, 1)==0                     \
                          && (DFWORD(df, 0)&0x0003ffff)==0)
      #define DFISCC01(df)  ((DFWORD(df, 0)&~0xfffc9124)==0        \
                          && (DFWORD(df, 1)&~0x49124491)==0)
    #elif DECPMAX==34
      #define ZEROWORD QUADZERO
      #define DFISZERO(df)  ((DFWORD(df, 3)==0                     \
                          &&  DFWORD(df, 2)==0                     \
                          &&  DFWORD(df, 1)==0                     \
                          && (DFWORD(df, 0)&0x1c003fff)==0         \
                          && (DFWORD(df, 0)&0x60000000)!=0x60000000))
      #define DFISINT(df) ((DFWORD(df, 0)&0x63ffc000)==0x22080000  \
                         ||(DFWORD(df, 0)&0x7bffc000)==0x6a080000)
      #define DFISUINT01(df) ((DFWORD(df, 0)&0xfbffc000)==0x22080000)
      #define DFISCCZERO(df) (DFWORD(df, 3)==0                     \
                          &&  DFWORD(df, 2)==0                     \
                          &&  DFWORD(df, 1)==0                     \
                          && (DFWORD(df, 0)&0x00003fff)==0)

      #define DFISCC01(df)   ((DFWORD(df, 0)&~0xffffc912)==0       \
                          &&  (DFWORD(df, 1)&~0x44912449)==0       \
                          &&  (DFWORD(df, 2)&~0x12449124)==0       \
                          &&  (DFWORD(df, 3)&~0x49124491)==0)
    #endif

    /* Macros to test if a certain 10 bits of a uInt or pair of uInts */
    /* are a canonical declet [higher or lower bits are ignored].     */
    /* declet is at offset 0 (from the right) in a uInt:              */
    #define CANONDPD(dpd) (((dpd)&0x300)==0 || ((dpd)&0x6e)!=0x6e)
    /* declet is at offset k (a multiple of 2) in a uInt:             */
    #define CANONDPDOFF(dpd, k) (((dpd)&(0x300<<(k)))==0            \
      || ((dpd)&(((uInt)0x6e)<<(k)))!=(((uInt)0x6e)<<(k)))
    /* declet is at offset k (a multiple of 2) in a pair of uInts:    */
    /* [the top 2 bits will always be in the more-significant uInt]   */
    #define CANONDPDTWO(hi, lo, k) (((hi)&(0x300>>(32-(k))))==0     \
      || ((hi)&(0x6e>>(32-(k))))!=(0x6e>>(32-(k)))                  \
      || ((lo)&(((uInt)0x6e)<<(k)))!=(((uInt)0x6e)<<(k)))

    /* Macro to test whether a full-length (length DECPMAX) BCD8      */
    /* coefficient, starting at uByte u, is all zeros                 */
    /* Test just the LSWord first, then the remainder as a sequence   */
    /* of tests in order to avoid same-level use of UBTOUI            */
    #if DECPMAX==7
      #define ISCOEFFZERO(u) (                                      \
           UBTOUI((u)+DECPMAX-4)==0                                 \
        && UBTOUS((u)+DECPMAX-6)==0                                 \
        && *(u)==0)
    #elif DECPMAX==16
      #define ISCOEFFZERO(u) (                                      \
           UBTOUI((u)+DECPMAX-4)==0                                 \
        && UBTOUI((u)+DECPMAX-8)==0                                 \
        && UBTOUI((u)+DECPMAX-12)==0                                \
        && UBTOUI(u)==0)
    #elif DECPMAX==34
      #define ISCOEFFZERO(u) (                                      \
           UBTOUI((u)+DECPMAX-4)==0                                 \
        && UBTOUI((u)+DECPMAX-8)==0                                 \
        && UBTOUI((u)+DECPMAX-12)==0                                \
        && UBTOUI((u)+DECPMAX-16)==0                                \
        && UBTOUI((u)+DECPMAX-20)==0                                \
        && UBTOUI((u)+DECPMAX-24)==0                                \
        && UBTOUI((u)+DECPMAX-28)==0                                \
        && UBTOUI((u)+DECPMAX-32)==0                                \
        && UBTOUS(u)==0)
    #endif

    /* Macros and masks for the exponent continuation field and MSD   */
    /* Get the exponent continuation from a decFloat *df as an Int    */
    #define GETECON(df) ((Int)((DFWORD((df), 0)&0x03ffffff)>>(32-6-DECECONL)))
    /* Ditto, from the next-wider format                              */
    #define GETWECON(df) ((Int)((DFWWORD((df), 0)&0x03ffffff)>>(32-6-DECWECONL)))
    /* Get the biased exponent similarly                              */
    #define GETEXP(df)  ((Int)(DECCOMBEXP[DFWORD((df), 0)>>26]+GETECON(df)))
    /* Get the unbiased exponent similarly                            */
    #define GETEXPUN(df) ((Int)GETEXP(df)-DECBIAS)
    /* Get the MSD similarly (as uInt)                                */
    #define GETMSD(df)   (DECCOMBMSD[DFWORD((df), 0)>>26])

    /* Compile-time computes of the exponent continuation field masks */
    /* full exponent continuation field:                              */
    #define ECONMASK ((0x03ffffff>>(32-6-DECECONL))<<(32-6-DECECONL))
    /* same, not including its first digit (the qNaN/sNaN selector):  */
    #define ECONNANMASK ((0x01ffffff>>(32-6-DECECONL))<<(32-6-DECECONL))

    /* Macros to decode the coefficient in a finite decFloat *df into */
    /* a BCD string (uByte *bcdin) of length DECPMAX uBytes.          */

    /* In-line sequence to convert least significant 10 bits of uInt  */
    /* dpd to three BCD8 digits starting at uByte u.  Note that an    */
    /* extra byte is written to the right of the three digits because */
    /* four bytes are moved at a time for speed; the alternative      */
    /* macro moves exactly three bytes (usually slower).              */
    #define dpd2bcd8(u, dpd)  memcpy(u, &DPD2BCD8[((dpd)&0x3ff)*4], 4)
    #define dpd2bcd83(u, dpd) memcpy(u, &DPD2BCD8[((dpd)&0x3ff)*4], 3)

    /* Decode the declets.  After extracting each one, it is decoded  */
    /* to BCD8 using a table lookup (also used for variable-length    */
    /* decode).  Each DPD decode is 3 bytes BCD8 plus a one-byte      */
    /* length which is not used, here).  Fixed-length 4-byte moves    */
    /* are fast, however, almost everywhere, and so are used except   */
    /* for the final three bytes (to avoid overrun).  The code below  */
    /* is 36 instructions for Doubles and about 70 for Quads, even    */
    /* on IA32.                                                       */

    /* Two macros are defined for each format:                        */
    /*   GETCOEFF extracts the coefficient of the current format      */
    /*   GETWCOEFF extracts the coefficient of the next-wider format. */
    /* The latter is a copy of the next-wider GETCOEFF using DFWWORD. */

    #if DECPMAX==7
    #define GETCOEFF(df, bcd) {                          \
      uInt sourhi=DFWORD(df, 0);                         \
      *(bcd)=(uByte)DECCOMBMSD[sourhi>>26];              \
      dpd2bcd8(bcd+1, sourhi>>10);                       \
      dpd2bcd83(bcd+4, sourhi);}
    #define GETWCOEFF(df, bcd) {                         \
      uInt sourhi=DFWWORD(df, 0);                        \
      uInt sourlo=DFWWORD(df, 1);                        \
      *(bcd)=(uByte)DECCOMBMSD[sourhi>>26];              \
      dpd2bcd8(bcd+1, sourhi>>8);                        \
      dpd2bcd8(bcd+4, (sourhi<<2) | (sourlo>>30));       \
      dpd2bcd8(bcd+7, sourlo>>20);                       \
      dpd2bcd8(bcd+10, sourlo>>10);                      \
      dpd2bcd83(bcd+13, sourlo);}

    #elif DECPMAX==16
    #define GETCOEFF(df, bcd) {                          \
      uInt sourhi=DFWORD(df, 0);                         \
      uInt sourlo=DFWORD(df, 1);                         \
      *(bcd)=(uByte)DECCOMBMSD[sourhi>>26];              \
      dpd2bcd8(bcd+1, sourhi>>8);                        \
      dpd2bcd8(bcd+4, (sourhi<<2) | (sourlo>>30));       \
      dpd2bcd8(bcd+7, sourlo>>20);                       \
      dpd2bcd8(bcd+10, sourlo>>10);                      \
      dpd2bcd83(bcd+13, sourlo);}
    #define GETWCOEFF(df, bcd) {                         \
      uInt sourhi=DFWWORD(df, 0);                        \
      uInt sourmh=DFWWORD(df, 1);                        \
      uInt sourml=DFWWORD(df, 2);                        \
      uInt sourlo=DFWWORD(df, 3);                        \
      *(bcd)=(uByte)DECCOMBMSD[sourhi>>26];              \
      dpd2bcd8(bcd+1, sourhi>>4);                        \
      dpd2bcd8(bcd+4, ((sourhi)<<6) | (sourmh>>26));     \
      dpd2bcd8(bcd+7, sourmh>>16);                       \
      dpd2bcd8(bcd+10, sourmh>>6);                       \
      dpd2bcd8(bcd+13, ((sourmh)<<4) | (sourml>>28));    \
      dpd2bcd8(bcd+16, sourml>>18);                      \
      dpd2bcd8(bcd+19, sourml>>8);                       \
      dpd2bcd8(bcd+22, ((sourml)<<2) | (sourlo>>30));    \
      dpd2bcd8(bcd+25, sourlo>>20);                      \
      dpd2bcd8(bcd+28, sourlo>>10);                      \
      dpd2bcd83(bcd+31, sourlo);}

    #elif DECPMAX==34
    #define GETCOEFF(df, bcd) {                          \
      uInt sourhi=DFWORD(df, 0);                         \
      uInt sourmh=DFWORD(df, 1);                         \
      uInt sourml=DFWORD(df, 2);                         \
      uInt sourlo=DFWORD(df, 3);                         \
      *(bcd)=(uByte)DECCOMBMSD[sourhi>>26];              \
      dpd2bcd8(bcd+1, sourhi>>4);                        \
      dpd2bcd8(bcd+4, ((sourhi)<<6) | (sourmh>>26));     \
      dpd2bcd8(bcd+7, sourmh>>16);                       \
      dpd2bcd8(bcd+10, sourmh>>6);                       \
      dpd2bcd8(bcd+13, ((sourmh)<<4) | (sourml>>28));    \
      dpd2bcd8(bcd+16, sourml>>18);                      \
      dpd2bcd8(bcd+19, sourml>>8);                       \
      dpd2bcd8(bcd+22, ((sourml)<<2) | (sourlo>>30));    \
      dpd2bcd8(bcd+25, sourlo>>20);                      \
      dpd2bcd8(bcd+28, sourlo>>10);                      \
      dpd2bcd83(bcd+31, sourlo);}

      #define GETWCOEFF(df, bcd) {??} /* [should never be used]       */
    #endif

    /* Macros to decode the coefficient in a finite decFloat *df into */
    /* a base-billion uInt array, with the least-significant          */
    /* 0-999999999 'digit' at offset 0.                               */

    /* Decode the declets.  After extracting each one, it is decoded  */
    /* to binary using a table lookup.  Three tables are used; one    */
    /* the usual DPD to binary, the other two pre-multiplied by 1000  */
    /* and 1000000 to avoid multiplication during decode.  These      */
    /* tables can also be used for multiplying up the MSD as the DPD  */
    /* code for 0 through 9 is the identity.                          */
    #define DPD2BIN0 DPD2BIN         /* for prettier code             */

    #if DECPMAX==7
    #define GETCOEFFBILL(df, buf) {                           \
      uInt sourhi=DFWORD(df, 0);                              \
      (buf)[0]=DPD2BIN0[sourhi&0x3ff]                         \
              +DPD2BINK[(sourhi>>10)&0x3ff]                   \
              +DPD2BINM[DECCOMBMSD[sourhi>>26]];}

    #elif DECPMAX==16
    #define GETCOEFFBILL(df, buf) {                           \
      uInt sourhi, sourlo;                                    \
      sourlo=DFWORD(df, 1);                                   \
      (buf)[0]=DPD2BIN0[sourlo&0x3ff]                         \
              +DPD2BINK[(sourlo>>10)&0x3ff]                   \
              +DPD2BINM[(sourlo>>20)&0x3ff];                  \
      sourhi=DFWORD(df, 0);                                   \
      (buf)[1]=DPD2BIN0[((sourhi<<2) | (sourlo>>30))&0x3ff]   \
              +DPD2BINK[(sourhi>>8)&0x3ff]                    \
              +DPD2BINM[DECCOMBMSD[sourhi>>26]];}

    #elif DECPMAX==34
    #define GETCOEFFBILL(df, buf) {                           \
      uInt sourhi, sourmh, sourml, sourlo;                    \
      sourlo=DFWORD(df, 3);                                   \
      (buf)[0]=DPD2BIN0[sourlo&0x3ff]                         \
              +DPD2BINK[(sourlo>>10)&0x3ff]                   \
              +DPD2BINM[(sourlo>>20)&0x3ff];                  \
      sourml=DFWORD(df, 2);                                   \
      (buf)[1]=DPD2BIN0[((sourml<<2) | (sourlo>>30))&0x3ff]   \
              +DPD2BINK[(sourml>>8)&0x3ff]                    \
              +DPD2BINM[(sourml>>18)&0x3ff];                  \
      sourmh=DFWORD(df, 1);                                   \
      (buf)[2]=DPD2BIN0[((sourmh<<4) | (sourml>>28))&0x3ff]   \
              +DPD2BINK[(sourmh>>6)&0x3ff]                    \
              +DPD2BINM[(sourmh>>16)&0x3ff];                  \
      sourhi=DFWORD(df, 0);                                   \
      (buf)[3]=DPD2BIN0[((sourhi<<6) | (sourmh>>26))&0x3ff]   \
              +DPD2BINK[(sourhi>>4)&0x3ff]                    \
              +DPD2BINM[DECCOMBMSD[sourhi>>26]];}

    #endif

    /* Macros to decode the coefficient in a finite decFloat *df into */
    /* a base-thousand uInt array (of size DECLETS+1, to allow for    */
    /* the MSD), with the least-significant 0-999 'digit' at offset 0.*/

    /* Decode the declets.  After extracting each one, it is decoded  */
    /* to binary using a table lookup.                                */
    #if DECPMAX==7
    #define GETCOEFFTHOU(df, buf) {                           \
      uInt sourhi=DFWORD(df, 0);                              \
      (buf)[0]=DPD2BIN[sourhi&0x3ff];                         \
      (buf)[1]=DPD2BIN[(sourhi>>10)&0x3ff];                   \
      (buf)[2]=DECCOMBMSD[sourhi>>26];}

    #elif DECPMAX==16
    #define GETCOEFFTHOU(df, buf) {                           \
      uInt sourhi, sourlo;                                    \
      sourlo=DFWORD(df, 1);                                   \
      (buf)[0]=DPD2BIN[sourlo&0x3ff];                         \
      (buf)[1]=DPD2BIN[(sourlo>>10)&0x3ff];                   \
      (buf)[2]=DPD2BIN[(sourlo>>20)&0x3ff];                   \
      sourhi=DFWORD(df, 0);                                   \
      (buf)[3]=DPD2BIN[((sourhi<<2) | (sourlo>>30))&0x3ff];   \
      (buf)[4]=DPD2BIN[(sourhi>>8)&0x3ff];                    \
      (buf)[5]=DECCOMBMSD[sourhi>>26];}

    #elif DECPMAX==34
    #define GETCOEFFTHOU(df, buf) {                           \
      uInt sourhi, sourmh, sourml, sourlo;                    \
      sourlo=DFWORD(df, 3);                                   \
      (buf)[0]=DPD2BIN[sourlo&0x3ff];                         \
      (buf)[1]=DPD2BIN[(sourlo>>10)&0x3ff];                   \
      (buf)[2]=DPD2BIN[(sourlo>>20)&0x3ff];                   \
      sourml=DFWORD(df, 2);                                   \
      (buf)[3]=DPD2BIN[((sourml<<2) | (sourlo>>30))&0x3ff];   \
      (buf)[4]=DPD2BIN[(sourml>>8)&0x3ff];                    \
      (buf)[5]=DPD2BIN[(sourml>>18)&0x3ff];                   \
      sourmh=DFWORD(df, 1);                                   \
      (buf)[6]=DPD2BIN[((sourmh<<4) | (sourml>>28))&0x3ff];   \
      (buf)[7]=DPD2BIN[(sourmh>>6)&0x3ff];                    \
      (buf)[8]=DPD2BIN[(sourmh>>16)&0x3ff];                   \
      sourhi=DFWORD(df, 0);                                   \
      (buf)[9]=DPD2BIN[((sourhi<<6) | (sourmh>>26))&0x3ff];   \
      (buf)[10]=DPD2BIN[(sourhi>>4)&0x3ff];                   \
      (buf)[11]=DECCOMBMSD[sourhi>>26];}
    #endif


    /* Macros to decode the coefficient in a finite decFloat *df and  */
    /* add to a base-thousand uInt array (as for GETCOEFFTHOU).       */
    /* After the addition then most significant 'digit' in the array  */
    /* might have a value larger then 10 (with a maximum of 19).      */
    #if DECPMAX==7
    #define ADDCOEFFTHOU(df, buf) {                           \
      uInt sourhi=DFWORD(df, 0);                              \
      (buf)[0]+=DPD2BIN[sourhi&0x3ff];                        \
      if (buf[0]>999) {buf[0]-=1000; buf[1]++;}               \
      (buf)[1]+=DPD2BIN[(sourhi>>10)&0x3ff];                  \
      if (buf[1]>999) {buf[1]-=1000; buf[2]++;}               \
      (buf)[2]+=DECCOMBMSD[sourhi>>26];}

    #elif DECPMAX==16
    #define ADDCOEFFTHOU(df, buf) {                           \
      uInt sourhi, sourlo;                                    \
      sourlo=DFWORD(df, 1);                                   \
      (buf)[0]+=DPD2BIN[sourlo&0x3ff];                        \
      if (buf[0]>999) {buf[0]-=1000; buf[1]++;}               \
      (buf)[1]+=DPD2BIN[(sourlo>>10)&0x3ff];                  \
      if (buf[1]>999) {buf[1]-=1000; buf[2]++;}               \
      (buf)[2]+=DPD2BIN[(sourlo>>20)&0x3ff];                  \
      if (buf[2]>999) {buf[2]-=1000; buf[3]++;}               \
      sourhi=DFWORD(df, 0);                                   \
      (buf)[3]+=DPD2BIN[((sourhi<<2) | (sourlo>>30))&0x3ff];  \
      if (buf[3]>999) {buf[3]-=1000; buf[4]++;}               \
      (buf)[4]+=DPD2BIN[(sourhi>>8)&0x3ff];                   \
      if (buf[4]>999) {buf[4]-=1000; buf[5]++;}               \
      (buf)[5]+=DECCOMBMSD[sourhi>>26];}

    #elif DECPMAX==34
    #define ADDCOEFFTHOU(df, buf) {                           \
      uInt sourhi, sourmh, sourml, sourlo;                    \
      sourlo=DFWORD(df, 3);                                   \
      (buf)[0]+=DPD2BIN[sourlo&0x3ff];                        \
      if (buf[0]>999) {buf[0]-=1000; buf[1]++;}               \
      (buf)[1]+=DPD2BIN[(sourlo>>10)&0x3ff];                  \
      if (buf[1]>999) {buf[1]-=1000; buf[2]++;}               \
      (buf)[2]+=DPD2BIN[(sourlo>>20)&0x3ff];                  \
      if (buf[2]>999) {buf[2]-=1000; buf[3]++;}               \
      sourml=DFWORD(df, 2);                                   \
      (buf)[3]+=DPD2BIN[((sourml<<2) | (sourlo>>30))&0x3ff];  \
      if (buf[3]>999) {buf[3]-=1000; buf[4]++;}               \
      (buf)[4]+=DPD2BIN[(sourml>>8)&0x3ff];                   \
      if (buf[4]>999) {buf[4]-=1000; buf[5]++;}               \
      (buf)[5]+=DPD2BIN[(sourml>>18)&0x3ff];                  \
      if (buf[5]>999) {buf[5]-=1000; buf[6]++;}               \
      sourmh=DFWORD(df, 1);                                   \
      (buf)[6]+=DPD2BIN[((sourmh<<4) | (sourml>>28))&0x3ff];  \
      if (buf[6]>999) {buf[6]-=1000; buf[7]++;}               \
      (buf)[7]+=DPD2BIN[(sourmh>>6)&0x3ff];                   \
      if (buf[7]>999) {buf[7]-=1000; buf[8]++;}               \
      (buf)[8]+=DPD2BIN[(sourmh>>16)&0x3ff];                  \
      if (buf[8]>999) {buf[8]-=1000; buf[9]++;}               \
      sourhi=DFWORD(df, 0);                                   \
      (buf)[9]+=DPD2BIN[((sourhi<<6) | (sourmh>>26))&0x3ff];  \
      if (buf[9]>999) {buf[9]-=1000; buf[10]++;}              \
      (buf)[10]+=DPD2BIN[(sourhi>>4)&0x3ff];                  \
      if (buf[10]>999) {buf[10]-=1000; buf[11]++;}            \
      (buf)[11]+=DECCOMBMSD[sourhi>>26];}
    #endif


    /* Set a decFloat to the maximum positive finite number (Nmax)    */
    #if DECPMAX==7
    #define DFSETNMAX(df)            \
      {DFWORD(df, 0)=0x77f3fcff;}
    #elif DECPMAX==16
    #define DFSETNMAX(df)            \
      {DFWORD(df, 0)=0x77fcff3f;     \
       DFWORD(df, 1)=0xcff3fcff;}
    #elif DECPMAX==34
    #define DFSETNMAX(df)            \
      {DFWORD(df, 0)=0x77ffcff3;     \
       DFWORD(df, 1)=0xfcff3fcf;     \
       DFWORD(df, 2)=0xf3fcff3f;     \
       DFWORD(df, 3)=0xcff3fcff;}
    #endif

  /* [end of format-dependent macros and constants]                   */
  #endif

#else
  #error decNumberLocal included more than once
#endif

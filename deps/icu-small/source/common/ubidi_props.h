// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2004-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  ubidi_props.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2004dec30
*   created by: Markus W. Scherer
*
*   Low-level Unicode bidi/shaping properties access.
*/

#ifndef __UBIDI_PROPS_H__
#define __UBIDI_PROPS_H__

#include "unicode/utypes.h"
#include "unicode/uset.h"
#include "putilimp.h"
#include "uset_imp.h"
#include "udataswp.h"

U_CDECL_BEGIN

/* library API -------------------------------------------------------------- */

struct UBiDiProps;
typedef struct UBiDiProps UBiDiProps;

U_CFUNC const UBiDiProps *
ubidi_getSingleton(void);

U_CFUNC void
ubidi_addPropertyStarts(const UBiDiProps *bdp, const USetAdder *sa, UErrorCode *pErrorCode);

/* property access functions */

U_CFUNC int32_t
ubidi_getMaxValue(const UBiDiProps *bdp, UProperty which);

U_CAPI UCharDirection
ubidi_getClass(const UBiDiProps *bdp, UChar32 c);

U_CFUNC UBool
ubidi_isMirrored(const UBiDiProps *bdp, UChar32 c);

U_CFUNC UChar32
ubidi_getMirror(const UBiDiProps *bdp, UChar32 c);

U_CFUNC UBool
ubidi_isBidiControl(const UBiDiProps *bdp, UChar32 c);

U_CFUNC UBool
ubidi_isJoinControl(const UBiDiProps *bdp, UChar32 c);

U_CFUNC UJoiningType
ubidi_getJoiningType(const UBiDiProps *bdp, UChar32 c);

U_CFUNC UJoiningGroup
ubidi_getJoiningGroup(const UBiDiProps *bdp, UChar32 c);

U_CFUNC UBidiPairedBracketType
ubidi_getPairedBracketType(const UBiDiProps *bdp, UChar32 c);

U_CFUNC UChar32
ubidi_getPairedBracket(const UBiDiProps *bdp, UChar32 c);

/* file definitions --------------------------------------------------------- */

#define UBIDI_DATA_NAME "ubidi"
#define UBIDI_DATA_TYPE "icu"

/* format "BiDi" */
#define UBIDI_FMT_0 0x42
#define UBIDI_FMT_1 0x69
#define UBIDI_FMT_2 0x44
#define UBIDI_FMT_3 0x69

/* indexes into indexes[] */
enum {
    UBIDI_IX_INDEX_TOP,
    UBIDI_IX_LENGTH,
    UBIDI_IX_TRIE_SIZE,
    UBIDI_IX_MIRROR_LENGTH,

    UBIDI_IX_JG_START,
    UBIDI_IX_JG_LIMIT,
    UBIDI_IX_JG_START2,  /* new in format version 2.2, ICU 54 */
    UBIDI_IX_JG_LIMIT2,

    UBIDI_MAX_VALUES_INDEX=15,
    UBIDI_IX_TOP=16
};

/* definitions for 16-bit bidi/shaping properties word ---------------------- */

enum {
 /* UBIDI_CLASS_SHIFT=0, */     /* bidi class: 5 bits (4..0) */
    UBIDI_JT_SHIFT=5,           /* joining type: 3 bits (7..5) */

    UBIDI_BPT_SHIFT=8,          /* Bidi_Paired_Bracket_Type(bpt): 2 bits (9..8) */

    UBIDI_JOIN_CONTROL_SHIFT=10,
    UBIDI_BIDI_CONTROL_SHIFT=11,

    UBIDI_IS_MIRRORED_SHIFT=12,         /* 'is mirrored' */
    UBIDI_MIRROR_DELTA_SHIFT=13,        /* bidi mirroring delta: 3 bits (15..13) */

    UBIDI_MAX_JG_SHIFT=16               /* max JG value in indexes[UBIDI_MAX_VALUES_INDEX] bits 23..16 */
};

#define UBIDI_CLASS_MASK        0x0000001f
#define UBIDI_JT_MASK           0x000000e0
#define UBIDI_BPT_MASK          0x00000300

#define UBIDI_MAX_JG_MASK       0x00ff0000

#define UBIDI_GET_CLASS(props) ((props)&UBIDI_CLASS_MASK)
#define UBIDI_GET_FLAG(props, shift) (((props)>>(shift))&1)

#if U_SIGNED_RIGHT_SHIFT_IS_ARITHMETIC
#   define UBIDI_GET_MIRROR_DELTA(props) ((int16_t)(props)>>UBIDI_MIRROR_DELTA_SHIFT)
#else
#   define UBIDI_GET_MIRROR_DELTA(props) (int16_t)(((props)&0x8000) ? (((props)>>UBIDI_MIRROR_DELTA_SHIFT)|0xe000) : ((props)>>UBIDI_MIRROR_DELTA_SHIFT))
#endif

enum {
    UBIDI_ESC_MIRROR_DELTA=-4,
    UBIDI_MIN_MIRROR_DELTA=-3,
    UBIDI_MAX_MIRROR_DELTA=3
};

/* definitions for 32-bit mirror table entry -------------------------------- */

enum {
    /* the source Unicode code point takes 21 bits (20..0) */
    UBIDI_MIRROR_INDEX_SHIFT=21,
    UBIDI_MAX_MIRROR_INDEX=0x7ff
};

#define UBIDI_GET_MIRROR_CODE_POINT(m) (UChar32)((m)&0x1fffff)

#define UBIDI_GET_MIRROR_INDEX(m) ((m)>>UBIDI_MIRROR_INDEX_SHIFT)

U_CDECL_END

#endif

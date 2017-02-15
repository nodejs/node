// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1999-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  ubidiimp.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 1999aug06
*   created by: Markus W. Scherer, updated by Matitiahu Allouche
*/

#ifndef UBIDIIMP_H
#define UBIDIIMP_H

#include "unicode/utypes.h"
#include "unicode/ubidi.h"
#include "unicode/uchar.h"
#include "ubidi_props.h"

/* miscellaneous definitions ---------------------------------------------- */

typedef uint8_t DirProp;
typedef uint32_t Flags;

/*  Comparing the description of the BiDi algorithm with this implementation
    is easier with the same names for the BiDi types in the code as there.
    See UCharDirection in uchar.h .
*/
enum {
    L=  U_LEFT_TO_RIGHT,                /*  0 */
    R=  U_RIGHT_TO_LEFT,                /*  1 */
    EN= U_EUROPEAN_NUMBER,              /*  2 */
    ES= U_EUROPEAN_NUMBER_SEPARATOR,    /*  3 */
    ET= U_EUROPEAN_NUMBER_TERMINATOR,   /*  4 */
    AN= U_ARABIC_NUMBER,                /*  5 */
    CS= U_COMMON_NUMBER_SEPARATOR,      /*  6 */
    B=  U_BLOCK_SEPARATOR,              /*  7 */
    S=  U_SEGMENT_SEPARATOR,            /*  8 */
    WS= U_WHITE_SPACE_NEUTRAL,          /*  9 */
    ON= U_OTHER_NEUTRAL,                /* 10 */
    LRE=U_LEFT_TO_RIGHT_EMBEDDING,      /* 11 */
    LRO=U_LEFT_TO_RIGHT_OVERRIDE,       /* 12 */
    AL= U_RIGHT_TO_LEFT_ARABIC,         /* 13 */
    RLE=U_RIGHT_TO_LEFT_EMBEDDING,      /* 14 */
    RLO=U_RIGHT_TO_LEFT_OVERRIDE,       /* 15 */
    PDF=U_POP_DIRECTIONAL_FORMAT,       /* 16 */
    NSM=U_DIR_NON_SPACING_MARK,         /* 17 */
    BN= U_BOUNDARY_NEUTRAL,             /* 18 */
    FSI=U_FIRST_STRONG_ISOLATE,         /* 19 */
    LRI=U_LEFT_TO_RIGHT_ISOLATE,        /* 20 */
    RLI=U_RIGHT_TO_LEFT_ISOLATE,        /* 21 */
    PDI=U_POP_DIRECTIONAL_ISOLATE,      /* 22 */
    ENL,    /* EN after W7 */           /* 23 */
    ENR,    /* EN not subject to W7 */  /* 24 */
    dirPropCount
};

/*  Sometimes, bit values are more appropriate
    to deal with directionality properties.
    Abbreviations in these macro names refer to names
    used in the BiDi algorithm.
*/
#define DIRPROP_FLAG(dir) (1UL<<(dir))
#define PURE_DIRPROP(prop)  ((prop)&~0xE0)    ?????????????????????????

/* special flag for multiple runs from explicit embedding codes */
#define DIRPROP_FLAG_MULTI_RUNS (1UL<<31)

/* are there any characters that are LTR or RTL? */
#define MASK_LTR (DIRPROP_FLAG(L)|DIRPROP_FLAG(EN)|DIRPROP_FLAG(ENL)|DIRPROP_FLAG(ENR)|DIRPROP_FLAG(AN)|DIRPROP_FLAG(LRE)|DIRPROP_FLAG(LRO)|DIRPROP_FLAG(LRI))
#define MASK_RTL (DIRPROP_FLAG(R)|DIRPROP_FLAG(AL)|DIRPROP_FLAG(RLE)|DIRPROP_FLAG(RLO)|DIRPROP_FLAG(RLI))
#define MASK_R_AL (DIRPROP_FLAG(R)|DIRPROP_FLAG(AL))
#define MASK_STRONG_EN_AN (DIRPROP_FLAG(L)|DIRPROP_FLAG(R)|DIRPROP_FLAG(AL)|DIRPROP_FLAG(EN)|DIRPROP_FLAG(AN))

/* explicit embedding codes */
#define MASK_EXPLICIT (DIRPROP_FLAG(LRE)|DIRPROP_FLAG(LRO)|DIRPROP_FLAG(RLE)|DIRPROP_FLAG(RLO)|DIRPROP_FLAG(PDF))

/* explicit isolate codes */
#define MASK_ISO (DIRPROP_FLAG(LRI)|DIRPROP_FLAG(RLI)|DIRPROP_FLAG(FSI)|DIRPROP_FLAG(PDI))

#define MASK_BN_EXPLICIT (DIRPROP_FLAG(BN)|MASK_EXPLICIT)

/* paragraph and segment separators */
#define MASK_B_S (DIRPROP_FLAG(B)|DIRPROP_FLAG(S))

/* all types that are counted as White Space or Neutral in some steps */
#define MASK_WS (MASK_B_S|DIRPROP_FLAG(WS)|MASK_BN_EXPLICIT|MASK_ISO)

/* types that are neutrals or could becomes neutrals in (Wn) */
#define MASK_POSSIBLE_N (DIRPROP_FLAG(ON)|DIRPROP_FLAG(CS)|DIRPROP_FLAG(ES)|DIRPROP_FLAG(ET)|MASK_WS)

/*
 *  These types may be changed to "e",
 *  the embedding type (L or R) of the run,
 *  in the BiDi algorithm (N2)
 */
#define MASK_EMBEDDING (DIRPROP_FLAG(NSM)|MASK_POSSIBLE_N)

/* the dirProp's L and R are defined to 0 and 1 values in UCharDirection */
#define GET_LR_FROM_LEVEL(level) ((DirProp)((level)&1))

#define IS_DEFAULT_LEVEL(level) ((level)>=0xfe)

/*
 *  The following bit is used for the directional isolate status.
 *  Stack entries corresponding to isolate sequences are greater than ISOLATE.
 */
#define ISOLATE  0x0100

U_CFUNC UBiDiLevel
ubidi_getParaLevelAtIndex(const UBiDi *pBiDi, int32_t index);

#define GET_PARALEVEL(ubidi, index) \
            ((UBiDiLevel)(!(ubidi)->defaultParaLevel || (index)<(ubidi)->paras[0].limit ? \
                         (ubidi)->paraLevel : ubidi_getParaLevelAtIndex((ubidi), (index))))

/* number of paras entries allocated initially without malloc */
#define SIMPLE_PARAS_COUNT      10
/* number of isolate entries allocated initially without malloc */
#define SIMPLE_ISOLATES_COUNT   5
/* number of isolate run entries for paired brackets allocated initially without malloc */
#define SIMPLE_OPENINGS_COUNT   20

#define CR  0x000D
#define LF  0x000A

/* Run structure for reordering --------------------------------------------- */
enum {
    LRM_BEFORE=1,
    LRM_AFTER=2,
    RLM_BEFORE=4,
    RLM_AFTER=8
};

typedef struct Para {
    int32_t limit;
    int32_t level;
} Para;

enum {                                  /* flags for Opening.flags */
    FOUND_L=DIRPROP_FLAG(L),
    FOUND_R=DIRPROP_FLAG(R)
};

typedef struct Opening {
    int32_t position;                   /* position of opening bracket */
    int32_t match;                      /* matching char or -position of closing bracket */
    int32_t contextPos;                 /* position of last strong char found before opening */
    uint16_t flags;                     /* bits for L or R/AL found within the pair */
    UBiDiDirection contextDir;          /* L or R according to last strong char before opening */
    uint8_t filler;                     /* to complete a nice multiple of 4 chars */
} Opening;

typedef struct IsoRun {
    int32_t  contextPos;                /* position of char determining context */
    uint16_t start;                     /* index of first opening entry for this run */
    uint16_t limit;                     /* index after last opening entry for this run */
    UBiDiLevel level;                   /* level of this run */
    DirProp lastStrong;                 /* bidi class of last strong char found in this run */
    DirProp lastBase;                   /* bidi class of last base char found in this run */
    UBiDiDirection contextDir;          /* L or R to use as context for following openings */
} IsoRun;

typedef struct BracketData {
    UBiDi   *pBiDi;
    /* array of opening entries which should be enough in most cases; no malloc() */
    Opening simpleOpenings[SIMPLE_OPENINGS_COUNT];
    Opening *openings;                  /* pointer to current array of entries */
    int32_t openingsCount;              /* number of allocated entries */
    int32_t isoRunLast;                 /* index of last used entry */
    /* array of nested isolated sequence entries; can never excess UBIDI_MAX_EXPLICIT_LEVEL
       + 1 for index 0, + 1 for before the first isolated sequence */
    IsoRun  isoRuns[UBIDI_MAX_EXPLICIT_LEVEL+2];
    UBool isNumbersSpecial;             /* reordering mode for NUMBERS_SPECIAL */
} BracketData;

typedef struct Isolate {
    int32_t startON;
    int32_t start1;
    int32_t state;
    int16_t stateImp;
} Isolate;

typedef struct Run {
    int32_t logicalStart,   /* first character of the run; b31 indicates even/odd level */
            visualLimit,    /* last visual position of the run +1 */
            insertRemove;   /* if >0, flags for inserting LRM/RLM before/after run,
                               if <0, count of bidi controls within run            */
} Run;

/* in a Run, logicalStart will get this bit set if the run level is odd */
#define INDEX_ODD_BIT (1UL<<31)

#define MAKE_INDEX_ODD_PAIR(index, level) ((index)|((int32_t)(level)<<31))
#define ADD_ODD_BIT_FROM_LEVEL(x, level)  ((x)|=((int32_t)(level)<<31))
#define REMOVE_ODD_BIT(x)                 ((x)&=~INDEX_ODD_BIT)

#define GET_INDEX(x)   ((x)&~INDEX_ODD_BIT)
#define GET_ODD_BIT(x) ((uint32_t)(x)>>31)
#define IS_ODD_RUN(x)  ((UBool)(((x)&INDEX_ODD_BIT)!=0))
#define IS_EVEN_RUN(x) ((UBool)(((x)&INDEX_ODD_BIT)==0))

U_CFUNC UBool
ubidi_getRuns(UBiDi *pBiDi, UErrorCode *pErrorCode);

/** BiDi control code points */
enum {
    ZWNJ_CHAR=0x200c,
    ZWJ_CHAR,
    LRM_CHAR,
    RLM_CHAR,
    LRE_CHAR=0x202a,
    RLE_CHAR,
    PDF_CHAR,
    LRO_CHAR,
    RLO_CHAR,
    LRI_CHAR=0x2066,
    RLI_CHAR,
    FSI_CHAR,
    PDI_CHAR
};

#define IS_BIDI_CONTROL_CHAR(c) (((uint32_t)(c)&0xfffffffc)==ZWNJ_CHAR || (uint32_t)((c)-LRE_CHAR)<5 || (uint32_t)((c)-LRI_CHAR)<4)

/* InsertPoints structure for noting where to put BiDi marks ---------------- */

typedef struct Point {
    int32_t pos;            /* position in text */
    int32_t flag;           /* flag for LRM/RLM, before/after */
} Point;

typedef struct InsertPoints {
    int32_t capacity;       /* number of points allocated */
    int32_t size;           /* number of points used */
    int32_t confirmed;      /* number of points confirmed */
    UErrorCode errorCode;   /* for eventual memory shortage */
    Point *points;          /* pointer to array of points */
} InsertPoints;


/* UBiDi structure ----------------------------------------------------------- */

struct UBiDi {
    /* pointer to parent paragraph object (pointer to self if this object is
     * a paragraph object); set to NULL in a newly opened object; set to a
     * real value after a successful execution of ubidi_setPara or ubidi_setLine
     */
    const UBiDi * pParaBiDi;

    const UBiDiProps *bdp;

    /* alias pointer to the current text */
    const UChar *text;

    /* length of the current text */
    int32_t originalLength;

    /* if the UBIDI_OPTION_STREAMING option is set, this is the length
     * of text actually processed by ubidi_setPara, which may be shorter than
     * the original length.
     * Otherwise, it is identical to the original length.
     */
    int32_t length;

    /* if the UBIDI_OPTION_REMOVE_CONTROLS option is set, and/or
     * marks are allowed to be inserted in one of the reordering mode, the
     * length of the result string may be different from the processed length.
     */
    int32_t resultLength;

    /* memory sizes in bytes */
    int32_t dirPropsSize, levelsSize, openingsSize, parasSize, runsSize, isolatesSize;

    /* allocated memory */
    DirProp *dirPropsMemory;
    UBiDiLevel *levelsMemory;
    Opening *openingsMemory;
    Para *parasMemory;
    Run *runsMemory;
    Isolate *isolatesMemory;

    /* indicators for whether memory may be allocated after ubidi_open() */
    UBool mayAllocateText, mayAllocateRuns;

    /* arrays with one value per text-character */
    DirProp *dirProps;
    UBiDiLevel *levels;

    /* are we performing an approximation of the "inverse BiDi" algorithm? */
    UBool isInverse;

    /* are we using the basic algorithm or its variation? */
    UBiDiReorderingMode reorderingMode;

    /* UBIDI_REORDER_xxx values must be ordered so that all the regular
     * logical to visual modes come first, and all inverse BiDi modes
     * come last.
     */
    #define UBIDI_REORDER_LAST_LOGICAL_TO_VISUAL    UBIDI_REORDER_NUMBERS_SPECIAL

    /* bitmask for reordering options */
    uint32_t reorderingOptions;

    /* must block separators receive level 0? */
    UBool orderParagraphsLTR;

    /* the paragraph level */
    UBiDiLevel paraLevel;
    /* original paraLevel when contextual */
    /* must be one of UBIDI_DEFAULT_xxx or 0 if not contextual */
    UBiDiLevel defaultParaLevel;

    /* context data */
    const UChar *prologue;
    int32_t proLength;
    const UChar *epilogue;
    int32_t epiLength;

    /* the following is set in ubidi_setPara, used in processPropertySeq */
    const struct ImpTabPair * pImpTabPair;  /* pointer to levels state table pair */

    /* the overall paragraph or line directionality - see UBiDiDirection */
    UBiDiDirection direction;

    /* flags is a bit set for which directional properties are in the text */
    Flags flags;

    /* lastArabicPos is index to the last AL in the text, -1 if none */
    int32_t lastArabicPos;

    /* characters after trailingWSStart are WS and are */
    /* implicitly at the paraLevel (rule (L1)) - levels may not reflect that */
    int32_t trailingWSStart;

    /* fields for paragraph handling */
    int32_t paraCount;                  /* set in getDirProps() */
    /* filled in getDirProps() */
    Para *paras;

    /* for relatively short text, we only need a tiny array of paras (no malloc()) */
    Para simpleParas[SIMPLE_PARAS_COUNT];

    /* fields for line reordering */
    int32_t runCount;     /* ==-1: runs not set up yet */
    Run *runs;

    /* for non-mixed text, we only need a tiny array of runs (no malloc()) */
    Run simpleRuns[1];

    /* maximum or current nesting depth of isolate sequences */
    /* Within resolveExplicitLevels() and checkExplicitLevels(), this is the maximal
       nesting encountered.
       Within resolveImplicitLevels(), this is the index of the current isolates
       stack entry. */
    int32_t isolateCount;
    Isolate *isolates;

    /* for simple text, have a small stack (no malloc()) */
    Isolate simpleIsolates[SIMPLE_ISOLATES_COUNT];

    /* for inverse Bidi with insertion of directional marks */
    InsertPoints insertPoints;

    /* for option UBIDI_OPTION_REMOVE_CONTROLS */
    int32_t controlCount;

    /* for Bidi class callback */
    UBiDiClassCallback *fnClassCallback;    /* action pointer */
    const void *coClassCallback;            /* context pointer */
};

#define IS_VALID_PARA(x) ((x) && ((x)->pParaBiDi==(x)))
#define IS_VALID_PARA_OR_LINE(x) ((x) && ((x)->pParaBiDi==(x) || (((x)->pParaBiDi) && (x)->pParaBiDi->pParaBiDi==(x)->pParaBiDi)))

typedef union {
    DirProp *dirPropsMemory;
    UBiDiLevel *levelsMemory;
    Opening *openingsMemory;
    Para *parasMemory;
    Run *runsMemory;
    Isolate *isolatesMemory;
} BidiMemoryForAllocation;

/* Macros for initial checks at function entry */
#define RETURN_IF_NULL_OR_FAILING_ERRCODE(pErrcode, retvalue)   \
        if((pErrcode)==NULL || U_FAILURE(*pErrcode)) return retvalue
#define RETURN_IF_NOT_VALID_PARA(bidi, errcode, retvalue)   \
        if(!IS_VALID_PARA(bidi)) {  \
            errcode=U_INVALID_STATE_ERROR;  \
            return retvalue;                \
        }
#define RETURN_IF_NOT_VALID_PARA_OR_LINE(bidi, errcode, retvalue)   \
        if(!IS_VALID_PARA_OR_LINE(bidi)) {  \
            errcode=U_INVALID_STATE_ERROR;  \
            return retvalue;                \
        }
#define RETURN_IF_BAD_RANGE(arg, start, limit, errcode, retvalue)   \
        if((arg)<(start) || (arg)>=(limit)) {       \
            (errcode)=U_ILLEGAL_ARGUMENT_ERROR;     \
            return retvalue;                        \
        }

#define RETURN_VOID_IF_NULL_OR_FAILING_ERRCODE(pErrcode)   \
        if((pErrcode)==NULL || U_FAILURE(*pErrcode)) return
#define RETURN_VOID_IF_NOT_VALID_PARA(bidi, errcode)   \
        if(!IS_VALID_PARA(bidi)) {  \
            errcode=U_INVALID_STATE_ERROR;  \
            return;                \
        }
#define RETURN_VOID_IF_NOT_VALID_PARA_OR_LINE(bidi, errcode)   \
        if(!IS_VALID_PARA_OR_LINE(bidi)) {  \
            errcode=U_INVALID_STATE_ERROR;  \
            return;                \
        }
#define RETURN_VOID_IF_BAD_RANGE(arg, start, limit, errcode)   \
        if((arg)<(start) || (arg)>=(limit)) {       \
            (errcode)=U_ILLEGAL_ARGUMENT_ERROR;     \
            return;                        \
        }

/* helper function to (re)allocate memory if allowed */
U_CFUNC UBool
ubidi_getMemory(BidiMemoryForAllocation *pMemory, int32_t *pSize, UBool mayAllocate, int32_t sizeNeeded);

/* helper macros for each allocated array in UBiDi */
#define getDirPropsMemory(pBiDi, length) \
        ubidi_getMemory((BidiMemoryForAllocation *)&(pBiDi)->dirPropsMemory, &(pBiDi)->dirPropsSize, \
                        (pBiDi)->mayAllocateText, (length))

#define getLevelsMemory(pBiDi, length) \
        ubidi_getMemory((BidiMemoryForAllocation *)&(pBiDi)->levelsMemory, &(pBiDi)->levelsSize, \
                        (pBiDi)->mayAllocateText, (length))

#define getRunsMemory(pBiDi, length) \
        ubidi_getMemory((BidiMemoryForAllocation *)&(pBiDi)->runsMemory, &(pBiDi)->runsSize, \
                        (pBiDi)->mayAllocateRuns, (length)*sizeof(Run))

/* additional macros used by ubidi_open() - always allow allocation */
#define getInitialDirPropsMemory(pBiDi, length) \
        ubidi_getMemory((BidiMemoryForAllocation *)&(pBiDi)->dirPropsMemory, &(pBiDi)->dirPropsSize, \
                        TRUE, (length))

#define getInitialLevelsMemory(pBiDi, length) \
        ubidi_getMemory((BidiMemoryForAllocation *)&(pBiDi)->levelsMemory, &(pBiDi)->levelsSize, \
                        TRUE, (length))

#define getInitialOpeningsMemory(pBiDi, length) \
        ubidi_getMemory((BidiMemoryForAllocation *)&(pBiDi)->openingsMemory, &(pBiDi)->openingsSize, \
                        TRUE, (length)*sizeof(Opening))

#define getInitialParasMemory(pBiDi, length) \
        ubidi_getMemory((BidiMemoryForAllocation *)&(pBiDi)->parasMemory, &(pBiDi)->parasSize, \
                        TRUE, (length)*sizeof(Para))

#define getInitialRunsMemory(pBiDi, length) \
        ubidi_getMemory((BidiMemoryForAllocation *)&(pBiDi)->runsMemory, &(pBiDi)->runsSize, \
                        TRUE, (length)*sizeof(Run))

#define getInitialIsolatesMemory(pBiDi, length) \
        ubidi_getMemory((BidiMemoryForAllocation *)&(pBiDi)->isolatesMemory, &(pBiDi)->isolatesSize, \
                        TRUE, (length)*sizeof(Isolate))

#endif

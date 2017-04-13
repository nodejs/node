// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1999-2014 International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  rbbidata.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   RBBI data formats  Includes
*
*                          Structs that describes the format of the Binary RBBI data,
*                          as it is stored in ICU's data file.
*
*      RBBIDataWrapper  -  Instances of this class sit between the
*                          raw data structs and the RulesBasedBreakIterator objects
*                          that are created by applications.  The wrapper class
*                          provides reference counting for the underlying data,
*                          and direct pointers to data that would not otherwise
*                          be accessible without ugly pointer arithmetic.  The
*                          wrapper does not attempt to provide any higher level
*                          abstractions for the data itself.
*
*                          There will be only one instance of RBBIDataWrapper for any
*                          set of RBBI run time data being shared by instances
*                          (clones) of RulesBasedBreakIterator.
*/

#ifndef __RBBIDATA_H__
#define __RBBIDATA_H__

#include "unicode/utypes.h"
#include "unicode/udata.h"
#include "udataswp.h"

/**
 * Swap RBBI data. See udataswp.h.
 * @internal
 */
U_CAPI int32_t U_EXPORT2
ubrk_swap(const UDataSwapper *ds,
          const void *inData, int32_t length, void *outData,
          UErrorCode *pErrorCode);

#ifdef __cplusplus

#include "unicode/uobject.h"
#include "unicode/unistr.h"
#include "umutex.h"
#include "utrie.h"

U_NAMESPACE_BEGIN

/*
 *   The following structs map exactly onto the raw data from ICU common data file.
 */
struct RBBIDataHeader {
    uint32_t         fMagic;           /*  == 0xbla0                                               */
    uint8_t          fFormatVersion[4]; /* Data Format.  Same as the value in struct UDataInfo      */
                                       /*   if there is one associated with this data.             */
                                       /*     (version originates in rbbi, is copied to UDataInfo) */
                                       /*   For ICU 3.2 and earlier, this field was                */
                                       /*       uint32_t  fVersion                                 */
                                       /*   with a value of 1.                                     */
    uint32_t         fLength;          /*  Total length in bytes of this RBBI Data,                */
                                       /*      including all sections, not just the header.        */
    uint32_t         fCatCount;        /*  Number of character categories.                         */

    /*                                                                        */
    /*  Offsets and sizes of each of the subsections within the RBBI data.    */
    /*  All offsets are bytes from the start of the RBBIDataHeader.           */
    /*  All sizes are in bytes.                                               */
    /*                                                                        */
    uint32_t         fFTable;         /*  forward state transition table. */
    uint32_t         fFTableLen;
    uint32_t         fRTable;         /*  Offset to the reverse state transition table. */
    uint32_t         fRTableLen;
    uint32_t         fSFTable;        /*  safe point forward transition table */
    uint32_t         fSFTableLen;
    uint32_t         fSRTable;        /*  safe point reverse transition table */
    uint32_t         fSRTableLen;
    uint32_t         fTrie;           /*  Offset to Trie data for character categories */
    uint32_t         fTrieLen;
    uint32_t         fRuleSource;     /*  Offset to the source for for the break */
    uint32_t         fRuleSourceLen;  /*    rules.  Stored UChar *. */
    uint32_t         fStatusTable;    /* Offset to the table of rule status values */
    uint32_t         fStatusTableLen;

    uint32_t         fReserved[6];    /*  Reserved for expansion */

};



struct  RBBIStateTableRow {
    int16_t          fAccepting;    /*  Non-zero if this row is for an accepting state.   */
                                    /*  Value 0: not an accepting state.                  */
                                    /*       -1: Unconditional Accepting state.           */
                                    /*    positive:  Look-ahead match has completed.      */
                                    /*           Actual boundary position happened earlier */
                                    /*           Value here == fLookAhead in earlier      */
                                    /*              state, at actual boundary pos.        */
    int16_t          fLookAhead;    /*  Non-zero if this row is for a state that          */
                                    /*    corresponds to a '/' in the rule source.        */
                                    /*    Value is the same as the fAccepting             */
                                    /*      value for the rule (which will appear         */
                                    /*      in a different state.                         */
    int16_t          fTagIdx;       /*  Non-zero if this row covers a {tagged} position   */
                                    /*     from a rule.  Value is the index in the        */
                                    /*     StatusTable of the set of matching             */
                                    /*     tags (rule status values)                      */
    int16_t          fReserved;
    uint16_t         fNextState[2]; /*  Next State, indexed by char category.             */
                                    /*  This array does not have two elements             */
                                    /*    Array Size is actually fData->fHeader->fCatCount         */
                                    /*    CAUTION:  see RBBITableBuilder::getTableSize()  */
                                    /*              before changing anything here.        */
};


struct RBBIStateTable {
    uint32_t         fNumStates;    /*  Number of states.                                 */
    uint32_t         fRowLen;       /*  Length of a state table row, in bytes.            */
    uint32_t         fFlags;        /*  Option Flags for this state table                 */
    uint32_t         fReserved;     /*  reserved                                          */
    char             fTableData[4]; /*  First RBBIStateTableRow begins here.              */
                                    /*    (making it char[] simplifies ugly address       */
                                    /*     arithmetic for indexing variable length rows.) */
};

typedef enum {
    RBBI_LOOKAHEAD_HARD_BREAK = 1,
    RBBI_BOF_REQUIRED = 2
} RBBIStateTableFlags;


/*                                        */
/*   The reference counting wrapper class */
/*                                        */
class RBBIDataWrapper : public UMemory {
public:
    enum EDontAdopt {
        kDontAdopt
    };
    RBBIDataWrapper(const RBBIDataHeader *data, UErrorCode &status);
    RBBIDataWrapper(const RBBIDataHeader *data, enum EDontAdopt dontAdopt, UErrorCode &status);
    RBBIDataWrapper(UDataMemory* udm, UErrorCode &status);
    ~RBBIDataWrapper();

    void                  init0();
    void                  init(const RBBIDataHeader *data, UErrorCode &status);
    RBBIDataWrapper      *addReference();
    void                  removeReference();
    UBool                 operator ==(const RBBIDataWrapper &other) const;
    int32_t               hashCode();
    const UnicodeString  &getRuleSourceString() const;
#ifdef RBBI_DEBUG
    void                  printData();
    void                  printTable(const char *heading, const RBBIStateTable *table);
#else
    #define printData()
    #define printTable(heading, table)
#endif

    /*                                     */
    /*   Pointers to items within the data */
    /*                                     */
    const RBBIDataHeader     *fHeader;
    const RBBIStateTable     *fForwardTable;
    const RBBIStateTable     *fReverseTable;
    const RBBIStateTable     *fSafeFwdTable;
    const RBBIStateTable     *fSafeRevTable;
    const UChar              *fRuleSource;
    const int32_t            *fRuleStatusTable;

    /* number of int32_t values in the rule status table.   Used to sanity check indexing */
    int32_t             fStatusMaxIdx;

    UTrie               fTrie;

private:
    u_atomic_int32_t    fRefCount;
    UDataMemory  *fUDataMem;
    UnicodeString       fRuleString;
    UBool               fDontFreeData;

    RBBIDataWrapper(const RBBIDataWrapper &other); /*  forbid copying of this class */
    RBBIDataWrapper &operator=(const RBBIDataWrapper &other); /*  forbid copying of this class */
};



U_NAMESPACE_END

#endif /* C++ */

#endif

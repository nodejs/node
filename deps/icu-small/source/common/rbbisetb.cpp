// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
//
//  rbbisetb.cpp
//
/*
***************************************************************************
*   Copyright (C) 2002-2008 International Business Machines Corporation   *
*   and others. All rights reserved.                                      *
***************************************************************************
*/
//
//  RBBISetBuilder   Handles processing of Unicode Sets from RBBI rules
//                   (part of the rule building process.)
//
//      Starting with the rules parse tree from the scanner,
//
//                   -  Enumerate the set of UnicodeSets that are referenced
//                      by the RBBI rules.
//                   -  compute a set of non-overlapping character ranges
//                      with all characters within a range belonging to the same
//                      set of input uniocde sets.
//                   -  Derive a set of non-overlapping UnicodeSet (like things)
//                      that will correspond to columns in the state table for
//                      the RBBI execution engine.  All characters within one
//                      of these sets belong to the same set of the original
//                      UnicodeSets from the user's rules.
//                   -  construct the trie table that maps input characters
//                      to the index of the matching non-overlapping set of set from
//                      the previous step.
//

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/uniset.h"
#include "utrie2.h"
#include "uvector.h"
#include "uassert.h"
#include "cmemory.h"
#include "cstring.h"

#include "rbbisetb.h"
#include "rbbinode.h"

U_NAMESPACE_BEGIN

//------------------------------------------------------------------------
//
//   Constructor
//
//------------------------------------------------------------------------
RBBISetBuilder::RBBISetBuilder(RBBIRuleBuilder *rb)
{
    fRB             = rb;
    fStatus         = rb->fStatus;
    fRangeList      = 0;
    fTrie           = 0;
    fTrieSize       = 0;
    fGroupCount     = 0;
    fSawBOF         = FALSE;
}


//------------------------------------------------------------------------
//
//   Destructor
//
//------------------------------------------------------------------------
RBBISetBuilder::~RBBISetBuilder()
{
    RangeDescriptor   *nextRangeDesc;

    // Walk through & delete the linked list of RangeDescriptors
    for (nextRangeDesc = fRangeList; nextRangeDesc!=NULL;) {
        RangeDescriptor *r = nextRangeDesc;
        nextRangeDesc      = r->fNext;
        delete r;
    }

    utrie2_close(fTrie);
}




//------------------------------------------------------------------------
//
//   build          Build the list of non-overlapping character ranges
//                  from the Unicode Sets.
//
//------------------------------------------------------------------------
void RBBISetBuilder::build() {
    RBBINode        *usetNode;
    RangeDescriptor *rlRange;

    if (fRB->fDebugEnv && uprv_strstr(fRB->fDebugEnv, "usets")) {printSets();}

    //
    //  Initialize the process by creating a single range encompassing all characters
    //  that is in no sets.
    //
    fRangeList                = new RangeDescriptor(*fStatus); // will check for status here
    if (fRangeList == NULL) {
        *fStatus = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    fRangeList->fStartChar    = 0;
    fRangeList->fEndChar      = 0x10ffff;

    if (U_FAILURE(*fStatus)) {
        return;
    }

    //
    //  Find the set of non-overlapping ranges of characters
    //
    int  ni;
    for (ni=0; ; ni++) {        // Loop over each of the UnicodeSets encountered in the input rules
        usetNode = (RBBINode *)this->fRB->fUSetNodes->elementAt(ni);
        if (usetNode==NULL) {
            break;
        }

        UnicodeSet      *inputSet             = usetNode->fInputSet;
        int32_t          inputSetRangeCount   = inputSet->getRangeCount();
        int              inputSetRangeIndex   = 0;
                         rlRange              = fRangeList;

        for (;;) {
            if (inputSetRangeIndex >= inputSetRangeCount) {
                break;
            }
            UChar32      inputSetRangeBegin  = inputSet->getRangeStart(inputSetRangeIndex);
            UChar32      inputSetRangeEnd    = inputSet->getRangeEnd(inputSetRangeIndex);

            // skip over ranges from the range list that are completely
            //   below the current range from the input unicode set.
            while (rlRange->fEndChar < inputSetRangeBegin) {
                rlRange = rlRange->fNext;
            }

            // If the start of the range from the range list is before with
            //   the start of the range from the unicode set, split the range list range
            //   in two, with one part being before (wholly outside of) the unicode set
            //   and the other containing the rest.
            //   Then continue the loop; the post-split current range will then be skipped
            //     over
            if (rlRange->fStartChar < inputSetRangeBegin) {
                rlRange->split(inputSetRangeBegin, *fStatus);
                if (U_FAILURE(*fStatus)) {
                    return;
                }
                continue;
            }

            // Same thing at the end of the ranges...
            // If the end of the range from the range list doesn't coincide with
            //   the end of the range from the unicode set, split the range list
            //   range in two.  The first part of the split range will be
            //   wholly inside the Unicode set.
            if (rlRange->fEndChar > inputSetRangeEnd) {
                rlRange->split(inputSetRangeEnd+1, *fStatus);
                if (U_FAILURE(*fStatus)) {
                    return;
                }
            }

            // The current rlRange is now entirely within the UnicodeSet range.
            // Add this unicode set to the list of sets for this rlRange
            if (rlRange->fIncludesSets->indexOf(usetNode) == -1) {
                rlRange->fIncludesSets->addElement(usetNode, *fStatus);
                if (U_FAILURE(*fStatus)) {
                    return;
                }
            }

            // Advance over ranges that we are finished with.
            if (inputSetRangeEnd == rlRange->fEndChar) {
                inputSetRangeIndex++;
            }
            rlRange = rlRange->fNext;
        }
    }

    if (fRB->fDebugEnv && uprv_strstr(fRB->fDebugEnv, "range")) { printRanges();}

    //
    //  Group the above ranges, with each group consisting of one or more
    //    ranges that are in exactly the same set of original UnicodeSets.
    //    The groups are numbered, and these group numbers are the set of
    //    input symbols recognized by the run-time state machine.
    //
    //    Numbering: # 0  (state table column 0) is unused.
    //               # 1  is reserved - table column 1 is for end-of-input
    //               # 2  is reserved - table column 2 is for beginning-in-input
    //               # 3  is the first range list.
    //
    RangeDescriptor *rlSearchRange;
    for (rlRange = fRangeList; rlRange!=0; rlRange=rlRange->fNext) {
        for (rlSearchRange=fRangeList; rlSearchRange != rlRange; rlSearchRange=rlSearchRange->fNext) {
            if (rlRange->fIncludesSets->equals(*rlSearchRange->fIncludesSets)) {
                rlRange->fNum = rlSearchRange->fNum;
                break;
            }
        }
        if (rlRange->fNum == 0) {
            fGroupCount ++;
            rlRange->fNum = fGroupCount+2;
            rlRange->setDictionaryFlag();
            addValToSets(rlRange->fIncludesSets, fGroupCount+2);
        }
    }

    // Handle input sets that contain the special string {eof}.
    //   Column 1 of the state table is reserved for EOF on input.
    //   Column 2 is reserved for before-the-start-input.
    //            (This column can be optimized away later if there are no rule
    //             references to {bof}.)
    //   Add this column value (1 or 2) to the equivalent expression
    //     subtree for each UnicodeSet that contains the string {eof}
    //   Because {bof} and {eof} are not a characters in the normal sense,
    //   they doesn't affect the computation of ranges or TRIE.
    static const UChar eofUString[] = {0x65, 0x6f, 0x66, 0};
    static const UChar bofUString[] = {0x62, 0x6f, 0x66, 0};

    UnicodeString eofString(eofUString);
    UnicodeString bofString(bofUString);
    for (ni=0; ; ni++) {        // Loop over each of the UnicodeSets encountered in the input rules
        usetNode = (RBBINode *)this->fRB->fUSetNodes->elementAt(ni);
        if (usetNode==NULL) {
            break;
        }
        UnicodeSet      *inputSet = usetNode->fInputSet;
        if (inputSet->contains(eofString)) {
            addValToSet(usetNode, 1);
        }
        if (inputSet->contains(bofString)) {
            addValToSet(usetNode, 2);
            fSawBOF = TRUE;
        }
    }


    if (fRB->fDebugEnv && uprv_strstr(fRB->fDebugEnv, "rgroup")) {printRangeGroups();}
    if (fRB->fDebugEnv && uprv_strstr(fRB->fDebugEnv, "esets")) {printSets();}

    //
    // Build the Trie table for mapping UChar32 values to the corresponding
    //   range group number
    //
    fTrie = utrie2_open(0,       //  Initial value for all code points.
                        0,       //  Error value for out-of-range input.
                        fStatus);

    for (rlRange = fRangeList; rlRange!=0 && U_SUCCESS(*fStatus); rlRange=rlRange->fNext) {
        utrie2_setRange32(fTrie,
                          rlRange->fStartChar,     // Range start
                          rlRange->fEndChar,       // Range end (inclusive)
                          rlRange->fNum,           // value for range
                          TRUE,                    // Overwrite previously written values
                          fStatus);
    }
}


//-----------------------------------------------------------------------------------
//
//  getTrieSize()    Return the size that will be required to serialize the Trie.
//
//-----------------------------------------------------------------------------------
int32_t RBBISetBuilder::getTrieSize()  {
    if (U_FAILURE(*fStatus)) {
        return 0;
    }
    utrie2_freeze(fTrie, UTRIE2_16_VALUE_BITS, fStatus);
    fTrieSize  = utrie2_serialize(fTrie,
                                  NULL,                // Buffer
                                  0,                   // Capacity
                                  fStatus);
    if (*fStatus == U_BUFFER_OVERFLOW_ERROR) {
        *fStatus = U_ZERO_ERROR;
    }
    // RBBIDebugPrintf("Trie table size is %d\n", trieSize);
    return fTrieSize;
}


//-----------------------------------------------------------------------------------
//
//  serializeTrie()   Put the serialized trie at the specified address.
//                    Trust the caller to have given us enough memory.
//                    getTrieSize() MUST be called first.
//
//-----------------------------------------------------------------------------------
void RBBISetBuilder::serializeTrie(uint8_t *where) {
    utrie2_serialize(fTrie,
                     where,                   // Buffer
                     fTrieSize,               // Capacity
                     fStatus);
}

//------------------------------------------------------------------------
//
//  addValToSets     Add a runtime-mapped input value to each uset from a
//                   list of uset nodes. (val corresponds to a state table column.)
//                   For each of the original Unicode sets - which correspond
//                   directly to uset nodes - a logically equivalent expression
//                   is constructed in terms of the remapped runtime input
//                   symbol set.  This function adds one runtime input symbol to
//                   a list of sets.
//
//                   The "logically equivalent expression" is the tree for an
//                   or-ing together of all of the symbols that go into the set.
//
//------------------------------------------------------------------------
void  RBBISetBuilder::addValToSets(UVector *sets, uint32_t val) {
    int32_t       ix;

    for (ix=0; ix<sets->size(); ix++) {
        RBBINode *usetNode = (RBBINode *)sets->elementAt(ix);
        addValToSet(usetNode, val);
    }
}

void  RBBISetBuilder::addValToSet(RBBINode *usetNode, uint32_t val) {
    RBBINode *leafNode = new RBBINode(RBBINode::leafChar);
    if (leafNode == NULL) {
        *fStatus = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    leafNode->fVal = (unsigned short)val;
    if (usetNode->fLeftChild == NULL) {
        usetNode->fLeftChild = leafNode;
        leafNode->fParent    = usetNode;
    } else {
        // There are already input symbols present for this set.
        // Set up an OR node, with the previous stuff as the left child
        //   and the new value as the right child.
        RBBINode *orNode = new RBBINode(RBBINode::opOr);
        if (orNode == NULL) {
            *fStatus = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        orNode->fLeftChild  = usetNode->fLeftChild;
        orNode->fRightChild = leafNode;
        orNode->fLeftChild->fParent  = orNode;
        orNode->fRightChild->fParent = orNode;
        usetNode->fLeftChild = orNode;
        orNode->fParent = usetNode;
    }
}


//------------------------------------------------------------------------
//
//   getNumCharCategories
//
//------------------------------------------------------------------------
int32_t  RBBISetBuilder::getNumCharCategories() const {
    return fGroupCount + 3;
}


//------------------------------------------------------------------------
//
//   sawBOF
//
//------------------------------------------------------------------------
UBool  RBBISetBuilder::sawBOF() const {
    return fSawBOF;
}


//------------------------------------------------------------------------
//
//   getFirstChar      Given a runtime RBBI character category, find
//                     the first UChar32 that is in the set of chars
//                     in the category.
//------------------------------------------------------------------------
UChar32  RBBISetBuilder::getFirstChar(int32_t category) const {
    RangeDescriptor   *rlRange;
    UChar32            retVal = (UChar32)-1;
    for (rlRange = fRangeList; rlRange!=0; rlRange=rlRange->fNext) {
        if (rlRange->fNum == category) {
            retVal = rlRange->fStartChar;
            break;
        }
    }
    return retVal;
}



//------------------------------------------------------------------------
//
//   printRanges        A debugging function.
//                      dump out all of the range definitions.
//
//------------------------------------------------------------------------
#ifdef RBBI_DEBUG
void RBBISetBuilder::printRanges() {
    RangeDescriptor       *rlRange;
    int                    i;

    RBBIDebugPrintf("\n\n Nonoverlapping Ranges ...\n");
    for (rlRange = fRangeList; rlRange!=0; rlRange=rlRange->fNext) {
        RBBIDebugPrintf("%2i  %4x-%4x  ", rlRange->fNum, rlRange->fStartChar, rlRange->fEndChar);

        for (i=0; i<rlRange->fIncludesSets->size(); i++) {
            RBBINode       *usetNode    = (RBBINode *)rlRange->fIncludesSets->elementAt(i);
            UnicodeString   setName = UNICODE_STRING("anon", 4);
            RBBINode       *setRef = usetNode->fParent;
            if (setRef != NULL) {
                RBBINode *varRef = setRef->fParent;
                if (varRef != NULL  &&  varRef->fType == RBBINode::varRef) {
                    setName = varRef->fText;
                }
            }
            RBBI_DEBUG_printUnicodeString(setName); RBBIDebugPrintf("  ");
        }
        RBBIDebugPrintf("\n");
    }
}
#endif


//------------------------------------------------------------------------
//
//   printRangeGroups     A debugging function.
//                        dump out all of the range groups.
//
//------------------------------------------------------------------------
#ifdef RBBI_DEBUG
void RBBISetBuilder::printRangeGroups() {
    RangeDescriptor       *rlRange;
    RangeDescriptor       *tRange;
    int                    i;
    int                    lastPrintedGroupNum = 0;

    RBBIDebugPrintf("\nRanges grouped by Unicode Set Membership...\n");
    for (rlRange = fRangeList; rlRange!=0; rlRange=rlRange->fNext) {
        int groupNum = rlRange->fNum & 0xbfff;
        if (groupNum > lastPrintedGroupNum) {
            lastPrintedGroupNum = groupNum;
            RBBIDebugPrintf("%2i  ", groupNum);

            if (rlRange->fNum & 0x4000) { RBBIDebugPrintf(" <DICT> ");}

            for (i=0; i<rlRange->fIncludesSets->size(); i++) {
                RBBINode       *usetNode    = (RBBINode *)rlRange->fIncludesSets->elementAt(i);
                UnicodeString   setName = UNICODE_STRING("anon", 4);
                RBBINode       *setRef = usetNode->fParent;
                if (setRef != NULL) {
                    RBBINode *varRef = setRef->fParent;
                    if (varRef != NULL  &&  varRef->fType == RBBINode::varRef) {
                        setName = varRef->fText;
                    }
                }
                RBBI_DEBUG_printUnicodeString(setName); RBBIDebugPrintf(" ");
            }

            i = 0;
            for (tRange = rlRange; tRange != 0; tRange = tRange->fNext) {
                if (tRange->fNum == rlRange->fNum) {
                    if (i++ % 5 == 0) {
                        RBBIDebugPrintf("\n    ");
                    }
                    RBBIDebugPrintf("  %05x-%05x", tRange->fStartChar, tRange->fEndChar);
                }
            }
            RBBIDebugPrintf("\n");
        }
    }
    RBBIDebugPrintf("\n");
}
#endif


//------------------------------------------------------------------------
//
//   printSets          A debugging function.
//                      dump out all of the set definitions.
//
//------------------------------------------------------------------------
#ifdef RBBI_DEBUG
void RBBISetBuilder::printSets() {
    int                   i;

    RBBIDebugPrintf("\n\nUnicode Sets List\n------------------\n");
    for (i=0; ; i++) {
        RBBINode        *usetNode;
        RBBINode        *setRef;
        RBBINode        *varRef;
        UnicodeString    setName;

        usetNode = (RBBINode *)fRB->fUSetNodes->elementAt(i);
        if (usetNode == NULL) {
            break;
        }

        RBBIDebugPrintf("%3d    ", i);
        setName = UNICODE_STRING("anonymous", 9);
        setRef = usetNode->fParent;
        if (setRef != NULL) {
            varRef = setRef->fParent;
            if (varRef != NULL  &&  varRef->fType == RBBINode::varRef) {
                setName = varRef->fText;
            }
        }
        RBBI_DEBUG_printUnicodeString(setName);
        RBBIDebugPrintf("   ");
        RBBI_DEBUG_printUnicodeString(usetNode->fText);
        RBBIDebugPrintf("\n");
        if (usetNode->fLeftChild != NULL) {
            RBBINode::printTree(usetNode->fLeftChild, TRUE);
        }
    }
    RBBIDebugPrintf("\n");
}
#endif



//-------------------------------------------------------------------------------------
//
//  RangeDescriptor copy constructor
//
//-------------------------------------------------------------------------------------

RangeDescriptor::RangeDescriptor(const RangeDescriptor &other, UErrorCode &status) {
    int  i;

    this->fStartChar    = other.fStartChar;
    this->fEndChar      = other.fEndChar;
    this->fNum          = other.fNum;
    this->fNext         = NULL;
    UErrorCode oldstatus = status;
    this->fIncludesSets = new UVector(status);
    if (U_FAILURE(oldstatus)) {
        status = oldstatus;
    }
    if (U_FAILURE(status)) {
        return;
    }
    /* test for NULL */
    if (this->fIncludesSets == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    for (i=0; i<other.fIncludesSets->size(); i++) {
        this->fIncludesSets->addElement(other.fIncludesSets->elementAt(i), status);
    }
}


//-------------------------------------------------------------------------------------
//
//  RangeDesriptor default constructor
//
//-------------------------------------------------------------------------------------
RangeDescriptor::RangeDescriptor(UErrorCode &status) {
    this->fStartChar    = 0;
    this->fEndChar      = 0;
    this->fNum          = 0;
    this->fNext         = NULL;
    UErrorCode oldstatus = status;
    this->fIncludesSets = new UVector(status);
    if (U_FAILURE(oldstatus)) {
        status = oldstatus;
    }
    if (U_FAILURE(status)) {
        return;
    }
    /* test for NULL */
    if(this->fIncludesSets == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

}


//-------------------------------------------------------------------------------------
//
//  RangeDesriptor Destructor
//
//-------------------------------------------------------------------------------------
RangeDescriptor::~RangeDescriptor() {
    delete  fIncludesSets;
    fIncludesSets = NULL;
}

//-------------------------------------------------------------------------------------
//
//  RangeDesriptor::split()
//
//-------------------------------------------------------------------------------------
void RangeDescriptor::split(UChar32 where, UErrorCode &status) {
    U_ASSERT(where>fStartChar && where<=fEndChar);
    RangeDescriptor *nr = new RangeDescriptor(*this, status);
    if(nr == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    if (U_FAILURE(status)) {
        delete nr;
        return;
    }
    //  RangeDescriptor copy constructor copies all fields.
    //  Only need to update those that are different after the split.
    nr->fStartChar = where;
    this->fEndChar = where-1;
    nr->fNext      = this->fNext;
    this->fNext    = nr;
}


//-------------------------------------------------------------------------------------
//
//   RangeDescriptor::setDictionaryFlag
//
//            Character Category Numbers that include characters from
//            the original Unicode Set named "dictionary" have bit 14
//            set to 1.  The RBBI runtime engine uses this to trigger
//            use of the word dictionary.
//
//            This function looks through the Unicode Sets that it
//            (the range) includes, and sets the bit in fNum when
//            "dictionary" is among them.
//
//            TODO:  a faster way would be to find the set node for
//                   "dictionary" just once, rather than looking it
//                   up by name every time.
//
//-------------------------------------------------------------------------------------
void RangeDescriptor::setDictionaryFlag() {
    int i;

    for (i=0; i<this->fIncludesSets->size(); i++) {
        RBBINode       *usetNode    = (RBBINode *)fIncludesSets->elementAt(i);
        UnicodeString   setName;
        RBBINode       *setRef = usetNode->fParent;
        if (setRef != NULL) {
            RBBINode *varRef = setRef->fParent;
            if (varRef != NULL  &&  varRef->fType == RBBINode::varRef) {
                setName = varRef->fText;
            }
        }
        if (setName.compare(UNICODE_STRING("dictionary", 10)) == 0) {   // TODO:  no string literals.
            this->fNum |= 0x4000;
            break;
        }
    }
}



U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */

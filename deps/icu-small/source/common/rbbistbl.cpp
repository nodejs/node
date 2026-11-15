// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
//
//  file:  rbbistbl.cpp    Implementation of the ICU RBBISymbolTable class
//
/*
***************************************************************************
*   Copyright (C) 2002-2014 International Business Machines Corporation
*   and others. All rights reserved.
***************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/unistr.h"
#include "unicode/uniset.h"
#include "unicode/uchar.h"
#include "unicode/parsepos.h"

#include "cstr.h"
#include "rbbinode.h"
#include "rbbirb.h"
#include "umutex.h"


//
//  RBBISymbolTableEntry_deleter    Used by the UHashTable to delete the contents
//                                  when the hash table is deleted.
//
U_CDECL_BEGIN
static void U_CALLCONV RBBISymbolTableEntry_deleter(void *p) {
    icu::RBBISymbolTableEntry *px = (icu::RBBISymbolTableEntry *)p;
    delete px;
}
U_CDECL_END



U_NAMESPACE_BEGIN

RBBISymbolTable::RBBISymbolTable(RBBIRuleScanner *rs, const UnicodeString &rules, UErrorCode &status)
    : fRules(rules), fRuleScanner(rs)
{
    fHashTable       = nullptr;

    fHashTable = uhash_open(uhash_hashUnicodeString, uhash_compareUnicodeString, nullptr, &status);
    // uhash_open checks status
    if (U_FAILURE(status)) {
        return;
    }
    uhash_setValueDeleter(fHashTable, RBBISymbolTableEntry_deleter);
}



RBBISymbolTable::~RBBISymbolTable()
{
    uhash_close(fHashTable);
}


//
//  RBBISymbolTable::lookup       This function from the abstract symbol table interface
//                                looks up a variable name and returns a UnicodeString
//                                containing the substitution text.
//
//                                The variable name does NOT include the leading $.
//
const UnicodeString  *RBBISymbolTable::lookup(const UnicodeString& s) const
{
    const RBBISymbolTableEntry* const el =
        static_cast<const RBBISymbolTableEntry*>(uhash_get(fHashTable, &s));
    if (el == nullptr) {
        return nullptr;
    }
    const RBBINode& exprNode = *el->val->fLeftChild; // Root node of expression for variable
    // Return the original source string for the expression.
    // Note that for set-valued variables used in UnicodeSet expressions, this would be rejected by
    // the UnicodeSet parser if the source itself contains variable references.  For instance, with
    //     $CaseIgnorable   = [[:Mn:][:Me:][:Cf:][:Lm:][:Sk:] \u0027 \u00AD \u2019];
    //     $Cased = [[:Upper_Case:][:Lower_Case:][:Lt:] - $CaseIgnorable];
    // If lookupSet were not overridden, when parsing the right-hand side of
    //     $NotCased        = [[^ $Cased] - $CaseIgnorable];
    // there would be a call to lookup("Cased") which would return
    //     "[[:Upper_Case:][:Lower_Case:][:Lt:]-$CaseIgnorable]". This contains a variable, which is
    // disallowed by the UnicodeSet parser inside a variable expansion.
    // However, set-valued variables are pre-parsed, and returned by lookupSet instead, so this call
    // to lookup() never happens; instead, lookupSet("CaseIgnorable") is called when computing
    // $Cased and returns the non-null value of $CaseIgnorable, and then when computing $NotCased,
    // lookupSet("Cased") returns the value computed for $Cased.
    return &exprNode.fText;
}

const UnicodeSet* RBBISymbolTable::lookupSet(const UnicodeString& s) const {
    const RBBISymbolTableEntry* const el = static_cast<const RBBISymbolTableEntry*>(uhash_get(fHashTable, &s));
    if (el == nullptr) {
        return nullptr;
    }
    const RBBINode& exprNode = *el->val->fLeftChild;
    if (exprNode.fType == RBBINode::setRef) {
        return exprNode.fLeftChild->fInputSet;
    } else {
        return nullptr;
    }
}



//  No longer used, see ICU-23297.
const UnicodeFunctor* RBBISymbolTable::lookupMatcher(UChar32 /*ch*/) const {
    return nullptr;
}

//
// RBBISymbolTable::parseReference   This function from the abstract symbol table interface
//                                   looks for a $variable name in the source text.
//                                   It does not look it up, only scans for it.
//                                   It is used by the UnicodeSet parser.
//
//                                   This implementation is lifted pretty much verbatim
//                                   from the rules based transliterator implementation.
//                                   I didn't see an obvious way of sharing it.
//
UnicodeString   RBBISymbolTable::parseReference(const UnicodeString& text,
                                                ParsePosition& pos, int32_t limit) const
{
    int32_t start = pos.getIndex();
    int32_t i = start;
    UnicodeString result;
    while (i < limit) {
        char16_t c = text.charAt(i);
        if ((i==start && !u_isIDStart(c)) || !u_isIDPart(c)) {
            break;
        }
        ++i;
    }
    if (i == start) { // No valid name chars
        return result; // Indicate failure with empty string
    }
    pos.setIndex(i);
    text.extractBetween(start, i, result);
    return result;
}



//
// RBBISymbolTable::lookupNode      Given a key (a variable name), return the
//                                  corresponding RBBI Node.  If there is no entry
//                                  in the table for this name, return nullptr.
//
RBBINode       *RBBISymbolTable::lookupNode(const UnicodeString &key) const{

    RBBINode             *retNode = nullptr;
    RBBISymbolTableEntry *el;

    el = static_cast<RBBISymbolTableEntry*>(uhash_get(fHashTable, &key));
    if (el != nullptr) {
        retNode = el->val;
    }
    return retNode;
}


//
//    RBBISymbolTable::addEntry     Add a new entry to the symbol table.
//                                  Indicate an error if the name already exists -
//                                    this will only occur in the case of duplicate
//                                    variable assignments.
//
void            RBBISymbolTable::addEntry  (const UnicodeString &key, RBBINode *val, UErrorCode &err) {
    RBBISymbolTableEntry *e;
    /* test for buffer overflows */
    if (U_FAILURE(err)) {
        return;
    }
    e = static_cast<RBBISymbolTableEntry*>(uhash_get(fHashTable, &key));
    if (e != nullptr) {
        err = U_BRK_VARIABLE_REDFINITION;
        return;
    }

    e = new RBBISymbolTableEntry;
    if (e == nullptr) {
        err = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    e->key = key;
    e->val = val;
    uhash_put( fHashTable, &e->key, e, &err);
}


RBBISymbolTableEntry::RBBISymbolTableEntry() : UMemory(), key(), val(nullptr) {}

RBBISymbolTableEntry::~RBBISymbolTableEntry() {
    // The "val" of a symbol table entry is a variable reference node.
    // The l. child of the val is the rhs expression from the assignment.
    // Unlike other node types, children of variable reference nodes are not
    //    automatically recursively deleted.  We do it manually here.
    delete val->fLeftChild;
    val->fLeftChild = nullptr;

    delete  val;

    // Note: the key UnicodeString is destructed by virtue of being in the object by value.
}


//
//  RBBISymbolTable::print    Debugging function, dump out the symbol table contents.
//
#ifdef RBBI_DEBUG
void RBBISymbolTable::rbbiSymtablePrint() const {
    RBBIDebugPrintf("Variable Definitions Symbol Table\n"
           "Name                  Node         serial  String Val\n"
           "-------------------------------------------------------------------\n");

    int32_t pos = UHASH_FIRST;
    const UHashElement  *e   = nullptr;
    for (;;) {
        e = uhash_nextElement(fHashTable,  &pos);
        if (e == nullptr ) {
            break;
        }
        RBBISymbolTableEntry  *s   = (RBBISymbolTableEntry *)e->value.pointer;

        RBBIDebugPrintf("%-19s   %8p %7d ", CStr(s->key)(), (void *)s->val, s->val->fSerialNum);
        RBBIDebugPrintf(" %s\n", CStr(s->val->fLeftChild->fText)());
    }

    RBBIDebugPrintf("\nParsed Variable Definitions\n");
    pos = -1;
    for (;;) {
        e = uhash_nextElement(fHashTable,  &pos);
        if (e == nullptr ) {
            break;
        }
        RBBISymbolTableEntry  *s   = (RBBISymbolTableEntry *)e->value.pointer;
        RBBIDebugPrintf("%s\n", CStr(s->key)());
        RBBINode::printTree(s->val, true);
        RBBINode::printTree(s->val->fLeftChild, false);
        RBBIDebugPrintf("\n");
    }
}
#endif





U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */

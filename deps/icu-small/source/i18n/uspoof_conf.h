/*
******************************************************************************
*
*   Copyright (C) 2008-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  uspoof_conf.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009Jan05
*   created by: Andy Heninger
*
*   Internal classes for compiling confusable data into its binary (runtime) form.
*/

#ifndef __USPOOF_BUILDCONF_H__
#define __USPOOF_BUILDCONF_H__

#if !UCONFIG_NO_NORMALIZATION

#if !UCONFIG_NO_REGULAR_EXPRESSIONS 

#include "unicode/uregex.h"
#include "uhash.h"
#include "uspoof_impl.h"

U_NAMESPACE_BEGIN

// SPUString
//              Holds a string that is the result of one of the mappings defined
//              by the confusable mapping data (confusables.txt from Unicode.org)
//              Instances of SPUString exist during the compilation process only.

struct SPUString : public UMemory {
    UnicodeString  *fStr;             // The actual string.
    int32_t         fStrTableIndex;   // Index into the final runtime data for this string.
                                      //  (or, for length 1, the single string char itself,
                                      //   there being no string table entry for it.)
    SPUString(UnicodeString *s);
    ~SPUString();
};


//  String Pool   A utility class for holding the strings that are the result of
//                the spoof mappings.  These strings will utimately end up in the
//                run-time String Table.
//                This is sort of like a sorted set of strings, except that ICU's anemic
//                built-in collections don't support those, so it is implemented with a
//                combination of a uhash and a UVector.


class SPUStringPool : public UMemory {
  public:
    SPUStringPool(UErrorCode &status);
    ~SPUStringPool();
    
    // Add a string. Return the string from the table.
    // If the input parameter string is already in the table, delete the
    //  input parameter and return the existing string.
    SPUString *addString(UnicodeString *src, UErrorCode &status);


    // Get the n-th string in the collection.
    SPUString *getByIndex(int32_t i);

    // Sort the contents; affects the ordering of getByIndex().
    void sort(UErrorCode &status);

    int32_t size();

  private:
    UVector     *fVec;    // Elements are SPUString *
    UHashtable  *fHash;   // Key: UnicodeString  Value: SPUString
};


// class ConfusabledataBuilder
//     An instance of this class exists while the confusable data is being built from source.
//     It encapsulates the intermediate data structures that are used for building.
//     It exports one static function, to do a confusable data build.

class ConfusabledataBuilder : public UMemory {
  private:
    SpoofImpl  *fSpoofImpl;
    UChar      *fInput;
    UHashtable *fSLTable;
    UHashtable *fSATable; 
    UHashtable *fMLTable; 
    UHashtable *fMATable;
    UnicodeSet *fKeySet;     // A set of all keys (UChar32s) that go into the four mapping tables.

    // The binary data is first assembled into the following four collections, then
    //   copied to its final raw-memory destination.
    UVector            *fKeyVec;
    UVector            *fValueVec;
    UnicodeString      *fStringTable;
    UVector            *fStringLengthsTable;
    
    SPUStringPool      *stringPool;
    URegularExpression *fParseLine;
    URegularExpression *fParseHexNum;
    int32_t             fLineNum;

    ConfusabledataBuilder(SpoofImpl *spImpl, UErrorCode &status);
    ~ConfusabledataBuilder();
    void build(const char * confusables, int32_t confusablesLen, UErrorCode &status);

    // Add an entry to the key and value tables being built
    //   input:  data from SLTable, MATable, etc.
    //   outut:  entry added to fKeyVec and fValueVec
    void addKeyEntry(UChar32     keyChar,     // The key character
                     UHashtable *table,       // The table, one of SATable, MATable, etc.
                     int32_t     tableFlag,   // One of USPOOF_SA_TABLE_FLAG, etc.
                     UErrorCode &status);

    // From an index into fKeyVec & fValueVec
    //   get a UnicodeString with the corresponding mapping.
    UnicodeString getMapping(int32_t index);

    // Populate the final binary output data array with the compiled data.
    void outputData(UErrorCode &status);

  public:
    static void buildConfusableData(SpoofImpl *spImpl, const char * confusables,
        int32_t confusablesLen, int32_t *errorType, UParseError *pe, UErrorCode &status);
};
U_NAMESPACE_END

#endif
#endif  // !UCONFIG_NO_REGULAR_EXPRESSIONS 
#endif  // __USPOOF_BUILDCONF_H__

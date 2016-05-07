/*
***************************************************************************
* Copyright (C) 2008-2013, International Business Machines Corporation
* and others. All Rights Reserved.
***************************************************************************
*
*  uspoof_impl.h
*
*    Implemenation header for spoof detection
*
*/

#ifndef USPOOFIM_H
#define USPOOFIM_H

#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "unicode/uscript.h"
#include "unicode/udata.h"

#include "utrie2.h"

#if !UCONFIG_NO_NORMALIZATION

#ifdef __cplusplus

U_NAMESPACE_BEGIN

// The maximium length (in UTF-16 UChars) of the skeleton replacement string resulting from
//   a single input code point.  This is function of the unicode.org data.
#define USPOOF_MAX_SKELETON_EXPANSION 20

// The default stack buffer size for copies or conversions or normalizations
// of input strings being checked.  (Used in multiple places.)
#define USPOOF_STACK_BUFFER_SIZE 100

// Magic number for sanity checking spoof data.
#define USPOOF_MAGIC 0x3845fdef

class IdentifierInfo;
class ScriptSet;
class SpoofData;
struct SpoofDataHeader;
struct SpoofStringLengthsElement;

/**
  *  Class SpoofImpl corresponds directly to the plain C API opaque type
  *  USpoofChecker.  One can be cast to the other.
  */
class SpoofImpl : public UObject  {
public:
	SpoofImpl(SpoofData *data, UErrorCode &status);
	SpoofImpl();
	virtual ~SpoofImpl();

    /** Copy constructor, used by the user level uspoof_clone() function.
     */
    SpoofImpl(const SpoofImpl &src, UErrorCode &status);

    static SpoofImpl *validateThis(USpoofChecker *sc, UErrorCode &status);
    static const SpoofImpl *validateThis(const USpoofChecker *sc, UErrorCode &status);

    /** Get the confusable skeleton transform for a single code point.
     *  The result is a string with a length between 1 and 18.
     *  @param    tableMask  bit flag specifying which confusable table to use.
     *                       One of USPOOF_SL_TABLE_FLAG, USPOOF_MA_TABLE_FLAG, etc.
     *  @return   The length in UTF-16 code units of the substition string.
     */
    int32_t confusableLookup(UChar32 inChar, int32_t tableMask, UnicodeString &destBuf) const;

    /** Set and Get AllowedLocales, implementations of the corresponding API */
    void setAllowedLocales(const char *localesList, UErrorCode &status);
    const char * getAllowedLocales(UErrorCode &status);

    // Add (union) to the UnicodeSet all of the characters for the scripts used for
    // the specified locale.  Part of the implementation of setAllowedLocales.
    void addScriptChars(const char *locale, UnicodeSet *allowedChars, UErrorCode &status);


    /** parse a hex number.  Untility used by the builders.   */
    static UChar32 ScanHex(const UChar *s, int32_t start, int32_t limit, UErrorCode &status);

    // Implementation for Whole Script tests.
    // Return the test bit flag to be ORed into the eventual user return value
    //    if a Spoof opportunity is detected.
    void wholeScriptCheck(
        const UnicodeString &text, ScriptSet *result, UErrorCode &status) const;

    static UClassID U_EXPORT2 getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;

    // IdentifierInfo Cache. IdentifierInfo objects are somewhat expensive to create.
    //                       Maintain a one-element cache, which is sufficient to avoid repeatedly
    //                       creating new ones unless we get multi-thread concurrency in spoof
    //                       check operations, which should be statistically uncommon.
    IdentifierInfo *getIdentifierInfo(UErrorCode &status) const;
    void releaseIdentifierInfo(IdentifierInfo *idInfo) const;

    //
    // Data Members
    //

    int32_t           fMagic;             // Internal sanity check.
    int32_t           fChecks;            // Bit vector of checks to perform.

    SpoofData        *fSpoofData;

    const UnicodeSet *fAllowedCharsSet;   // The UnicodeSet of allowed characters.
                                          //   for this Spoof Checker.  Defaults to all chars.

    const char       *fAllowedLocales;    // The list of allowed locales.
    URestrictionLevel fRestrictionLevel;  // The maximum restriction level for an acceptable identifier.

    IdentifierInfo    *fCachedIdentifierInfo;    // Do not use directly. See getIdentifierInfo().:w
};



//
//  Confusable Mappings Data Structures
//
//    For the confusable data, we are essentially implementing a map,
//       key:    a code point
//       value:  a string.  Most commonly one char in length, but can be more.
//
//    The keys are stored as a sorted array of 32 bit ints.
//             bits 0-23    a code point value
//             bits 24-31   flags
//                24:  1 if entry applies to SL table
//                25:  1 if entry applies to SA table
//                26:  1 if entry applies to ML table
//                27:  1 if entry applies to MA table
//                28:  1 if there are multiple entries for this code point.
//                29-30:  length of value string, in UChars.
//                         values are (1, 2, 3, other)
//        The key table is sorted in ascending code point order.  (not on the
//        32 bit int value, the flag bits do not participate in the sorting.)
//
//        Lookup is done by means of a binary search in the key table.
//
//    The corresponding values are kept in a parallel array of 16 bit ints.
//        If the value string is of length 1, it is literally in the value array.
//        For longer strings, the value array contains an index into the strings table.
//
//    String Table:
//       The strings table contains all of the value strings (those of length two or greater)
//       concatentated together into one long UChar (UTF-16) array.
//
//       The array is arranged by length of the strings - all strings of the same length
//       are stored together.  The sections are ordered by length of the strings -
//       all two char strings first, followed by all of the three Char strings, etc.
//
//       There is no nul character or other mark between adjacent strings.
//
//    String Lengths table
//       The length of strings from 1 to 3 is flagged in the key table.
//       For strings of length 4 or longer, the string length table provides a
//       mapping between an index into the string table and the corresponding length.
//       Strings of these lengths are rare, so lookup time is not an issue.
//       Each entry consists of
//            uint16_t      index of the _last_ string with this length
//            uint16_t      the length
//

// Flag bits in the Key entries
#define USPOOF_SL_TABLE_FLAG (1<<24)
#define USPOOF_SA_TABLE_FLAG (1<<25)
#define USPOOF_ML_TABLE_FLAG (1<<26)
#define USPOOF_MA_TABLE_FLAG (1<<27)
#define USPOOF_KEY_MULTIPLE_VALUES (1<<28)
#define USPOOF_KEY_LENGTH_SHIFT 29
#define USPOOF_KEY_LENGTH_FIELD(x) (((x)>>29) & 3)


struct SpoofStringLengthsElement {
    uint16_t      fLastString;         // index in string table of last string with this length
    uint16_t      fStrLength;           // Length of strings
};



//-------------------------------------------------------------------------------------
//
//  SpoofData
//
//    A small class that wraps the raw (usually memory mapped) spoof data.
//    Serves two primary functions:
//      1.  Convenience.  Contains real pointers to the data, to avoid dealing with
//          the offsets in the raw data.
//      2.  Reference counting.  When a spoof checker is cloned, the raw data is shared
//          and must be retained until all checkers using the data are closed.
//    Nothing in this struct includes state that is specific to any particular
//    USpoofDetector object.
//
//---------------------------------------------------------------------------------------
class SpoofData: public UMemory {
  public:
    static SpoofData *getDefault(UErrorCode &status);   // Load standard ICU spoof data.
    SpoofData(UErrorCode &status);   // Create new spoof data wrapper.
                                     // Only used when building new data from rules.

    // Constructor for use when creating from prebuilt default data.
    //   A UDataMemory is what the ICU internal data loading functions provide.
    //   The udm is adopted by the SpoofData.
    SpoofData(UDataMemory *udm, UErrorCode &status);

    // Constructor for use when creating from serialized data.
    //
    SpoofData(const void *serializedData, int32_t length, UErrorCode &status);

    //  Check raw Spoof Data Version compatibility.
    //  Return TRUE it looks good.
    static UBool validateDataVersion(const SpoofDataHeader *rawData, UErrorCode &status);
    ~SpoofData();                    // Destructor not normally used.
                                     // Use removeReference() instead.
    // Reference Counting functions.
    //    Clone of a user-level spoof detector increments the ref count on the data.
    //    Close of a user-level spoof detector decrements the ref count.
    //    If the data is owned by us, it will be deleted when count goes to zero.
    SpoofData *addReference();
    void removeReference();

    // Reserve space in the raw data.  For use by builder when putting together a
    //   new set of data.  Init the new storage to zero, to prevent inconsistent
    //   results if it is not all otherwise set by the requester.
    //  Return:
    //    pointer to the new space that was added by this function.
    void *reserveSpace(int32_t numBytes, UErrorCode &status);

    // initialize the pointers from this object to the raw data.
    void initPtrs(UErrorCode &status);

    // Reset all fields to an initial state.
    // Called from the top of all constructors.
    void reset();

    SpoofDataHeader             *fRawData;          // Ptr to the raw memory-mapped data
    UBool                       fDataOwned;         // True if the raw data is owned, and needs
                                                    //  to be deleted when refcount goes to zero.
    UDataMemory                 *fUDM;              // If not NULL, our data came from a
                                                    //   UDataMemory, which we must close when
                                                    //   we are done.

    uint32_t                    fMemLimit;          // Limit of available raw data space
    u_atomic_int32_t            fRefCount;

    // Confusable data
    int32_t                     *fCFUKeys;
    uint16_t                    *fCFUValues;
    SpoofStringLengthsElement   *fCFUStringLengths;
    UChar                       *fCFUStrings;

    // Whole Script Confusable Data
    UTrie2                      *fAnyCaseTrie;
    UTrie2                      *fLowerCaseTrie;
    ScriptSet                   *fScriptSets;
    };


//---------------------------------------------------------------------------------------
//
//  Raw Binary Data Formats, as loaded from the ICU data file,
//    or as built by the builder.
//
//---------------------------------------------------------------------------------------
struct SpoofDataHeader {
    int32_t       fMagic;                // (0x3845fdef)
    uint8_t       fFormatVersion[4];     // Data Format. Same as the value in struct UDataInfo
                                         //   if there is one associated with this data.
    int32_t       fLength;               // Total lenght in bytes of this spoof data,
                                         //   including all sections, not just the header.

    // The following four sections refer to data representing the confusable data
    //   from the Unicode.org data from "confusables.txt"

    int32_t       fCFUKeys;               // byte offset to Keys table (from SpoofDataHeader *)
    int32_t       fCFUKeysSize;           // number of entries in keys table  (32 bits each)

    // TODO: change name to fCFUValues, for consistency.
    int32_t       fCFUStringIndex;        // byte offset to String Indexes table
    int32_t       fCFUStringIndexSize;    // number of entries in String Indexes table (16 bits each)
                                          //     (number of entries must be same as in Keys table

    int32_t       fCFUStringTable;        // byte offset of String table
    int32_t       fCFUStringTableLen;     // length of string table (in 16 bit UChars)

    int32_t       fCFUStringLengths;      // byte offset to String Lengths table
    int32_t       fCFUStringLengthsSize;  // number of entries in lengths table. (2 x 16 bits each)


    // The following sections are for data from confusablesWholeScript.txt

    int32_t       fAnyCaseTrie;           // byte offset to the serialized Any Case Trie
    int32_t       fAnyCaseTrieLength;     // Length (bytes) of the serialized Any Case Trie

    int32_t       fLowerCaseTrie;         // byte offset to the serialized Lower Case Trie
    int32_t       fLowerCaseTrieLength;   // Length (bytes) of the serialized Lower Case Trie

    int32_t       fScriptSets;            // byte offset to array of ScriptSets
    int32_t       fScriptSetsLength;      // Number of ScriptSets (24 bytes each)


    // The following sections are for data from xidmodifications.txt


    int32_t       unused[15];              // Padding, Room for Expansion

 };




//
//  Structure for the Whole Script Confusable Data
//    See Unicode UAX-39, Unicode Security Mechanisms, for a description of the
//    Whole Script confusable data
//
//  The data provides mappings from code points to a set of scripts
//    that contain characters that might be confused with the code point.
//  There are two mappings, one for lower case only, and one for characters
//    of any case.
//
//  The actual data consists of a utrie2 to map from a code point to an offset,
//  and an array of UScriptSets (essentially bit maps) that is indexed
//  by the offsets obtained from the Trie.
//
//


U_NAMESPACE_END
#endif /* __cplusplus */

/**
  * Endianness swap function for binary spoof data.
  * @internal
  */
U_CAPI int32_t U_EXPORT2
uspoof_swap(const UDataSwapper *ds, const void *inData, int32_t length, void *outData,
            UErrorCode *status);


#endif

#endif  /* USPOOFIM_H */

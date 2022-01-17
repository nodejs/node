// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
***************************************************************************
* Copyright (C) 2008-2013, International Business Machines Corporation
* and others. All Rights Reserved.
***************************************************************************
*
*  uspoof_impl.h
*
*    Implementation header for spoof detection
*
*/

#ifndef USPOOFIM_H
#define USPOOFIM_H

#include "uassert.h"
#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "unicode/uscript.h"
#include "unicode/udata.h"
#include "udataswp.h"
#include "utrie2.h"

#if !UCONFIG_NO_NORMALIZATION

#ifdef __cplusplus

#include "capi_helper.h"

U_NAMESPACE_BEGIN

// The maximum length (in UTF-16 UChars) of the skeleton replacement string resulting from
//   a single input code point.  This is function of the unicode.org data.
#define USPOOF_MAX_SKELETON_EXPANSION 20

// The default stack buffer size for copies or conversions or normalizations
// of input strings being checked.  (Used in multiple places.)
#define USPOOF_STACK_BUFFER_SIZE 100

// Magic number for sanity checking spoof data.
#define USPOOF_MAGIC 0x3845fdef

// Magic number for sanity checking spoof checkers.
#define USPOOF_CHECK_MAGIC 0x2734ecde

class ScriptSet;
class SpoofData;
struct SpoofDataHeader;
class ConfusableDataUtils;

/**
  *  Class SpoofImpl corresponds directly to the plain C API opaque type
  *  USpoofChecker.  One can be cast to the other.
  */
class SpoofImpl : public UObject,
        public IcuCApiHelper<USpoofChecker, SpoofImpl, USPOOF_MAGIC> {
public:
    SpoofImpl(SpoofData *data, UErrorCode& status);
    SpoofImpl(UErrorCode& status);
    SpoofImpl();
    void construct(UErrorCode& status);
    virtual ~SpoofImpl();

    /** Copy constructor, used by the user level uspoof_clone() function.
     */
    SpoofImpl(const SpoofImpl &src, UErrorCode &status);
    
    USpoofChecker *asUSpoofChecker();
    static SpoofImpl *validateThis(USpoofChecker *sc, UErrorCode &status);
    static const SpoofImpl *validateThis(const USpoofChecker *sc, UErrorCode &status);

    /** Set and Get AllowedLocales, implementations of the corresponding API */
    void setAllowedLocales(const char *localesList, UErrorCode &status);
    const char * getAllowedLocales(UErrorCode &status);

    // Add (union) to the UnicodeSet all of the characters for the scripts used for
    // the specified locale.  Part of the implementation of setAllowedLocales.
    void addScriptChars(const char *locale, UnicodeSet *allowedChars, UErrorCode &status);

    // Functions implementing the features of UTS 39 section 5.
    static void getAugmentedScriptSet(UChar32 codePoint, ScriptSet& result, UErrorCode& status);
    void getResolvedScriptSet(const UnicodeString& input, ScriptSet& result, UErrorCode& status) const;
    void getResolvedScriptSetWithout(const UnicodeString& input, UScriptCode script, ScriptSet& result, UErrorCode& status) const;
    void getNumerics(const UnicodeString& input, UnicodeSet& result, UErrorCode& status) const;
    URestrictionLevel getRestrictionLevel(const UnicodeString& input, UErrorCode& status) const;

    int32_t findHiddenOverlay(const UnicodeString& input, UErrorCode& status) const;
    bool isIllegalCombiningDotLeadCharacter(UChar32 cp) const;

    /** parse a hex number.  Untility used by the builders.   */
    static UChar32 ScanHex(const UChar *s, int32_t start, int32_t limit, UErrorCode &status);

    static UClassID U_EXPORT2 getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const override;

    //
    // Data Members
    //

    int32_t           fChecks;            // Bit vector of checks to perform.

    SpoofData        *fSpoofData;
    
    const UnicodeSet *fAllowedCharsSet;   // The UnicodeSet of allowed characters.
                                          //   for this Spoof Checker.  Defaults to all chars.

    const char       *fAllowedLocales;    // The list of allowed locales.
    URestrictionLevel fRestrictionLevel;  // The maximum restriction level for an acceptable identifier.
};

/**
 *  Class CheckResult corresponds directly to the plain C API opaque type
 *  USpoofCheckResult.  One can be cast to the other.
 */
class CheckResult : public UObject,
        public IcuCApiHelper<USpoofCheckResult, CheckResult, USPOOF_CHECK_MAGIC> {
public:
    CheckResult();
    virtual ~CheckResult();

    USpoofCheckResult *asUSpoofCheckResult();
    static CheckResult *validateThis(USpoofCheckResult *ptr, UErrorCode &status);
    static const CheckResult *validateThis(const USpoofCheckResult *ptr, UErrorCode &status);

    void clear();

    // Used to convert this CheckResult to the older int32_t return value API
    int32_t toCombinedBitmask(int32_t expectedChecks);

    // Data Members
    int32_t fChecks;                       // Bit vector of checks that were failed.
    UnicodeSet fNumerics;                  // Set of numerics found in the string.
    URestrictionLevel fRestrictionLevel;   // The restriction level of the string.
};


//
//  Confusable Mappings Data Structures, version 2.0
//
//    For the confusable data, we are essentially implementing a map,
//       key:    a code point
//       value:  a string.  Most commonly one char in length, but can be more.
//
//    The keys are stored as a sorted array of 32 bit ints.
//             bits 0-23    a code point value
//             bits 24-31   length of value string, in UChars (between 1 and 256 UChars).
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
//       concatenated together into one long UChar (UTF-16) array.
//
//       There is no nul character or other mark between adjacent strings.
//
//----------------------------------------------------------------------------
//
//  Changes from format version 1 to format version 2:
//      1) Removal of the whole-script confusable data tables.
//      2) Removal of the SL/SA/ML/MA and multi-table flags in the key bitmask.
//      3) Expansion of string length value in the key bitmask from 2 bits to 8 bits.
//      4) Removal of the string lengths table since 8 bits is sufficient for the
//         lengths of all entries in confusables.txt.



// Internal functions for manipulating confusable data table keys
#define USPOOF_CONFUSABLE_DATA_FORMAT_VERSION 2  // version for ICU 58
class ConfusableDataUtils {
public:
    inline static UChar32 keyToCodePoint(int32_t key) {
        return key & 0x00ffffff;
    }
    inline static int32_t keyToLength(int32_t key) {
        return ((key & 0xff000000) >> 24) + 1;
    }
    inline static int32_t codePointAndLengthToKey(UChar32 codePoint, int32_t length) {
        U_ASSERT((codePoint & 0x00ffffff) == codePoint);
        U_ASSERT(length <= 256);
        return codePoint | ((length - 1) << 24);
    }
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
    static SpoofData* getDefault(UErrorCode &status);   // Get standard ICU spoof data.
    static void releaseDefault();   // Cleanup reference to default spoof data.

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
    //  Return true it looks good.
    UBool validateDataVersion(UErrorCode &status) const;

    ~SpoofData();                    // Destructor not normally used.
                                     // Use removeReference() instead.
    // Reference Counting functions.
    //    Clone of a user-level spoof detector increments the ref count on the data.
    //    Close of a user-level spoof detector decrements the ref count.
    //    If the data is owned by us, it will be deleted when count goes to zero.
    SpoofData *addReference(); 
    void removeReference();

    // Reset all fields to an initial state.
    // Called from the top of all constructors.
    void reset();

    // Copy this instance's raw data buffer to the specified address.
    int32_t serialize(void *buf, int32_t capacity, UErrorCode &status) const;

    // Get the total number of bytes of data backed by this SpoofData.
    // Not to be confused with length, which returns the number of confusable entries.
    int32_t size() const;

    // Get the confusable skeleton transform for a single code point.
    // The result is a string with a length between 1 and 18 as of Unicode 9.
    // This is the main public endpoint for this class.
    // @return   The length in UTF-16 code units of the substitution string.
    int32_t confusableLookup(UChar32 inChar, UnicodeString &dest) const;

    // Get the number of confusable entries in this SpoofData.
    int32_t length() const;

    // Get the code point (key) at the specified index.
    UChar32 codePointAt(int32_t index) const;

    // Get the confusable skeleton (value) at the specified index.
    // Append it to the specified UnicodeString&.
    // @return   The length in UTF-16 code units of the skeleton string.
    int32_t appendValueTo(int32_t index, UnicodeString& dest) const;

  private:
    // Reserve space in the raw data.  For use by builder when putting together a
    //   new set of data.  Init the new storage to zero, to prevent inconsistent
    //   results if it is not all otherwise set by the requester.
    //  Return:
    //    pointer to the new space that was added by this function.
    void *reserveSpace(int32_t numBytes, UErrorCode &status);

    // initialize the pointers from this object to the raw data.
    void initPtrs(UErrorCode &status);

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
    UChar                       *fCFUStrings;

    friend class ConfusabledataBuilder;
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
    int32_t       fLength;               // Total length in bytes of this spoof data,
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

    // The following sections are for data from xidmodifications.txt

    int32_t       unused[15];              // Padding, Room for Expansion
};



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


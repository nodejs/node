/*
******************************************************************************
*
*   Copyright (C) 2008-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  uspoof_conf.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009Jan05  (refactoring earlier files)
*   created by: Andy Heninger
*
*   Internal classes for compililing confusable data into its binary (runtime) form.
*/

#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#if !UCONFIG_NO_REGULAR_EXPRESSIONS
#if !UCONFIG_NO_NORMALIZATION

#include "unicode/unorm.h"
#include "unicode/uregex.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "uspoof_impl.h"
#include "uhash.h"
#include "uvector.h"
#include "uassert.h"
#include "uarrsort.h"
#include "uspoof_conf.h"

U_NAMESPACE_USE


//---------------------------------------------------------------------
//
//  buildConfusableData   Compile the source confusable data, as defined by
//                        the Unicode data file confusables.txt, into the binary
//                        structures used by the confusable detector.
//
//                        The binary structures are described in uspoof_impl.h
//
//     1.  parse the data, building 4 hash tables, one each for the SL, SA, ML and MA
//         tables.  Each maps from a UChar32 to a String.
//
//     2.  Sort all of the strings encountered by length, since they will need to
//         be stored in that order in the final string table.
//
//     3.  Build a list of keys (UChar32s) from the four mapping tables.  Sort the
//         list because that will be the ordering of our runtime table.
//
//     4.  Generate the run time string table.  This is generated before the key & value
//         tables because we need the string indexes when building those tables.
//
//     5.  Build the run-time key and value tables.  These are parallel tables, and are built
//         at the same time
//

SPUString::SPUString(UnicodeString *s) {
    fStr = s;
    fStrTableIndex = 0;
}


SPUString::~SPUString() {
    delete fStr;
}


SPUStringPool::SPUStringPool(UErrorCode &status) : fVec(NULL), fHash(NULL) {
    fVec = new UVector(status);
    fHash = uhash_open(uhash_hashUnicodeString,           // key hash function
                       uhash_compareUnicodeString,        // Key Comparator
                       NULL,                              // Value Comparator
                       &status);
}


SPUStringPool::~SPUStringPool() {
    int i;
    for (i=fVec->size()-1; i>=0; i--) {
        SPUString *s = static_cast<SPUString *>(fVec->elementAt(i));
        delete s;
    }
    delete fVec;
    uhash_close(fHash);
}


int32_t SPUStringPool::size() {
    return fVec->size();
}

SPUString *SPUStringPool::getByIndex(int32_t index) {
    SPUString *retString = (SPUString *)fVec->elementAt(index);
    return retString;
}


// Comparison function for ordering strings in the string pool.
// Compare by length first, then, within a group of the same length,
// by code point order.
// Conforms to the type signature for a USortComparator in uvector.h

static int8_t U_CALLCONV SPUStringCompare(UHashTok left, UHashTok right) {
	const SPUString *sL = const_cast<const SPUString *>(
        static_cast<SPUString *>(left.pointer));
 	const SPUString *sR = const_cast<const SPUString *>(
 	    static_cast<SPUString *>(right.pointer));
    int32_t lenL = sL->fStr->length();
    int32_t lenR = sR->fStr->length();
    if (lenL < lenR) {
        return -1;
    } else if (lenL > lenR) {
        return 1;
    } else {
        return sL->fStr->compare(*(sR->fStr));
    }
}

void SPUStringPool::sort(UErrorCode &status) {
    fVec->sort(SPUStringCompare, status);
}


SPUString *SPUStringPool::addString(UnicodeString *src, UErrorCode &status) {
    SPUString *hashedString = static_cast<SPUString *>(uhash_get(fHash, src));
    if (hashedString != NULL) {
        delete src;
    } else {
        hashedString = new SPUString(src);
        uhash_put(fHash, src, hashedString, &status);
        fVec->addElement(hashedString, status);
    }
    return hashedString;
}



ConfusabledataBuilder::ConfusabledataBuilder(SpoofImpl *spImpl, UErrorCode &status) :
    fSpoofImpl(spImpl),
    fInput(NULL),
    fSLTable(NULL),
    fSATable(NULL),
    fMLTable(NULL),
    fMATable(NULL),
    fKeySet(NULL),
    fKeyVec(NULL),
    fValueVec(NULL),
    fStringTable(NULL),
    fStringLengthsTable(NULL),
    stringPool(NULL),
    fParseLine(NULL),
    fParseHexNum(NULL),
    fLineNum(0)
{
    if (U_FAILURE(status)) {
        return;
    }
    fSLTable    = uhash_open(uhash_hashLong, uhash_compareLong, NULL, &status);
    fSATable    = uhash_open(uhash_hashLong, uhash_compareLong, NULL, &status);
    fMLTable    = uhash_open(uhash_hashLong, uhash_compareLong, NULL, &status);
    fMATable    = uhash_open(uhash_hashLong, uhash_compareLong, NULL, &status);
    fKeySet     = new UnicodeSet();
    fKeyVec     = new UVector(status);
    fValueVec   = new UVector(status);
    stringPool = new SPUStringPool(status);
}


ConfusabledataBuilder::~ConfusabledataBuilder() {
    uprv_free(fInput);
    uregex_close(fParseLine);
    uregex_close(fParseHexNum);
    uhash_close(fSLTable);
    uhash_close(fSATable);
    uhash_close(fMLTable);
    uhash_close(fMATable);
    delete fKeySet;
    delete fKeyVec;
    delete fStringTable;
    delete fStringLengthsTable;
    delete fValueVec;
    delete stringPool;
}


void ConfusabledataBuilder::buildConfusableData(SpoofImpl * spImpl, const char * confusables,
    int32_t confusablesLen, int32_t *errorType, UParseError *pe, UErrorCode &status) {

    if (U_FAILURE(status)) {
        return;
    }
    ConfusabledataBuilder builder(spImpl, status);
    builder.build(confusables, confusablesLen, status);
    if (U_FAILURE(status) && errorType != NULL) {
        *errorType = USPOOF_SINGLE_SCRIPT_CONFUSABLE;
        pe->line = builder.fLineNum;
    }
}


void ConfusabledataBuilder::build(const char * confusables, int32_t confusablesLen,
               UErrorCode &status) {

    // Convert the user input data from UTF-8 to UChar (UTF-16)
    int32_t inputLen = 0;
    if (U_FAILURE(status)) {
        return;
    }
    u_strFromUTF8(NULL, 0, &inputLen, confusables, confusablesLen, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) {
        return;
    }
    status = U_ZERO_ERROR;
    fInput = static_cast<UChar *>(uprv_malloc((inputLen+1) * sizeof(UChar)));
    if (fInput == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    u_strFromUTF8(fInput, inputLen+1, NULL, confusables, confusablesLen, &status);


    // Regular Expression to parse a line from Confusables.txt.  The expression will match
    // any line.  What was matched is determined by examining which capture groups have a match.
    //   Capture Group 1:  the source char
    //   Capture Group 2:  the replacement chars
    //   Capture Group 3-6  the table type, SL, SA, ML, or MA
    //   Capture Group 7:  A blank or comment only line.
    //   Capture Group 8:  A syntactically invalid line.  Anything that didn't match before.
    // Example Line from the confusables.txt source file:
    //   "1D702 ;	006E 0329 ;	SL	# MATHEMATICAL ITALIC SMALL ETA ... "
    UnicodeString pattern(
        "(?m)^[ \\t]*([0-9A-Fa-f]+)[ \\t]+;"      // Match the source char
        "[ \\t]*([0-9A-Fa-f]+"                    // Match the replacement char(s)
           "(?:[ \\t]+[0-9A-Fa-f]+)*)[ \\t]*;"    //     (continued)
        "\\s*(?:(SL)|(SA)|(ML)|(MA))"             // Match the table type
        "[ \\t]*(?:#.*?)?$"                       // Match any trailing #comment
        "|^([ \\t]*(?:#.*?)?)$"       // OR match empty lines or lines with only a #comment
        "|^(.*?)$", -1, US_INV);      // OR match any line, which catches illegal lines.
    // TODO: Why are we using the regex C API here? C++ would just take UnicodeString...
    fParseLine = uregex_open(pattern.getBuffer(), pattern.length(), 0, NULL, &status);

    // Regular expression for parsing a hex number out of a space-separated list of them.
    //   Capture group 1 gets the number, with spaces removed.
    pattern = UNICODE_STRING_SIMPLE("\\s*([0-9A-F]+)");
    fParseHexNum = uregex_open(pattern.getBuffer(), pattern.length(), 0, NULL, &status);

    // Zap any Byte Order Mark at the start of input.  Changing it to a space is benign
    //   given the syntax of the input.
    if (*fInput == 0xfeff) {
        *fInput = 0x20;
    }

    // Parse the input, one line per iteration of this loop.
    uregex_setText(fParseLine, fInput, inputLen, &status);
    while (uregex_findNext(fParseLine, &status)) {
        fLineNum++;
        if (uregex_start(fParseLine, 7, &status) >= 0) {
            // this was a blank or comment line.
            continue;
        }
        if (uregex_start(fParseLine, 8, &status) >= 0) {
            // input file syntax error.
            status = U_PARSE_ERROR;
            return;
        }

        // We have a good input line.  Extract the key character and mapping string, and
        //    put them into the appropriate mapping table.
        UChar32 keyChar = SpoofImpl::ScanHex(fInput, uregex_start(fParseLine, 1, &status),
                          uregex_end(fParseLine, 1, &status), status);

        int32_t mapStringStart = uregex_start(fParseLine, 2, &status);
        int32_t mapStringLength = uregex_end(fParseLine, 2, &status) - mapStringStart;
        uregex_setText(fParseHexNum, &fInput[mapStringStart], mapStringLength, &status);

        UnicodeString  *mapString = new UnicodeString();
        if (mapString == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        while (uregex_findNext(fParseHexNum, &status)) {
            UChar32 c = SpoofImpl::ScanHex(&fInput[mapStringStart], uregex_start(fParseHexNum, 1, &status),
                                 uregex_end(fParseHexNum, 1, &status), status);
            mapString->append(c);
        }
        U_ASSERT(mapString->length() >= 1);

        // Put the map (value) string into the string pool
        // This a little like a Java intern() - any duplicates will be eliminated.
        SPUString *smapString = stringPool->addString(mapString, status);

        // Add the UChar32 -> string mapping to the appropriate table.
        UHashtable *table = uregex_start(fParseLine, 3, &status) >= 0 ? fSLTable :
                            uregex_start(fParseLine, 4, &status) >= 0 ? fSATable :
                            uregex_start(fParseLine, 5, &status) >= 0 ? fMLTable :
                            uregex_start(fParseLine, 6, &status) >= 0 ? fMATable :
                            NULL;
        if (U_SUCCESS(status) && table == NULL) {
            status = U_PARSE_ERROR;
        }
        if (U_FAILURE(status)) {
            return;
        }

        // For Unicode 8, the SL, SA and ML tables have been discontinued.
        //                All input data from confusables.txt is tagged MA.
        //                ICU spoof check functions should ignore the specified table and always
        //                use this MA Data.
        //                For now, implement by populating the MA data into all four tables, and
        //                keep the multiple table implementation in place, in case it comes back
        //                at some time in the future.
        //                There is no run time size penalty to keeping the four table implementation -
        //                the data is shared when it's the same betweeen tables.
        if (table != fMATable) {
            status = U_PARSE_ERROR;
            return;
        };
        //  uhash_iput(table, keyChar, smapString, &status);
        uhash_iput(fSLTable, keyChar, smapString, &status);
        uhash_iput(fSATable, keyChar, smapString, &status);
        uhash_iput(fMLTable, keyChar, smapString, &status);
        uhash_iput(fMATable, keyChar, smapString, &status);
        fKeySet->add(keyChar);
        if (U_FAILURE(status)) {
            return;
        }
    }

    // Input data is now all parsed and collected.
    // Now create the run-time binary form of the data.
    //
    // This is done in two steps.  First the data is assembled into vectors and strings,
    //   for ease of construction, then the contents of these collections are dumped
    //   into the actual raw-bytes data storage.

    // Build up the string array, and record the index of each string therein
    //  in the (build time only) string pool.
    // Strings of length one are not entered into the strings array.
    // At the same time, build up the string lengths table, which records the
    // position in the string table of the first string of each length >= 4.
    // (Strings in the table are sorted by length)
    stringPool->sort(status);
    fStringTable = new UnicodeString();
    fStringLengthsTable = new UVector(status);
    int32_t previousStringLength = 0;
    int32_t previousStringIndex  = 0;
    int32_t poolSize = stringPool->size();
    int32_t i;
    for (i=0; i<poolSize; i++) {
        SPUString *s = stringPool->getByIndex(i);
        int32_t strLen = s->fStr->length();
        int32_t strIndex = fStringTable->length();
        U_ASSERT(strLen >= previousStringLength);
        if (strLen == 1) {
            // strings of length one do not get an entry in the string table.
            // Keep the single string character itself here, which is the same
            //  convention that is used in the final run-time string table index.
            s->fStrTableIndex = s->fStr->charAt(0);
        } else {
            if ((strLen > previousStringLength) && (previousStringLength >= 4)) {
                fStringLengthsTable->addElement(previousStringIndex, status);
                fStringLengthsTable->addElement(previousStringLength, status);
            }
            s->fStrTableIndex = strIndex;
            fStringTable->append(*(s->fStr));
        }
        previousStringLength = strLen;
        previousStringIndex  = strIndex;
    }
    // Make the final entry to the string lengths table.
    //   (it holds an entry for the _last_ string of each length, so adding the
    //    final one doesn't happen in the main loop because no longer string was encountered.)
    if (previousStringLength >= 4) {
        fStringLengthsTable->addElement(previousStringIndex, status);
        fStringLengthsTable->addElement(previousStringLength, status);
    }

    // Construct the compile-time Key and Value tables
    //
    // For each key code point, check which mapping tables it applies to,
    //   and create the final data for the key & value structures.
    //
    //   The four logical mapping tables are conflated into one combined table.
    //   If multiple logical tables have the same mapping for some key, they
    //     share a single entry in the combined table.
    //   If more than one mapping exists for the same key code point, multiple
    //     entries will be created in the table

    for (int32_t range=0; range<fKeySet->getRangeCount(); range++) {
        // It is an oddity of the UnicodeSet API that simply enumerating the contained
        //   code points requires a nested loop.
        for (UChar32 keyChar=fKeySet->getRangeStart(range);
                keyChar <= fKeySet->getRangeEnd(range); keyChar++) {
            addKeyEntry(keyChar, fSLTable, USPOOF_SL_TABLE_FLAG, status);
            addKeyEntry(keyChar, fSATable, USPOOF_SA_TABLE_FLAG, status);
            addKeyEntry(keyChar, fMLTable, USPOOF_ML_TABLE_FLAG, status);
            addKeyEntry(keyChar, fMATable, USPOOF_MA_TABLE_FLAG, status);
        }
    }

    // Put the assembled data into the flat runtime array
    outputData(status);

    // All of the intermediate allocated data belongs to the ConfusabledataBuilder
    //  object  (this), and is deleted in the destructor.
    return;
}

//
// outputData     The confusable data has been compiled and stored in intermediate
//                collections and strings.  Copy it from there to the final flat
//                binary array.
//
//                Note that as each section is added to the output data, the
//                expand (reserveSpace() function will likely relocate it in memory.
//                Be careful with pointers.
//
void ConfusabledataBuilder::outputData(UErrorCode &status) {

    U_ASSERT(fSpoofImpl->fSpoofData->fDataOwned == TRUE);

    //  The Key Table
    //     While copying the keys to the runtime array,
    //       also sanity check that they are sorted.

    int32_t numKeys = fKeyVec->size();
    int32_t *keys =
        static_cast<int32_t *>(fSpoofImpl->fSpoofData->reserveSpace(numKeys*sizeof(int32_t), status));
    if (U_FAILURE(status)) {
        return;
    }
    int i;
    int32_t previousKey = 0;
    for (i=0; i<numKeys; i++) {
        int32_t key =  fKeyVec->elementAti(i);
        (void)previousKey;         // Suppress unused variable warning on gcc.
        U_ASSERT((key & 0x00ffffff) >= (previousKey & 0x00ffffff));
        U_ASSERT((key & 0xff000000) != 0);
        keys[i] = key;
        previousKey = key;
    }
    SpoofDataHeader *rawData = fSpoofImpl->fSpoofData->fRawData;
    rawData->fCFUKeys = (int32_t)((char *)keys - (char *)rawData);
    rawData->fCFUKeysSize = numKeys;
    fSpoofImpl->fSpoofData->fCFUKeys = keys;


    // The Value Table, parallels the key table
    int32_t numValues = fValueVec->size();
    U_ASSERT(numKeys == numValues);
    uint16_t *values =
        static_cast<uint16_t *>(fSpoofImpl->fSpoofData->reserveSpace(numKeys*sizeof(uint16_t), status));
    if (U_FAILURE(status)) {
        return;
    }
    for (i=0; i<numValues; i++) {
        uint32_t value = static_cast<uint32_t>(fValueVec->elementAti(i));
        U_ASSERT(value < 0xffff);
        values[i] = static_cast<uint16_t>(value);
    }
    rawData = fSpoofImpl->fSpoofData->fRawData;
    rawData->fCFUStringIndex = (int32_t)((char *)values - (char *)rawData);
    rawData->fCFUStringIndexSize = numValues;
    fSpoofImpl->fSpoofData->fCFUValues = values;

    // The Strings Table.

    uint32_t stringsLength = fStringTable->length();
    // Reserve an extra space so the string will be nul-terminated.  This is
    // only a convenience, for when debugging; it is not needed otherwise.
    UChar *strings =
        static_cast<UChar *>(fSpoofImpl->fSpoofData->reserveSpace(stringsLength*sizeof(UChar)+2, status));
    if (U_FAILURE(status)) {
        return;
    }
    fStringTable->extract(strings, stringsLength+1, status);
    rawData = fSpoofImpl->fSpoofData->fRawData;
    U_ASSERT(rawData->fCFUStringTable == 0);
    rawData->fCFUStringTable = (int32_t)((char *)strings - (char *)rawData);
    rawData->fCFUStringTableLen = stringsLength;
    fSpoofImpl->fSpoofData->fCFUStrings = strings;

    // The String Lengths Table
    //    While copying into the runtime array do some sanity checks on the values
    //    Each complete entry contains two fields, an index and an offset.
    //    Lengths should increase with each entry.
    //    Offsets should be less than the size of the string table.
    int32_t lengthTableLength = fStringLengthsTable->size();
    uint16_t *stringLengths =
        static_cast<uint16_t *>(fSpoofImpl->fSpoofData->reserveSpace(lengthTableLength*sizeof(uint16_t), status));
    if (U_FAILURE(status)) {
        return;
    }
    int32_t destIndex = 0;
    uint32_t previousLength = 0;
    for (i=0; i<lengthTableLength; i+=2) {
        uint32_t offset = static_cast<uint32_t>(fStringLengthsTable->elementAti(i));
        uint32_t length = static_cast<uint32_t>(fStringLengthsTable->elementAti(i+1));
        U_ASSERT(offset < stringsLength);
        U_ASSERT(length < 40);
        (void)previousLength;  // Suppress unused variable warning on gcc.
        U_ASSERT(length > previousLength);
        stringLengths[destIndex++] = static_cast<uint16_t>(offset);
        stringLengths[destIndex++] = static_cast<uint16_t>(length);
        previousLength = length;
    }
    rawData = fSpoofImpl->fSpoofData->fRawData;
    rawData->fCFUStringLengths = (int32_t)((char *)stringLengths - (char *)rawData);
    // Note: StringLengthsSize in the raw data is the number of complete entries,
    //       each consisting of a pair of 16 bit values, hence the divide by 2.
    rawData->fCFUStringLengthsSize = lengthTableLength / 2;
    fSpoofImpl->fSpoofData->fCFUStringLengths =
        reinterpret_cast<SpoofStringLengthsElement *>(stringLengths);
}



//  addKeyEntry   Construction of the confusable Key and Mapping Values tables.
//                This is an intermediate point in the building process.
//                We already have the mappings in the hash tables fSLTable, etc.
//                This function builds corresponding run-time style table entries into
//                  fKeyVec and fValueVec

void ConfusabledataBuilder::addKeyEntry(
    UChar32     keyChar,     // The key character
    UHashtable *table,       // The table, one of SATable, MATable, etc.
    int32_t     tableFlag,   // One of USPOOF_SA_TABLE_FLAG, etc.
    UErrorCode &status) {

    SPUString *targetMapping = static_cast<SPUString *>(uhash_iget(table, keyChar));
    if (targetMapping == NULL) {
        // No mapping for this key character.
        //   (This function is called for all four tables for each key char that
        //    is seen anywhere, so this no entry cases are very much expected.)
        return;
    }

    // Check whether there is already an entry with the correct mapping.
    // If so, simply set the flag in the keyTable saying that the existing entry
    // applies to the table that we're doing now.

    UBool keyHasMultipleValues = FALSE;
    int32_t i;
    for (i=fKeyVec->size()-1; i>=0 ; i--) {
        int32_t key = fKeyVec->elementAti(i);
        if ((key & 0x0ffffff) != keyChar) {
            // We have now checked all existing key entries for this key char (if any)
            //  without finding one with the same mapping.
            break;
        }
        UnicodeString mapping = getMapping(i);
        if (mapping == *(targetMapping->fStr)) {
            // The run time entry we are currently testing has the correct mapping.
            // Set the flag in it indicating that it applies to the new table also.
            key |= tableFlag;
            fKeyVec->setElementAt(key, i);
            return;
        }
        keyHasMultipleValues = TRUE;
    }

    // Need to add a new entry to the binary data being built for this mapping.
    // Includes adding entries to both the key table and the parallel values table.

    int32_t newKey = keyChar | tableFlag;
    if (keyHasMultipleValues) {
        newKey |= USPOOF_KEY_MULTIPLE_VALUES;
    }
    int32_t adjustedMappingLength = targetMapping->fStr->length() - 1;
    if (adjustedMappingLength>3) {
        adjustedMappingLength = 3;
    }
    newKey |= adjustedMappingLength << USPOOF_KEY_LENGTH_SHIFT;

    int32_t newData = targetMapping->fStrTableIndex;

    fKeyVec->addElement(newKey, status);
    fValueVec->addElement(newData, status);

    // If the preceding key entry is for the same key character (but with a different mapping)
    //   set the multiple-values flag on it.
    if (keyHasMultipleValues) {
        int32_t previousKeyIndex = fKeyVec->size() - 2;
        int32_t previousKey = fKeyVec->elementAti(previousKeyIndex);
        previousKey |= USPOOF_KEY_MULTIPLE_VALUES;
        fKeyVec->setElementAt(previousKey, previousKeyIndex);
    }
}



UnicodeString ConfusabledataBuilder::getMapping(int32_t index) {
    int32_t key = fKeyVec->elementAti(index);
    int32_t value = fValueVec->elementAti(index);
    int32_t length = USPOOF_KEY_LENGTH_FIELD(key);
    int32_t lastIndexWithLen;
    switch (length) {
      case 0:
        return UnicodeString(static_cast<UChar>(value));
      case 1:
      case 2:
        return UnicodeString(*fStringTable, value, length+1);
      case 3:
        length = 0;
        int32_t i;
        for (i=0; i<fStringLengthsTable->size(); i+=2) {
            lastIndexWithLen = fStringLengthsTable->elementAti(i);
            if (value <= lastIndexWithLen) {
                length = fStringLengthsTable->elementAti(i+1);
                break;
            }
        }
        U_ASSERT(length>=3);
        return UnicodeString(*fStringTable, value, length);
      default:
        U_ASSERT(FALSE);
    }
    return UnicodeString();
}

#endif
#endif // !UCONFIG_NO_REGULAR_EXPRESSIONS


/*
******************************************************************************
*
*   Copyright (C) 2008-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  uspoof_wsconf.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009Jan05  (refactoring earlier files)
*   created by: Andy Heninger
*
*   Internal functions for compililing Whole Script confusable source data
*   into its binary (runtime) form.  The binary data format is described
*   in uspoof_impl.h
*/

#include "unicode/utypes.h"
#include "unicode/uspoof.h"

#if !UCONFIG_NO_NORMALIZATION

#if !UCONFIG_NO_REGULAR_EXPRESSIONS 

#include "unicode/unorm.h"
#include "unicode/uregex.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "scriptset.h"
#include "uspoof_impl.h"
#include "uhash.h"
#include "uvector.h"
#include "uassert.h"
#include "uspoof_wsconf.h"

U_NAMESPACE_USE


// Regular expression for parsing a line from the Unicode file confusablesWholeScript.txt
// Example Lines:
//   006F          ; Latn; Deva; A #      (o)  LATIN SMALL LETTER O
//   0048..0049    ; Latn; Grek; A #  [2] (H..I)  LATIN CAPITAL LETTER H..LATIN CAPITAL LETTER I
//    |               |     |    |
//    |               |     |    |---- Which table, Any Case or Lower Case (A or L)
//    |               |     |----------Target script.   We need this.
//    |               |----------------Src script.  Should match the script of the source
//    |                                code points.  Beyond checking that, we don't keep it.
//    |--------------------------------Source code points or range.
//
// The expression will match _all_ lines, including erroneous lines.
// The result of the parse is returned via the contents of the (match) groups.
static const char *parseExp = 
        "(?m)"                                         // Multi-line mode
        "^([ \\t]*(?:#.*?)?)$"                         // A blank or comment line.  Matches Group 1.
        "|^(?:"                                        //   OR
        "\\s*([0-9A-F]{4,})(?:..([0-9A-F]{4,}))?\\s*;" // Code point range.  Groups 2 and 3.
        "\\s*([A-Za-z]+)\\s*;"                         // The source script.  Group 4.
        "\\s*([A-Za-z]+)\\s*;"                         // The target script.  Group 5.
        "\\s*(?:(A)|(L))"                              // The table A or L.   Group 6 or 7
        "[ \\t]*(?:#.*?)?"                             // Trailing commment
        ")$|"                                          //   OR
        "^(.*?)$";                                     // An error line.      Group 8.
                                                       //    Any line not matching the preceding
                                                       //    parts of the expression.will match
                                                       //    this, and thus be flagged as an error


// Extract a regular expression match group into a char * string.
//    The group must contain only invariant characters.
//    Used for script names
// 
static void extractGroup(
    URegularExpression *e, int32_t group, char *destBuf, int32_t destCapacity, UErrorCode &status) {

    UChar ubuf[50];
    ubuf[0] = 0;
    destBuf[0] = 0;
    int32_t len = uregex_group(e, group, ubuf, 50, &status);
    if (U_FAILURE(status) || len == -1 || len >= destCapacity) {
        return;
    }
    UnicodeString s(FALSE, ubuf, len);   // Aliasing constructor
    s.extract(0, len, destBuf, destCapacity, US_INV);
}



U_NAMESPACE_BEGIN

//  Build the Whole Script Confusable data
//
//     TODO:  Reorganize.  Either get rid of the WSConfusableDataBuilder class,
//                         because everything is local to this one build function anyhow,
//                           OR
//                         break this function into more reasonably sized pieces, with
//                         state in WSConfusableDataBuilder.
//
void buildWSConfusableData(SpoofImpl *spImpl, const char * confusablesWS,
          int32_t confusablesWSLen, UParseError *pe, UErrorCode &status) 
{
    if (U_FAILURE(status)) {
        return;
    }
    URegularExpression *parseRegexp = NULL;
    int32_t             inputLen    = 0;
    UChar              *input       = NULL;
    int32_t             lineNum     = 0;
    
    UVector            *scriptSets        = NULL;
    uint32_t            rtScriptSetsCount = 2;

    UTrie2             *anyCaseTrie   = NULL;
    UTrie2             *lowerCaseTrie = NULL;

    anyCaseTrie = utrie2_open(0, 0, &status);
    lowerCaseTrie = utrie2_open(0, 0, &status);

    UnicodeString pattern(parseExp, -1, US_INV);

    // The scriptSets vector provides a mapping from TRIE values to the set of scripts.
    //
    // Reserved TRIE values:
    //   0:  Code point has no whole script confusables.
    //   1:  Code point is of script Common or Inherited.
    //       These code points do not participate in whole script confusable detection.
    //       (This is logically equivalent to saying that they contain confusables in
    //        all scripts)
    //
    // Because Trie values are indexes into the ScriptSets vector, pre-fill
    // vector positions 0 and 1 to avoid conflicts with the reserved values.
    
    scriptSets = new UVector(status);
    if (scriptSets == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        goto cleanup;
    }
    scriptSets->addElement((void *)NULL, status);
    scriptSets->addElement((void *)NULL, status);

    // Convert the user input data from UTF-8 to UChar (UTF-16)
    u_strFromUTF8(NULL, 0, &inputLen, confusablesWS, confusablesWSLen, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) {
        goto cleanup;
    }
    status = U_ZERO_ERROR;
    input = static_cast<UChar *>(uprv_malloc((inputLen+1) * sizeof(UChar)));
    if (input == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        goto cleanup;
    }
    u_strFromUTF8(input, inputLen+1, NULL, confusablesWS, confusablesWSLen, &status);

    parseRegexp = uregex_open(pattern.getBuffer(), pattern.length(), 0, NULL, &status);

    // Zap any Byte Order Mark at the start of input.  Changing it to a space is benign
    //   given the syntax of the input.
    if (*input == 0xfeff) {
        *input = 0x20;
    }

    // Parse the input, one line per iteration of this loop.
    uregex_setText(parseRegexp, input, inputLen, &status);
    while (uregex_findNext(parseRegexp, &status)) {
        lineNum++;
        if (uregex_start(parseRegexp, 1, &status) >= 0) {
            // this was a blank or comment line.
            continue;
        }
        if (uregex_start(parseRegexp, 8, &status) >= 0) {
            // input file syntax error.
            status = U_PARSE_ERROR;
            goto cleanup;
        }
        if (U_FAILURE(status)) {
            goto cleanup;
        }

        // Pick up the start and optional range end code points from the parsed line.
        UChar32  startCodePoint = SpoofImpl::ScanHex(
            input, uregex_start(parseRegexp, 2, &status), uregex_end(parseRegexp, 2, &status), status);
        UChar32  endCodePoint = startCodePoint;
        if (uregex_start(parseRegexp, 3, &status) >=0) {
            endCodePoint = SpoofImpl::ScanHex(
                input, uregex_start(parseRegexp, 3, &status), uregex_end(parseRegexp, 3, &status), status);
        }

        // Extract the two script names from the source line.  We need these in an 8 bit
        //   default encoding (will be EBCDIC on IBM mainframes) in order to pass them on
        //   to the ICU u_getPropertyValueEnum() function.  Ugh.
        char  srcScriptName[20];
        char  targScriptName[20];
        extractGroup(parseRegexp, 4, srcScriptName, sizeof(srcScriptName), status);
        extractGroup(parseRegexp, 5, targScriptName, sizeof(targScriptName), status);
        UScriptCode srcScript  =
            static_cast<UScriptCode>(u_getPropertyValueEnum(UCHAR_SCRIPT, srcScriptName));
        UScriptCode targScript =
            static_cast<UScriptCode>(u_getPropertyValueEnum(UCHAR_SCRIPT, targScriptName));
        if (U_FAILURE(status)) {
            goto cleanup;
        }
        if (srcScript == USCRIPT_INVALID_CODE || targScript == USCRIPT_INVALID_CODE) {
            status = U_INVALID_FORMAT_ERROR;
            goto cleanup;
        }

        // select the table - (A) any case or (L) lower case only
        UTrie2 *table = anyCaseTrie;
        if (uregex_start(parseRegexp, 7, &status) >= 0) {
            table = lowerCaseTrie;
        }

        // Build the set of scripts containing confusable characters for
        //   the code point(s) specified in this input line.
        // Sanity check that the script of the source code point is the same
        //   as the source script indicated in the input file.  Failure of this check is
        //   an error in the input file.
        // Include the source script in the set (needed for Mixed Script Confusable detection).
        //
        UChar32 cp;
        for (cp=startCodePoint; cp<=endCodePoint; cp++) {
            int32_t setIndex = utrie2_get32(table, cp);
            BuilderScriptSet *bsset = NULL;
            if (setIndex > 0) {
                U_ASSERT(setIndex < scriptSets->size());
                bsset = static_cast<BuilderScriptSet *>(scriptSets->elementAt(setIndex));
            } else {
                bsset = new BuilderScriptSet();
                if (bsset == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    goto cleanup;
                }
                bsset->codePoint = cp;
                bsset->trie = table;
                bsset->sset = new ScriptSet();
                setIndex = scriptSets->size();
                bsset->index = setIndex;
                bsset->rindex = 0;
                if (bsset->sset == NULL) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    goto cleanup;
                }
                scriptSets->addElement(bsset, status);
                utrie2_set32(table, cp, setIndex, &status);
            }
            bsset->sset->set(targScript, status);
            bsset->sset->set(srcScript, status);

            if (U_FAILURE(status)) {
                goto cleanup;
            }
            UScriptCode cpScript = uscript_getScript(cp, &status);
            if (cpScript != srcScript) {
                status = U_INVALID_FORMAT_ERROR;
                goto cleanup;
            }
        }
    }

    // Eliminate duplicate script sets.  At this point we have a separate
    // script set for every code point that had data in the input file.
    //
    // We eliminate underlying ScriptSet objects, not the BuildScriptSets that wrap them
    //
    // printf("Number of scriptSets: %d\n", scriptSets->size());
    {
        int32_t duplicateCount = 0;
        rtScriptSetsCount = 2;
        for (int32_t outeri=2; outeri<scriptSets->size(); outeri++) {
            BuilderScriptSet *outerSet = static_cast<BuilderScriptSet *>(scriptSets->elementAt(outeri));
            if (outerSet->index != static_cast<uint32_t>(outeri)) {
                // This set was already identified as a duplicate.
                //   It will not be allocated a position in the runtime array of ScriptSets.
                continue;
            }
            outerSet->rindex = rtScriptSetsCount++;
            for (int32_t inneri=outeri+1; inneri<scriptSets->size(); inneri++) {
                BuilderScriptSet *innerSet = static_cast<BuilderScriptSet *>(scriptSets->elementAt(inneri));
                if (*(outerSet->sset) == *(innerSet->sset) && outerSet->sset != innerSet->sset) {
                    delete innerSet->sset;
                    innerSet->scriptSetOwned = FALSE;
                    innerSet->sset = outerSet->sset;
                    innerSet->index = outeri;
                    innerSet->rindex = outerSet->rindex;
                    duplicateCount++;
                }
                // But this doesn't get all.  We need to fix the TRIE.
            }
        }
        // printf("Number of distinct script sets: %d\n", rtScriptSetsCount);
    }

    

    // Update the Trie values to be reflect the run time script indexes (after duplicate merging).
    //    (Trie Values 0 and 1 are reserved, and the corresponding slots in scriptSets
    //     are unused, which is why the loop index starts at 2.)
    {
        for (int32_t i=2; i<scriptSets->size(); i++) {
            BuilderScriptSet *bSet = static_cast<BuilderScriptSet *>(scriptSets->elementAt(i));
            if (bSet->rindex != (uint32_t)i) {
                utrie2_set32(bSet->trie, bSet->codePoint, bSet->rindex, &status);
            }
        }
    }

    // For code points with script==Common or script==Inherited,
    //   Set the reserved value of 1 into both Tries.  These characters do not participate
    //   in Whole Script Confusable detection; this reserved value is the means
    //   by which they are detected.
    {
        UnicodeSet ignoreSet;
        ignoreSet.applyIntPropertyValue(UCHAR_SCRIPT, USCRIPT_COMMON, status);
        UnicodeSet inheritedSet;
        inheritedSet.applyIntPropertyValue(UCHAR_SCRIPT, USCRIPT_INHERITED, status);
        ignoreSet.addAll(inheritedSet);
        for (int32_t rn=0; rn<ignoreSet.getRangeCount(); rn++) {
            UChar32 rangeStart = ignoreSet.getRangeStart(rn);
            UChar32 rangeEnd   = ignoreSet.getRangeEnd(rn);
            utrie2_setRange32(anyCaseTrie,   rangeStart, rangeEnd, 1, TRUE, &status);
            utrie2_setRange32(lowerCaseTrie, rangeStart, rangeEnd, 1, TRUE, &status);
        }
    }

    // Serialize the data to the Spoof Detector
    {
        utrie2_freeze(anyCaseTrie,   UTRIE2_16_VALUE_BITS, &status);
        int32_t size = utrie2_serialize(anyCaseTrie, NULL, 0, &status);
        // printf("Any case Trie size: %d\n", size);
        if (status != U_BUFFER_OVERFLOW_ERROR) {
            goto cleanup;
        }
        status = U_ZERO_ERROR;
        spImpl->fSpoofData->fRawData->fAnyCaseTrie = spImpl->fSpoofData->fMemLimit;
        spImpl->fSpoofData->fRawData->fAnyCaseTrieLength = size;
        spImpl->fSpoofData->fAnyCaseTrie = anyCaseTrie;
        void *where = spImpl->fSpoofData->reserveSpace(size, status);
        utrie2_serialize(anyCaseTrie, where, size, &status);
        
        utrie2_freeze(lowerCaseTrie, UTRIE2_16_VALUE_BITS, &status);
        size = utrie2_serialize(lowerCaseTrie, NULL, 0, &status);
        // printf("Lower case Trie size: %d\n", size);
        if (status != U_BUFFER_OVERFLOW_ERROR) {
            goto cleanup;
        }
        status = U_ZERO_ERROR;
        spImpl->fSpoofData->fRawData->fLowerCaseTrie = spImpl->fSpoofData->fMemLimit;
        spImpl->fSpoofData->fRawData->fLowerCaseTrieLength = size;
        spImpl->fSpoofData->fLowerCaseTrie = lowerCaseTrie;
        where = spImpl->fSpoofData->reserveSpace(size, status);
        utrie2_serialize(lowerCaseTrie, where, size, &status);

        spImpl->fSpoofData->fRawData->fScriptSets = spImpl->fSpoofData->fMemLimit;
        spImpl->fSpoofData->fRawData->fScriptSetsLength = rtScriptSetsCount;
        ScriptSet *rtScriptSets =  static_cast<ScriptSet *>
            (spImpl->fSpoofData->reserveSpace(rtScriptSetsCount * sizeof(ScriptSet), status));
        uint32_t rindex = 2;
        for (int32_t i=2; i<scriptSets->size(); i++) {
            BuilderScriptSet *bSet = static_cast<BuilderScriptSet *>(scriptSets->elementAt(i));
            if (bSet->rindex < rindex) {
                // We have already copied this script set to the serialized data.
                continue;
            }
            U_ASSERT(rindex == bSet->rindex);
            rtScriptSets[rindex] = *bSet->sset;   // Assignment of a ScriptSet just copies the bits.
            rindex++;
        }
    }

    // Open new utrie2s from the serialized data.  We don't want to keep the ones
    //   we just built because we would then have two copies of the data, one internal to
    //   the utries that we have already constructed, and one in the serialized data area.
    //   An alternative would be to not pre-serialize the Trie data, but that makes the
    //   spoof detector data different, depending on how the detector was constructed.
    //   It's simpler to keep the data always the same.
    
    spImpl->fSpoofData->fAnyCaseTrie = utrie2_openFromSerialized(
            UTRIE2_16_VALUE_BITS,
            (const char *)spImpl->fSpoofData->fRawData + spImpl->fSpoofData->fRawData->fAnyCaseTrie,
            spImpl->fSpoofData->fRawData->fAnyCaseTrieLength,
            NULL,
            &status);

    spImpl->fSpoofData->fLowerCaseTrie = utrie2_openFromSerialized(
            UTRIE2_16_VALUE_BITS,
            (const char *)spImpl->fSpoofData->fRawData + spImpl->fSpoofData->fRawData->fLowerCaseTrie,
            spImpl->fSpoofData->fRawData->fAnyCaseTrieLength,
            NULL,
            &status);

    

cleanup:
    if (U_FAILURE(status)) {
        pe->line = lineNum;
    }
    uregex_close(parseRegexp);
    uprv_free(input);

    int32_t i;
    if (scriptSets != NULL) {
        for (i=0; i<scriptSets->size(); i++) {
            BuilderScriptSet *bsset = static_cast<BuilderScriptSet *>(scriptSets->elementAt(i));
            delete bsset;
        }
        delete scriptSets;
    }
    utrie2_close(anyCaseTrie);
    utrie2_close(lowerCaseTrie);
    return;
}

U_NAMESPACE_END



BuilderScriptSet::BuilderScriptSet() {
    codePoint = -1;
    trie = NULL;
    sset = NULL;
    index = 0;
    rindex = 0;
    scriptSetOwned = TRUE;
}

BuilderScriptSet::~BuilderScriptSet() {
    if (scriptSetOwned) {
        delete sset;
    }
}

#endif
#endif //  !UCONFIG_NO_REGULAR_EXPRESSIONS 


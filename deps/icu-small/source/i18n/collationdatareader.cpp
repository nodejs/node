// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2013-2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdatareader.cpp
*
* created on: 2013feb07
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucol.h"
#include "unicode/udata.h"
#include "unicode/uscript.h"
#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatareader.h"
#include "collationfastlatin.h"
#include "collationkeys.h"
#include "collationrootelements.h"
#include "collationsettings.h"
#include "collationtailoring.h"
#include "collunsafe.h"
#include "normalizer2impl.h"
#include "uassert.h"
#include "ucmndata.h"
#include "utrie2.h"

U_NAMESPACE_BEGIN

namespace {

int32_t getIndex(const int32_t *indexes, int32_t length, int32_t i) {
    return (i < length) ? indexes[i] : -1;
}

}  // namespace

void
CollationDataReader::read(const CollationTailoring *base, const uint8_t *inBytes, int32_t inLength,
                          CollationTailoring &tailoring, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(base != nullptr) {
        if(inBytes == nullptr || (0 <= inLength && inLength < 24)) {
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        const DataHeader *header = reinterpret_cast<const DataHeader *>(inBytes);
        if(!(header->dataHeader.magic1 == 0xda && header->dataHeader.magic2 == 0x27 &&
                isAcceptable(tailoring.version, nullptr, nullptr, &header->info))) {
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
        if(base->getUCAVersion() != tailoring.getUCAVersion()) {
            errorCode = U_COLLATOR_VERSION_MISMATCH;
            return;
        }
        int32_t headerLength = header->dataHeader.headerSize;
        inBytes += headerLength;
        if(inLength >= 0) {
            inLength -= headerLength;
        }
    }

    if(inBytes == nullptr || (0 <= inLength && inLength < 8)) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    const int32_t *inIndexes = reinterpret_cast<const int32_t *>(inBytes);
    int32_t indexesLength = inIndexes[IX_INDEXES_LENGTH];
    if(indexesLength < 2 || (0 <= inLength && inLength < indexesLength * 4)) {
        errorCode = U_INVALID_FORMAT_ERROR;  // Not enough indexes.
        return;
    }

    // Assume that the tailoring data is in initial state,
    // with nullptr pointers and 0 lengths.

    // Set pointers to non-empty data parts.
    // Do this in order of their byte offsets. (Should help porting to Java.)

    int32_t index;  // one of the indexes[] slots
    int32_t offset;  // byte offset for the index part
    int32_t length;  // number of bytes in the index part

    if(indexesLength > IX_TOTAL_SIZE) {
        length = inIndexes[IX_TOTAL_SIZE];
    } else if(indexesLength > IX_REORDER_CODES_OFFSET) {
        length = inIndexes[indexesLength - 1];
    } else {
        length = 0;  // only indexes, and inLength was already checked for them
    }
    if(0 <= inLength && inLength < length) {
        errorCode = U_INVALID_FORMAT_ERROR;
        return;
    }

    const CollationData *baseData = base == nullptr ? nullptr : base->data;
    const int32_t *reorderCodes = nullptr;
    int32_t reorderCodesLength = 0;
    const uint32_t *reorderRanges = nullptr;
    int32_t reorderRangesLength = 0;
    index = IX_REORDER_CODES_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 4) {
        if(baseData == nullptr) {
            // We assume for collation settings that
            // the base data does not have a reordering.
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
        reorderCodes = reinterpret_cast<const int32_t *>(inBytes + offset);
        reorderCodesLength = length / 4;

        // The reorderRanges (if any) are the trailing reorderCodes entries.
        // Split the array at the boundary.
        // Script or reorder codes do not exceed 16-bit values.
        // Range limits are stored in the upper 16 bits, and are never 0.
        while(reorderRangesLength < reorderCodesLength &&
                (reorderCodes[reorderCodesLength - reorderRangesLength - 1] & 0xffff0000) != 0) {
            ++reorderRangesLength;
        }
        U_ASSERT(reorderRangesLength < reorderCodesLength);
        if(reorderRangesLength != 0) {
            reorderCodesLength -= reorderRangesLength;
            reorderRanges = reinterpret_cast<const uint32_t *>(reorderCodes + reorderCodesLength);
        }
    }

    // There should be a reorder table only if there are reorder codes.
    // However, when there are reorder codes the reorder table may be omitted to reduce
    // the data size.
    const uint8_t *reorderTable = nullptr;
    index = IX_REORDER_TABLE_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 256) {
        if(reorderCodesLength == 0) {
            errorCode = U_INVALID_FORMAT_ERROR;  // Reordering table without reordering codes.
            return;
        }
        reorderTable = inBytes + offset;
    } else {
        // If we have reorder codes, then build the reorderTable at the end,
        // when the CollationData is otherwise complete.
    }

    if(baseData != nullptr && baseData->numericPrimary != (inIndexes[IX_OPTIONS] & 0xff000000)) {
        errorCode = U_INVALID_FORMAT_ERROR;
        return;
    }
    CollationData *data = nullptr;  // Remains nullptr if there are no mappings.

    index = IX_TRIE_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 8) {
        if(!tailoring.ensureOwnedData(errorCode)) { return; }
        data = tailoring.ownedData;
        data->base = baseData;
        data->numericPrimary = inIndexes[IX_OPTIONS] & 0xff000000;
        data->trie = tailoring.trie = utrie2_openFromSerialized(
            UTRIE2_32_VALUE_BITS, inBytes + offset, length, nullptr,
            &errorCode);
        if(U_FAILURE(errorCode)) { return; }
    } else if(baseData != nullptr) {
        // Use the base data. Only the settings are tailored.
        tailoring.data = baseData;
    } else {
        errorCode = U_INVALID_FORMAT_ERROR;  // No mappings.
        return;
    }

    index = IX_CES_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 8) {
        if(data == nullptr) {
            errorCode = U_INVALID_FORMAT_ERROR;  // Tailored ces without tailored trie.
            return;
        }
        data->ces = reinterpret_cast<const int64_t *>(inBytes + offset);
        data->cesLength = length / 8;
    }

    index = IX_CE32S_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 4) {
        if(data == nullptr) {
            errorCode = U_INVALID_FORMAT_ERROR;  // Tailored ce32s without tailored trie.
            return;
        }
        data->ce32s = reinterpret_cast<const uint32_t *>(inBytes + offset);
        data->ce32sLength = length / 4;
    }

    int32_t jamoCE32sStart = getIndex(inIndexes, indexesLength, IX_JAMO_CE32S_START);
    if(jamoCE32sStart >= 0) {
        if(data == nullptr || data->ce32s == nullptr) {
            errorCode = U_INVALID_FORMAT_ERROR;  // Index into non-existent ce32s[].
            return;
        }
        data->jamoCE32s = data->ce32s + jamoCE32sStart;
    } else if(data == nullptr) {
        // Nothing to do.
    } else if(baseData != nullptr) {
        data->jamoCE32s = baseData->jamoCE32s;
    } else {
        errorCode = U_INVALID_FORMAT_ERROR;  // No Jamo CE32s for Hangul processing.
        return;
    }

    index = IX_ROOT_ELEMENTS_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 4) {
        length /= 4;
        if(data == nullptr || length <= CollationRootElements::IX_SEC_TER_BOUNDARIES) {
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
        data->rootElements = reinterpret_cast<const uint32_t *>(inBytes + offset);
        data->rootElementsLength = length;
        uint32_t commonSecTer = data->rootElements[CollationRootElements::IX_COMMON_SEC_AND_TER_CE];
        if(commonSecTer != Collation::COMMON_SEC_AND_TER_CE) {
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
        uint32_t secTerBoundaries = data->rootElements[CollationRootElements::IX_SEC_TER_BOUNDARIES];
        if((secTerBoundaries >> 24) < CollationKeys::SEC_COMMON_HIGH) {
            // [fixed last secondary common byte] is too low,
            // and secondary weights would collide with compressed common secondaries.
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
    }

    index = IX_CONTEXTS_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 2) {
        if(data == nullptr) {
            errorCode = U_INVALID_FORMAT_ERROR;  // Tailored contexts without tailored trie.
            return;
        }
        data->contexts = reinterpret_cast<const char16_t *>(inBytes + offset);
        data->contextsLength = length / 2;
    }

    index = IX_UNSAFE_BWD_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 2) {
        if(data == nullptr) {
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
        if(baseData == nullptr) {
#if defined(COLLUNSAFE_COLL_VERSION) && defined (COLLUNSAFE_SERIALIZE)
          tailoring.unsafeBackwardSet = new UnicodeSet(unsafe_serializedData, unsafe_serializedCount, UnicodeSet::kSerialized, errorCode);
          if(tailoring.unsafeBackwardSet == nullptr) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return;
          } else if (U_FAILURE(errorCode)) {
            return;
          }
#else
            // Create the unsafe-backward set for the root collator.
            // Include all non-zero combining marks and trail surrogates.
            // We do this at load time, rather than at build time,
            // to simplify Unicode version bootstrapping:
            // The root data builder only needs the new FractionalUCA.txt data,
            // but it need not be built with a version of ICU already updated to
            // the corresponding new Unicode Character Database.
            //
            // The following is an optimized version of
            // new UnicodeSet("[[:^lccc=0:][\\udc00-\\udfff]]").
            // It is faster and requires fewer code dependencies.
            tailoring.unsafeBackwardSet = new UnicodeSet(0xdc00, 0xdfff);  // trail surrogates
            if(tailoring.unsafeBackwardSet == nullptr) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            data->nfcImpl.addLcccChars(*tailoring.unsafeBackwardSet);
#endif // !COLLUNSAFE_SERIALIZE || !COLLUNSAFE_COLL_VERSION
        } else {
            // Clone the root collator's set contents.
            tailoring.unsafeBackwardSet = static_cast<UnicodeSet *>(
                baseData->unsafeBackwardSet->cloneAsThawed());
            if(tailoring.unsafeBackwardSet == nullptr) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
        }
        // Add the ranges from the data file to the unsafe-backward set.
        USerializedSet sset;
        const uint16_t *unsafeData = reinterpret_cast<const uint16_t *>(inBytes + offset);
        if(!uset_getSerializedSet(&sset, unsafeData, length / 2)) {
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
        int32_t count = uset_getSerializedRangeCount(&sset);
        for(int32_t i = 0; i < count; ++i) {
            UChar32 start, end;
            uset_getSerializedRange(&sset, i, &start, &end);
            tailoring.unsafeBackwardSet->add(start, end);
        }
        // Mark each lead surrogate as "unsafe"
        // if any of its 1024 associated supplementary code points is "unsafe".
        UChar32 c = 0x10000;
        for(char16_t lead = 0xd800; lead < 0xdc00; ++lead, c += 0x400) {
            if(!tailoring.unsafeBackwardSet->containsNone(c, c + 0x3ff)) {
                tailoring.unsafeBackwardSet->add(lead);
            }
        }
        tailoring.unsafeBackwardSet->freeze();
        data->unsafeBackwardSet = tailoring.unsafeBackwardSet;
    } else if(data == nullptr) {
        // Nothing to do.
    } else if(baseData != nullptr) {
        // No tailoring-specific data: Alias the root collator's set.
        data->unsafeBackwardSet = baseData->unsafeBackwardSet;
    } else {
        errorCode = U_INVALID_FORMAT_ERROR;  // No unsafeBackwardSet.
        return;
    }

    // If the fast Latin format version is different,
    // or the version is set to 0 for "no fast Latin table",
    // then just always use the normal string comparison path.
    if(data != nullptr) {
        data->fastLatinTable = nullptr;
        data->fastLatinTableLength = 0;
        if(((inIndexes[IX_OPTIONS] >> 16) & 0xff) == CollationFastLatin::VERSION) {
            index = IX_FAST_LATIN_TABLE_OFFSET;
            offset = getIndex(inIndexes, indexesLength, index);
            length = getIndex(inIndexes, indexesLength, index + 1) - offset;
            if(length >= 2) {
                data->fastLatinTable = reinterpret_cast<const uint16_t *>(inBytes + offset);
                data->fastLatinTableLength = length / 2;
                if((*data->fastLatinTable >> 8) != CollationFastLatin::VERSION) {
                    errorCode = U_INVALID_FORMAT_ERROR;  // header vs. table version mismatch
                    return;
                }
            } else if(baseData != nullptr) {
                data->fastLatinTable = baseData->fastLatinTable;
                data->fastLatinTableLength = baseData->fastLatinTableLength;
            }
        }
    }

    index = IX_SCRIPTS_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 2) {
        if(data == nullptr) {
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
        const uint16_t *scripts = reinterpret_cast<const uint16_t *>(inBytes + offset);
        int32_t scriptsLength = length / 2;
        data->numScripts = scripts[0];
        // There must be enough entries for both arrays, including more than two range starts.
        data->scriptStartsLength = scriptsLength - (1 + data->numScripts + 16);
        if(data->scriptStartsLength <= 2 ||
                CollationData::MAX_NUM_SCRIPT_RANGES < data->scriptStartsLength) {
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
        data->scriptsIndex = scripts + 1;
        data->scriptStarts = scripts + 1 + data->numScripts + 16;
        if(!(data->scriptStarts[0] == 0 &&
                data->scriptStarts[1] == ((Collation::MERGE_SEPARATOR_BYTE + 1) << 8) &&
                data->scriptStarts[data->scriptStartsLength - 1] ==
                        (Collation::TRAIL_WEIGHT_BYTE << 8))) {
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
    } else if(data == nullptr) {
        // Nothing to do.
    } else if(baseData != nullptr) {
        data->numScripts = baseData->numScripts;
        data->scriptsIndex = baseData->scriptsIndex;
        data->scriptStarts = baseData->scriptStarts;
        data->scriptStartsLength = baseData->scriptStartsLength;
    }

    index = IX_COMPRESSIBLE_BYTES_OFFSET;
    offset = getIndex(inIndexes, indexesLength, index);
    length = getIndex(inIndexes, indexesLength, index + 1) - offset;
    if(length >= 256) {
        if(data == nullptr) {
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
        data->compressibleBytes = reinterpret_cast<const UBool *>(inBytes + offset);
    } else if(data == nullptr) {
        // Nothing to do.
    } else if(baseData != nullptr) {
        data->compressibleBytes = baseData->compressibleBytes;
    } else {
        errorCode = U_INVALID_FORMAT_ERROR;  // No compressibleBytes[].
        return;
    }

    const CollationSettings &ts = *tailoring.settings;
    int32_t options = inIndexes[IX_OPTIONS] & 0xffff;
    uint16_t fastLatinPrimaries[CollationFastLatin::LATIN_LIMIT];
    int32_t fastLatinOptions = CollationFastLatin::getOptions(
            tailoring.data, ts, fastLatinPrimaries, UPRV_LENGTHOF(fastLatinPrimaries));
    if(options == ts.options && ts.variableTop != 0 &&
            reorderCodesLength == ts.reorderCodesLength &&
            (reorderCodesLength == 0 ||
                uprv_memcmp(reorderCodes, ts.reorderCodes, reorderCodesLength * 4) == 0) &&
            fastLatinOptions == ts.fastLatinOptions &&
            (fastLatinOptions < 0 ||
                uprv_memcmp(fastLatinPrimaries, ts.fastLatinPrimaries,
                            sizeof(fastLatinPrimaries)) == 0)) {
        return;
    }

    CollationSettings *settings = SharedObject::copyOnWrite(tailoring.settings);
    if(settings == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    settings->options = options;
    // Set variableTop from options and scripts data.
    settings->variableTop = tailoring.data->getLastPrimaryForGroup(
            UCOL_REORDER_CODE_FIRST + int32_t{settings->getMaxVariable()});
    if(settings->variableTop == 0) {
        errorCode = U_INVALID_FORMAT_ERROR;
        return;
    }

    if(reorderCodesLength != 0) {
        settings->aliasReordering(*baseData, reorderCodes, reorderCodesLength,
                                  reorderRanges, reorderRangesLength,
                                  reorderTable, errorCode);
    }

    settings->fastLatinOptions = CollationFastLatin::getOptions(
        tailoring.data, *settings,
        settings->fastLatinPrimaries, UPRV_LENGTHOF(settings->fastLatinPrimaries));
}

UBool U_CALLCONV
CollationDataReader::isAcceptable(void *context,
                                  const char * /* type */, const char * /*name*/,
                                  const UDataInfo *pInfo) {
    if(
        pInfo->size >= 20 &&
        pInfo->isBigEndian == U_IS_BIG_ENDIAN &&
        pInfo->charsetFamily == U_CHARSET_FAMILY &&
        pInfo->dataFormat[0] == 0x55 &&  // dataFormat="UCol"
        pInfo->dataFormat[1] == 0x43 &&
        pInfo->dataFormat[2] == 0x6f &&
        pInfo->dataFormat[3] == 0x6c &&
        pInfo->formatVersion[0] == 5
    ) {
        UVersionInfo *version = static_cast<UVersionInfo *>(context);
        if(version != nullptr) {
            uprv_memcpy(version, pInfo->dataVersion, 4);
        }
        return true;
    } else {
        return false;
    }
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION

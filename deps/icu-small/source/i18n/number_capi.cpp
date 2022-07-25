// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include "fphdlimp.h"
#include "number_utypes.h"
#include "numparse_types.h"
#include "formattedval_impl.h"
#include "number_decnum.h"
#include "unicode/numberformatter.h"
#include "unicode/unumberformatter.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;


U_NAMESPACE_BEGIN
namespace number {
namespace impl {

/**
 * Implementation class for UNumberFormatter. Wraps a LocalizedNumberFormatter.
 */
struct UNumberFormatterData : public UMemory,
        // Magic number as ASCII == "NFR" (NumberFormatteR)
        public IcuCApiHelper<UNumberFormatter, UNumberFormatterData, 0x4E465200> {
    LocalizedNumberFormatter fFormatter;
};

struct UFormattedNumberImpl;

// Magic number as ASCII == "FDN" (FormatteDNumber)
typedef IcuCApiHelper<UFormattedNumber, UFormattedNumberImpl, 0x46444E00> UFormattedNumberApiHelper;

struct UFormattedNumberImpl : public UFormattedValueImpl, public UFormattedNumberApiHelper {
    UFormattedNumberImpl();
    ~UFormattedNumberImpl();

    FormattedNumber fImpl;
    UFormattedNumberData fData;
};

UFormattedNumberImpl::UFormattedNumberImpl()
        : fImpl(&fData) {
    fFormattedValue = &fImpl;
}

UFormattedNumberImpl::~UFormattedNumberImpl() {
    // Disown the data from fImpl so it doesn't get deleted twice
    fImpl.fData = nullptr;
}

}
}
U_NAMESPACE_END


UPRV_FORMATTED_VALUE_CAPI_NO_IMPLTYPE_AUTO_IMPL(
    UFormattedNumber,
    UFormattedNumberImpl,
    UFormattedNumberApiHelper,
    unumf)


const DecimalQuantity* icu::number::impl::validateUFormattedNumberToDecimalQuantity(
        const UFormattedNumber* uresult, UErrorCode& status) {
    auto* result = UFormattedNumberApiHelper::validate(uresult, status);
    if (U_FAILURE(status)) {
        return nullptr;
    }
    return &result->fData.quantity;
}



U_CAPI UNumberFormatter* U_EXPORT2
unumf_openForSkeletonAndLocale(const UChar* skeleton, int32_t skeletonLen, const char* locale,
                               UErrorCode* ec) {
    auto* impl = new UNumberFormatterData();
    if (impl == nullptr) {
        *ec = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    // Readonly-alias constructor (first argument is whether we are NUL-terminated)
    UnicodeString skeletonString(skeletonLen == -1, skeleton, skeletonLen);
    impl->fFormatter = NumberFormatter::forSkeleton(skeletonString, *ec).locale(locale);
    return impl->exportForC();
}

U_CAPI UNumberFormatter* U_EXPORT2
unumf_openForSkeletonAndLocaleWithError(const UChar* skeleton, int32_t skeletonLen, const char* locale,
                                         UParseError* perror, UErrorCode* ec) {
    auto* impl = new UNumberFormatterData();
    if (impl == nullptr) {
        *ec = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    // Readonly-alias constructor (first argument is whether we are NUL-terminated)
    UnicodeString skeletonString(skeletonLen == -1, skeleton, skeletonLen);
    impl->fFormatter = NumberFormatter::forSkeleton(skeletonString, *perror, *ec).locale(locale);
    return impl->exportForC();
}

U_CAPI void U_EXPORT2
unumf_formatInt(const UNumberFormatter* uformatter, int64_t value, UFormattedNumber* uresult,
                UErrorCode* ec) {
    const UNumberFormatterData* formatter = UNumberFormatterData::validate(uformatter, *ec);
    auto* result = UFormattedNumberApiHelper::validate(uresult, *ec);
    if (U_FAILURE(*ec)) { return; }

    result->fData.resetString();
    result->fData.quantity.clear();
    result->fData.quantity.setToLong(value);
    formatter->fFormatter.formatImpl(&result->fData, *ec);
}

U_CAPI void U_EXPORT2
unumf_formatDouble(const UNumberFormatter* uformatter, double value, UFormattedNumber* uresult,
                   UErrorCode* ec) {
    const UNumberFormatterData* formatter = UNumberFormatterData::validate(uformatter, *ec);
    auto* result = UFormattedNumberApiHelper::validate(uresult, *ec);
    if (U_FAILURE(*ec)) { return; }

    result->fData.resetString();
    result->fData.quantity.clear();
    result->fData.quantity.setToDouble(value);
    formatter->fFormatter.formatImpl(&result->fData, *ec);
}

U_CAPI void U_EXPORT2
unumf_formatDecimal(const UNumberFormatter* uformatter, const char* value, int32_t valueLen,
                    UFormattedNumber* uresult, UErrorCode* ec) {
    const UNumberFormatterData* formatter = UNumberFormatterData::validate(uformatter, *ec);
    auto* result = UFormattedNumberApiHelper::validate(uresult, *ec);
    if (U_FAILURE(*ec)) { return; }

    result->fData.resetString();
    result->fData.quantity.clear();
    result->fData.quantity.setToDecNumber({value, valueLen}, *ec);
    if (U_FAILURE(*ec)) { return; }
    formatter->fFormatter.formatImpl(&result->fData, *ec);
}

U_CAPI int32_t U_EXPORT2
unumf_resultToString(const UFormattedNumber* uresult, UChar* buffer, int32_t bufferCapacity,
                     UErrorCode* ec) {
    const auto* result = UFormattedNumberApiHelper::validate(uresult, *ec);
    if (U_FAILURE(*ec)) { return 0; }

    if (buffer == nullptr ? bufferCapacity != 0 : bufferCapacity < 0) {
        *ec = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    return result->fData.toTempString(*ec).extract(buffer, bufferCapacity, *ec);
}

U_CAPI UBool U_EXPORT2
unumf_resultNextFieldPosition(const UFormattedNumber* uresult, UFieldPosition* ufpos, UErrorCode* ec) {
    const auto* result = UFormattedNumberApiHelper::validate(uresult, *ec);
    if (U_FAILURE(*ec)) { return FALSE; }

    if (ufpos == nullptr) {
        *ec = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }

    FieldPosition fp;
    fp.setField(ufpos->field);
    fp.setBeginIndex(ufpos->beginIndex);
    fp.setEndIndex(ufpos->endIndex);
    bool retval = result->fData.nextFieldPosition(fp, *ec);
    ufpos->beginIndex = fp.getBeginIndex();
    ufpos->endIndex = fp.getEndIndex();
    // NOTE: MSVC sometimes complains when implicitly converting between bool and UBool
    return retval ? TRUE : FALSE;
}

U_CAPI void U_EXPORT2
unumf_resultGetAllFieldPositions(const UFormattedNumber* uresult, UFieldPositionIterator* ufpositer,
                                 UErrorCode* ec) {
    const auto* result = UFormattedNumberApiHelper::validate(uresult, *ec);
    if (U_FAILURE(*ec)) { return; }

    if (ufpositer == nullptr) {
        *ec = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    auto* fpi = reinterpret_cast<FieldPositionIterator*>(ufpositer);
    FieldPositionIteratorHandler fpih(fpi, *ec);
    result->fData.getAllFieldPositions(fpih, *ec);
}

U_CAPI int32_t U_EXPORT2
unumf_resultToDecimalNumber(
        const UFormattedNumber* uresult,
        char* dest,
        int32_t destCapacity,
        UErrorCode* ec) {
    const auto* result = UFormattedNumberApiHelper::validate(uresult, *ec);
    if (U_FAILURE(*ec)) {
        return 0;
    }
    DecNum decnum;
    return result->fData.quantity
        .toDecNum(decnum, *ec)
        .toCharString(*ec)
        .extract(dest, destCapacity, *ec);
}

U_CAPI void U_EXPORT2
unumf_close(UNumberFormatter* f) {
    UErrorCode localStatus = U_ZERO_ERROR;
    const UNumberFormatterData* impl = UNumberFormatterData::validate(f, localStatus);
    delete impl;
}


#endif /* #if !UCONFIG_NO_FORMATTING */




























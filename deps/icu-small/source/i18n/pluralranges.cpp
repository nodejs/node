// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include "unicode/numberrangeformatter.h"
#include "pluralranges.h"
#include "uresimp.h"
#include "charstr.h"
#include "uassert.h"
#include "util.h"
#include "numrange_impl.h"

U_NAMESPACE_BEGIN


namespace {

class PluralRangesDataSink : public ResourceSink {
  public:
    PluralRangesDataSink(StandardPluralRanges& output) : fOutput(output) {}

    void put(const char* /*key*/, ResourceValue& value, UBool /*noFallback*/, UErrorCode& status) override {
        ResourceArray entriesArray = value.getArray(status);
        if (U_FAILURE(status)) { return; }
        fOutput.setCapacity(entriesArray.getSize(), status);
        if (U_FAILURE(status)) { return; }
        for (int i = 0; entriesArray.getValue(i, value); i++) {
            ResourceArray pluralFormsArray = value.getArray(status);
            if (U_FAILURE(status)) { return; }
            if (pluralFormsArray.getSize() != 3) {
                status = U_RESOURCE_TYPE_MISMATCH;
                return;
            }
            pluralFormsArray.getValue(0, value);
            StandardPlural::Form first = StandardPlural::fromString(value.getUnicodeString(status), status);
            if (U_FAILURE(status)) { return; }
            pluralFormsArray.getValue(1, value);
            StandardPlural::Form second = StandardPlural::fromString(value.getUnicodeString(status), status);
            if (U_FAILURE(status)) { return; }
            pluralFormsArray.getValue(2, value);
            StandardPlural::Form result = StandardPlural::fromString(value.getUnicodeString(status), status);
            if (U_FAILURE(status)) { return; }
            fOutput.addPluralRange(first, second, result);
        }
    }

  private:
    StandardPluralRanges& fOutput;
};

void getPluralRangesData(const Locale& locale, StandardPluralRanges& output, UErrorCode& status) {
    LocalUResourceBundlePointer rb(ures_openDirect(nullptr, "pluralRanges", &status));
    if (U_FAILURE(status)) { return; }

    CharString dataPath;
    dataPath.append("locales/", -1, status);
    dataPath.append(locale.getLanguage(), -1, status);
    if (U_FAILURE(status)) { return; }
    int32_t setLen;
    // Not all languages are covered: fail gracefully
    UErrorCode internalStatus = U_ZERO_ERROR;
    const char16_t* set = ures_getStringByKeyWithFallback(rb.getAlias(), dataPath.data(), &setLen, &internalStatus);
    if (U_FAILURE(internalStatus)) { return; }

    dataPath.clear();
    dataPath.append("rules/", -1, status);
    dataPath.appendInvariantChars(set, setLen, status);
    if (U_FAILURE(status)) { return; }
    PluralRangesDataSink sink(output);
    ures_getAllItemsWithFallback(rb.getAlias(), dataPath.data(), sink, status);
}

} // namespace


StandardPluralRanges
StandardPluralRanges::forLocale(const Locale& locale, UErrorCode& status) {
    StandardPluralRanges result;
    getPluralRangesData(locale, result, status);
    return result;
}

StandardPluralRanges
StandardPluralRanges::copy(UErrorCode& status) const {
    StandardPluralRanges result;
    if (fTriplesLen > result.fTriples.getCapacity()) {
        if (result.fTriples.resize(fTriplesLen) == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return result;
        }
    }
    uprv_memcpy(result.fTriples.getAlias(),
        fTriples.getAlias(),
        fTriplesLen * sizeof(fTriples[0]));
    result.fTriplesLen = fTriplesLen;
    return result;
}

LocalPointer<StandardPluralRanges>
StandardPluralRanges::toPointer(UErrorCode& status) && noexcept {
    return LocalPointer<StandardPluralRanges>(new StandardPluralRanges(std::move(*this)), status);
}

void StandardPluralRanges::addPluralRange(
        StandardPlural::Form first,
        StandardPlural::Form second,
        StandardPlural::Form result) {
    U_ASSERT(fTriplesLen < fTriples.getCapacity());
    fTriples[fTriplesLen] = {first, second, result};
    fTriplesLen++;
}

void StandardPluralRanges::setCapacity(int32_t length, UErrorCode& status) {
    if (U_FAILURE(status)) { return; }
    if (length > fTriples.getCapacity()) {
        if (fTriples.resize(length, 0) == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
    }
}

StandardPlural::Form
StandardPluralRanges::resolve(StandardPlural::Form first, StandardPlural::Form second) const {
    for (int32_t i=0; i<fTriplesLen; i++) {
        const auto& triple = fTriples[i];
        if (triple.first == first && triple.second == second) {
            return triple.result;
        }
    }
    // Default fallback
    return StandardPlural::OTHER;
}


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

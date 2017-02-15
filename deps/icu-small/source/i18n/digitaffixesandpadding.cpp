// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: digitaffixesandpadding.cpp
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/plurrule.h"
#include "charstr.h"
#include "digitaffix.h"
#include "digitaffixesandpadding.h"
#include "digitlst.h"
#include "uassert.h"
#include "valueformatter.h"
#include "visibledigits.h"

U_NAMESPACE_BEGIN

UBool
DigitAffixesAndPadding::needsPluralRules() const {
    return (
            fPositivePrefix.hasMultipleVariants() ||
            fPositiveSuffix.hasMultipleVariants() ||
            fNegativePrefix.hasMultipleVariants() ||
            fNegativeSuffix.hasMultipleVariants());
}

UnicodeString &
DigitAffixesAndPadding::formatInt32(
        int32_t value,
        const ValueFormatter &formatter,
        FieldPositionHandler &handler,
        const PluralRules *optPluralRules,
        UnicodeString &appendTo,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    if (optPluralRules != NULL || fWidth > 0 || !formatter.isFastFormattable(value)) {
        VisibleDigitsWithExponent digits;
        formatter.toVisibleDigitsWithExponent(
                (int64_t) value, digits, status);
        return format(
                digits,
                formatter,
                handler,
                optPluralRules,
                appendTo,
                status);
    }
    UBool bPositive = value >= 0;
    const DigitAffix *prefix = bPositive ? &fPositivePrefix.getOtherVariant() : &fNegativePrefix.getOtherVariant();
    const DigitAffix *suffix = bPositive ? &fPositiveSuffix.getOtherVariant() : &fNegativeSuffix.getOtherVariant();
    if (value < 0) {
        value = -value;
    }
    prefix->format(handler, appendTo);
    formatter.formatInt32(value, handler, appendTo);
    return suffix->format(handler, appendTo);
}

static UnicodeString &
formatAffix(
        const DigitAffix *affix,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) {
    if (affix) {
        affix->format(handler, appendTo);
    }
    return appendTo;
}

static int32_t
countAffixChar32(const DigitAffix *affix) {
    if (affix) {
        return affix->countChar32();
    }
    return 0;
}

UnicodeString &
DigitAffixesAndPadding::format(
        const VisibleDigitsWithExponent &digits,
        const ValueFormatter &formatter,
        FieldPositionHandler &handler,
        const PluralRules *optPluralRules,
        UnicodeString &appendTo,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    const DigitAffix *prefix = NULL;
    const DigitAffix *suffix = NULL;
    if (!digits.isNaN()) {
        UBool bPositive = !digits.isNegative();
        const PluralAffix *pluralPrefix = bPositive ? &fPositivePrefix : &fNegativePrefix;
        const PluralAffix *pluralSuffix = bPositive ? &fPositiveSuffix : &fNegativeSuffix;
        if (optPluralRules == NULL || digits.isInfinite()) {
            prefix = &pluralPrefix->getOtherVariant();
            suffix = &pluralSuffix->getOtherVariant();
        } else {
            UnicodeString count(optPluralRules->select(digits));
            prefix = &pluralPrefix->getByCategory(count);
            suffix = &pluralSuffix->getByCategory(count);
        }
    }
    if (fWidth <= 0) {
        formatAffix(prefix, handler, appendTo);
        formatter.format(digits, handler, appendTo);
        return formatAffix(suffix, handler, appendTo);
    }
    int32_t codePointCount = countAffixChar32(prefix) + formatter.countChar32(digits) + countAffixChar32(suffix);
    int32_t paddingCount = fWidth - codePointCount;
    switch (fPadPosition) {
    case kPadBeforePrefix:
        appendPadding(paddingCount, appendTo);
        formatAffix(prefix, handler, appendTo);
        formatter.format(digits, handler, appendTo);
        return formatAffix(suffix, handler, appendTo);
    case kPadAfterPrefix:
        formatAffix(prefix, handler, appendTo);
        appendPadding(paddingCount, appendTo);
        formatter.format(digits, handler, appendTo);
        return formatAffix(suffix, handler, appendTo);
    case kPadBeforeSuffix:
        formatAffix(prefix, handler, appendTo);
        formatter.format(digits, handler, appendTo);
        appendPadding(paddingCount, appendTo);
        return formatAffix(suffix, handler, appendTo);
    case kPadAfterSuffix:
        formatAffix(prefix, handler, appendTo);
        formatter.format(digits, handler, appendTo);
        formatAffix(suffix, handler, appendTo);
        return appendPadding(paddingCount, appendTo);
    default:
        U_ASSERT(FALSE);
        return appendTo;
    }
}

UnicodeString &
DigitAffixesAndPadding::format(
        DigitList &value,
        const ValueFormatter &formatter,
        FieldPositionHandler &handler,
        const PluralRules *optPluralRules,
        UnicodeString &appendTo,
        UErrorCode &status) const {
    VisibleDigitsWithExponent digits;
    formatter.toVisibleDigitsWithExponent(
            value, digits, status);
    if (U_FAILURE(status)) {
        return appendTo;
    }
    return format(
            digits, formatter, handler, optPluralRules, appendTo, status);
}

UnicodeString &
DigitAffixesAndPadding::appendPadding(int32_t paddingCount, UnicodeString &appendTo) const {
    for (int32_t i = 0; i < paddingCount; ++i) {
        appendTo.append(fPadChar);
    }
    return appendTo;
}


U_NAMESPACE_END
#endif /* #if !UCONFIG_NO_FORMATTING */

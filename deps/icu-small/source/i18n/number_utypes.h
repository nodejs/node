// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __SOURCE_NUMBER_UTYPES_H__
#define __SOURCE_NUMBER_UTYPES_H__

#include "unicode/numberformatter.h"
#include "number_types.h"
#include "number_decimalquantity.h"
#include "number_stringbuilder.h"

U_NAMESPACE_BEGIN namespace number {
namespace impl {


/**
 * Implementation class for UNumberFormatter with a magic number for safety.
 *
 * Wraps a LocalizedNumberFormatter by value.
 */
struct UNumberFormatterData : public UMemory {
    // The magic number to identify incoming objects.
    // Reads in ASCII as "NFR" (NumberFormatteR with room at the end)
    static constexpr int32_t kMagic = 0x4E465200;

    // Data members:
    int32_t fMagic = kMagic;
    LocalizedNumberFormatter fFormatter;

    /** Convert from UNumberFormatter -> UNumberFormatterData. */
    static UNumberFormatterData* validate(UNumberFormatter* input, UErrorCode& status);

    /** Convert from UNumberFormatter -> UNumberFormatterData (const version). */
    static const UNumberFormatterData* validate(const UNumberFormatter* input, UErrorCode& status);

    /** Convert from UNumberFormatterData -> UNumberFormatter. */
    UNumberFormatter* exportForC();
};


/**
 * Implementation class for UFormattedNumber with magic number for safety.
 *
 * This struct is also held internally by the C++ version FormattedNumber since the member types are not
 * declared in the public header file.
 *
 * The DecimalQuantity is not currently being used by FormattedNumber, but at some point it could be used
 * to add a toDecNumber() or similar method.
 */
struct UFormattedNumberData : public UMemory {
    // The magic number to identify incoming objects.
    // Reads in ASCII as "FDN" (FormatteDNumber with room at the end)
    static constexpr int32_t kMagic = 0x46444E00;

    // Data members:
    int32_t fMagic = kMagic;
    DecimalQuantity quantity;
    NumberStringBuilder string;

    /** Convert from UFormattedNumber -> UFormattedNumberData. */
    static UFormattedNumberData* validate(UFormattedNumber* input, UErrorCode& status);

    /** Convert from UFormattedNumber -> UFormattedNumberData (const version). */
    static const UFormattedNumberData* validate(const UFormattedNumber* input, UErrorCode& status);

    /** Convert from UFormattedNumberData -> UFormattedNumber. */
    UFormattedNumber* exportForC();
};


} // namespace impl
} // namespace number
U_NAMESPACE_END

#endif //__SOURCE_NUMBER_UTYPES_H__
#endif /* #if !UCONFIG_NO_FORMATTING */

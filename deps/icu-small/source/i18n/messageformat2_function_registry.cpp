// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include <math.h>
#include <cmath>

#include "unicode/dtptngen.h"
#include "unicode/messageformat2.h"
#include "unicode/messageformat2_data_model_names.h"
#include "unicode/messageformat2_function_registry.h"
#include "unicode/normalizer2.h"
#include "unicode/simpletz.h"
#include "unicode/smpdtfmt.h"
#include "charstr.h"
#include "double-conversion.h"
#include "messageformat2_allocation.h"
#include "messageformat2_function_registry_internal.h"
#include "messageformat2_macros.h"
#include "hash.h"
#include "mutex.h"
#include "number_types.h"
#include "ucln_in.h"
#include "uvector.h" // U_ASSERT

// The C99 standard suggested that C++ implementations not define PRId64 etc. constants
// unless this macro is defined.
// See the Notes at https://en.cppreference.com/w/cpp/types/integer .
// Similar to defining __STDC_LIMIT_MACROS in unicode/ptypes.h .
#ifndef __STDC_FORMAT_MACROS
#   define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <math.h>

U_NAMESPACE_BEGIN

namespace message2 {

// Function registry implementation

Formatter::~Formatter() {}
Selector::~Selector() {}
FormatterFactory::~FormatterFactory() {}
SelectorFactory::~SelectorFactory() {}

MFFunctionRegistry MFFunctionRegistry::Builder::build() {
    U_ASSERT(formatters != nullptr && selectors != nullptr && formattersByType != nullptr);
    MFFunctionRegistry result = MFFunctionRegistry(formatters, selectors, formattersByType);
    formatters = nullptr;
    selectors = nullptr;
    formattersByType = nullptr;
    return result;
}

MFFunctionRegistry::Builder& MFFunctionRegistry::Builder::adoptSelector(const FunctionName& selectorName, SelectorFactory* selectorFactory, UErrorCode& errorCode) {
    if (U_SUCCESS(errorCode)) {
        U_ASSERT(selectors != nullptr);
        selectors->put(selectorName, selectorFactory, errorCode);
    }
    return *this;
}

MFFunctionRegistry::Builder& MFFunctionRegistry::Builder::adoptFormatter(const FunctionName& formatterName, FormatterFactory* formatterFactory, UErrorCode& errorCode) {
    if (U_SUCCESS(errorCode)) {
        U_ASSERT(formatters != nullptr);
        formatters->put(formatterName, formatterFactory, errorCode);
    }
    return *this;
}

MFFunctionRegistry::Builder& MFFunctionRegistry::Builder::setDefaultFormatterNameByType(const UnicodeString& type, const FunctionName& functionName, UErrorCode& errorCode) {
    if (U_SUCCESS(errorCode)) {
        U_ASSERT(formattersByType != nullptr);
        FunctionName* f = create<FunctionName>(FunctionName(functionName), errorCode);
        formattersByType->put(type, f, errorCode);
    }
    return *this;
}

MFFunctionRegistry::Builder::Builder(UErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    formatters = new Hashtable();
    selectors = new Hashtable();
    formattersByType = new Hashtable();
    if (!(formatters != nullptr && selectors != nullptr && formattersByType != nullptr)) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    } else {
        formatters->setValueDeleter(uprv_deleteUObject);
        selectors->setValueDeleter(uprv_deleteUObject);
        formattersByType->setValueDeleter(uprv_deleteUObject);
    }
}

MFFunctionRegistry::Builder::~Builder() {
    if (formatters != nullptr) {
        delete formatters;
    }
    if (selectors != nullptr) {
        delete selectors;
    }
    if (formattersByType != nullptr) {
        delete formattersByType;
    }
}

// Returns non-owned pointer. Returns pointer rather than reference because it can fail.
// Returns non-const because FormatterFactory is mutable.
// TODO: This is unsafe because of the cached-formatters map
// (the caller could delete the resulting pointer)
FormatterFactory* MFFunctionRegistry::getFormatter(const FunctionName& formatterName) const {
    U_ASSERT(formatters != nullptr);
    return static_cast<FormatterFactory*>(formatters->get(formatterName));
}

UBool MFFunctionRegistry::getDefaultFormatterNameByType(const UnicodeString& type, FunctionName& name) const {
    U_ASSERT(formatters != nullptr);
    const FunctionName* f = static_cast<FunctionName*>(formattersByType->get(type));
    if (f != nullptr) {
        name = *f;
        return true;
    }
    return false;
}

const SelectorFactory* MFFunctionRegistry::getSelector(const FunctionName& selectorName) const {
    U_ASSERT(selectors != nullptr);
    return static_cast<const SelectorFactory*>(selectors->get(selectorName));
}

bool MFFunctionRegistry::hasFormatter(const FunctionName& f) const {
    return getFormatter(f) != nullptr;
}

bool MFFunctionRegistry::hasSelector(const FunctionName& s) const {
    return getSelector(s) != nullptr;
}

void MFFunctionRegistry::checkFormatter(const char* s) const {
#if U_DEBUG
    U_ASSERT(hasFormatter(FunctionName(UnicodeString(s))));
#else
   (void) s;
#endif
}

void MFFunctionRegistry::checkSelector(const char* s) const {
#if U_DEBUG
    U_ASSERT(hasSelector(FunctionName(UnicodeString(s))));
#else
    (void) s;
#endif
}

// Debugging
void MFFunctionRegistry::checkStandard() const {
    checkFormatter("datetime");
    checkFormatter("date");
    checkFormatter("time");
    checkFormatter("number");
    checkFormatter("integer");
    checkFormatter("test:function");
    checkFormatter("test:format");
    checkSelector("number");
    checkSelector("integer");
    checkSelector("string");
    checkSelector("test:function");
    checkSelector("test:select");
}

// Formatter/selector helpers

// Returns the NFC-normalized version of s, returning s itself
// if it's already normalized.
/* static */ UnicodeString StandardFunctions::normalizeNFC(const UnicodeString& s) {
    UErrorCode status = U_ZERO_ERROR;
    const Normalizer2* nfcNormalizer = Normalizer2::getNFCInstance(status);
    if (U_FAILURE(status)) {
        return s;
    }
    // Check if string is already normalized
    UNormalizationCheckResult result = nfcNormalizer->quickCheck(s, status);
    // If so, return it
    if (U_SUCCESS(status) && result == UNORM_YES) {
        return s;
    }
    // Otherwise, normalize it
    UnicodeString normalized = nfcNormalizer->normalize(s, status);
    if (U_FAILURE(status)) {
        return {};
    }
    return normalized;
}

// Converts `s` to a double, indicating failure via `errorCode`
static void strToDouble(const UnicodeString& s, double& result, UErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    // Using en-US locale because it happens to correspond to the spec:
    // https://github.com/unicode-org/message-format-wg/blob/main/spec/registry.md#number-operands
    // Ideally, this should re-use the code for parsing number literals (Parser::parseUnquotedLiteral())
    // It's hard to reuse the same code because of how parse errors work.
    // TODO: Refactor
    LocalPointer<NumberFormat> numberFormat(NumberFormat::createInstance(Locale("en-US"), errorCode));
    CHECK_ERROR(errorCode);
    icu::Formattable asNumber;
    numberFormat->parse(s, asNumber, errorCode);
    CHECK_ERROR(errorCode);
    result = asNumber.getDouble(errorCode);
}

static double tryStringAsNumber(const Locale& locale, const Formattable& val, UErrorCode& errorCode) {
    // Check for a string option, try to parse it as a number if present
    UnicodeString tempString = val.getString(errorCode);
    LocalPointer<NumberFormat> numberFormat(NumberFormat::createInstance(locale, errorCode));
    if (U_SUCCESS(errorCode)) {
        icu::Formattable asNumber;
        numberFormat->parse(tempString, asNumber, errorCode);
        if (U_SUCCESS(errorCode)) {
            return asNumber.getDouble(errorCode);
        }
    }
    return 0;
}

static int64_t getInt64Value(const Locale& locale, const Formattable& value, UErrorCode& errorCode) {
    if (U_SUCCESS(errorCode)) {
        if (!value.isNumeric()) {
            double doubleResult = tryStringAsNumber(locale, value, errorCode);
            if (U_SUCCESS(errorCode)) {
                return static_cast<int64_t>(doubleResult);
            }
        }
        else {
            int64_t result = value.getInt64(errorCode);
            if (U_SUCCESS(errorCode)) {
                return result;
            }
        }
    }
    // Option was numeric but couldn't be converted to int64_t -- could be overflow
    return 0;
}

// Adopts its arguments
MFFunctionRegistry::MFFunctionRegistry(FormatterMap* f, SelectorMap* s, Hashtable* byType) : formatters(f), selectors(s), formattersByType(byType) {
    U_ASSERT(f != nullptr && s != nullptr && byType != nullptr);
}

MFFunctionRegistry& MFFunctionRegistry::operator=(MFFunctionRegistry&& other) noexcept {
    cleanup();

    formatters = other.formatters;
    selectors = other.selectors;
    formattersByType = other.formattersByType;
    other.formatters = nullptr;
    other.selectors = nullptr;
    other.formattersByType = nullptr;

    return *this;
}

void MFFunctionRegistry::cleanup() noexcept {
    if (formatters != nullptr) {
        delete formatters;
    }
    if (selectors != nullptr) {
        delete selectors;
    }
    if (formattersByType != nullptr) {
        delete formattersByType;
    }
}


MFFunctionRegistry::~MFFunctionRegistry() {
    cleanup();
}

// Specific formatter implementations

// --------- Number

bool inBounds(const UnicodeString& s, int32_t i) {
    return i < s.length();
}

bool isDigit(UChar32 c) {
    return c >= '0' && c <= '9';
}

bool parseDigits(const UnicodeString& s, int32_t& i) {
    if (!isDigit(s[i])) {
        return false;
    }
    while (inBounds(s, i) && isDigit(s[i])) {
        i++;
    }
    return true;
}

// number-literal = ["-"] (%x30 / (%x31-39 *DIGIT)) ["." 1*DIGIT] [%i"e" ["-" / "+"] 1*DIGIT]
bool validateNumberLiteral(const UnicodeString& s) {
    int32_t i = 0;

    if (s.isEmpty()) {
        return false;
    }

    // Parse optional sign
    // ["-"]
    if (s[0] == HYPHEN) {
        i++;
    }

    if (!inBounds(s, i)) {
        return false;
    }

    // Parse integer digits
    // (%x30 / (%x31-39 *DIGIT))
    if (s[i] == '0') {
        if (!inBounds(s, i + 1) || s[i + 1] != PERIOD) {
            return false;
        }
        i++;
    } else {
        if (!parseDigits(s, i)) {
            return false;
        }
    }
    // The rest is optional
    if (!inBounds(s, i)) {
        return true;
    }

    // Parse optional decimal digits
    // ["." 1*DIGIT]
    if (s[i] == PERIOD) {
        i++;
        if (!parseDigits(s, i)) {
            return false;
        }
    }

    if (!inBounds(s, i)) {
        return true;
    }

    // Parse optional exponent
    // [%i"e" ["-" / "+"] 1*DIGIT]
    if (s[i] == 'e' || s[i] == 'E') {
        i++;
        if (!inBounds(s, i)) {
            return false;
        }
        // Parse optional sign
        if (s[i] == HYPHEN || s[i] == PLUS) {
            i++;
        }
        if (!inBounds(s, i)) {
            return false;
        }
        if (!parseDigits(s, i)) {
            return false;
        }
    }
    if (i != s.length()) {
        return false;
    }
    return true;
}

bool isInteger(const Formattable& s) {
    switch (s.getType()) {
        case UFMT_DOUBLE:
        case UFMT_LONG:
        case UFMT_INT64:
            return true;
        case UFMT_STRING: {
            UErrorCode ignore = U_ZERO_ERROR;
            const UnicodeString& str = s.getString(ignore);
            return validateNumberLiteral(str);
        }
        default:
            return false;
    }
}

bool isDigitSizeOption(const UnicodeString& s) {
    return s == UnicodeString("minimumIntegerDigits")
        || s == UnicodeString("minimumFractionDigits")
        || s == UnicodeString("maximumFractionDigits")
        || s == UnicodeString("minimumSignificantDigits")
        || s == UnicodeString("maximumSignificantDigits");
}

/* static */ void StandardFunctions::validateDigitSizeOptions(const FunctionOptions& opts,
                                                              UErrorCode& status) {
    CHECK_ERROR(status);

    for (int32_t i = 0; i < opts.optionsCount(); i++) {
        const ResolvedFunctionOption& opt = opts.options[i];
        if (isDigitSizeOption(opt.getName()) && !isInteger(opt.getValue())) {
            status = U_MF_BAD_OPTION;
            return;
        }
    }
}

/* static */ number::LocalizedNumberFormatter StandardFunctions::formatterForOptions(const Number& number,
                                                                                     const FunctionOptions& opts,
                                                                                     UErrorCode& status) {
    number::UnlocalizedNumberFormatter nf;

    using namespace number;

    validateDigitSizeOptions(opts, status);
    if (U_FAILURE(status)) {
        return {};
    }

    if (U_SUCCESS(status)) {
        Formattable opt;
        nf = NumberFormatter::with();
        bool isInteger = number.isInteger;

        if (isInteger) {
            nf = nf.precision(Precision::integer());
        }

        // Notation options
        if (!isInteger) {
            // These options only apply to `:number`

            // Default notation is simple
            Notation notation = Notation::simple();
            UnicodeString notationOpt = opts.getStringFunctionOption(options::NOTATION);
            if (notationOpt == options::SCIENTIFIC) {
                notation = Notation::scientific();
            } else if (notationOpt == options::ENGINEERING) {
                notation = Notation::engineering();
            } else if (notationOpt == options::COMPACT) {
                UnicodeString displayOpt = opts.getStringFunctionOption(options::COMPACT_DISPLAY);
                if (displayOpt == options::LONG) {
                    notation = Notation::compactLong();
                } else {
                    // Default is short
                    notation = Notation::compactShort();
                }
            } else {
                // Already set to default
            }
            nf = nf.notation(notation);
        }

        // Style options -- specific to `:number`
        if (!isInteger) {
            if (number.usePercent(opts)) {
                nf = nf.unit(NoUnit::percent()).scale(Scale::powerOfTen(2));
            }
        }

        int32_t maxSignificantDigits = number.maximumSignificantDigits(opts);
        if (!isInteger) {
            int32_t minFractionDigits = number.minimumFractionDigits(opts);
            int32_t maxFractionDigits = number.maximumFractionDigits(opts);
            int32_t minSignificantDigits = number.minimumSignificantDigits(opts);
            Precision p = Precision::unlimited();
            bool precisionOptions = false;

            // Returning -1 means the option wasn't provided
            if (maxFractionDigits != -1 && minFractionDigits != -1) {
                precisionOptions = true;
                p = Precision::minMaxFraction(minFractionDigits, maxFractionDigits);
            } else if (minFractionDigits != -1) {
                precisionOptions = true;
                p = Precision::minFraction(minFractionDigits);
            } else if (maxFractionDigits != -1) {
                precisionOptions = true;
                p = Precision::maxFraction(maxFractionDigits);
            }

            if (minSignificantDigits != -1) {
                precisionOptions = true;
                p = p.minSignificantDigits(minSignificantDigits);
            }
            if (maxSignificantDigits != -1) {
                precisionOptions = true;
                p = p.maxSignificantDigits(maxSignificantDigits);
            }
            if (precisionOptions) {
                nf = nf.precision(p);
            }
        } else {
            // maxSignificantDigits applies to `:integer`, but the other precision options don't
            Precision p = Precision::integer();
            if (maxSignificantDigits != -1) {
                p = p.maxSignificantDigits(maxSignificantDigits);
            }
            nf = nf.precision(p);
        }

        // All other options apply to both `:number` and `:integer`
        int32_t minIntegerDigits = number.minimumIntegerDigits(opts);
        nf = nf.integerWidth(IntegerWidth::zeroFillTo(minIntegerDigits));

        // signDisplay
        UnicodeString sd = opts.getStringFunctionOption(options::SIGN_DISPLAY);
        UNumberSignDisplay signDisplay;
        if (sd == options::ALWAYS) {
            signDisplay = UNumberSignDisplay::UNUM_SIGN_ALWAYS;
        } else if (sd == options::EXCEPT_ZERO) {
            signDisplay = UNumberSignDisplay::UNUM_SIGN_EXCEPT_ZERO;
        } else if (sd == options::NEGATIVE) {
            signDisplay = UNumberSignDisplay::UNUM_SIGN_NEGATIVE;
        } else if (sd == options::NEVER) {
            signDisplay = UNumberSignDisplay::UNUM_SIGN_NEVER;
        } else {
            signDisplay = UNumberSignDisplay::UNUM_SIGN_AUTO;
        }
        nf = nf.sign(signDisplay);

        // useGrouping
        UnicodeString ug = opts.getStringFunctionOption(options::USE_GROUPING);
        UNumberGroupingStrategy grp;
        if (ug == options::ALWAYS) {
            grp = UNumberGroupingStrategy::UNUM_GROUPING_ON_ALIGNED;
        } else if (ug == options::NEVER) {
            grp = UNumberGroupingStrategy::UNUM_GROUPING_OFF;
        } else if (ug == options::MIN2) {
            grp = UNumberGroupingStrategy::UNUM_GROUPING_MIN2;
        } else {
            // Default is "auto"
            grp = UNumberGroupingStrategy::UNUM_GROUPING_AUTO;
        }
        nf = nf.grouping(grp);

        // numberingSystem
        UnicodeString ns = opts.getStringFunctionOption(options::NUMBERING_SYSTEM);
        if (ns.length() > 0) {
            ns = ns.toLower(Locale("en-US"));
            CharString buffer;
            // Ignore bad option values, so use a local status
            UErrorCode localStatus = U_ZERO_ERROR;
            // Copied from number_skeletons.cpp (helpers::parseNumberingSystemOption)
            buffer.appendInvariantChars({false, ns.getBuffer(), ns.length()}, localStatus);
            if (U_SUCCESS(localStatus)) {
                LocalPointer<NumberingSystem> symbols
                    (NumberingSystem::createInstanceByName(buffer.data(), localStatus));
                if (U_SUCCESS(localStatus)) {
                    nf = nf.adoptSymbols(symbols.orphan());
                }
            }
        }
    }
    return nf.locale(number.locale);
}

Formatter* StandardFunctions::NumberFactory::createFormatter(const Locale& locale, UErrorCode& errorCode) {
    NULL_ON_ERROR(errorCode);

    Formatter* result = new Number(locale);
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

Formatter* StandardFunctions::IntegerFactory::createFormatter(const Locale& locale, UErrorCode& errorCode) {
    NULL_ON_ERROR(errorCode);

    Formatter* result = new Number(Number::integer(locale));
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

StandardFunctions::IntegerFactory::~IntegerFactory() {}

static FormattedPlaceholder notANumber(const FormattedPlaceholder& input) {
    return FormattedPlaceholder(input, FormattedValue(UnicodeString("NaN")));
}

static double parseNumberLiteral(const Formattable& input, UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return {};
    }

    // Copying string to avoid GCC dangling-reference warning
    // (although the reference is safe)
    UnicodeString inputStr = input.getString(errorCode);
    // Precondition: `input`'s source Formattable has type string
    if (U_FAILURE(errorCode)) {
        return {};
    }

    // Validate string according to `number-literal` production
    // in the spec for `:number`. This is because some cases are
    // forbidden by this grammar, but allowed by StringToDouble.
    if (!validateNumberLiteral(inputStr)) {
        errorCode = U_MF_OPERAND_MISMATCH_ERROR;
        return 0;
    }

    // Convert to double using double_conversion::StringToDoubleConverter
    using namespace double_conversion;
    int processedCharactersCount = 0;
    StringToDoubleConverter converter(0, 0, 0, "", "");
    int32_t len = inputStr.length();
    double result =
        converter.StringToDouble(reinterpret_cast<const uint16_t*>(inputStr.getBuffer()),
                                 len,
                                 &processedCharactersCount);
    if (processedCharactersCount != len) {
        errorCode = U_MF_OPERAND_MISMATCH_ERROR;
    }
    return result;
}

static UChar32 digitToChar(int32_t val, UErrorCode errorCode) {
    if (U_FAILURE(errorCode)) {
        return '0';
    }
    if (val < 0 || val > 9) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
    }
    switch(val) {
        case 0:
            return '0';
        case 1:
            return '1';
        case 2:
            return '2';
        case 3:
            return '3';
        case 4:
            return '4';
        case 5:
            return '5';
        case 6:
            return '6';
        case 7:
            return '7';
        case 8:
            return '8';
        case 9:
            return '9';
        default:
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            return '0';
    }
}

int32_t StandardFunctions::Number::maximumFractionDigits(const FunctionOptions& opts) const {
    Formattable opt;

    if (isInteger) {
        return 0;
    }

    if (opts.getFunctionOption(options::MAXIMUM_FRACTION_DIGITS, opt)) {
        UErrorCode localErrorCode = U_ZERO_ERROR;
        int64_t val = getInt64Value(locale, opt, localErrorCode);
        if (U_SUCCESS(localErrorCode)) {
            return static_cast<int32_t>(val);
        }
    }
    // Returning -1 indicates that the option wasn't provided or was a non-integer.
    // The caller needs to check for that case, since passing -1 to Precision::maxFraction()
    // is an error.
    return -1;
}

int32_t StandardFunctions::Number::minimumFractionDigits(const FunctionOptions& opts) const {
    Formattable opt;

    if (!isInteger) {
        if (opts.getFunctionOption(options::MINIMUM_FRACTION_DIGITS, opt)) {
            UErrorCode localErrorCode = U_ZERO_ERROR;
            int64_t val = getInt64Value(locale, opt, localErrorCode);
            if (U_SUCCESS(localErrorCode)) {
                return static_cast<int32_t>(val);
            }
        }
    }
    // Returning -1 indicates that the option wasn't provided or was a non-integer.
    // The caller needs to check for that case, since passing -1 to Precision::minFraction()
    // is an error.
    return -1;
}

int32_t StandardFunctions::Number::minimumIntegerDigits(const FunctionOptions& opts) const {
    Formattable opt;

    if (opts.getFunctionOption(options::MINIMUM_INTEGER_DIGITS, opt)) {
        UErrorCode localErrorCode = U_ZERO_ERROR;
        int64_t val = getInt64Value(locale, opt, localErrorCode);
        if (U_SUCCESS(localErrorCode)) {
            return static_cast<int32_t>(val);
        }
    }
    return 1;
}

int32_t StandardFunctions::Number::minimumSignificantDigits(const FunctionOptions& opts) const {
    Formattable opt;

    if (!isInteger) {
        if (opts.getFunctionOption(options::MINIMUM_SIGNIFICANT_DIGITS, opt)) {
            UErrorCode localErrorCode = U_ZERO_ERROR;
            int64_t val = getInt64Value(locale, opt, localErrorCode);
            if (U_SUCCESS(localErrorCode)) {
                return static_cast<int32_t>(val);
            }
        }
    }
    // Returning -1 indicates that the option wasn't provided or was a non-integer.
    // The caller needs to check for that case, since passing -1 to Precision::minSignificantDigits()
    // is an error.
    return -1;
}

int32_t StandardFunctions::Number::maximumSignificantDigits(const FunctionOptions& opts) const {
    Formattable opt;

    if (opts.getFunctionOption(options::MAXIMUM_SIGNIFICANT_DIGITS, opt)) {
        UErrorCode localErrorCode = U_ZERO_ERROR;
        int64_t val = getInt64Value(locale, opt, localErrorCode);
        if (U_SUCCESS(localErrorCode)) {
            return static_cast<int32_t>(val);
        }
    }
    // Returning -1 indicates that the option wasn't provided or was a non-integer.
    // The caller needs to check for that case, since passing -1 to Precision::maxSignificantDigits()
    // is an error.
    return -1; // Not a valid value for Precision; has to be checked
}

bool StandardFunctions::Number::usePercent(const FunctionOptions& opts) const {
    Formattable opt;
    if (isInteger
        || !opts.getFunctionOption(options::STYLE, opt)
        || opt.getType() != UFMT_STRING) {
        return false;
    }
    UErrorCode localErrorCode = U_ZERO_ERROR;
    const UnicodeString& style = opt.getString(localErrorCode);
    U_ASSERT(U_SUCCESS(localErrorCode));
    return (style == options::PERCENT_STRING);
}

/* static */ StandardFunctions::Number StandardFunctions::Number::integer(const Locale& loc) {
    return StandardFunctions::Number(loc, true);
}

FormattedPlaceholder StandardFunctions::Number::format(FormattedPlaceholder&& arg, FunctionOptions&& opts, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return {};
    }

    // No argument => return "NaN"
    if (!arg.canFormat()) {
        errorCode = U_MF_OPERAND_MISMATCH_ERROR;
        return notANumber(arg);
    }

    number::LocalizedNumberFormatter realFormatter;
    realFormatter = formatterForOptions(*this, opts, errorCode);

    number::FormattedNumber numberResult;
    int64_t integerValue = 0;

    if (U_SUCCESS(errorCode)) {
        // Already checked that contents can be formatted
        const Formattable& toFormat = arg.asFormattable();
        switch (toFormat.getType()) {
        case UFMT_DOUBLE: {
            double d = toFormat.getDouble(errorCode);
            U_ASSERT(U_SUCCESS(errorCode));
            numberResult = realFormatter.formatDouble(d, errorCode);
            integerValue = static_cast<int64_t>(std::round(d));
            break;
        }
        case UFMT_LONG: {
            int32_t l = toFormat.getLong(errorCode);
            U_ASSERT(U_SUCCESS(errorCode));
            numberResult = realFormatter.formatInt(l, errorCode);
            integerValue = l;
            break;
        }
        case UFMT_INT64: {
            int64_t i = toFormat.getInt64(errorCode);
            U_ASSERT(U_SUCCESS(errorCode));
            numberResult = realFormatter.formatInt(i, errorCode);
            integerValue = i;
            break;
        }
        case UFMT_STRING: {
            // Try to parse the string as a number
            double d = parseNumberLiteral(toFormat, errorCode);
            if (U_FAILURE(errorCode))
                return {};
            numberResult = realFormatter.formatDouble(d, errorCode);
            integerValue = static_cast<int64_t>(std::round(d));
            break;
        }
        default: {
            // Other types can't be parsed as a number
            errorCode = U_MF_OPERAND_MISMATCH_ERROR;
            return notANumber(arg);
        }
        }
    }

    // Need to return the integer value if invoked as :integer
    if (isInteger) {
        return FormattedPlaceholder(FormattedPlaceholder(Formattable(integerValue), arg.getFallback()),
                                    std::move(opts),
                                    FormattedValue(std::move(numberResult)));
    }
    return FormattedPlaceholder(arg, std::move(opts), FormattedValue(std::move(numberResult)));
}

StandardFunctions::Number::~Number() {}
StandardFunctions::NumberFactory::~NumberFactory() {}

// --------- PluralFactory

StandardFunctions::Plural::PluralType StandardFunctions::Plural::pluralType(const FunctionOptions& opts) const {
    Formattable opt;

    if (opts.getFunctionOption(options::SELECT, opt)) {
        UErrorCode localErrorCode = U_ZERO_ERROR;
        UnicodeString val = opt.getString(localErrorCode);
        if (U_SUCCESS(localErrorCode)) {
            if (val == options::ORDINAL) {
                return PluralType::PLURAL_ORDINAL;
            }
            if (val == options::EXACT) {
                return PluralType::PLURAL_EXACT;
            }
        }
    }
    return PluralType::PLURAL_CARDINAL;
}

Selector* StandardFunctions::PluralFactory::createSelector(const Locale& locale, UErrorCode& errorCode) const {
    NULL_ON_ERROR(errorCode);

    Selector* result;
    if (isInteger) {
        result = new Plural(Plural::integer(locale, errorCode));
    } else {
        result = new Plural(locale, errorCode);
    }
    NULL_ON_ERROR(errorCode);
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

void StandardFunctions::Plural::selectKey(FormattedPlaceholder&& toFormat,
                                          FunctionOptions&& opts,
                                          const UnicodeString* keys,
                                          int32_t keysLen,
                                          UnicodeString* prefs,
                                          int32_t& prefsLen,
					  UErrorCode& errorCode) const {
    CHECK_ERROR(errorCode);

    // No argument => return "NaN"
    if (!toFormat.canFormat()) {
        errorCode = U_MF_SELECTOR_ERROR;
        return;
    }

    // Handle any formatting options
    PluralType type = pluralType(opts);
    FormattedPlaceholder resolvedSelector = numberFormatter->format(std::move(toFormat),
                                                                    std::move(opts),
                                                                    errorCode);
    CHECK_ERROR(errorCode);

    U_ASSERT(resolvedSelector.isEvaluated() && resolvedSelector.output().isNumber());

    // See  https://github.com/unicode-org/message-format-wg/blob/main/spec/registry.md#number-selection
    // 1. Let exact be the JSON string representation of the numeric value of resolvedSelector
    const number::FormattedNumber& formattedNumber = resolvedSelector.output().getNumber();
    UnicodeString exact = formattedNumber.toString(errorCode);

    if (U_FAILURE(errorCode)) {
        // Non-number => selector error
        errorCode = U_MF_SELECTOR_ERROR;
        return;
    }

    // Step 2. Let keyword be a string which is the result of rule selection on resolvedSelector.
    // If the option select is set to exact, rule-based selection is not used. Return the empty string.
    UnicodeString keyword;
    if (type != PluralType::PLURAL_EXACT) {
        UPluralType t = type == PluralType::PLURAL_ORDINAL ? UPLURAL_TYPE_ORDINAL : UPLURAL_TYPE_CARDINAL;
        // Look up plural rules by locale and type
        LocalPointer<PluralRules> rules(PluralRules::forLocale(locale, t, errorCode));
        CHECK_ERROR(errorCode);

        keyword = rules->select(formattedNumber, errorCode);
    }

    // Steps 3-4 elided:
    // 3. Let resultExact be a new empty list of strings.
    // 4. Let resultKeyword be a new empty list of strings.
    // Instead, we use `prefs` the concatenation of `resultExact`
    // and `resultKeyword`.

    prefsLen = 0;

    // 5. For each string key in keys:
    double keyAsDouble = 0;
    for (int32_t i = 0; i < keysLen; i++) {
        // Try parsing the key as a double
        UErrorCode localErrorCode = U_ZERO_ERROR;
        strToDouble(keys[i], keyAsDouble, localErrorCode);
        // 5i. If the value of key matches the production number-literal, then
        if (U_SUCCESS(localErrorCode)) {
            // 5i(a). If key and exact consist of the same sequence of Unicode code points, then
            if (exact == keys[i]) {
                // 5i(a)(a) Append key as the last element of the list resultExact.
		prefs[prefsLen] = keys[i];
                prefsLen++;
                break;
            }
        }
    }

    // Return immediately if exact matching was requested
    if (prefsLen == keysLen || type == PluralType::PLURAL_EXACT) {
        return;
    }


    for (int32_t i = 0; i < keysLen; i ++) {
        if (prefsLen >= keysLen) {
            break;
        }
        // 5ii. Else if key is one of the keywords zero, one, two, few, many, or other, then
        // 5ii(a). If key and keyword consist of the same sequence of Unicode code points, then
        if (keyword == keys[i]) {
            // 5ii(a)(a) Append key as the last element of the list resultKeyword.
            prefs[prefsLen] = keys[i];
            prefsLen++;
        }
    }

    // Note: Step 5(iii) "Else, emit a Selection Error" is omitted in both loops

    // 6. Return a new list whose elements are the concatenation of the elements
    // (in order) of resultExact followed by the elements (in order) of resultKeyword.
    // (Implicit, since `prefs` is an out-parameter)
}

StandardFunctions::Plural::Plural(const Locale& loc, UErrorCode& status) : locale(loc) {
    CHECK_ERROR(status);

    numberFormatter.adoptInstead(new StandardFunctions::Number(loc));
    if (!numberFormatter.isValid()) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
}

StandardFunctions::Plural::Plural(const Locale& loc, bool isInt, UErrorCode& status) : locale(loc), isInteger(isInt) {
    CHECK_ERROR(status);

    if (isInteger) {
        numberFormatter.adoptInstead(new StandardFunctions::Number(loc, true));
    } else {
        numberFormatter.adoptInstead(new StandardFunctions::Number(loc));
    }

    if (!numberFormatter.isValid()) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
}

StandardFunctions::Plural::~Plural() {}

StandardFunctions::PluralFactory::~PluralFactory() {}

// --------- DateTimeFactory

/* static */ UnicodeString StandardFunctions::getStringOption(const FunctionOptions& opts,
                                                              std::u16string_view optionName,
                                                              UErrorCode& errorCode) {
    if (U_SUCCESS(errorCode)) {
        Formattable opt;
        if (opts.getFunctionOption(optionName, opt)) {
            return opt.getString(errorCode); // In case it's not a string, error code will be set
        } else {
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        }
    }
    // Default is empty string
    return {};
}

// Date/time options only
static UnicodeString defaultForOption(std::u16string_view optionName) {
    if (optionName == options::DATE_STYLE
        || optionName == options::TIME_STYLE
        || optionName == options::STYLE) {
        return UnicodeString(options::SHORT);
    }
    return {}; // Empty string is default
}

// TODO
// Only DateTime currently uses the function options stored in the placeholder.
// It also doesn't use them very consistently (it looks at the previous set of options,
// and others aren't preserved). This needs to be generalized,
// but that depends on https://github.com/unicode-org/message-format-wg/issues/515
// Finally, the option value is assumed to be a string,
// which works for datetime options but not necessarily in general.
UnicodeString StandardFunctions::DateTime::getFunctionOption(const FormattedPlaceholder& toFormat,
                                                             const FunctionOptions& opts,
                                                             std::u16string_view optionName) const {
    // Options passed to the current function invocation take priority
    Formattable opt;
    UnicodeString s;
    UErrorCode localErrorCode = U_ZERO_ERROR;
    s = getStringOption(opts, optionName, localErrorCode);
    if (U_SUCCESS(localErrorCode)) {
        return s;
    }
    // Next try the set of options used to construct `toFormat`
    localErrorCode = U_ZERO_ERROR;
    s = getStringOption(toFormat.options(), optionName, localErrorCode);
    if (U_SUCCESS(localErrorCode)) {
        return s;
    }
    // Finally, use default
    return defaultForOption(optionName);
}

// Used for options that don't have defaults
UnicodeString StandardFunctions::DateTime::getFunctionOption(const FormattedPlaceholder& toFormat,
                                                             const FunctionOptions& opts,
                                                             std::u16string_view optionName,
                                                             UErrorCode& errorCode) const {
    if (U_SUCCESS(errorCode)) {
        // Options passed to the current function invocation take priority
        Formattable opt;
        UnicodeString s;
        UErrorCode localErrorCode = U_ZERO_ERROR;
        s = getStringOption(opts, optionName, localErrorCode);
        if (U_SUCCESS(localErrorCode)) {
            return s;
        }
        // Next try the set of options used to construct `toFormat`
        localErrorCode = U_ZERO_ERROR;
        s = getStringOption(toFormat.options(), optionName, localErrorCode);
        if (U_SUCCESS(localErrorCode)) {
            return s;
        }
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
    }
    return {};
}

static DateFormat::EStyle stringToStyle(UnicodeString option, UErrorCode& errorCode) {
    if (U_SUCCESS(errorCode)) {
        UnicodeString upper = option.toUpper();
        if (upper == options::FULL_UPPER) {
            return DateFormat::EStyle::kFull;
        }
        if (upper == options::LONG_UPPER) {
            return DateFormat::EStyle::kLong;
        }
        if (upper == options::MEDIUM_UPPER) {
            return DateFormat::EStyle::kMedium;
        }
        if (upper == options::SHORT_UPPER) {
            return DateFormat::EStyle::kShort;
        }
        if (upper.isEmpty() || upper == options::DEFAULT_UPPER) {
            return DateFormat::EStyle::kDefault;
        }
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
    }
    return DateFormat::EStyle::kNone;
}

/* static */ StandardFunctions::DateTimeFactory* StandardFunctions::DateTimeFactory::dateTime(UErrorCode& errorCode) {
    NULL_ON_ERROR(errorCode);

    DateTimeFactory* result = new StandardFunctions::DateTimeFactory(DateTimeType::DateTime);
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

/* static */ StandardFunctions::DateTimeFactory* StandardFunctions::DateTimeFactory::date(UErrorCode& errorCode) {
    NULL_ON_ERROR(errorCode);

    DateTimeFactory* result = new DateTimeFactory(DateTimeType::Date);
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

/* static */ StandardFunctions::DateTimeFactory* StandardFunctions::DateTimeFactory::time(UErrorCode& errorCode) {
    NULL_ON_ERROR(errorCode);

    DateTimeFactory* result = new DateTimeFactory(DateTimeType::Time);
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

Formatter* StandardFunctions::DateTimeFactory::createFormatter(const Locale& locale, UErrorCode& errorCode) {
    NULL_ON_ERROR(errorCode);

    Formatter* result = new StandardFunctions::DateTime(locale, type);
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

// DateFormat parsers that are shared across threads
static DateFormat* dateParser = nullptr;
static DateFormat* dateTimeParser = nullptr;
static DateFormat* dateTimeUTCParser = nullptr;
static DateFormat* dateTimeZoneParser = nullptr;
static icu::UInitOnce gMF2DateParsersInitOnce {};

// Clean up shared DateFormat objects
static UBool mf2_date_parsers_cleanup() {
    if (dateParser != nullptr) {
        delete dateParser;
        dateParser = nullptr;
    }
    if (dateTimeParser != nullptr) {
        delete dateTimeParser;
        dateTimeParser = nullptr;
    }
    if (dateTimeUTCParser != nullptr) {
        delete dateTimeUTCParser;
        dateTimeUTCParser = nullptr;
    }
    if (dateTimeZoneParser != nullptr) {
        delete dateTimeZoneParser;
        dateTimeZoneParser = nullptr;
    }
    return true;
}

// Initialize DateFormat objects used for parsing date literals
static void initDateParsersOnce(UErrorCode& errorCode) {
    U_ASSERT(dateParser == nullptr);
    U_ASSERT(dateTimeParser == nullptr);
    U_ASSERT(dateTimeUTCParser == nullptr);
    U_ASSERT(dateTimeZoneParser == nullptr);

    // Handles ISO 8601 date
    dateParser = new SimpleDateFormat(UnicodeString("YYYY-MM-dd"), errorCode);
    // Handles ISO 8601 datetime without time zone
    dateTimeParser = new SimpleDateFormat(UnicodeString("YYYY-MM-dd'T'HH:mm:ss"), errorCode);
    // Handles ISO 8601 datetime with 'Z' to denote UTC
    dateTimeUTCParser = new SimpleDateFormat(UnicodeString("YYYY-MM-dd'T'HH:mm:ssZ"), errorCode);
    // Handles ISO 8601 datetime with timezone offset; 'zzzz' denotes timezone offset
    dateTimeZoneParser = new SimpleDateFormat(UnicodeString("YYYY-MM-dd'T'HH:mm:sszzzz"), errorCode);

    if (!dateParser || !dateTimeParser || !dateTimeUTCParser || !dateTimeZoneParser) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        mf2_date_parsers_cleanup();
        return;
    }
    ucln_i18n_registerCleanup(UCLN_I18N_MF2_DATE_PARSERS, mf2_date_parsers_cleanup);
}

// Lazily initialize DateFormat objects used for parsing date literals
static void initDateParsers(UErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    umtx_initOnce(gMF2DateParsersInitOnce, &initDateParsersOnce, errorCode);
}

// From https://github.com/unicode-org/message-format-wg/blob/main/spec/registry.md#date-and-time-operands :
// "A date/time literal value is a non-empty string consisting of an ISO 8601 date, or
// an ISO 8601 datetime optionally followed by a timezone offset."
UDate StandardFunctions::DateTime::tryPatterns(const UnicodeString& sourceStr,
                                               UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return 0;
    }
    // Handle ISO 8601 datetime (tryTimeZonePatterns() handles the case
    // where a timezone offset follows)
    if (sourceStr.length() > 10) {
        return dateTimeParser->parse(sourceStr, errorCode);
    }
    // Handle ISO 8601 date
    return dateParser->parse(sourceStr, errorCode);
}

// See comment on tryPatterns() for spec reference
UDate StandardFunctions::DateTime::tryTimeZonePatterns(const UnicodeString& sourceStr,
                                                       UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return 0;
    }
    int32_t len = sourceStr.length();
    if (len > 0 && sourceStr[len] == 'Z') {
        return dateTimeUTCParser->parse(sourceStr, errorCode);
    }
    return dateTimeZoneParser->parse(sourceStr, errorCode);
}

static TimeZone* createTimeZone(const DateInfo& dateInfo, UErrorCode& errorCode) {
    NULL_ON_ERROR(errorCode);

    TimeZone* tz;
    if (dateInfo.zoneId.isEmpty()) {
        // Floating time value -- use default time zone
        tz = TimeZone::createDefault();
    } else {
        tz = TimeZone::createTimeZone(dateInfo.zoneId);
    }
    if (tz == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return tz;
}

// Returns true iff `sourceStr` ends in an offset like +03:30 or -06:00
// (This function is just used to determine whether to call tryPatterns()
// or tryTimeZonePatterns(); tryTimeZonePatterns() checks fully that the
// string matches the expected format)
static bool hasTzOffset(const UnicodeString& sourceStr) {
    int32_t len = sourceStr.length();

    if (len <= 6) {
        return false;
    }
    return ((sourceStr[len - 6] == PLUS || sourceStr[len - 6] == HYPHEN)
            && sourceStr[len - 3] == COLON);
}

// Note: `calendar` option to :datetime not implemented yet;
// Gregorian calendar is assumed
DateInfo StandardFunctions::DateTime::createDateInfoFromString(const UnicodeString& sourceStr,
                                                               UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return {};
    }

    UDate absoluteDate;

    // Check if the string has a time zone part
    int32_t indexOfZ = sourceStr.indexOf('Z');
    int32_t indexOfPlus = sourceStr.lastIndexOf('+');
    int32_t indexOfMinus = sourceStr.lastIndexOf('-');
    int32_t indexOfSign = indexOfPlus > -1 ? indexOfPlus : indexOfMinus;
    bool isTzOffset = hasTzOffset(sourceStr);
    bool isGMT = indexOfZ > 0;
    UnicodeString offsetPart;
    bool hasTimeZone = isTzOffset || isGMT;

    if (!hasTimeZone) {
        // No time zone; parse the date and time
        absoluteDate = tryPatterns(sourceStr, errorCode);
        if (U_FAILURE(errorCode)) {
            return {};
        }
    } else {
        // Try to split into time zone and non-time-zone parts
        UnicodeString dateTimePart;
        if (isGMT) {
            dateTimePart = sourceStr.tempSubString(0, indexOfZ);
        } else {
            dateTimePart = sourceStr.tempSubString(0, indexOfSign);
        }
        // Parse the date from the date/time part
        tryPatterns(dateTimePart, errorCode);
        // Failure -- can't parse this string
        if (U_FAILURE(errorCode)) {
            return {};
        }
        // Success -- now parse the time zone part
        if (isGMT) {
            dateTimePart += UnicodeString("GMT");
            absoluteDate = tryTimeZonePatterns(dateTimePart, errorCode);
            if (U_FAILURE(errorCode)) {
                return {};
            }
        } else {
            // Try to parse time zone in offset format: [+-]nn:nn
            absoluteDate = tryTimeZonePatterns(sourceStr, errorCode);
            if (U_FAILURE(errorCode)) {
                return {};
            }
            offsetPart = sourceStr.tempSubString(indexOfSign, sourceStr.length());
        }
    }

    // If the time zone was provided, get its canonical ID,
    // in order to return it in the DateInfo
    UnicodeString canonicalID;
    if (hasTimeZone) {
        UnicodeString tzID("GMT");
        if (!isGMT) {
            tzID += offsetPart;
        }
        TimeZone::getCanonicalID(tzID, canonicalID, errorCode);
        if (U_FAILURE(errorCode)) {
            return {};
        }
    }

    return { absoluteDate, canonicalID };
}

void formatDateWithDefaults(const Locale& locale,
                            const DateInfo& dateInfo,
                            UnicodeString& result,
                            UErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    LocalPointer<DateFormat> df(defaultDateTimeInstance(locale, errorCode));
    CHECK_ERROR(errorCode);

    df->adoptTimeZone(createTimeZone(dateInfo, errorCode));
    CHECK_ERROR(errorCode);
    df->format(dateInfo.date, result, nullptr, errorCode);
}

FormattedPlaceholder StandardFunctions::DateTime::format(FormattedPlaceholder&& toFormat,
                                                   FunctionOptions&& opts,
                                                   UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return {};
    }

    // Argument must be present
    if (!toFormat.canFormat()) {
        errorCode = U_MF_OPERAND_MISMATCH_ERROR;
        return std::move(toFormat);
    }

    LocalPointer<DateFormat> df;
    Formattable opt;

    DateFormat::EStyle dateStyle = DateFormat::kShort;
    DateFormat::EStyle timeStyle = DateFormat::kShort;

    UnicodeString dateStyleName("dateStyle");
    UnicodeString timeStyleName("timeStyle");
    UnicodeString styleName("style");

    bool hasDateStyleOption = opts.getFunctionOption(dateStyleName, opt);
    bool hasTimeStyleOption = opts.getFunctionOption(timeStyleName, opt);
    bool noOptions = opts.optionsCount() == 0;

    bool useStyle = (type == DateTimeFactory::DateTimeType::DateTime
                     && (hasDateStyleOption || hasTimeStyleOption
                         || noOptions))
        || (type != DateTimeFactory::DateTimeType::DateTime);

    bool useDate = type == DateTimeFactory::DateTimeType::Date
        || (type == DateTimeFactory::DateTimeType::DateTime
            && hasDateStyleOption);
    bool useTime = type == DateTimeFactory::DateTimeType::Time
        || (type == DateTimeFactory::DateTimeType::DateTime
            && hasTimeStyleOption);

    if (useStyle) {
        // Extract style options
        if (type == DateTimeFactory::DateTimeType::DateTime) {
            // Note that the options-getting has to be repeated across the three cases,
            // since `:datetime` uses "dateStyle"/"timeStyle" and `:date` and `:time`
            // use "style"
            dateStyle = stringToStyle(getFunctionOption(toFormat, opts, dateStyleName), errorCode);
            timeStyle = stringToStyle(getFunctionOption(toFormat, opts, timeStyleName), errorCode);

            if (useDate && !useTime) {
                df.adoptInstead(DateFormat::createDateInstance(dateStyle, locale));
            } else if (useTime && !useDate) {
                df.adoptInstead(DateFormat::createTimeInstance(timeStyle, locale));
            } else {
                df.adoptInstead(DateFormat::createDateTimeInstance(dateStyle, timeStyle, locale));
            }
        } else if (type == DateTimeFactory::DateTimeType::Date) {
            dateStyle = stringToStyle(getFunctionOption(toFormat, opts, styleName), errorCode);
            df.adoptInstead(DateFormat::createDateInstance(dateStyle, locale));
        } else {
            // :time
            timeStyle = stringToStyle(getFunctionOption(toFormat, opts, styleName), errorCode);
            df.adoptInstead(DateFormat::createTimeInstance(timeStyle, locale));
        }
    } else {
        // Build up a skeleton based on the field options, then use that to
        // create the date formatter

        UnicodeString skeleton;
        #define ADD_PATTERN(s) skeleton += UnicodeString(s)
        if (U_SUCCESS(errorCode)) {
            // Year
            UnicodeString year = getFunctionOption(toFormat, opts, options::YEAR, errorCode);
            if (U_FAILURE(errorCode)) {
                errorCode = U_ZERO_ERROR;
            } else {
                useDate = true;
                if (year == options::TWO_DIGIT) {
                    ADD_PATTERN("YY");
                } else if (year == options::NUMERIC) {
                    ADD_PATTERN("YYYY");
                }
            }
            // Month
            UnicodeString month = getFunctionOption(toFormat, opts, options::MONTH, errorCode);
            if (U_FAILURE(errorCode)) {
                errorCode = U_ZERO_ERROR;
            } else {
                useDate = true;
                /* numeric, 2-digit, long, short, narrow */
                if (month == options::LONG) {
                    ADD_PATTERN("MMMM");
                } else if (month == options::SHORT) {
                    ADD_PATTERN("MMM");
                } else if (month == options::NARROW) {
                    ADD_PATTERN("MMMMM");
                } else if (month == options::NUMERIC) {
                    ADD_PATTERN("M");
                } else if (month == options::TWO_DIGIT) {
                    ADD_PATTERN("MM");
                }
            }
            // Weekday
            UnicodeString weekday = getFunctionOption(toFormat, opts, options::WEEKDAY, errorCode);
            if (U_FAILURE(errorCode)) {
                errorCode = U_ZERO_ERROR;
            } else {
                useDate = true;
                if (weekday == options::LONG) {
                    ADD_PATTERN("EEEE");
                } else if (weekday == options::SHORT) {
                    ADD_PATTERN("EEEEE");
                } else if (weekday == options::NARROW) {
                    ADD_PATTERN("EEEEE");
                }
            }
            // Day
            UnicodeString day = getFunctionOption(toFormat, opts, options::DAY, errorCode);
            if (U_FAILURE(errorCode)) {
                errorCode = U_ZERO_ERROR;
            } else {
                useDate = true;
                if (day == options::NUMERIC) {
                    ADD_PATTERN("d");
                } else if (day == options::TWO_DIGIT) {
                    ADD_PATTERN("dd");
                }
            }
            // Hour
            UnicodeString hour = getFunctionOption(toFormat, opts, options::HOUR, errorCode);
            if (U_FAILURE(errorCode)) {
                errorCode = U_ZERO_ERROR;
            } else {
                useTime = true;
                if (hour == options::NUMERIC) {
                    ADD_PATTERN("h");
                } else if (hour == options::TWO_DIGIT) {
                    ADD_PATTERN("hh");
                }
            }
            // Minute
            UnicodeString minute = getFunctionOption(toFormat, opts, options::MINUTE, errorCode);
            if (U_FAILURE(errorCode)) {
                errorCode = U_ZERO_ERROR;
            } else {
                useTime = true;
                if (minute == options::NUMERIC) {
                    ADD_PATTERN("m");
                } else if (minute == options::TWO_DIGIT) {
                    ADD_PATTERN("mm");
                }
            }
            // Second
            UnicodeString second = getFunctionOption(toFormat, opts, options::SECOND, errorCode);
            if (U_FAILURE(errorCode)) {
                errorCode = U_ZERO_ERROR;
            } else {
                useTime = true;
                if (second == options::NUMERIC) {
                    ADD_PATTERN("s");
                } else if (second == options::TWO_DIGIT) {
                    ADD_PATTERN("ss");
                }
            }
        }
        /*
          TODO
          fractionalSecondDigits
          hourCycle
          timeZoneName
          era
         */
        df.adoptInstead(DateFormat::createInstanceForSkeleton(skeleton, errorCode));
    }

    if (U_FAILURE(errorCode)) {
        return {};
    }
    if (!df.isValid()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return {};
    }

    UnicodeString result;
    const Formattable& source = toFormat.asFormattable();
    switch (source.getType()) {
    case UFMT_STRING: {
        // Lazily initialize date parsers used for parsing date literals
        initDateParsers(errorCode);
        if (U_FAILURE(errorCode)) {
            return {};
        }

        const UnicodeString& sourceStr = source.getString(errorCode);
        U_ASSERT(U_SUCCESS(errorCode));

        DateInfo dateInfo = createDateInfoFromString(sourceStr, errorCode);
        if (U_FAILURE(errorCode)) {
            errorCode = U_MF_OPERAND_MISMATCH_ERROR;
            return {};
        }
        df->adoptTimeZone(createTimeZone(dateInfo, errorCode));

        // Use the parsed date as the source value
        // in the returned FormattedPlaceholder; this is necessary
        // so the date can be re-formatted
        df->format(dateInfo.date, result, 0, errorCode);
        toFormat = FormattedPlaceholder(message2::Formattable(std::move(dateInfo)),
                                        toFormat.getFallback());
        break;
    }
    case UFMT_DATE: {
        const DateInfo* dateInfo = source.getDate(errorCode);
        if (U_SUCCESS(errorCode)) {
            // If U_SUCCESS(errorCode), then source.getDate() returned
            // a non-null pointer
            df->adoptTimeZone(createTimeZone(*dateInfo, errorCode));
            df->format(dateInfo->date, result, 0, errorCode);
            if (U_FAILURE(errorCode)) {
                if (errorCode == U_ILLEGAL_ARGUMENT_ERROR) {
                    errorCode = U_MF_OPERAND_MISMATCH_ERROR;
                }
            }
        }
        break;
    }
    // Any other cases are an error
    default: {
        errorCode = U_MF_OPERAND_MISMATCH_ERROR;
        break;
    }
    }
    if (U_FAILURE(errorCode)) {
        return {};
    }
    return FormattedPlaceholder(toFormat, std::move(opts), FormattedValue(std::move(result)));
}

StandardFunctions::DateTimeFactory::~DateTimeFactory() {}
StandardFunctions::DateTime::~DateTime() {}

// --------- TextFactory

Selector* StandardFunctions::TextFactory::createSelector(const Locale& locale, UErrorCode& errorCode) const {
    Selector* result = new TextSelector(locale);
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    return result;
}

void StandardFunctions::TextSelector::selectKey(FormattedPlaceholder&& toFormat,
                                                FunctionOptions&& opts,
                                                const UnicodeString* keys,
                                                int32_t keysLen,
                                                UnicodeString* prefs,
                                                int32_t& prefsLen,
						UErrorCode& errorCode) const {
    // No options
    (void) opts;

    CHECK_ERROR(errorCode);

    // Just compares the key and value as strings

    // Argument must be present
    if (!toFormat.canFormat()) {
        errorCode = U_MF_SELECTOR_ERROR;
        return;
    }

    prefsLen = 0;

    // Convert to string
    const UnicodeString& formattedValue = toFormat.formatToString(locale, errorCode);
    if (U_FAILURE(errorCode)) {
        return;
    }
    // Normalize result
    UnicodeString normalized = normalizeNFC(formattedValue);

    for (int32_t i = 0; i < keysLen; i++) {
        if (keys[i] == normalized) {
	    prefs[0] = keys[i];
            prefsLen = 1;
            break;
        }
    }
}

StandardFunctions::TextFactory::~TextFactory() {}
StandardFunctions::TextSelector::~TextSelector() {}

// ------------ TestFormatFactory

Formatter* StandardFunctions::TestFormatFactory::createFormatter(const Locale& locale, UErrorCode& errorCode) {
    NULL_ON_ERROR(errorCode);

    // Results are not locale-dependent
    (void) locale;

    Formatter* result = new TestFormat();
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

StandardFunctions::TestFormatFactory::~TestFormatFactory() {}
StandardFunctions::TestFormat::~TestFormat() {}

// Extract numeric value from a Formattable or, if it's a string,
// parse it as a number according to the MF2 `number-literal` grammar production
double formattableToNumber(const Formattable& arg, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return 0;
    }

    double result = 0;

    switch (arg.getType()) {
        case UFMT_DOUBLE: {
            result = arg.getDouble(status);
            U_ASSERT(U_SUCCESS(status));
            break;
        }
        case UFMT_LONG: {
            result = (double) arg.getLong(status);
            U_ASSERT(U_SUCCESS(status));
            break;
        }
        case UFMT_INT64: {
            result = (double) arg.getInt64(status);
            U_ASSERT(U_SUCCESS(status));
            break;
        }
        case UFMT_STRING: {
            // Try to parse the string as a number
            result = parseNumberLiteral(arg, status);
            if (U_FAILURE(status)) {
                status = U_MF_OPERAND_MISMATCH_ERROR;
            }
            break;
        }
        default: {
            // Other types can't be parsed as a number
            status = U_MF_OPERAND_MISMATCH_ERROR;
            break;
        }
        }
    return result;
}


/* static */ void StandardFunctions::TestFormat::testFunctionParameters(const FormattedPlaceholder& arg,
                                                                        const FunctionOptions& options,
                                                                        int32_t& decimalPlaces,
                                                                        bool& failsFormat,
                                                                        bool& failsSelect,
                                                                        double& input,
                                                                        UErrorCode& status) {
    CHECK_ERROR(status);

    // 1. Let DecimalPlaces be 0.
    decimalPlaces = 0;

    // 2. Let FailsFormat be false.
    failsFormat = false;

    // 3. Let FailsSelect be false.
    failsSelect = false;

    // 4. Let arg be the resolved value of the expression operand.
    // (already true)

    // Step 5 omitted because composition isn't fully implemented yet
    // 6. Else if arg is a numerical value or a string matching the number-literal production, then
    input = formattableToNumber(arg.asFormattable(), status);
    if (U_FAILURE(status)) {
        // 7. Else,
        // 7i. Emit "bad-input" Resolution Error.
        status = U_MF_OPERAND_MISMATCH_ERROR;
        // 7ii. Use a fallback value as the resolved value of the expression.
        // Further steps of this algorithm are not followed.
    }
    // 8. If the decimalPlaces option is set, then
    Formattable opt;
    if (options.getFunctionOption(options::DECIMAL_PLACES, opt)) {
        // 8i. If its value resolves to a numerical integer value 0 or 1
        // or their corresponding string representations '0' or '1', then
        double decimalPlacesInput = formattableToNumber(opt, status);
        if (U_SUCCESS(status)) {
            if (decimalPlacesInput == 0 || decimalPlacesInput == 1) {
                // 8ia. Set DecimalPlaces to be the numerical value of the option.
                decimalPlaces = decimalPlacesInput;
            }
        }
        // 8ii. Else if its value is not an unresolved value set by option resolution,
        else {
            // 8iia. Emit "bad-option" Resolution Error.
            status = U_MF_BAD_OPTION;
            // 8iib. Use a fallback value as the resolved value of the expression.
        }
    }
    // 9. If the fails option is set, then
    Formattable failsOpt;
    if (options.getFunctionOption(options::FAILS, failsOpt)) {
        UnicodeString failsString = failsOpt.getString(status);
        if (U_SUCCESS(status)) {
            // 9i. If its value resolves to the string 'always', then
            if (failsString == u"always") {
                // 9ia. Set FailsFormat to be true
                failsFormat = true;
                // 9ib. Set FailsSelect to be true.
                failsSelect = true;
            }
            // 9ii. Else if its value resolves to the string "format", then
            else if (failsString == u"format") {
                // 9ia. Set FailsFormat to be true
                failsFormat = true;
            }
            // 9iii. Else if its value resolves to the string "select", then
            else if (failsString == u"select") {
                // 9iiia. Set FailsSelect to be true.
                failsSelect = true;
            }
            // 9iv. Else if its value does not resolve to the string "never", then
            else if (failsString != u"never") {
                // 9iv(a). Emit "bad-option" Resolution Error.
                status = U_MF_BAD_OPTION;
            }
        } else {
            // 9iv. again
            status = U_MF_BAD_OPTION;
        }
    }
}

FormattedPlaceholder StandardFunctions::TestFormat::format(FormattedPlaceholder&& arg,
                                                           FunctionOptions&& options,
                                                           UErrorCode& status) const{

    int32_t decimalPlaces;
    bool failsFormat;
    bool failsSelect;
    double input;

    testFunctionParameters(arg, options, decimalPlaces,
                           failsFormat, failsSelect, input, status);
    if (U_FAILURE(status)) {
        return FormattedPlaceholder(arg.getFallback());
    }

    // If FailsFormat is true, attempting to format the placeholder to any
    // formatting target will fail.
    if (failsFormat) {
        status = U_MF_FORMATTING_ERROR;
        return FormattedPlaceholder(arg.getFallback());
    }
    UnicodeString result;
    // When :test:function is used as a formatter, a placeholder resolving to a value
    // with a :test:function expression is formatted as a concatenation of the following parts:
    // 1. If Input is less than 0, the character - U+002D Hyphen-Minus.
    if (input < 0) {
        result += HYPHEN;
    }
    // 2. The truncated absolute integer value of Input, i.e. floor(abs(Input)), formatted as a
    // sequence of decimal digit characters (U+0030...U+0039).
    char buffer[256];
    bool ignore;
    int ignoreLen;
    int ignorePoint;
    double_conversion::DoubleToStringConverter::DoubleToAscii(floor(abs(input)),
                                                              double_conversion::DoubleToStringConverter::DtoaMode::SHORTEST,
                                                              0,
                                                              buffer,
                                                              256,
                                                              &ignore,
                                                              &ignoreLen,
                                                              &ignorePoint);
    result += UnicodeString(buffer);
    // 3. If DecimalPlaces is 1, then
    if (decimalPlaces == 1) {
        // 3i. The character . U+002E Full Stop.
        result += u".";
        // 3ii. The single decimal digit character representing the value
        // floor((abs(Input) - floor(abs(Input))) * 10)
        int32_t val = floor((abs(input) - floor(abs(input)) * 10));
        result += digitToChar(val, status);
        U_ASSERT(U_SUCCESS(status));
    }
    return FormattedPlaceholder(result);
}

// ------------ TestSelectFactory

StandardFunctions::TestSelectFactory::~TestSelectFactory() {}
StandardFunctions::TestSelect::~TestSelect() {}

Selector* StandardFunctions::TestSelectFactory::createSelector(const Locale& locale,
                                                               UErrorCode& errorCode) const {
    NULL_ON_ERROR(errorCode);

    // Results are not locale-dependent
    (void) locale;

    Selector* result = new TestSelect();
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

void StandardFunctions::TestSelect::selectKey(FormattedPlaceholder&& val,
                                              FunctionOptions&& options,
                                              const UnicodeString* keys,
                                              int32_t keysLen,
                                              UnicodeString* prefs,
                                              int32_t& prefsLen,
                                              UErrorCode& status) const {
    int32_t decimalPlaces;
    bool failsFormat;
    bool failsSelect;
    double input;

    TestFormat::testFunctionParameters(val, options, decimalPlaces,
                                       failsFormat, failsSelect, input, status);

    if (U_FAILURE(status)) {
        return;
    }

    if (failsSelect) {
        status = U_MF_SELECTOR_ERROR;
        return;
    }

    // If the Input is 1 and DecimalPlaces is 1, the method will return some slice
    // of the list « '1.0', '1' », depending on whether those values are included in keys.
    bool include1point0 = false;
    bool include1 = false;
    if (input == 1 && decimalPlaces == 1) {
        include1point0 = true;
        include1 = true;
    } else if (input == 1 && decimalPlaces == 0) {
        include1 = true;
    }

    // If the Input is 1 and DecimalPlaces is 0, the method will return the list « '1' » if
    // keys includes '1', or an empty list otherwise.
    // If the Input is any other value, the method will return an empty list.
    for (int32_t i = 0; i < keysLen; i++) {
        if ((keys[i] == u"1" && include1)
            || (keys[i] == u"1.0" && include1point0)) {
            prefs[prefsLen] = keys[i];
            prefsLen++;
        }
    }
}

} // namespace message2
U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* #if !UCONFIG_NO_NORMALIZATION */

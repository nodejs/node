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
#include "messageformat2_evaluation.h"
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

Function::~Function() {}
FunctionValue::~FunctionValue() {}

MFFunctionRegistry MFFunctionRegistry::Builder::build() {
    U_ASSERT(functions != nullptr);
    U_ASSERT(formattersByType != nullptr);
    MFFunctionRegistry result = MFFunctionRegistry(functions, formattersByType);
    functions = nullptr;
    formattersByType = nullptr;
    return result;
}

MFFunctionRegistry::Builder&
MFFunctionRegistry::Builder::adoptFunction(const FunctionName& functionName,
                                           Function* function,
                                           UErrorCode& errorCode) {
    if (U_SUCCESS(errorCode)) {
        U_ASSERT(functions != nullptr);
        functions->put(functionName, function, errorCode);
    }
    return *this;
}

MFFunctionRegistry::Builder&
MFFunctionRegistry::Builder::setDefaultFormatterNameByType(const UnicodeString& type,
                                                           const FunctionName& functionName,
                                                           UErrorCode& errorCode) {
    if (U_SUCCESS(errorCode)) {
        U_ASSERT(formattersByType != nullptr);
        FunctionName* f = create<FunctionName>(FunctionName(functionName), errorCode);
        formattersByType->put(type, f, errorCode);
     }
     return *this;
 }

MFFunctionRegistry::Builder::Builder(UErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    functions = new Hashtable();
    formattersByType = new Hashtable();
    if (functions == nullptr || formattersByType == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }

    functions->setValueDeleter(uprv_deleteUObject);
    formattersByType->setValueDeleter(uprv_deleteUObject);
}

MFFunctionRegistry::Builder::~Builder() {
    if (functions != nullptr) {
        delete functions;
        functions = nullptr;
    }
    if (formattersByType != nullptr) {
        delete formattersByType;
        formattersByType = nullptr;
    }
}

// Returns non-owned pointer. Returns pointer rather than reference because it can fail.
const Function* MFFunctionRegistry::getFunction(const FunctionName& functionName) const {
    U_ASSERT(functions != nullptr);
    return static_cast<const Function*>(functions->get(functionName));
}

UBool MFFunctionRegistry::getDefaultFormatterNameByType(const UnicodeString& type, FunctionName& name) const {
    U_ASSERT(formattersByType != nullptr);
    const FunctionName* f = static_cast<FunctionName*>(formattersByType->get(type));
    if (f != nullptr) {
        name = *f;
        return true;
    }
    return false;
}

bool MFFunctionRegistry::hasFunction(const FunctionName& f) const {
    return getFunction(f) != nullptr;
}

void MFFunctionRegistry::checkFunction(std::u16string_view s) const {
#if U_DEBUG
    U_ASSERT(hasFunction(FunctionName(UnicodeString(s))));
#else
   (void) s;
#endif
}

// Debugging
void MFFunctionRegistry::checkStandard() const {
    checkFunction(functions::DATETIME);
    checkFunction(functions::DATE);
    checkFunction(functions::TIME);
    checkFunction(functions::CURRENCY);
    checkFunction(functions::NUMBER);
    checkFunction(functions::INTEGER);
    checkFunction(functions::OFFSET);
    checkFunction(functions::STRING);
    checkFunction(functions::TEST_FUNCTION);
    checkFunction(functions::TEST_FORMAT);
    checkFunction(functions::TEST_SELECT);
}

// Function/selector helpers

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

// Adopts its argument
MFFunctionRegistry::MFFunctionRegistry(FunctionMap* f, Hashtable* byType)
    : functions(f), formattersByType(byType) {
    U_ASSERT(f != nullptr);
    U_ASSERT(byType != nullptr);
}

MFFunctionRegistry& MFFunctionRegistry::operator=(MFFunctionRegistry&& other) noexcept {
    cleanup();

    functions = other.functions;
    other.functions = nullptr;
    formattersByType = other.formattersByType;
    other.formattersByType = nullptr;

    return *this;
}

void MFFunctionRegistry::cleanup() noexcept {
    if (functions != nullptr) {
        delete functions;
        functions = nullptr;
    }
    if (formattersByType != nullptr) {
        delete formattersByType;
        formattersByType = nullptr;
    }
}


MFFunctionRegistry::~MFFunctionRegistry() {
    cleanup();
}

// Specific function implementations

UMFDirectionality
outputDirectionalityFromUDir(UMFBidiOption uDir, const Locale& locale) {
    switch (uDir) {
    case U_MF_BIDI_OPTION_LTR:
        return U_MF_DIRECTIONALITY_LTR;
    case U_MF_BIDI_OPTION_RTL:
        return U_MF_DIRECTIONALITY_RTL;
    case U_MF_BIDI_OPTION_AUTO:
    case U_MF_BIDI_OPTION_INHERIT:
        if (locale.isRightToLeft()) {
            return U_MF_DIRECTIONALITY_RTL;
        }
        return U_MF_DIRECTIONALITY_LTR;
    }
    return U_MF_DIRECTIONALITY_LTR;
}

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
        if (inBounds(s, i + 1) && s[i + 1] != PERIOD) {
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

bool isDigitSizeOption(std::u16string_view s) {
    return s == options::FRACTION_DIGITS
        || s == options::MINIMUM_INTEGER_DIGITS
        || s == options::MINIMUM_FRACTION_DIGITS
        || s == options::MAXIMUM_FRACTION_DIGITS
        || s == options::MINIMUM_SIGNIFICANT_DIGITS
        || s == options::MAXIMUM_SIGNIFICANT_DIGITS
        || s == options::ADD
        || s == options::SUBTRACT;
}

/* static */ void StandardFunctions::validateDigitSizeOptions(const FunctionOptions& opts,
                                                              UErrorCode& status) {
    CHECK_ERROR(status);
    for (int32_t i = 0; i < opts.optionsCount(); i++) {
        const ResolvedFunctionOption& opt = opts.options[i];
        if (opt.getName() == options::FRACTION_DIGITS) {
            UErrorCode localStatus = U_ZERO_ERROR;
            auto s = opt.getValue().unwrap().getString(localStatus);
            if (U_SUCCESS(localStatus) && s == options::AUTO) {
                continue;
            }
        }
        if (isDigitSizeOption(opt.getName()) && !isInteger(opt.getValue().unwrap())) {
            status = U_MF_BAD_OPTION;
            return;
        }
    }
}

static double parseNumberLiteral(const UnicodeString& inputStr, UErrorCode& errorCode) {
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

static UNumberFormatRoundingMode validateRoundingMode(const UnicodeString& opt, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return UNumberFormatRoundingMode::UNUM_ROUND_HALFUP;
    }
    if (opt.isEmpty()) {
        return UNumberFormatRoundingMode::UNUM_ROUND_HALFUP;
    }
    if (opt == options::CEIL) {
        return UNumberFormatRoundingMode::UNUM_ROUND_CEILING;
    }
    if (opt == options::FLOOR) {
        return UNumberFormatRoundingMode::UNUM_ROUND_FLOOR;
    }
    if (opt == options::EXPAND) {
        return UNumberFormatRoundingMode::UNUM_ROUND_UP;
    }
    if (opt == options::TRUNC) {
        return UNumberFormatRoundingMode::UNUM_ROUND_DOWN;
    }
    if (opt == options::HALF_CEIL) {
        return UNumberFormatRoundingMode::UNUM_ROUND_HALF_CEILING;
    }
    if (opt == options::HALF_FLOOR) {
        return UNumberFormatRoundingMode::UNUM_ROUND_HALF_FLOOR;
    }
    if (opt == options::HALF_EXPAND) {
        return UNumberFormatRoundingMode::UNUM_ROUND_HALFUP;
    }
    if (opt == options::HALF_TRUNC) {
        return UNumberFormatRoundingMode::UNUM_ROUND_HALFDOWN;
    }
    if (opt == options::HALF_EVEN) {
        return UNumberFormatRoundingMode::UNUM_ROUND_HALFEVEN;
    }
    status = U_MF_BAD_OPTION;
    return UNumberFormatRoundingMode::UNUM_ROUND_HALFUP;
}

static int validateRoundingIncrement(const UnicodeString& opt, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return -1;
    }

    if (opt.isEmpty()) {
        return 1;
    }

    double num = parseNumberLiteral(opt, status);
    if (U_FAILURE(status)) {
        return -1;
    }

    int result = std::trunc(num);

    if (num != result) {
        status = U_MF_BAD_OPTION;
        return -1;
    }

    switch (result) {
        case 1:
        case 2:
        case 5:
        case 10:
        case 20:
        case 25:
        case 50:
        case 100:
        case 200:
        case 250:
        case 500:
        case 1000:
        case 2000:
        case 2500:
        case 5000:
            return result;
        default: {
            status = U_MF_BAD_OPTION;
            return -1;
        }
    }
}

/* static */ StandardFunctions::Number*
StandardFunctions::Number::currency(UErrorCode& success) {
    return create(NumberType::kCurrency, success);
}

/* static */ StandardFunctions::Number*
StandardFunctions::Number::integer(UErrorCode& success) {
    return create(NumberType::kInteger, success);
}

/* static */ StandardFunctions::Number*
StandardFunctions::Number::number(UErrorCode& success) {
    return create(NumberType::kNumber, success);
}

/* static */ StandardFunctions::Number*
StandardFunctions::Number::offset(UErrorCode& success) {
    return create(NumberType::kOffset, success);
}

/* static */ StandardFunctions::Number*
StandardFunctions::Number::percent(UErrorCode& success) {
    return create(NumberType::kPercent, success);
}

/* static */ StandardFunctions::Number*
StandardFunctions::Number::create(StandardFunctions::NumberType numberType, UErrorCode& success) {
    NULL_ON_ERROR(success);

    LocalPointer<Number> result(new Number(numberType));
    if (!result.isValid()) {
        success = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    return result.orphan();
}

LocalPointer<FunctionValue> StandardFunctions::Number::call(const FunctionContext& context,
                                                            const FunctionValue& operand,
                                                            const FunctionOptions& options,
                                                            UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return LocalPointer<FunctionValue>();
    }
    LocalPointer<FunctionValue>
        val(new NumberValue(*this, context, operand, options, errorCode));
    if (!val.isValid()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return val;
}

void StandardFunctions::requireNoRoundingIncrement(const FunctionOptions& opts, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }
    UnicodeString opt = opts.getStringFunctionOption(options::ROUNDING_INCREMENT);
    if (!opt.isEmpty()) {
        status = U_MF_BAD_OPTION;
    }
}

static number::Precision significantDigits(int32_t min, int32_t max) {
    using namespace number;

    Precision p = Precision::unlimited();
    U_ASSERT(min != -1 || max != -1);
    if (min != -1 && max != -1) {
        p = Precision::minMaxSignificantDigits(min, max);
    } else if (min != -1) {
        p = Precision::minSignificantDigits(min);
    } else {
        p = Precision::maxSignificantDigits(max);
    }
    return p;
}

static number::Precision significantDigitsWithRoundingPriority(int32_t min, int32_t max, const number::FractionPrecision& p, UNumberRoundingPriority priority) {
    using namespace number;

    U_ASSERT(priority == UNumberRoundingPriority::UNUM_ROUNDING_PRIORITY_RELAXED || priority == UNumberRoundingPriority::UNUM_ROUNDING_PRIORITY_STRICT);
    Precision result = Precision::unlimited();
    U_ASSERT(min != -1 || max != -1);
    if (min != -1 && max != -1) {
        result = p.withSignificantDigits(min, max, priority);
    } else if (min != -1) {
        // Only the maximum precision value is used for the calculation
    } else {
        result = p.withSignificantDigits(1, max, priority);
    }
    return result;
}

// This function is only used when formatting currency.
// It extracts the user-provided `roundingIncrement` option from the function
// options. If no such option was provided, it returns the default currency formatter.
// If the option was provided, it validates the option's value according to the
// Unicode MessageFormat spec, setting the `status` argument if validation fails.
// If validation succeeds, the option value is divided by 100 to achieve
// cents-based rounding, returning a `number::Precision` object that formats
// numbers with the given rounding increment and minium fraction digits.
// The out-parameter `hasIncrement` is set to true if and only if a valid
// `roundingIncrement` option was provided.
number::Precision StandardFunctions::withRoundingIncrement(const FunctionOptions& opts, bool& hasIncrement, int32_t minFractionDigits, UErrorCode& status) {
    using namespace number;

    UErrorCode localStatus = U_ZERO_ERROR;
    UnicodeString roundingIncrement = opts.getStringFunctionOption(options::ROUNDING_INCREMENT, localStatus);
    int increment = 1;
    if (U_SUCCESS(localStatus)) {
        increment = validateRoundingIncrement(roundingIncrement, status);
        if (U_FAILURE(status)) {
            status = U_MF_BAD_OPTION;
            return CurrencyPrecision::currency(UCURR_USAGE_STANDARD);
        }
    }
    if (increment != 1) {
        hasIncrement = true;
    }
    // Divide by 100 to get cents-based rounding
    double incrementToUse = U_SUCCESS(localStatus) ? increment / 100.0 : 0.01;
    return Precision::increment(incrementToUse).withMinFraction(minFractionDigits);
}

/* static */ number::LocalizedNumberFormatter StandardFunctions::formatterForOptions(const Number& number,
                                                                                     const Locale& locale,
                                                                                     const FunctionOptions& opts,
                                                                                     bool usePercent,
                                                                                     UErrorCode& status) {
    number::UnlocalizedNumberFormatter nf;

    using namespace number;

    UnicodeString currency;
    CurrencyUnit currencyUnit;
    if (number.numberType == NumberType::kCurrency) {
        currency = opts.getStringFunctionOption(options::CURRENCY);
        if (currency.isEmpty()) {
            status = U_MF_OPERAND_MISMATCH_ERROR;
            return {};
        }
        // Validate currency code
        currencyUnit = CurrencyUnit(currency.getBuffer(), status);
        if (U_FAILURE(status)) {
            // Either the operand is numeric (in which case the spec says
            // "A numeric operand without a currency option results in a Bad Operand error")
            // or it's non-numeric, in which case the error will be U_MF_OPERAND_MISMATCH_ERROR anyway.
            status = U_MF_OPERAND_MISMATCH_ERROR;
            return {};
        }
    }

    validateDigitSizeOptions(opts, status);
    if (U_FAILURE(status)) {
        return {};
    }

    if (U_SUCCESS(status)) {
        Formattable opt;
        nf = NumberFormatter::with();

        if (number.numberType == NumberType::kInteger) {
            nf = nf.precision(Precision::integer());
        }

        // Notation options
        if (number.numberType == NumberType::kNumber || number.numberType == NumberType::kPercent) {
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

            if (number.numberType == NumberType::kPercent && usePercent) {
                nf = nf.unit(NoUnit::percent()).scale(Scale::powerOfTen(2));
            }
        }

        int32_t maxSignificantDigits = number.maximumSignificantDigits(opts);
        if (number.numberType == NumberType::kCurrency || number.numberType == NumberType::kNumber
	    || number.numberType == NumberType::kPercent) {
            int32_t minFractionDigits = number.minimumFractionDigits(opts);
            int32_t maxFractionDigits = number.maximumFractionDigits(opts);
            int32_t minSignificantDigits = number.minimumSignificantDigits(opts);
            FractionPrecision fractionPrecision;
            Precision p = Precision::unlimited();
            bool precisionOptions = false;
            bool noSignificantDigits = minSignificantDigits == -1 && maxSignificantDigits == -1;
            if (number.numberType == NumberType::kPercent) {
                if (minFractionDigits == -1) {
                    minFractionDigits = 0;
                }
                if (maxFractionDigits == -1 && minSignificantDigits == -1) {
                    maxFractionDigits = minFractionDigits;
                }
            }
            // Returning -1 means the option wasn't provided
            if (number.numberType == NumberType::kNumber || number.numberType == NumberType::kPercent) {
                if (maxFractionDigits != -1 && minFractionDigits != -1) {
                    precisionOptions = true;
                    fractionPrecision = Precision::minMaxFraction(minFractionDigits, maxFractionDigits);
                } else if (minFractionDigits != -1) {
                    precisionOptions = true;
                    fractionPrecision = Precision::minFraction(minFractionDigits);
                    maxFractionDigits = minFractionDigits;
                } else if (maxFractionDigits != -1) {
                    precisionOptions = true;
                    fractionPrecision = Precision::maxFraction(maxFractionDigits);
                    minFractionDigits = 0;
                }
            }

            if (number.numberType == NumberType::kNumber || number.numberType == NumberType::kCurrency || number.numberType == NumberType::kPercent) {
                bool noFractionDigits = false;
                if (number.numberType == NumberType::kCurrency) {
                    // Determine the minimum fraction digits to use for formatting this currency
                    // amount. Prefer the non-auto `fractionDigits` option if one exists;
                    // otherwise, the number of minor unit digits provided by the ISO 4217
                    // currency code list; otherwise, U_CURR_DEFAULT_DIGITS (defined in ucurr.h).
                    DigitSizeOption fractionDigits = number.fractionDigits(opts);
                    noFractionDigits = fractionDigits.isAuto() || fractionDigits.isInvalid();
                    UErrorCode localStatus = U_ZERO_ERROR;
                    if (fractionDigits.isAuto() || fractionDigits.isInvalid()) {
                        minFractionDigits = ucurr_getDefaultFractionDigits(currency.getBuffer(), &localStatus);
                        if (U_FAILURE(localStatus)) {
                            localStatus = U_ZERO_ERROR;
                            minFractionDigits = U_CURR_DEFAULT_DIGITS;
                        }
                    } else {
                        minFractionDigits = fractionDigits.value();
                    }
                    maxFractionDigits = minFractionDigits;
                } else {
                    noFractionDigits = (minFractionDigits == 0 || minFractionDigits == -1) && (maxFractionDigits == 0 || maxFractionDigits == -1);
                }

                if (!noSignificantDigits) {
                    // Can't specify rounding increment if significant digits are specified
                    requireNoRoundingIncrement(opts, status);
                    if (U_FAILURE(status)) {
                        status = U_MF_BAD_OPTION;
                        return {};
                    }
                }

                // No fraction digits and no significant digits
                if (noFractionDigits && noSignificantDigits) {
                    if (number.numberType == NumberType::kCurrency) {
                        p = withRoundingIncrement(opts, precisionOptions, minFractionDigits, status);
                    } else {
                        p = Precision::fixedFraction(0);
                    }
                    if (U_FAILURE(status)) {
                        status = U_MF_BAD_OPTION;
                        return {};
                    }
                }
                // No fraction digits and significant digits
                else if (noFractionDigits && !noSignificantDigits) {
                    precisionOptions = true;
                    p = significantDigits(minSignificantDigits, maxSignificantDigits);
                    // Can't specify rounding increment if significant digits is specified
                    requireNoRoundingIncrement(opts, status);
                // Fraction digits and significant digits
                } else if (!noFractionDigits && !noSignificantDigits) {
                    // Both fractionDigits and significant digits are specified --
                    // use roundingPriority to resolve conflict

                    precisionOptions = true;
                    fractionPrecision = FractionPrecision::minMaxFraction(minFractionDigits, maxFractionDigits);

                    UnicodeString roundingPriority = opts.getStringFunctionOption(options::ROUNDING_PRIORITY);
                    if (roundingPriority == options::MORE_PRECISION) {
                        precisionOptions = true;
                        p = significantDigitsWithRoundingPriority(minSignificantDigits, maxSignificantDigits, fractionPrecision, UNumberRoundingPriority::UNUM_ROUNDING_PRIORITY_RELAXED);
                    } else if (roundingPriority == options::LESS_PRECISION) {
                        precisionOptions = true;
                        p = significantDigitsWithRoundingPriority(minSignificantDigits, maxSignificantDigits, fractionPrecision, UNumberRoundingPriority::UNUM_ROUNDING_PRIORITY_STRICT);
                    } else {
                        p = significantDigits(minSignificantDigits, maxSignificantDigits);
                    }
                } else {
                    precisionOptions = true;

                    // Fraction digits and no significant digits
                    if (number.numberType == NumberType::kCurrency) {
                        p = withRoundingIncrement(opts, precisionOptions, minFractionDigits, status);
                    } else {
                        p = Precision::minMaxFraction(minFractionDigits, maxFractionDigits);
                    }

                    if (U_FAILURE(status)) {
                        status = U_MF_BAD_OPTION;
                        return {};                    }

                }
            } else {
                p = fractionPrecision;
            }

            if (number.numberType == NumberType::kCurrency) {
                UnicodeString trailingZeroDisplay = opts.getStringFunctionOption(options::TRAILING_ZERO_DISPLAY);
                if (trailingZeroDisplay == options::STRIP_IF_INTEGER) {
                    precisionOptions = true;
                    p = p.trailingZeroDisplay(UNumberTrailingZeroDisplay::UNUM_TRAILING_ZERO_HIDE_IF_WHOLE);
                } else {
                    p = p.trailingZeroDisplay(UNumberTrailingZeroDisplay::UNUM_TRAILING_ZERO_AUTO);
                }

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
        if (minIntegerDigits != -1) {
            nf = nf.integerWidth(IntegerWidth::zeroFillTo(minIntegerDigits));
        }

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

    // Currency options
    if (number.numberType == NumberType::kCurrency) {
        nf = nf.unit(currencyUnit);

        UnicodeString currencySign = opts.getStringFunctionOption(options::CURRENCY_SIGN);
        if (currencySign == options::ACCOUNTING) {
            nf = nf.sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING);
        }

        UnicodeString currencyDisplay = opts.getStringFunctionOption(options::CURRENCY_DISPLAY);
        if (currencyDisplay == options::NARROW_SYMBOL) {
            nf = nf.unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_NARROW);
        } else if (currencyDisplay == options::NAME) {
            nf = nf.unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME);
        } else if (currencyDisplay == options::CODE) {
            nf = nf.unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_ISO_CODE);
        } else if (currencyDisplay == options::NEVER) {
            nf = nf.unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_HIDDEN);
        } else {
            nf = nf.unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FORMAL);
        }
    }

    if (number.numberType == NumberType::kCurrency || number.numberType == NumberType::kPercent) {
        UnicodeString roundingMode = opts.getStringFunctionOption(options::ROUNDING_MODE);
        UNumberFormatRoundingMode mode = validateRoundingMode(roundingMode, status);
        if (U_FAILURE(status)) {
            status = U_MF_BAD_OPTION;
            return {};
        }
        nf = nf.roundingMode(mode);
    }
    return nf.locale(locale);
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
    return '0';
}

StandardFunctions::DigitSizeOption StandardFunctions::Number::digitSizeOptionWithAuto(const FunctionOptions& opts,
                                                                                      std::u16string_view k) const {
    UErrorCode localStatus = U_ZERO_ERROR;
    DigitSizeOption result = digitSizeOption(opts, k, true, localStatus);
    if (U_FAILURE(localStatus)) {
        return DigitSizeOption::invalid();
    }
    return result;
}

int32_t StandardFunctions::Number::digitSizeOptionNoAuto(const FunctionOptions& opts,
                                                         std::u16string_view k) const {
    UErrorCode localStatus = U_ZERO_ERROR;
    StandardFunctions::DigitSizeOption result = digitSizeOption(opts, k, false, localStatus);
    U_ASSERT(!result.isAuto());
    if (result.isInvalid() || U_FAILURE(localStatus)) {
        return -1;
    }
    return result.value();
}

StandardFunctions::DigitSizeOption StandardFunctions::Number::digitSizeOption(const FunctionOptions& opts,
                                                                              const std::u16string_view k,
                                                                              bool allowAuto,
                                                                              UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return DigitSizeOption::invalid();
    }
    UErrorCode localStatus = U_ZERO_ERROR;
    const FunctionValue* opt = opts.getFunctionOption(k, localStatus);
    if (U_SUCCESS(localStatus)) {
        // First try the formatted value
        UnicodeString formatted = opt->formatToString(localStatus);
        int64_t val = 0;
        if (U_SUCCESS(localStatus)) {
            if (formatted == options::AUTO && allowAuto) {
                return DigitSizeOption::autoVal();
            }
            val = getInt64Value(Locale("en-US"), Formattable(formatted), localStatus);
            if (U_SUCCESS(localStatus)) {
                return DigitSizeOption::intVal(static_cast<int32_t>(val));
            }
        }
        if (U_FAILURE(localStatus)) {
            localStatus = U_ZERO_ERROR;

            // Next try the operand
            auto unwrapped = opt->unwrap();
            UnicodeString s = unwrapped.getString(localStatus);
            if (U_SUCCESS(localStatus)) {
                if (s == options::AUTO && allowAuto) {
                    return DigitSizeOption::autoVal();
                }
            } else {
                localStatus = U_ZERO_ERROR;
            }
            val = getInt64Value(Locale("en-US"), opt->unwrap(), localStatus);
            if (U_SUCCESS(localStatus)) {
                return DigitSizeOption::intVal(static_cast<int32_t>(val));
            }
        }
    }
    // Returning -1 indicates that the option wasn't provided or was a non-integer.
    // The caller needs to check for that case, since passing -1 to Precision::maxFraction()
    // is an error.
    status = U_MF_BAD_OPTION;
    return DigitSizeOption::invalid();
}

StandardFunctions::DigitSizeOption StandardFunctions::Number::fractionDigits(const FunctionOptions& opts) const {
    if (numberType != NumberType::kCurrency) {
        return DigitSizeOption::invalid();
    }

    return digitSizeOptionWithAuto(opts, options::FRACTION_DIGITS);
}

int32_t StandardFunctions::Number::maximumFractionDigits(const FunctionOptions& opts) const {
    if (numberType == NumberType::kInteger) {
        return 0;
    }

    return digitSizeOptionNoAuto(opts, options::MAXIMUM_FRACTION_DIGITS);
}

int32_t StandardFunctions::Number::minimumFractionDigits(const FunctionOptions& opts) const {
    Formattable opt;

    if (numberType == NumberType::kInteger) {
        return -1;
    }
    return digitSizeOptionNoAuto(opts, options::MINIMUM_FRACTION_DIGITS);
}

int32_t StandardFunctions::Number::minimumIntegerDigits(const FunctionOptions& opts) const {
    return digitSizeOptionNoAuto(opts, options::MINIMUM_INTEGER_DIGITS);
}

int32_t StandardFunctions::Number::minimumSignificantDigits(const FunctionOptions& opts) const {
    if (numberType == NumberType::kInteger) {
        return -1;
    }
    return digitSizeOptionNoAuto(opts, options::MINIMUM_SIGNIFICANT_DIGITS);
}

int32_t StandardFunctions::Number::maximumSignificantDigits(const FunctionOptions& opts) const {
    return digitSizeOptionNoAuto(opts, options::MAXIMUM_SIGNIFICANT_DIGITS);
}

int32_t StandardFunctions::Number::addOption(const FunctionOptions& opts, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return -1;
    }
    DigitSizeOption result = digitSizeOption(opts, options::ADD, false, errorCode);
    U_ASSERT(!result.isAuto());
    if (result.isInvalid()) {
        errorCode = U_MF_BAD_OPTION;
        return -1;
    }
    return result.value();
}

int32_t StandardFunctions::Number::subtractOption(const FunctionOptions& opts, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return -1;
    }
    DigitSizeOption result = digitSizeOption(opts, options::SUBTRACT, false, errorCode);
    U_ASSERT(!result.isAuto());
    if (result.isInvalid()) {
        errorCode = U_MF_BAD_OPTION;
        return -1;
    }
    return result.value();
}

StandardFunctions::NumberValue::NumberValue(const Number& parent,
                                            const FunctionContext& context,
                                            const FunctionValue& arg,
                                            const FunctionOptions& options,
                                            UErrorCode& errorCode) {
    CHECK_ERROR(errorCode);
    // Must have an argument
    if (arg.isNullOperand()) {
        errorCode = U_MF_OPERAND_MISMATCH_ERROR;
        return;
    }

    locale = context.getLocale();
    opts = options.mergeOptions(arg.getResolvedOptions(), errorCode);
    innerValue = arg.unwrap();
    functionName = functions::NUMBER;
    numberType = parent.numberType;

    int64_t offset = 0;
    switch (numberType) {
        case NumberType::kInteger:
            functionName = functions::INTEGER;
            break;
        case NumberType::kCurrency:
            functionName = functions::CURRENCY;
            break;
        case NumberType::kOffset: {
            functionName = functions::OFFSET;
            UErrorCode localStatus = U_ZERO_ERROR;
            int32_t addOptionValue = parent.addOption(opts, localStatus);
            bool hasAdd = U_SUCCESS(localStatus);
            localStatus = U_ZERO_ERROR;
            int32_t subtractOptionValue = parent.subtractOption(opts, localStatus);
            bool hasSubtract = U_SUCCESS(localStatus);
            if ((hasAdd && hasSubtract) || (!hasAdd && !hasSubtract)) {
                // Either add or subtract must be provided,
                // but not both
                errorCode = U_MF_BAD_OPTION;
                return;
            }
            offset = hasAdd ? addOptionValue : (-1 * subtractOptionValue);
            break;
        }
        case NumberType::kPercent: {
            functionName = functions::PER_CENT;
            break;
        }
        default:
            break;
    }
    inputDir = context.getDirection();
    dir = outputDirectionalityFromUDir(inputDir, locale);

    UErrorCode localStatus = U_ZERO_ERROR;
    number::LocalizedNumberFormatter realFormatter = formatterForOptions(parent, locale, opts, false, localStatus);
    number::LocalizedNumberFormatter percentFormatter = formatterForOptions(parent, locale, opts, true, localStatus);

    int64_t integerValue = 0;
    double valueWithOffset = 0;

    // Need to validate operand before validating options
    if (U_SUCCESS(errorCode)) {
        switch (innerValue.getType()) {
        case UFMT_DOUBLE: {
            double d = innerValue.getDouble(errorCode);
            U_ASSERT(U_SUCCESS(errorCode));
            if (U_SUCCESS(localStatus)) {
                valueWithOffset = d + offset;
                formattedNumber = realFormatter.formatDouble(valueWithOffset, errorCode);
                scaledFormattedNumber = realFormatter.formatDouble(valueWithOffset * 100, errorCode);
                percentFormattedNumber = percentFormatter.formatDouble(valueWithOffset, errorCode);
                integerValue = static_cast<int64_t>(std::round(d));
            }
            break;
        }
        case UFMT_LONG: {
            int32_t l = innerValue.getLong(errorCode);
            U_ASSERT(U_SUCCESS(errorCode));
            if (U_SUCCESS(localStatus)) {
                valueWithOffset = l + offset;
                formattedNumber = realFormatter.formatInt(valueWithOffset, errorCode);
                scaledFormattedNumber = realFormatter.formatInt(valueWithOffset * 100, errorCode);
                percentFormattedNumber = percentFormatter.formatInt(valueWithOffset, errorCode);
                integerValue = l;
            }
            break;
        }
        case UFMT_INT64: {
            int64_t i = innerValue.getInt64(errorCode);
            U_ASSERT(U_SUCCESS(errorCode));
            if (U_SUCCESS(localStatus)) {
                valueWithOffset = i + offset;
                formattedNumber = realFormatter.formatInt(valueWithOffset, errorCode);
                scaledFormattedNumber = realFormatter.formatInt(valueWithOffset * 100, errorCode);
                percentFormattedNumber = percentFormatter.formatInt(valueWithOffset, errorCode);
                integerValue = i;
            }
            break;
        }
        case UFMT_STRING: {
            // Try to parse the string as a number
            const UnicodeString& s = innerValue.getString(errorCode);
            U_ASSERT(U_SUCCESS(errorCode));
            double d = parseNumberLiteral(s, errorCode);
            if (U_FAILURE(errorCode))
                return;
            if (U_SUCCESS(localStatus)) {
                valueWithOffset = d + offset;
                formattedNumber = realFormatter.formatDouble(valueWithOffset, errorCode);
                scaledFormattedNumber = realFormatter.formatDouble(valueWithOffset * 100, errorCode);
                percentFormattedNumber = percentFormatter.formatDouble(valueWithOffset, errorCode);
                integerValue = static_cast<int64_t>(std::round(d));
            }
            break;
        }
        default: {
            // Other types can't be parsed as a number
            errorCode = U_MF_OPERAND_MISMATCH_ERROR;
            break;
        }
        }
    }

    if (U_SUCCESS(errorCode) && U_FAILURE(localStatus)) {
        errorCode = localStatus;
    }

    // Ignore U_USING_DEFAULT_WARNING
    if (errorCode == U_USING_DEFAULT_WARNING) {
        errorCode = U_ZERO_ERROR;
    }

    // Need to set the integer value if invoked as :integer
    if (numberType == NumberType::kInteger) {
        innerValue = Formattable(integerValue);
    }

    // Need to set the offsetted value if invoked as :offset
    if (numberType == NumberType::kOffset) {
        innerValue = Formattable(valueWithOffset);
    }
}

UnicodeString StandardFunctions::NumberValue::formatToString(UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return {};
    }

    return (numberType == kPercent)
        ? percentFormattedNumber.toString(errorCode)
        : formattedNumber.toString(errorCode);
}

StandardFunctions::Number::~Number() {}
StandardFunctions::NumberValue::~NumberValue() {}

/* static */ StandardFunctions::Number::PluralType
StandardFunctions::Number::pluralType(const FunctionOptions& opts) {
    UnicodeString val = opts.getStringFunctionOption(options::SELECT);
    if (!val.isEmpty()) {
        if (val == options::ORDINAL) {
            return PluralType::PLURAL_ORDINAL;
        }
        if (val == options::EXACT) {
            return PluralType::PLURAL_EXACT;
        }
    }
    return PluralType::PLURAL_CARDINAL;
}

void StandardFunctions::NumberValue::selectKeys(const UnicodeString* keys,
                                                int32_t keysLen,
                                                int32_t* prefs,
                                                int32_t& prefsLen,
                                                UErrorCode& errorCode) const {
    CHECK_ERROR(errorCode);

    Number::PluralType type = Number::pluralType(opts);

    // (resolvedSelector is `this`)

    // See  https://github.com/unicode-org/message-format-wg/blob/main/spec/registry.md#number-selection
    // 1. Let exact be the JSON string representation of the numeric value of resolvedSelector
    const number::FormattedNumber& numberToUse = (numberType == NumberType::kPercent) ? scaledFormattedNumber : formattedNumber;
    UnicodeString exact = numberToUse.toString(errorCode);

    if (U_FAILURE(errorCode)) {
        // Non-number => selector error
        errorCode = U_MF_SELECTOR_ERROR;
        return;
    }

    // Step 2. Let keyword be a string which is the result of rule selection on resolvedSelector.
    // If the option select is set to exact, rule-based selection is not used. Return the empty string.
    UnicodeString keyword;
    if (type != Number::PluralType::PLURAL_EXACT) {
        UPluralType t = type == Number::PluralType::PLURAL_ORDINAL ? UPLURAL_TYPE_ORDINAL : UPLURAL_TYPE_CARDINAL;
        // Look up plural rules by locale and type
        LocalPointer<PluralRules> rules(PluralRules::forLocale(locale, t, errorCode));
        CHECK_ERROR(errorCode);

        keyword = rules->select(numberToUse, errorCode);
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
		prefs[prefsLen] = i;
                prefsLen++;
                break;
            }
        }
    }

    // Return immediately if exact matching was requested
    if (prefsLen == keysLen || type == Number::PluralType::PLURAL_EXACT) {
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
            prefs[prefsLen] = i;
            prefsLen++;
        }
    }

    // Note: Step 5(iii) "Else, emit a Selection Error" is omitted in both loops

    // 6. Return a new list whose elements are the concatenation of the elements
    // (in order) of resultExact followed by the elements (in order) of resultKeyword.
    // (Implicit, since `prefs` is an out-parameter)
}

// --------- DateTime

/* static */ StandardFunctions::DateTime*
StandardFunctions::DateTime::date(UErrorCode& success) {
    return DateTime::create(DateTimeType::kDate, success);
}

/* static */ StandardFunctions::DateTime*
StandardFunctions::DateTime::time(UErrorCode& success) {
    return DateTime::create(DateTimeType::kTime, success);
}

/* static */ StandardFunctions::DateTime*
StandardFunctions::DateTime::dateTime(UErrorCode& success) {
    return DateTime::create(DateTimeType::kDateTime, success);
}

/* static */ StandardFunctions::DateTime*
StandardFunctions::DateTime::create(DateTime::DateTimeType type,
                                    UErrorCode& success) {
    NULL_ON_ERROR(success);

    LocalPointer<DateTime> result(new DateTime(type));
    if (!result.isValid()) {
        success = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    return result.orphan();
}

LocalPointer<FunctionValue>
StandardFunctions::DateTime::call(const FunctionContext& context,
                                  const FunctionValue& val,
                                  const FunctionOptions& opts,
                                  UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return LocalPointer<FunctionValue>();
    }
    LocalPointer<FunctionValue>
        result(new DateTimeValue(type, context, val, opts, errorCode));
    if (!result.isValid()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

static DateFormat::EStyle stringToStyle(UnicodeString option, UErrorCode& errorCode) {
    if (U_SUCCESS(errorCode)) {
        UnicodeString upper = option.toUpper();
        if (upper.isEmpty()) {
            return DateFormat::EStyle::kShort;
        }
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
    gMF2DateParsersInitOnce.reset();
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

static void initDateParsers(UErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    umtx_initOnce(gMF2DateParsersInitOnce, &initDateParsersOnce, errorCode);
}

UnicodeString StandardFunctions::DateTimeValue::formatToString(UErrorCode& status) const {
    (void) status;

    return formattedDate;
}

extern TimeZone* createTimeZone(const DateInfo&, UErrorCode&);

StandardFunctions::DateTimeValue::DateTimeValue(DateTime::DateTimeType type,
                                                const FunctionContext& context,
                                                const FunctionValue& arg,
                                                const FunctionOptions& options,
                                                UErrorCode& errorCode) {
    CHECK_ERROR(errorCode);
    using DateTimeType = DateTime::DateTimeType;

    // Function requires an operand
    if (arg.isNullOperand()) {
        errorCode = U_MF_OPERAND_MISMATCH_ERROR;
        return;
    }

    locale = context.getLocale();
    opts = options.mergeOptions(arg.getResolvedOptions(), errorCode);
    innerValue = arg.unwrap();
    switch (type) {
    case DateTimeType::kDate:
        functionName = functions::DATE;
        break;
    case DateTimeType::kTime:
        functionName = functions::TIME;
        break;
    case DateTimeType::kDateTime:
        functionName = functions::DATETIME;
        break;
    }
    inputDir = context.getDirection();
    dir = outputDirectionalityFromUDir(inputDir, locale);

    const Formattable* source = &innerValue;

    LocalPointer<DateFormat> df;
    Formattable opt;

    DateFormat::EStyle dateStyle = DateFormat::kShort;
    DateFormat::EStyle timeStyle = DateFormat::kShort;

    UnicodeString dateStyleName("dateStyle");
    UnicodeString timeStyleName("timeStyle");
    UnicodeString styleName("style");

    UnicodeString dateStyleOption = opts.getStringFunctionOption(dateStyleName);
    UnicodeString timeStyleOption = opts.getStringFunctionOption(timeStyleName);
    bool hasDateStyleOption = dateStyleOption.length() > 0;
    bool hasTimeStyleOption = timeStyleOption.length() > 0;
    bool noOptions = opts.optionsCount() == 0;

    bool useStyle = (type == DateTimeType::kDateTime
                     && (hasDateStyleOption || hasTimeStyleOption
                         || noOptions))
        || (type != DateTimeType::kDateTime);

    bool useDate = type == DateTimeType::kDate
        || (type == DateTimeType::kDateTime
            && hasDateStyleOption);
    bool useTime = type == DateTimeType::kTime
        || (type == DateTimeType::kDateTime
            && hasTimeStyleOption);

    if (useStyle) {
        // Extract style options
        if (type == DateTimeType::kDateTime) {
            // Note that the options-getting has to be repeated across the three cases,
            // since `:datetime` uses "dateStyle"/"timeStyle" and `:date` and `:time`
            // use "style"
            dateStyle = stringToStyle(opts.getStringFunctionOption(dateStyleName), errorCode);
            timeStyle = stringToStyle(opts.getStringFunctionOption(timeStyleName), errorCode);

            if (useDate && !useTime) {
                df.adoptInstead(DateFormat::createDateInstance(dateStyle, locale));
            } else if (useTime && !useDate) {
                df.adoptInstead(DateFormat::createTimeInstance(timeStyle, locale));
            } else {
                df.adoptInstead(DateFormat::createDateTimeInstance(dateStyle, timeStyle, locale));
            }
        } else if (type == DateTimeType::kDate) {
            dateStyle = stringToStyle(opts.getStringFunctionOption(styleName), errorCode);
            df.adoptInstead(DateFormat::createDateInstance(dateStyle, locale));
        } else {
            // :time
            timeStyle = stringToStyle(opts.getStringFunctionOption(styleName), errorCode);
            df.adoptInstead(DateFormat::createTimeInstance(timeStyle, locale));
        }
    } else {
        // Build up a skeleton based on the field options, then use that to
        // create the date formatter

        UnicodeString skeleton;
        #define ADD_PATTERN(s) skeleton += UnicodeString(s)
        if (U_SUCCESS(errorCode)) {
            // Year
            UnicodeString year = opts.getStringFunctionOption(options::YEAR, errorCode);
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
            UnicodeString month = opts.getStringFunctionOption(options::MONTH, errorCode);
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
            UnicodeString weekday = opts.getStringFunctionOption(options::WEEKDAY, errorCode);
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
            UnicodeString day = opts.getStringFunctionOption(options::DAY, errorCode);
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
            UnicodeString hour = opts.getStringFunctionOption(options::HOUR, errorCode);
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
            UnicodeString minute = opts.getStringFunctionOption(options::MINUTE, errorCode);
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
            UnicodeString second = opts.getStringFunctionOption(options::SECOND, errorCode);
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
          TODO(ICU-22754)
          fractionalSecondDigits
          hourCycle
          timeZoneName
          era
         */
        df.adoptInstead(DateFormat::createInstanceForSkeleton(skeleton, errorCode));
    }

    if (U_FAILURE(errorCode)) {
        return;
    }
    if (!df.isValid()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    UnicodeString result;
    switch (source->getType()) {
    case UFMT_STRING: {
        // Lazily initialize date parsers used for parsing date literals
        initDateParsers(errorCode);
        CHECK_ERROR(errorCode);

        const UnicodeString& sourceStr = source->getString(errorCode);
        U_ASSERT(U_SUCCESS(errorCode));

        DateInfo dateInfo = StandardFunctions::DateTime::createDateInfoFromString(sourceStr, errorCode);
        if (U_FAILURE(errorCode)) {
            errorCode = U_MF_OPERAND_MISMATCH_ERROR;
            return;
        }
        df->adoptTimeZone(createTimeZone(dateInfo, errorCode));
        df->format(dateInfo.date, result, 0, errorCode);
        innerValue = message2::Formattable(std::move(dateInfo));
        break;
    }
    case UFMT_DATE: {
        const DateInfo* dateInfo = source->getDate(errorCode);
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
        return;
    }
    // Ignore U_USING_DEFAULT_WARNING
    if (errorCode == U_USING_DEFAULT_WARNING) {
        errorCode = U_ZERO_ERROR;
    }
    formattedDate = result;
}

// From https://github.com/unicode-org/message-format-wg/blob/main/spec/registry.md#date-and-time-operands :
// "A date/time literal value is a non-empty string consisting of an ISO 8601 date, or
// an ISO 8601 datetime optionally followed by a timezone offset."
UDate StandardFunctions::DateTime::tryPatterns(const UnicodeString& sourceStr,
                                               UErrorCode& errorCode) {
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
                                                       UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return 0;
    }
    int32_t len = sourceStr.length();
    if (len > 0 && sourceStr[len] == 'Z') {
        return dateTimeUTCParser->parse(sourceStr, errorCode);
    }
    return dateTimeZoneParser->parse(sourceStr, errorCode);
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
                                                               UErrorCode& errorCode) {
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

StandardFunctions::DateTime::~DateTime() {}
StandardFunctions::DateTimeValue::~DateTimeValue() {}

// --------- String

/* static */ StandardFunctions::String*
StandardFunctions::String::string(UErrorCode& success) {
    NULL_ON_ERROR(success);

    LocalPointer<String> result(new String());
    if (!result.isValid()) {
        success = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    return result.orphan();
}

extern UnicodeString formattableToString(const Locale&,
                                         const Formattable&,
                                         UErrorCode&);

LocalPointer<FunctionValue>
StandardFunctions::String::call(const FunctionContext& context,
                                const FunctionValue& val,
                                const FunctionOptions& opts,
                                UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return LocalPointer<FunctionValue>();
    }
    LocalPointer<FunctionValue>
        result(new StringValue(context, val, opts, errorCode));
    if (!result.isValid()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

UnicodeString StandardFunctions::StringValue::formatToString(UErrorCode& errorCode) const {
    (void) errorCode;

    return formattedString;
}

UMFDirectionality stringOutputDirection(UMFBidiOption inputDir) {
    switch (inputDir) {
        case U_MF_BIDI_OPTION_INHERIT:
        case U_MF_BIDI_OPTION_AUTO:
            return U_MF_DIRECTIONALITY_UNKNOWN;
        case U_MF_BIDI_OPTION_LTR:
            return U_MF_DIRECTIONALITY_LTR;
        case U_MF_BIDI_OPTION_RTL:
            return U_MF_DIRECTIONALITY_RTL;
    }

    return U_MF_DIRECTIONALITY_LTR;
}

StandardFunctions::StringValue::StringValue(const FunctionContext& context,
                                            const FunctionValue& val,
                                            const FunctionOptions&,
                                            UErrorCode& status) {
    CHECK_ERROR(status);
    locale = context.getLocale();
    innerValue = val.unwrap();
    functionName = UnicodeString("string");
    inputDir = context.getDirection();
    dir = stringOutputDirection(inputDir);
    // No options
    // Convert to string
    formattedString = formattableToString(context.getLocale(), innerValue, status);
}

void StandardFunctions::StringValue::selectKeys(const UnicodeString* keys,
                                                int32_t keysLen,
                                                int32_t* prefs,
                                                int32_t& prefsLen,
                                                UErrorCode& errorCode) const {
    CHECK_ERROR(errorCode);

    // Just compares the key and value as strings

    prefsLen = 0;

    if (U_FAILURE(errorCode)) {
        return;
    }
    // Normalize result
    UnicodeString normalized = normalizeNFC(formattedString);

    for (int32_t i = 0; i < keysLen; i++) {
        if (keys[i] == normalized) {
	    prefs[0] = i;
            prefsLen = 1;
            break;
        }
    }
}

StandardFunctions::String::~String() {}
StandardFunctions::StringValue::~StringValue() {}

// ------------ TestFunction

StandardFunctions::TestFunction::~TestFunction() {}
StandardFunctions::TestFunctionValue::~TestFunctionValue() {}


LocalPointer<FunctionValue> StandardFunctions::TestFunction::call(const FunctionContext& context,
                                                                  const FunctionValue& operand,
                                                                  const FunctionOptions& options,
                                                                  UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return LocalPointer<FunctionValue>();
    }
    LocalPointer<FunctionValue>
        val(new TestFunctionValue(*this, context, operand, options, errorCode));
    if (!val.isValid()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return val;
}

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
            const UnicodeString& s = arg.getString(status);
            U_ASSERT(U_SUCCESS(status));
            result = parseNumberLiteral(s, status);
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

static bool isTestFunction(const UnicodeString& s) {
    return (s == functions::TEST_FORMAT
            || s == functions::TEST_SELECT
            || s == functions::TEST_FUNCTION);
}

static void setFailsFromFunctionValue(const FunctionValue& optionValue,
                                      bool& failsFormat,
                                      bool& failsSelect,
                                      UErrorCode& status) {
    UnicodeString failsString = optionValue.unwrap().getString(status);
    if (U_SUCCESS(status)) {
        // 9i. If its value resolves to the string 'always', then
        if (failsString == options::ALWAYS) {
            // 9ia. Set FailsFormat to be true
            failsFormat = true;
            // 9ib. Set FailsSelect to be true.
            failsSelect = true;
        }
        // 9ii. Else if its value resolves to the string "format", then
        else if (failsString == options::FORMAT) {
            // 9ia. Set FailsFormat to be true
            failsFormat = true;
        }
        // 9iii. Else if its value resolves to the string "select", then
        else if (failsString == options::SELECT) {
            // 9iiia. Set FailsSelect to be true.
            failsSelect = true;
        }
        // 9iv. Else if its value does not resolve to the string "never", then
        else if (failsString != options::NEVER) {
            // 9iv(a). Emit "bad-option" Resolution Error.
            status = U_MF_BAD_OPTION;
        }
    } else {
        // 9iv. again
        status = U_MF_BAD_OPTION;
    }
}

/* static */ void StandardFunctions::TestFunction::testFunctionParameters(const FunctionValue& arg,
                                                                          const FunctionOptions& options,
                                                                          int32_t& decimalPlaces,
                                                                          bool& failsFormat,
                                                                          bool& failsSelect,
                                                                          double& input,
                                                                          UErrorCode& status) const {
    CHECK_ERROR(status);

    // 1. Let DecimalPlaces be 0.
    decimalPlaces = 0;

    // 2. Let FailsFormat be false.
    failsFormat = false;

    // 3. Let FailsSelect be false.
    failsSelect = false;

    // 4. Let arg be the resolved value of the expression operand.
    // (already true)

    // 5. If arg is the resolved value of an expression with a :test:function, :test:select, or :test:format annotation for which resolution has succeeded, then
    if (isTestFunction(arg.getFunctionName())) {
        // 5i. Let Input be the Input value of arg.
        input = formattableToNumber(arg.unwrap(), status);
        if (U_FAILURE(status)) {
            status = U_MF_OPERAND_MISMATCH_ERROR;
            return;
        }
        const FunctionOptions& opts = arg.getResolvedOptions();
        // 5ii. Set DecimalPlaces to be DecimalPlaces value of arg.
        const FunctionValue* decimalPlacesFunctionValue = opts.getFunctionOption(UnicodeString("decimalPlaces"), status);
        if (U_SUCCESS(status)) {
            decimalPlaces = formattableToNumber(decimalPlacesFunctionValue->unwrap(), status);
            if (U_FAILURE(status)) {
                status = U_MF_OPERAND_MISMATCH_ERROR;
                return;
            }
        } else {
            // Option was not provided -- not an error
            status = U_ZERO_ERROR;
        }
        // 5iii. Set FailsFormat to be FailsFormat value of arg.
        const FunctionValue* failsFormatFunctionValue = opts.getFunctionOption(UnicodeString("fails"), status);
        if (U_SUCCESS(status)) {
            setFailsFromFunctionValue(*failsFormatFunctionValue, failsFormat, failsSelect, status);
            if (U_FAILURE(status)) {
                status = U_MF_BAD_OPTION;
                return;
            }
        } else {
            // Option was not provided -- not an error
            status = U_ZERO_ERROR;
        }
        // 5iv. Set FailsSelect to be FailsSelect value of arg.
        // (Done in previous step)
    } else {
        // 6. Else if arg is a numerical value or a string matching the number-literal production, then
        input = formattableToNumber(arg.unwrap(), status);
        if (U_FAILURE(status)) {
            // 7. Else,
            // 7i. Emit "bad-operand" Resolution Error.
            status = U_MF_OPERAND_MISMATCH_ERROR;
            // 7ii. Use a fallback value as the resolved value of the expression.
            // Further steps of this algorithm are not followed.
            return;
        }
    }

    const FunctionValue* decimalPlacesOpt = options.getFunctionOption(options::DECIMAL_PLACES, status);
    // 8. If the decimalPlaces option is set, then
    if (U_SUCCESS(status)) {
        // 8i. If its value resolves to a numerical integer value 0 or 1
        // or their corresponding string representations '0' or '1', then
        double decimalPlacesInput = formattableToNumber(decimalPlacesOpt->unwrap(), status);
        if (U_SUCCESS(status)) {
            if (decimalPlacesInput == 0 || decimalPlacesInput == 1) {
                // 8ia. Set DecimalPlaces to be the numerical value of the option.
                decimalPlaces = decimalPlacesInput;
            }
            // 8ii. Else if its value is not an unresolved value set by option resolution,
            else {
                // 8iia. Emit "bad-option" Resolution Error.
                status = U_MF_BAD_OPTION;
                return;
                // 8iib. Use a fallback value as the resolved value of the expression.
            }
        } else {
            status = U_MF_BAD_OPTION;
            return;
        }
    } else {
        // Option was not provided -- not an error
        status = U_ZERO_ERROR;
    }

    const FunctionValue* failsOpt = options.getFunctionOption(UnicodeString("fails"), status);
    // 9. If the fails option is set, then
    if (U_SUCCESS(status)) {
        setFailsFromFunctionValue(*failsOpt, failsFormat, failsSelect, status);
    } else {
        // Option was not provided -- not an error
        status = U_ZERO_ERROR;
    }
}

StandardFunctions::TestFunctionValue::TestFunctionValue(const TestFunction& parent,
                                                        const FunctionContext&,
                                                        const FunctionValue& arg,
                                                        const FunctionOptions& options,
                                                        UErrorCode& status) {
    parent.testFunctionParameters(arg, options, decimalPlaces,
                                  failsFormat, failsSelect, input, status);
    CHECK_ERROR(status);
    opts = options.mergeOptions(arg.getResolvedOptions(), status);
    innerValue = arg.unwrap();
    canFormat = parent.canFormat;
    canSelect = parent.canSelect;
    functionName = UnicodeString(canFormat && canSelect ?
                                 "test:function"
                                 : canFormat ? "test:format"
                                 : "test:select");

    CHECK_ERROR(status);

    // If FailsFormat is true, attempting to format the placeholder to any
    // formatting target will fail.
    if (failsFormat) {
        formattedString = arg.getFallback();
        return;
    }

    // When :test:function is used as a formatter, a placeholder resolving to a value
    // with a :test:function expression is formatted as a concatenation of the following parts:
    // 1. If Input is less than 0, the character - U+002D Hyphen-Minus.
    if (input < 0) {
        formattedString += HYPHEN;
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
    formattedString += UnicodeString(buffer);
    // 3. If DecimalPlaces is 1, then
    if (decimalPlaces == 1) {
        // 3i. The character . U+002E Full Stop.
        formattedString += u".";
        // 3ii. The single decimal digit character representing the value
        // floor((abs(Input) - floor(abs(Input))) * 10)
        int32_t val = floor((abs(input) - floor(abs(input)) * 10));
        formattedString += digitToChar(val, status);
        U_ASSERT(U_SUCCESS(status));
    }
}

UnicodeString StandardFunctions::TestFunctionValue::formatToString(UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return {};
    }
    if (failsFormat) {
        status = U_MF_BAD_OPTION;
    } else if (!canFormat) {
        status = U_MF_FORMATTING_ERROR;
    }
    if (!canFormat) {
        return {};
    }
    return formattedString;
}

void StandardFunctions::TestFunctionValue::selectKeys(const UnicodeString* keys,
                                                      int32_t keysLen,
                                                      int32_t* prefs,
                                                      int32_t& prefsLen,
                                                      UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return;
    }

    if (!canSelect || failsSelect) {
        status = U_MF_SELECTOR_ERROR;
        return;
    }

    prefsLen = 0;

    if (input != 1) {
        return;
    }

    // If the Input is 1 and DecimalPlaces is 1, the method will return some slice
    // of the list « '1.0', '1' », depending on whether those values are included in keys.
    if (input == 1 && decimalPlaces == 1) {
        // 1.0 must come first, so search the keys for 1.0 and then 1
        for (int32_t i = 0; i < keysLen; i++) {
            if (keys[i] == u"1.0") {
                prefs[0] = i;
                prefsLen++;
            }
        }
    }

    // If the Input is 1 and DecimalPlaces is 0, the method will return the list « '1' » if
    // keys includes '1', or an empty list otherwise.
    // If the Input is any other value, the method will return an empty list.
    for (int32_t i = 0; i < keysLen; i++) {
        if (keys[i] == u"1") {
            prefs[prefsLen] = i;
            prefsLen++;
        }
    }
}

StandardFunctions::TestFunction::TestFunction(bool format, bool select) : canFormat(format), canSelect(select) {
    U_ASSERT(format || select);
}

/* static */ StandardFunctions::TestFunction* StandardFunctions::TestFunction::testFunction(UErrorCode& status) {
    return create<TestFunction>(TestFunction(true, true), status);
}

/* static */ StandardFunctions::TestFunction* StandardFunctions::TestFunction::testFormat(UErrorCode& status) {
    return create<TestFunction>(TestFunction(true, false), status);
}

/* static */ StandardFunctions::TestFunction* StandardFunctions::TestFunction::testSelect(UErrorCode& status) {
    return create<TestFunction>(TestFunction(false, true), status);
}

} // namespace message2
U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* #if !UCONFIG_NO_NORMALIZATION */

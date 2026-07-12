// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#ifndef U_HIDE_DEPRECATED_API

#ifndef MESSAGEFORMAT2_FUNCTION_REGISTRY_INTERNAL_H
#define MESSAGEFORMAT2_FUNCTION_REGISTRY_INTERNAL_H

#if U_SHOW_CPLUSPLUS_API

#if !UCONFIG_NO_NORMALIZATION

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/datefmt.h"
#include "unicode/messageformat2_function_registry.h"

U_NAMESPACE_BEGIN

namespace message2 {

// Constants for option names
namespace options {
static constexpr std::u16string_view ALWAYS = u"always";
static constexpr std::u16string_view COMPACT = u"compact";
static constexpr std::u16string_view COMPACT_DISPLAY = u"compactDisplay";
static constexpr std::u16string_view DATE_STYLE = u"dateStyle";
static constexpr std::u16string_view DAY = u"day";
static constexpr std::u16string_view DECIMAL_PLACES = u"decimalPlaces";
static constexpr std::u16string_view DEFAULT_UPPER = u"DEFAULT";
static constexpr std::u16string_view ENGINEERING = u"engineering";
static constexpr std::u16string_view EXACT = u"exact";
static constexpr std::u16string_view EXCEPT_ZERO = u"exceptZero";
static constexpr std::u16string_view FAILS = u"fails";
static constexpr std::u16string_view FULL_UPPER = u"FULL";
static constexpr std::u16string_view HOUR = u"hour";
static constexpr std::u16string_view LONG = u"long";
static constexpr std::u16string_view LONG_UPPER = u"LONG";
static constexpr std::u16string_view MAXIMUM_FRACTION_DIGITS = u"maximumFractionDigits";
static constexpr std::u16string_view MAXIMUM_SIGNIFICANT_DIGITS = u"maximumSignificantDigits";
static constexpr std::u16string_view MEDIUM_UPPER = u"MEDIUM";
static constexpr std::u16string_view MIN2 = u"min2";
static constexpr std::u16string_view MINIMUM_FRACTION_DIGITS = u"minimumFractionDigits";
static constexpr std::u16string_view MINIMUM_INTEGER_DIGITS = u"minimumIntegerDigits";
static constexpr std::u16string_view MINIMUM_SIGNIFICANT_DIGITS = u"minimumSignificantDigits";
static constexpr std::u16string_view MINUTE = u"minute";
static constexpr std::u16string_view MONTH = u"month";
static constexpr std::u16string_view NARROW = u"narrow";
static constexpr std::u16string_view NEGATIVE = u"negative";
static constexpr std::u16string_view NEVER = u"never";
static constexpr std::u16string_view NOTATION = u"notation";
static constexpr std::u16string_view NUMBERING_SYSTEM = u"numberingSystem";
static constexpr std::u16string_view NUMERIC = u"numeric";
static constexpr std::u16string_view ORDINAL = u"ordinal";
static constexpr std::u16string_view PERCENT_STRING = u"percent";
static constexpr std::u16string_view SCIENTIFIC = u"scientific";
static constexpr std::u16string_view SECOND = u"second";
static constexpr std::u16string_view SELECT = u"select";
static constexpr std::u16string_view SHORT = u"short";
static constexpr std::u16string_view SHORT_UPPER = u"SHORT";
static constexpr std::u16string_view SIGN_DISPLAY = u"signDisplay";
static constexpr std::u16string_view STYLE = u"style";
static constexpr std::u16string_view TIME_STYLE = u"timeStyle";
static constexpr std::u16string_view TWO_DIGIT = u"2-digit";
static constexpr std::u16string_view USE_GROUPING = u"useGrouping";
static constexpr std::u16string_view WEEKDAY = u"weekday";
static constexpr std::u16string_view YEAR = u"year";
} // namespace options

    // Built-in functions
    /*
      The standard functions are :datetime, :date, :time,
      :number, :integer, and :string,
      per https://github.com/unicode-org/message-format-wg/blob/main/spec/registry.md
      as of https://github.com/unicode-org/message-format-wg/releases/tag/LDML45-alpha
    */
    class StandardFunctions {
        friend class MessageFormatter;

        public:
        // Used for normalizing variable names and keys for comparison
        static UnicodeString normalizeNFC(const UnicodeString&);

        private:
        static void validateDigitSizeOptions(const FunctionOptions&, UErrorCode&);
        static void checkSelectOption(const FunctionOptions&, UErrorCode&);
        static UnicodeString getStringOption(const FunctionOptions& opts,
                                             std::u16string_view optionName,
                                             UErrorCode& errorCode);

        class DateTime;

        class DateTimeFactory : public FormatterFactory {
        public:
            Formatter* createFormatter(const Locale& locale, UErrorCode& status) override;
            static DateTimeFactory* date(UErrorCode&);
            static DateTimeFactory* time(UErrorCode&);
            static DateTimeFactory* dateTime(UErrorCode&);
            DateTimeFactory() = delete;
            virtual ~DateTimeFactory();

        private:
            friend class DateTime;

            typedef enum DateTimeType {
                Date,
                Time,
                DateTime
            } DateTimeType;

            DateTimeType type;
            DateTimeFactory(DateTimeType t) : type(t) {}
        };

        class DateTime : public Formatter {
        public:
            FormattedPlaceholder format(FormattedPlaceholder&& toFormat, FunctionOptions&& options, UErrorCode& status) const override;
            virtual ~DateTime();

        private:
            const Locale& locale;
            const DateTimeFactory::DateTimeType type;
            friend class DateTimeFactory;
            DateTime(const Locale& l, DateTimeFactory::DateTimeType t)
                : locale(l), type(t) {}
            const LocalPointer<icu::DateFormat> icuFormatter;

            // Methods for parsing date literals
            UDate tryPatterns(const UnicodeString&, UErrorCode&) const;
            UDate tryTimeZonePatterns(const UnicodeString&, UErrorCode&) const;
            DateInfo createDateInfoFromString(const UnicodeString&, UErrorCode&) const;

            /*
              Looks up an option by name, first checking `opts`, then the cached options
              in `toFormat` if applicable, and finally using a default

              Ignores any options with non-string values
             */
            UnicodeString getFunctionOption(const FormattedPlaceholder& toFormat,
                                            const FunctionOptions& opts,
                                            std::u16string_view optionName) const;
            // Version for options that don't have defaults; sets the error
            // code instead of returning a default value
            UnicodeString getFunctionOption(const FormattedPlaceholder& toFormat,
                                            const FunctionOptions& opts,
                                            std::u16string_view optionName,
                                            UErrorCode& errorCode) const;

        };

        // Note: IntegerFactory doesn't implement SelectorFactory;
        // instead, an instance of PluralFactory is registered to the integer
        // selector
        // TODO
        class IntegerFactory : public FormatterFactory {
        public:
            Formatter* createFormatter(const Locale& locale, UErrorCode& status) override;
            virtual ~IntegerFactory();
        };

        class NumberFactory : public FormatterFactory {
        public:
            Formatter* createFormatter(const Locale& locale, UErrorCode& status) override;
            virtual ~NumberFactory();
        private:
            friend class IntegerFactory;
            static NumberFactory integer(const Locale& locale, UErrorCode& status);
        };

        class Number : public Formatter {
        public:
            FormattedPlaceholder format(FormattedPlaceholder&& toFormat, FunctionOptions&& options, UErrorCode& status) const override;
            virtual ~Number();

        private:
            friend class NumberFactory;
            friend class StandardFunctions;

            Number(const Locale& loc) : locale(loc), icuFormatter(number::NumberFormatter::withLocale(loc)) {}
            Number(const Locale& loc, bool isInt) : locale(loc), isInteger(isInt), icuFormatter(number::NumberFormatter::withLocale(loc)) {}
            static Number integer(const Locale& loc);

        // These options have their own accessor methods, since they have different default values.
            int32_t maximumFractionDigits(const FunctionOptions& options) const;
            int32_t minimumFractionDigits(const FunctionOptions& options) const;
            int32_t minimumSignificantDigits(const FunctionOptions& options) const;
            int32_t maximumSignificantDigits(const FunctionOptions& options) const;
            int32_t minimumIntegerDigits(const FunctionOptions& options) const;

            bool usePercent(const FunctionOptions& options) const;
            const Locale& locale;
            const bool isInteger = false;
            const number::LocalizedNumberFormatter icuFormatter;
        };

        static number::LocalizedNumberFormatter formatterForOptions(const Number& number,
                                                                    const FunctionOptions& opts,
                                                                    UErrorCode& status);

        class PluralFactory : public SelectorFactory {
        public:
            Selector* createSelector(const Locale& locale, UErrorCode& status) const override;
            virtual ~PluralFactory();

        private:
            friend class IntegerFactory;
            friend class MessageFormatter;

            PluralFactory() {}
            PluralFactory(bool isInt) : isInteger(isInt) {}
            static PluralFactory integer() { return PluralFactory(true);}
            const bool isInteger = false;
        };

        class Plural : public Selector {
        public:
            void selectKey(FormattedPlaceholder&& val,
                           FunctionOptions&& options,
                           const UnicodeString* keys,
                           int32_t keysLen,
                           UnicodeString* prefs,
                           int32_t& prefsLen,
                           UErrorCode& status) const override;
            virtual ~Plural();

        private:
            friend class IntegerFactory;
            friend class PluralFactory;

            // Can't use UPluralType for this since we want to include
            // exact matching as an option
            typedef enum PluralType {
                PLURAL_ORDINAL,
                PLURAL_CARDINAL,
                PLURAL_EXACT
            } PluralType;
            Plural(const Locale& loc, UErrorCode& errorCode);
            Plural(const Locale& loc, bool isInt, UErrorCode& errorCode);
            static Plural integer(const Locale& loc, UErrorCode& errorCode) { return Plural(loc, true, errorCode); }
            PluralType pluralType(const FunctionOptions& opts) const;
            const Locale& locale;
            const bool isInteger = false;
            LocalPointer<StandardFunctions::Number> numberFormatter;
        };

        class TextFactory : public SelectorFactory {
        public:
            Selector* createSelector(const Locale& locale, UErrorCode& status) const override;
            virtual ~TextFactory();
        };

        class TextSelector : public Selector {
        public:
            void selectKey(FormattedPlaceholder&& val,
                           FunctionOptions&& options,
                           const UnicodeString* keys,
                           int32_t keysLen,
                           UnicodeString* prefs,
                           int32_t& prefsLen,
                           UErrorCode& status) const override;
            virtual ~TextSelector();

        private:
            friend class TextFactory;

            // Formatting `value` to a string might require the locale
            const Locale& locale;

            TextSelector(const Locale& l) : locale(l) {}
        };

        // See https://github.com/unicode-org/message-format-wg/blob/main/test/README.md
        class TestFormatFactory : public FormatterFactory {
        public:
            Formatter* createFormatter(const Locale& locale, UErrorCode& status) override;
            TestFormatFactory() {}
            virtual ~TestFormatFactory();
        };

        class TestSelect;

        class TestFormat : public Formatter {
        public:
            FormattedPlaceholder format(FormattedPlaceholder&& toFormat, FunctionOptions&& options, UErrorCode& status) const override;
            virtual ~TestFormat();

        private:
            friend class TestFormatFactory;
            friend class TestSelect;
            TestFormat() {}
            static void testFunctionParameters(const FormattedPlaceholder& arg,
                                               const FunctionOptions& options,
                                               int32_t& decimalPlaces,
                                               bool& failsFormat,
                                               bool& failsSelect,
                                               double& input,
                                               UErrorCode& status);

        };

        // See https://github.com/unicode-org/message-format-wg/blob/main/test/README.md
        class TestSelectFactory : public SelectorFactory {
        public:
            Selector* createSelector(const Locale& locale, UErrorCode& status) const override;
            TestSelectFactory() {}
            virtual ~TestSelectFactory();
        };

        class TestSelect : public Selector {
        public:
            void selectKey(FormattedPlaceholder&& val,
                           FunctionOptions&& options,
                           const UnicodeString* keys,
                           int32_t keysLen,
                           UnicodeString* prefs,
                           int32_t& prefsLen,
                           UErrorCode& status) const override;
            virtual ~TestSelect();

        private:
            friend class TestSelectFactory;
            TestSelect() {}
        };

    };

    extern void formatDateWithDefaults(const Locale& locale, const DateInfo& date, UnicodeString&, UErrorCode& errorCode);
    extern number::FormattedNumber formatNumberWithDefaults(const Locale& locale, double toFormat, UErrorCode& errorCode);
    extern number::FormattedNumber formatNumberWithDefaults(const Locale& locale, int32_t toFormat, UErrorCode& errorCode);
    extern number::FormattedNumber formatNumberWithDefaults(const Locale& locale, int64_t toFormat, UErrorCode& errorCode);
    extern number::FormattedNumber formatNumberWithDefaults(const Locale& locale, StringPiece toFormat, UErrorCode& errorCode);
    extern DateFormat* defaultDateTimeInstance(const Locale&, UErrorCode&);

} // namespace message2

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* #if !UCONFIG_NO_NORMALIZATION */

#endif /* U_SHOW_CPLUSPLUS_API */

#endif // MESSAGEFORMAT2_FUNCTION_REGISTRY_INTERNAL_H

#endif // U_HIDE_DEPRECATED_API
// eof

// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#ifndef MESSAGEFORMAT2_FORMATTABLE_H
#define MESSAGEFORMAT2_FORMATTABLE_H

#if U_SHOW_CPLUSPLUS_API

#if !UCONFIG_NO_NORMALIZATION

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/chariter.h"
#include "unicode/numberformatter.h"
#include "unicode/messageformat2_data_model_names.h"
#include "unicode/smpdtfmt.h"

#ifndef U_HIDE_DEPRECATED_API

#include <map>
#include <variant>

U_NAMESPACE_BEGIN

class Hashtable;
class UVector;

namespace message2 {

    // Formattable
    // ----------

    /**
     * `FormattableObject` is an abstract class that can be implemented in order to define
     * an arbitrary class that can be passed to a custom formatter or selector function.
     * To be passed in such a way, it must be wrapped in a `Formattable` object.
     *
     * @internal ICU 75 technology preview
     * @deprecated This API is for technology preview only.
     */
    class U_I18N_API_CLASS FormattableObject : public UObject {
    public:
        /**
         * Returns an arbitrary string representing the type of this object.
         * It's up to the implementor of this class, as well as the implementors
         * of any custom functions that rely on particular values of this tag
         * corresponding to particular classes that the object contents can be
         * downcast to, to ensure that the type tags are used soundly.
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API virtual const UnicodeString& tag() const = 0;
        /**
         * Destructor.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API virtual ~FormattableObject();
    }; // class FormattableObject

    /**
     * The `DateInfo` struct represents all the information needed to
     * format a date with a time zone. It includes an absolute date and a time zone name,
     * as well as a calendar name. The calendar name is not currently used.
     *
     * @internal ICU 79 technology preview
     * @deprecated This API is for technology preview only.
     */
    struct U_I18N_API DateInfo {
        /**
         * Date in UTC
         *
         * @internal ICU 79 technology preview
         * @deprecated This API is for technology preview only.
         */
        UDate date;
        /**
         * IANA time zone name; "UTC" if UTC; empty string if value is floating
         * The time zone is required in order to format the date/time value
         * (its offset is added to/subtracted from the datestamp in order to
         * produce the formatted date).
         *
         * @internal ICU 79 technology preview
         * @deprecated This API is for technology preview only.
         */
        UnicodeString zoneId;
    };

    /**
     * The `Formattable` class represents a typed value that can be formatted,
     * originating either from a message argument or a literal in the code.
     * ICU's Formattable class is not used in MessageFormat 2 because it's unsafe to copy an
     * icu::Formattable value that contains an object. (See ICU-20275).
     *
     * `Formattable` is immutable (not deeply immutable) and
     * is movable and copyable.
     * (Copying does not do a deep copy when the wrapped value is an array or
     * object. Likewise, while a pointer to a wrapped array or object is `const`,
     * the referents of the pointers may be mutated by other code.)
     *
     * @internal ICU 75 technology preview
     * @deprecated This API is for technology preview only.
     */
    class U_I18N_API_CLASS Formattable : public UObject {
    public:

        /**
         * Gets the data type of this Formattable object.
         * @return    the data type of this Formattable object.
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API UFormattableType getType() const;

        /**
         * Gets the double value of this object. If this object is not of type
         * UFMT_DOUBLE, then the result is undefined and the error code is set.
         *
         * @param status Input/output error code.
         * @return    the double value of this object.
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API double getDouble(UErrorCode& status) const {
            if (U_SUCCESS(status)) {
                if (isDecimal() && getType() == UFMT_DOUBLE) {
                    return (std::get_if<icu::Formattable>(&contents))->getDouble();
                }
                if (std::holds_alternative<double>(contents)) {
                    return *(std::get_if<double>(&contents));
                }
                status = U_ILLEGAL_ARGUMENT_ERROR;
            }
            return 0;
        }

        /**
         * Gets the long value of this object. If this object is not of type
         * UFMT_LONG then the result is undefined and the error code is set.
         *
         * @param status Input/output error code.
         * @return    the long value of this object.
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API int32_t getLong(UErrorCode& status) const {
            if (U_SUCCESS(status)) {
                if (isDecimal() && getType() == UFMT_LONG) {
                    return std::get_if<icu::Formattable>(&contents)->getLong();
                }
                if (std::holds_alternative<int64_t>(contents)) {
                    return static_cast<int32_t>(*(std::get_if<int64_t>(&contents)));
                }
                status = U_ILLEGAL_ARGUMENT_ERROR;
            }
            return 0;
        }

        /**
         * Gets the int64 value of this object. If this object is not of type
         * kInt64 then the result is undefined and the error code is set.
         * If conversion to int64 is desired, call getInt64()
         *
         * @param status Input/output error code.
         * @return    the int64 value of this object.
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API int64_t getInt64Value(UErrorCode& status) const {
            if (U_SUCCESS(status)) {
                if (isDecimal() && getType() == UFMT_INT64) {
                    return std::get_if<icu::Formattable>(&contents)->getInt64();
                }
                if (std::holds_alternative<int64_t>(contents)) {
                    return *(std::get_if<int64_t>(&contents));
                }
                status = U_ILLEGAL_ARGUMENT_ERROR;
            }
            return 0;
        }

        /**
         * Gets the int64 value of this object. If this object is of a numeric
         * type and the magnitude is too large to fit in an int64, then
         * the maximum or minimum int64 value, as appropriate, is returned
         * and the status is set to U_INVALID_FORMAT_ERROR.  If the
         * magnitude fits in an int64, then a casting conversion is
         * performed, with truncation of any fractional part. If this object is
         * not a numeric type, then 0 is returned and
         * the status is set to U_INVALID_FORMAT_ERROR.
         * @param status the error code
         * @return    the int64 value of this object.
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API int64_t getInt64(UErrorCode& status) const;
        /**
         * Gets the string value of this object. If this object is not of type
         * kString then the result is undefined and the error code is set.
         *
         * @param status Input/output error code.
         * @return          A reference to the string value of this object.
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API const UnicodeString& getString(UErrorCode& status) const {
            if (U_SUCCESS(status)) {
                if (std::holds_alternative<UnicodeString>(contents)) {
                    return *std::get_if<UnicodeString>(&contents);
                }
                status = U_ILLEGAL_ARGUMENT_ERROR;
            }
            return bogusString;
        }

        /**
         * Gets the struct representing the date value of this object.
         * If this object is not of type kDate then the result is
         * undefined and the error code is set.
         *
         * @param status Input/output error code.
         * @return   A non-owned pointer to a DateInfo object
         *           representing the underlying date of this object.
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API const DateInfo* getDate(UErrorCode& status) const {
            if (U_SUCCESS(status)) {
                if (isDate()) {
                    return std::get_if<DateInfo>(&contents);
                }
                status = U_ILLEGAL_ARGUMENT_ERROR;
            }
            return nullptr;
        }

        /**
         * Returns true if the data type of this Formattable object
         * is kDouble
         * @return true if this is a pure numeric object
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API UBool isNumeric() const { return (getType() == UFMT_DOUBLE || getType() == UFMT_LONG || getType() == UFMT_INT64); }

        /**
         * Gets the array value and count of this object. If this object
         * is not of type kArray then the result is undefined and the error code is set.
         *
         * @param count    fill-in with the count of this object.
         * @param status Input/output error code.
         * @return         the array value of this object.
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API const Formattable* getArray(int32_t& count, UErrorCode& status) const;

        /**
         * Returns a pointer to the FormattableObject contained within this
         * formattable, or if this object does not contain a FormattableObject,
         * returns nullptr and sets the error code.
         *
         * @param status Input/output error code.
         * @return a FormattableObject pointer, or nullptr
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API const FormattableObject* getObject(UErrorCode& status) const {
            if (U_SUCCESS(status)) {
                // Can't return a reference since FormattableObject
                // is an abstract class
                if (getType() == UFMT_OBJECT) {
                    auto result = std::get_if<const FormattableObject*>(&contents);
                    // It would be better to U_ASSERT(result != nullptr) here, but
                    // this is a public header file so we can't include uassert.h
                    if (result != nullptr) {
                        return *result;
                    }
                }
                status = U_ILLEGAL_ARGUMENT_ERROR;
            }
            return nullptr;
        }
        /**
         * Non-member swap function.
         * @param f1 will get f2's contents
         * @param f2 will get f1's contents
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API friend inline void swap(Formattable& f1, Formattable& f2) noexcept {
            using std::swap;

            swap(f1.contents, f2.contents);
        }
        /**
         * Copy constructor.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API Formattable(const Formattable&);
        /**
         * Assignment operator
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API Formattable& operator=(Formattable) noexcept;
        /**
         * Default constructor. Leaves the Formattable in a
         * valid but undefined state.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API Formattable() : contents(0.0) {}
        /**
         * String constructor.
         *
         * @param s A string to wrap as a Formattable.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API Formattable(const UnicodeString& s) : contents(s) {}
        /**
         * Double constructor.
         *
         * @param d A double value to wrap as a Formattable.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API Formattable(double d) : contents(d) {}
        /**
         * Int64 constructor.
         *
         * @param i An int64 value to wrap as a Formattable.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API Formattable(int64_t i) : contents(i) {}
        /**
         * Date constructor.
         *
         * @param d A DateInfo struct representing a date,
         *          to wrap as a Formattable.
         *          Passed by move
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API Formattable(DateInfo&& d) : contents(std::move(d)) {}
        /**
         * Creates a Formattable object of an appropriate numeric type from a
         * a decimal number in string form.  The Formattable will retain the
         * full precision of the input in decimal format, even when it exceeds
         * what can be represented by a double or int64_t.
         *
         * @param number  the unformatted (not localized) string representation
         *                     of the Decimal number.
         * @param status  the error code.  Possible errors include U_INVALID_FORMAT_ERROR
         *                if the format of the string does not conform to that of a
         *                decimal number.
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API static Formattable forDecimal(std::string_view number, UErrorCode& status);
        /**
         * Array constructor.
         *
         * @param arr An array of Formattables, which is adopted.
         * @param len The length of the array.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API Formattable(const Formattable* arr, int32_t len) : contents(std::pair(arr, len)) {}
        /**
         * Object constructor.
         *
         * @param obj A FormattableObject (not adopted).
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API Formattable(const FormattableObject* obj) : contents(obj) {}
        /**
         * Destructor.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API virtual ~Formattable();
        /**
         * Converts the Formattable object to an ICU Formattable object.
         * If this has type UFMT_OBJECT or kArray, then `status` is set to
         * U_ILLEGAL_ARGUMENT_ERROR.
         *
         * @param status Input/output error code.
         * @return An icu::Formattable value with the same value as this.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        U_I18N_API icu::Formattable asICUFormattable(UErrorCode& status) const;
    private:

        std::variant<double,
                     int64_t,
                     UnicodeString,
                     icu::Formattable, // represents a Decimal
                     DateInfo,
                     const FormattableObject*,
                     std::pair<const Formattable*, int32_t>> contents;
        UnicodeString bogusString; // :((((

        UBool isDecimal() const {
            return std::holds_alternative<icu::Formattable>(contents);
        }
        UBool isDate() const {
            return std::holds_alternative<DateInfo>(contents);
        }
    }; // class Formattable

/**
 * Internal use only, but has to be included here as part of the implementation
 * of the header-only `FunctionOptions::getOptions()` method
 *
 *  A `ResolvedFunctionOption` represents the result of evaluating
 * a single named function option. It pairs the given name with the `Formattable`
 * value resulting from evaluating the option's value.
 *
 * `ResolvedFunctionOption` is immutable, movable, and copyable.
 *
 * @internal ICU 75 technology preview
 * @deprecated This API is for technology preview only.
 */
#ifndef U_IN_DOXYGEN
class FunctionValue;
class U_I18N_API_CLASS ResolvedFunctionOption : public UObject {
  private:
    friend class FunctionOptions;

    /* const */ UnicodeString name;
    // owned by the global environment
    const FunctionValue* value;
    // True if this option is the product of merging two
    // option maps together, and this option came from the
    // first argument (the "older" options map).
    bool thisWasMerged = false;

  public:
      U_I18N_API const UnicodeString& getName() const { return name; }
      U_I18N_API const FunctionValue& getValue() const { return *value; }
      U_I18N_API bool wasMerged() const { return thisWasMerged; }
      U_I18N_API ResolvedFunctionOption(const UnicodeString& n, const FunctionValue& f, bool b);
      U_I18N_API ResolvedFunctionOption() {}
      U_I18N_API ResolvedFunctionOption(ResolvedFunctionOption&&);
      U_I18N_API ResolvedFunctionOption& operator=(ResolvedFunctionOption&& other) = default;
      U_I18N_API ResolvedFunctionOption& operator=(const ResolvedFunctionOption& other) = default;
      U_I18N_API ResolvedFunctionOption(const ResolvedFunctionOption&) = default;
      U_I18N_API virtual ~ResolvedFunctionOption();
}; // class ResolvedFunctionOption
#endif

/**
 * Mapping from option names to `message2::Formattable` objects, obtained
 * by calling `getOptions()` on a `FunctionOptions` object.
 *
 * @internal ICU 75 technology preview
 * @deprecated This API is for technology preview only.
 */
using FunctionOptionsMap = std::map<UnicodeString, const message2::FunctionValue*>;

/**
 * Structure encapsulating named options passed to a custom selector or formatter.
 *
 * This class is immutable, movable and copyable.
 *
 * @internal ICU 75 technology preview
 * @deprecated This API is for technology preview only.
 */
class U_I18N_API FunctionOptions : public UObject {
 public:
    /**
     * Returns a map of all name-value pairs provided as options to this function.
     * The syntactic order of options is not guaranteed to
     * be preserved.
     *
     * @return           A map from strings to FunctionValue objects representing
     *                   the results of resolving each option value.
     *
     * @internal ICU 75 technology preview
     * @deprecated This API is for technology preview only.
     */
     FunctionOptionsMap getOptions() const {
        FunctionOptionsMap result;
        for (int32_t i = 0; i < functionOptionsLen; i++) {
            ResolvedFunctionOption& opt = options[i];
            result[opt.getName()] = &opt.getValue();
        }
        return result;
    }
    /**
     * Returns a new FunctionOptions object containing all the key-value
     * pairs from `this` and `other`. When `this` and `other` define options with
     * the same name, `this` takes preference.
     *
     * @return The result of merging `this` and `other`.
     *
     * @internal ICU 79 technology preview
     * @deprecated This API is for technology preview only.
     */
     FunctionOptions mergeOptions(const FunctionOptions& other, UErrorCode&) const;
    /**
     * Default constructor.
     * Returns an empty mapping.
     *
     * @internal ICU 75 technology preview
     * @deprecated This API is for technology preview only.
     */
     FunctionOptions() { options = nullptr; }
    /**
     * Destructor.
     *
     * @internal ICU 75 technology preview
     * @deprecated This API is for technology preview only.
     */
     virtual ~FunctionOptions();
    /**
     * Non-member swap function.
     * @param f1 will get f2's contents
     * @param f2 will get f1's contents
     *
     * @internal ICU 75 technology preview
     * @deprecated This API is for technology preview only.
     */
     friend inline void swap(FunctionOptions& f1, FunctionOptions& f2) noexcept {
        using std::swap;

        if (f1.bogus || f2.bogus) {
            f1.bogus = f2.bogus = true;
            return;
        }
        swap(f1.options, f2.options);
        swap(f1.functionOptionsLen, f2.functionOptionsLen);
    }
    /**
     * Assignment operator
     *
     * @internal ICU 75 technology preview
     * @deprecated This API is for technology preview only.
     */
     FunctionOptions& operator=(FunctionOptions) noexcept;
    /**
     * Copy constructor.
     *
     * @internal ICU 75 technology preview
     * @deprecated This API is for technology preview only.
     */
     FunctionOptions(const FunctionOptions&);
 private:
    friend class MessageFormatter;
    friend class StandardFunctions;

    explicit FunctionOptions(UVector&&, UErrorCode&);

    const ResolvedFunctionOption* getResolvedFunctionOptions(int32_t& len) const;
    const FunctionValue* getFunctionOption(const std::u16string_view, UErrorCode&) const;
    // Returns empty string if option doesn't exist
    UnicodeString getStringFunctionOption(const std::u16string_view) const;
    UBool wasSetFromLiteral(const std::u16string_view) const;
    // Sets error code if option doesn't exist
    UnicodeString getStringFunctionOption(const std::u16string_view, UErrorCode&) const;
    int32_t optionsCount() const { return functionOptionsLen; }

    bool bogus = false; // Used in case a copy fails
    // Named options passed to functions
    // This is not a Hashtable in order to make it possible for code in a public header file
    // to construct a std::map from it, on-the-fly. Otherwise, it would be impossible to put
    // that code in the header because it would have to call internal Hashtable methods.
    ResolvedFunctionOption* options;
    int32_t functionOptionsLen = 0;
}; // class FunctionOptions

    /**
     * Not yet implemented: The result of a message formatting operation. Based on
     * ICU4J's FormattedMessage.java.
     *
     * The class will contain information allowing the result to be viewed as a string,
     * iterator, etc. (TBD)
     *
     * @internal ICU 75 technology preview
     * @deprecated This API is for technology preview only.
     */
    class U_I18N_API FormattedMessage : public icu::FormattedValue {
    public:
        /**
         * Not yet implemented.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for ICU internal use only.
         */
        FormattedMessage(UErrorCode& status) {
            if (U_SUCCESS(status)) {
                status = U_UNSUPPORTED_ERROR;
            }
        }
        /**
         * Not yet implemented.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for ICU internal use only.
         */
        int32_t length(UErrorCode& status) const {
            if (U_SUCCESS(status)) {
                status = U_UNSUPPORTED_ERROR;
            }
            return -1;
        }
        /**
         * Not yet implemented.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for ICU internal use only.
         */
        char16_t charAt(int32_t index, UErrorCode& status) const {
            (void) index;
            if (U_SUCCESS(status)) {
                status = U_UNSUPPORTED_ERROR;
            }
            return 0;
        }
        /**
         * Not yet implemented.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for ICU internal use only.
         */
        StringPiece subSequence(int32_t start, int32_t end, UErrorCode& status) const {
            (void) start;
            (void) end;
            if (U_SUCCESS(status)) {
                status = U_UNSUPPORTED_ERROR;
            }
            return "";
        }
        /**
         * Not yet implemented.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for ICU internal use only.
         */
        UnicodeString toString(UErrorCode& status) const override {
            if (U_SUCCESS(status)) {
                status = U_UNSUPPORTED_ERROR;
            }
            return {};
        }
        /**
         * Not yet implemented.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for ICU internal use only.
         */
        UnicodeString toTempString(UErrorCode& status) const override {
            if (U_SUCCESS(status)) {
                status = U_UNSUPPORTED_ERROR;
            }
            return {};
        }
        /**
         * Not yet implemented.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for ICU internal use only.
         */
        Appendable& appendTo(Appendable& appendable, UErrorCode& status) const override {
            if (U_SUCCESS(status)) {
                status = U_UNSUPPORTED_ERROR;
            }
            return appendable;
        }
        /**
         * Not yet implemented.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for ICU internal use only.
         */
        UBool nextPosition(ConstrainedFieldPosition& cfpos, UErrorCode& status) const override {
            (void) cfpos;
            if (U_SUCCESS(status)) {
                status = U_UNSUPPORTED_ERROR;
            }
            return false;
        }
        /**
         * Not yet implemented.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for ICU internal use only.
         */
        CharacterIterator* toCharacterIterator(UErrorCode& status) {
            if (U_SUCCESS(status)) {
                status = U_UNSUPPORTED_ERROR;
            }
            return nullptr;
        }
        /**
         * Destructor.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for ICU internal use only.
         */
        virtual ~FormattedMessage();
    }; // class FormattedMessage

} // namespace message2

U_NAMESPACE_END

#endif // U_HIDE_DEPRECATED_API

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* #if !UCONFIG_NO_NORMALIZATION */

#endif /* U_SHOW_CPLUSPLUS_API */

#endif // MESSAGEFORMAT2_FORMATTABLE_H

// eof

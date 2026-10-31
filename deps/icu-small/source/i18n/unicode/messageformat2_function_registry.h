// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#ifndef MESSAGEFORMAT2_FUNCTION_REGISTRY_H
#define MESSAGEFORMAT2_FUNCTION_REGISTRY_H

#if U_SHOW_CPLUSPLUS_API

#if !UCONFIG_NO_NORMALIZATION

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/messageformat2_data_model_names.h"
#include "unicode/messageformat2_formattable.h"
#include "unicode/ubidi.h"

#ifndef U_HIDE_DEPRECATED_API

#include <map>

U_NAMESPACE_BEGIN

class Hashtable;
class UVector;

namespace message2 {

    using namespace data_model;

    class Function;

    /**
     * Defines mappings from names of formatters and selectors to functions implementing them.
     * The required set of formatter and selector functions is defined in the spec. Users can
     * also define custom formatter and selector functions.
     *
     * `MFFunctionRegistry` is immutable and movable. It is not copyable.
     *
     * @internal ICU 75 technology preview
     * @deprecated This API is for technology preview only.
     */
    class U_I18N_API MFFunctionRegistry : public UObject {
    private:

        using FunctionMap = Hashtable; // Map from function names to FunctionFactory*

    public:
        /**
         * Looks up a function by the name of the function.
         * Returns the result by pointer rather than by reference since it can fail.
         *
         * @param functionName Name of the desired function.
         * @return A pointer to the function registered under `functionName`, or null
         *         if no function was registered under that name. The pointer is not owned
         *         by the caller.
         *
         * @internal ICU 79 technology preview
         * @deprecated This API is for technology preview only.
         */
        const Function* getFunction(const FunctionName& functionName) const;
        /**
         * Looks up a function by a type tag. This method gets the name of the default formatter registered
         * for that type. If no formatter was explicitly registered for this type, it returns false.
         *
         * @param formatterType Type tag for the desired `FormattableObject` type to be formatted.
         * @param name Output parameter; initialized to the name of the default formatter for `formatterType`
         *        if one has been registered. Its value is undefined otherwise.
         * @return True if and only if the function registry contains a default formatter for `formatterType`.
         *         If the return value is false, then the value of `name` is undefined.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        UBool getDefaultFormatterNameByType(const UnicodeString& formatterType, FunctionName& name) const;
        /**
         * The mutable Builder class allows each formatter and selector factory
         * to be initialized separately; calling its `build()` method yields an
         * immutable MFFunctionRegistry object.
         *
         * Builder is not copyable or movable.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        class U_I18N_API Builder : public UObject {
        private:
            // Must use raw pointers to avoid instantiating `LocalPointer` on an internal type
            FunctionMap* functions;
            // Mapping from strings (type tags) to FunctionNames
            Hashtable* formattersByType = nullptr;

            // Do not define copy constructor/assignment operator
            Builder& operator=(const Builder&) = delete;
            Builder(const Builder&) = delete;

        public:
            /*
              Notes about `adoptFormatter()`'s type signature:

              Alternative considered: take a non-owned FormatterFactory*
              This is unsafe.

              Alternative considered: take a FormatterFactory&
                 This requires getFormatter() to cast the reference to a pointer,
                 as it must return an unowned FormatterFactory* since it can fail.
                 That is also unsafe, since the caller could delete the pointer.

              The "TemperatureFormatter" test from the previous ICU4J version doesn't work now,
              as it only works if the `formatterFactory` argument is non-owned.
              If registering a non-owned FormatterFactory is desirable, this could
              be re-thought.
              */
            /**
             * Registers a function to a given name.
             *
             * @param functionName Name of the formatter being registered.
             * @param function A pointer to a Function object.
             *                 This argument is adopted.
             *                 Functions with the same name as standard functions will
             *                 override the standard functions.
             * @param errorCode Input/output error code
             * @return A reference to the builder.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            Builder& adoptFunction(const data_model::FunctionName& functionName,
                                   Function* function,
                                   UErrorCode& errorCode);
            /**
             * Registers a formatter factory to a given type tag.
             * (See `FormattableObject` for details on type tags.)
             *
             * @param type Tag for objects to be formatted with this formatter.
             * @param functionName A reference to the name of the function to use for
             *        creating formatters for `formatterType` objects.
             * @param errorCode Input/output error code
             * @return A reference to the builder.
             *
             * @internal ICU 75 technology preview
             * @deprecated This API is for technology preview only.
             */
            Builder& setDefaultFormatterNameByType(const UnicodeString& type,
                                                   const data_model::FunctionName& functionName,
                                                   UErrorCode& errorCode);
            /**
             * Creates an immutable `MFFunctionRegistry` object with the selectors and formatters
             * that were previously registered. The builder cannot be used after this call.
             * The `build()` method is destructive to avoid the need for a deep copy of the
             * `FormatterFactory` and `SelectorFactory` objects (this would be necessary because
             * `FormatterFactory` can have mutable state), which in turn would require implementors
             * of those interfaces to implement a `clone()` method.
             *
             * @return The new MFFunctionRegistry
             *
             * @internal ICU 75 technology preview
             * @deprecated This API is for technology preview only.
             */
            MFFunctionRegistry build();
            /**
             * Default constructor.
             * Returns a Builder with no functions registered.
             *
             * @param errorCode Input/output error code
             *
             * @internal ICU 75 technology preview
             * @deprecated This API is for technology preview only.
             */
            Builder(UErrorCode& errorCode);
            /**
             * Destructor.
             *
             * @internal ICU 75 technology preview
             * @deprecated This API is for technology preview only.
             */
            virtual ~Builder();
        }; // class MFFunctionRegistry::Builder

        /**
         * Move assignment operator:
         * The source MFFunctionRegistry will be left in a valid but undefined state.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        MFFunctionRegistry& operator=(MFFunctionRegistry&&) noexcept;
        /**
         * Move constructor:
         * The source MFFunctionRegistry will be left in a valid but undefined state.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        MFFunctionRegistry(MFFunctionRegistry&& other) { *this = std::move(other); }
        /**
         * Destructor.
         *
         * @internal ICU 75 technology preview
         * @deprecated This API is for technology preview only.
         */
        virtual ~MFFunctionRegistry();

	/**
	 * Get the standard function registry.
	 * @internal ICU 79 technology preview
	 * @deprecated This API is for technology preview only.
	 * @param status Input/output error code
	 * @return A const alias to the registry, or nullptr on error
	 */
	static const MFFunctionRegistry* getStandardFunctionsRegistry(UErrorCode &status);
	
    private:
        friend class MessageContext;
        friend class MessageFormatter;
	friend class MF2RegistrySingleton;

        // Do not define copy constructor or copy assignment operator
        MFFunctionRegistry& operator=(const MFFunctionRegistry&) = delete;
        MFFunctionRegistry(const MFFunctionRegistry&) = delete;

        MFFunctionRegistry(FunctionMap*, Hashtable*);

        MFFunctionRegistry() {}

        // Debugging; should only be called on a function registry with
        // all the standard functions registered
        void checkFunction(std::u16string_view) const;
        void checkStandard() const;

        bool hasFunction(const data_model::FunctionName& f) const;
        void cleanup() noexcept;

        // Must use raw pointers to avoid instantiating `LocalPointer` on an internal type
        FunctionMap* functions = nullptr;
        // Mapping from strings (type tags) to FunctionNames
        Hashtable* formattersByType = nullptr;
    }; // class MFFunctionRegistry

    /**
     * Class implementing data from contextual options.
     * See https://github.com/unicode-org/message-format-wg/pull/846
     *
     * @internal ICU 79 technology preview
     * @deprecated This API is for technology preview only.
     */
    class U_I18N_API_CLASS FunctionContext : public UObject {
        public:
            /**
             * Returns the locale from this context.
             *
             * @return Locale the context was created with.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API const Locale& getLocale() const { return locale; }
            /**
             * Returns the text direction from this context.
             *
             * @return A UMFBidiOption indicating the text direction.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API UMFBidiOption getDirection() const { return dir; }
            /**
             * Returns the ID from this context.
             *
             * @return A string to be used in formatting to parts.
             *         (Formatting to parts is not yet implemented.)
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API const UnicodeString& getID() const { return id; }

            /**
             * Returns the original called function name from this context.
             *
             * @return The function name, as registered, used for this function
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API const FunctionName& getCalledFunctionName() const { return calledFunction; }

            /**
             * Returns a new context with the specified locale.
             *
             * @return a new locale
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API FunctionContext withLocale(const Locale& loc) const {
                return FunctionContext(loc, getDirection(), getID(), getCalledFunctionName());
            }

        private:
            friend class MessageFormatter;

            Locale locale;
            UMFBidiOption dir;
            UnicodeString id;
            const FunctionName& calledFunction;

            FunctionContext(const Locale& loc, UMFBidiOption d, UnicodeString i, const FunctionName& calledFn)
                : locale(loc), dir(d), id(i), calledFunction(calledFn) {}
    }; // class FunctionContext

    class FunctionValue;

    /**
     * Interface that function handler classes must implement.
     *
     * @internal ICU 79 technology preview
     * @deprecated This API is for technology preview only.
     */
    class U_I18N_API Function : public UObject {
        public:
            /**
             * Calls this Function on a FunctionValue operand and its FunctionOptions options,
             * returning a LocalPointer to a FunctionValue.
             *
             * @param context The context of this function, based on its contextual options
             * @param operand The unnamed argument to the function.
             * @param options Resolved options for this function.
             * @param status Input/output error code
             * @return The function value that is the result of calling this function on
             *         the arguments.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            virtual LocalPointer<FunctionValue> call(const FunctionContext& context,
                                                     const FunctionValue& operand,
                                                     const FunctionOptions& options,
                                                     UErrorCode& status) const = 0;
            /**
             * Destructor.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            virtual ~Function();
    }; // class Function

    /**
     * Type representing argument and return values for custom functions.
     * It encapsulates an operand and resolved options, and can be extended with
     * additional state.
     * Adding a new custom function requires adding a new class that
     * implements this interface.
     *
     * FunctionValues are assumed to be immutable (the call() method on
     * Function takes a const FunctionValue&, and the formatToString()
     * and selectKeys() methods are const.) Feedback on whether internal
     * mutable state in classes implementing FunctionValue is welcomed
     * during the Technology Preview period.
     *
     * @internal ICU 79 technology preview
     * @deprecated This API is for technology preview only.
     */
    class U_I18N_API_CLASS FunctionValue : public UObject {
        public:
            /**
             * Returns the string representation of this value. The default
             * method signals an error. Must be overridden by classes
             * implementing values that support formatting.
             *
             * @param status Input/output error code
             * @return A string.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API virtual UnicodeString formatToString(UErrorCode& status) const {
                if (U_SUCCESS(status)) {
                    status = U_MF_FORMATTING_ERROR;
                }
                return {};
            }
            /**
             * Returns the Formattable operand that was used to construct
             * this value. The operand may be obtained from calling getOperand()
             * on the input FunctionValue, or it may be constructed separately.
             *
             * @return A reference to a message2::Formattable object.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API virtual const Formattable& unwrap() const { return innerValue; }
            /**
             * Returns a reference to the resolved options for this value.
             *
             * @return A reference to the resolved options for this value.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API virtual const FunctionOptions& getResolvedOptions() const { return opts; }
            /**
             * Returns the directionality of this value, i.e. the directionality
             * that its formatted result should have.
             *
             * @return A UBiDiDirection indicating the directionality that
             *         the formatted result of this value should have.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API virtual UMFDirectionality getDirection() const { return dir; }
            /**
             * Returns the directionality that this value was annotated with.
             *
             * This is distinct from the directionality of the formatted text.
             * See the description of the "Default Bidi Strategy",
             * https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#handling-bidirectional-text
             * for further context.
             *
             * @return A UMFBidiOption indicating the directionality that
             *         this value was annotated with.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API virtual UMFBidiOption getDirectionAnnotation() const { return inputDir; }
            /**
             * Returns true if this value supports selection. The default method
             * returns false. The method must be overridden for values that support
             * selection.
             *
             * @return True iff this value supports selection.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API virtual UBool isSelectable() const {
                // In the future, this function could return a capability
                // indicating whether this function can format, select, or both.
                return false;
            }
            /**
             * Returns true if this value represents a null operand, that is,
             * the absence of an argument. This method should not be overridden.
             * It can be called in order to check whether the argument is present.
             * Some functions may be nullary (they may work with no arguments).
             *
             * @return True iff this value represents an absent operand.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API virtual UBool isNullOperand() const { return false; }
            /**
             * Compares this value to an array of keys, and returns an array of matching
             * keys sorted by preference. The default implementation of this method
             * signals an error. It should be overridden for value classes that support
             * selection.
             *
             * @param keys An array of strings to compare to the input.
             * @param keysLen The length of `keys`.
             * @param prefs An array of indices into `keys`.
             *        The initial contents of
             *        the array is undefined. `selectKey()` should set the contents
             *        of `prefs` to a subset of the indices in `keys`,
             *        with the best match placed at the lowest index in `prefs`.
             * @param prefsLen A reference that `selectKey()` should set to the length of `prefs`,
             *        which must be less than or equal to `keysLen`.
             * @param status    Input/output error code.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API virtual void selectKeys(const UnicodeString* keys,
                                    int32_t keysLen,
                                    int32_t* prefs,
                                    int32_t& prefsLen,
                                    UErrorCode& status) const {
                (void) keys;
                (void) keysLen;
                (void) prefs;
                (void) prefsLen;
                if (U_SUCCESS(status)) {
                    status = U_MF_SELECTOR_ERROR;
                }
            }
            /**
             * Returns the name of the function that constructed this value.
             *
             * @returns A string representing a function name. The string does
             *          not include a leading ':'.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API virtual const FunctionName& getFunctionName() const { return functionName; }
            /**
             * Returns a fallback string that can be used as output
             * if processing this function results in an error.
             *
             * @returns A string determined by the creator of this FunctionValue.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API virtual const UnicodeString& getFallback() const { return fallback; }
            /**
             * Destructor.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            U_I18N_API virtual ~FunctionValue();
         protected:
            /**
             * Computed result of the function invocation that
             * returned this FunctionValue. This may simply be the
             * operand, or may be a value computed from the operand.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            Formattable innerValue;
            /**
             * Resolved options attached to this value.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            FunctionOptions opts;
            /**
             * The name of the function that constructed this FunctionValue.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            FunctionName functionName;
            /**
             * Fallback string that can be used if a later function encounters
             * an error when processing this FunctionValue.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            UnicodeString fallback;
            /**
             * Locale from u:locale option.
             * Must be set from function context.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            Locale locale;
            /**
             * Directionality of formatted result.
             * Defaults to U_MF_DIRECTIONALITY_UNKNOWN if not set
             * by the subclass's constructor.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            UMFDirectionality dir = U_MF_DIRECTIONALITY_UNKNOWN;
            /**
             * Input directionality from u:dir option.
             * Defaults to U_MF_BIDI_OPTION_INHERIT if not set
             * by the subclass's constructor.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            UMFBidiOption inputDir = U_MF_BIDI_OPTION_INHERIT;
        private:
            friend class FunctionOptions;

            // Should only be overridden by BaseValue
            /**
             * Returns true iff this FunctionValue was created directly or indirectly
             * from a literal.
             * This method should not be overridden. It is overridden by an internal class
             * in the message formatter.
             * It is used to implement the MessageFormat specification for the `select`
             * option of `:number` and `:integer`.
             *
             * @returns A boolean.
             *
             * @internal ICU 79 technology preview
             * @deprecated This API is for technology preview only.
             */
            virtual UBool wasCreatedFromLiteral() const { return false; }
    }; // class FunctionValue

} // namespace message2

U_NAMESPACE_END

#endif // U_HIDE_DEPRECATED_API

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* #if !UCONFIG_NO_NORMALIZATION */

#endif /* U_SHOW_CPLUSPLUS_API */

#endif // MESSAGEFORMAT2_FUNCTION_REGISTRY_H

// eof

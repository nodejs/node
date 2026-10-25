// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#ifndef U_HIDE_DEPRECATED_API

#ifndef MESSAGEFORMAT2_EVALUATION_H
#define MESSAGEFORMAT2_EVALUATION_H

#if U_SHOW_CPLUSPLUS_API

/**
 * \file
 * \brief C++ API: Formats messages using the draft MessageFormat 2.0.
 */
#if !UCONFIG_NO_NORMALIZATION

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/messageformat2_arguments.h"
#include "unicode/messageformat2_data_model.h"
#include "unicode/messageformat2_function_registry.h"
#include "messageformat2_errors.h"

// Auxiliary data structures used during formatting a message

U_NAMESPACE_BEGIN

namespace message2 {

    namespace functions {
    static constexpr std::u16string_view CURRENCY = u"currency";
    static constexpr std::u16string_view DATETIME = u"datetime";
    static constexpr std::u16string_view DATE = u"date";
    static constexpr std::u16string_view TIME = u"time";
    static constexpr std::u16string_view NUMBER = u"number";
    static constexpr std::u16string_view INTEGER = u"integer";
    static constexpr std::u16string_view OFFSET = u"offset";
    static constexpr std::u16string_view PER_CENT = u"percent";
    static constexpr std::u16string_view TEST_FUNCTION = u"test:function";
    static constexpr std::u16string_view TEST_FORMAT = u"test:format";
    static constexpr std::u16string_view TEST_SELECT = u"test:select";
    static constexpr std::u16string_view STRING = u"string";
    }

    using namespace data_model;

    class Closure;
    class Environment;

    // InternalValue represents an intermediate value in the message
    // formatter.
    // It has four possible states:
    // 1. Fallback Value. A fallback value
    // is a string that serves as a replacement for expressions whose evaluation
    // caused an error. Fallback values are not passed to functions.
    // 2. Closure, representing the unevaluated right-hand side of a declaration.
    // 3. Evaluated Value (FunctionValue), representing an evaluated declaration.
    // 4. Indirection (const InternalValue*), representing a shared reference to another
    //    InternalValue. Note that all InternalValues are owned by the global
    //    environment.
    /*
      Example:

      .local $x = {$y}
      .local $z = {1 :number}
      .local $a = {$z}
      {{ {$x} {$z} {$a} }}

      If this message is formatted with no arguments,
      initially, x, z and a are all bound to Closures.
      When the value of x is demanded by the pattern, the contents of x's value
      are updated to a Fallback Value (because its RHS contains an unbound variable).
      When the value of z is demanded, the contents of z's value are updated to
      an Evaluated Value representing the result of :number on the operand.
      When the value of a is demanded, the contents of a's value are updated to
      an Indirection, pointing to z's value.

      Indirections are used so that a FunctionValue can be uniquely owned by an
      InternalValue. Since all InternalValues are owned by the global Environment,
      it's safe to use these non-owned pointers.
     */
    class InternalValue : public UObject {
    public:
        bool isFallback() const { return std::holds_alternative<UnicodeString>(val); }
        bool isNullOperand() const;
        bool isEvaluated() const;
        bool isClosure() const;
        virtual bool isSelectable() const;

        Closure& asClosure() {
            U_ASSERT(isClosure());
            return **std::get_if<LocalPointer<Closure>>(&val);
        }
        const FunctionValue* getValue(UErrorCode& status) const;
        FunctionValue* wrapInVariableValue(UErrorCode& status) const;

        UnicodeString asFallback() const { return fallbackString; }

        static LocalPointer<InternalValue> null(UErrorCode& status);
        static LocalPointer<InternalValue> fallback(const UnicodeString& s, UErrorCode& status);
        // Adopts `c`
        static InternalValue closure(Closure* c, const UnicodeString& s);

        // Updates the mutable contents of this InternalValue
        void update(InternalValue&);
        void update(LocalPointer<FunctionValue>);
        void update(const UnicodeString&);

        InternalValue() : val(UnicodeString()) {}
        explicit InternalValue(FunctionValue* v, const UnicodeString& fb);
        InternalValue& operator=(InternalValue&&);
        InternalValue(InternalValue&&);
        virtual ~InternalValue();
    private:
        UnicodeString fallbackString;
        std::variant<UnicodeString, // Fallback value
                     LocalPointer<Closure>, // Unevaluated thunk
                     LocalPointer<FunctionValue>, // Evaluated value
                     const InternalValue*> val; // Indirection to another value -- Not owned
        // Null operand constructor
        explicit InternalValue(UErrorCode& status);
        // Fallback constructor
        explicit InternalValue(const UnicodeString& fb)
            : fallbackString(fb), val(fb) {}
        // Closure (unevaluated) constructor
        explicit InternalValue(Closure* c, UnicodeString fallbackStr)
            : fallbackString(fallbackStr), val(LocalPointer<Closure>(c)) {}
        bool isIndirection() const;
    }; // class InternalValue


    // A BaseValue wraps a literal value or argument value so it can be used
    // in a context that expects a FunctionValue.
    class BaseValue : public FunctionValue {
        public:
            static BaseValue* create(const Locale&, const UnicodeString&, const Formattable&, bool, UErrorCode&);
            // Apply default formatters to the argument value
            UnicodeString formatToString(UErrorCode&) const override;
            UBool isSelectable() const override { return true; }
            UBool wasCreatedFromLiteral() const override { return fromLiteral; }
            BaseValue() {}
            BaseValue(BaseValue&&);
            BaseValue& operator=(BaseValue&&) noexcept;
       private:
            Locale locale;
            bool fromLiteral = false;

            BaseValue(const Locale&, const UnicodeString&, const Formattable&, bool);
    }; // class BaseValue

    // A NullValue represents the absence of an argument.
    class NullValue : public FunctionValue {
        public:
            virtual UBool isNullOperand() const { return true; }
    }; // class NullValue

    // A VariableValue wraps another FunctionValue and its sole purpose
    // is to override the wasCreatedFromLiteral() method to always return false.
    // This makes it easy to implement .local $foo = {exact}: the RHS returns a BaseValue
    // such that wasCreatedFromLiteral() is true, but then we can wrap it in a VariableValue,
    // which will always return false for this method.
    class VariableValue : public FunctionValue {
        public:
            static VariableValue* create(const FunctionValue*, UErrorCode&);
            UBool wasCreatedFromLiteral() const override { return false; }
            UnicodeString formatToString(UErrorCode& status) const override { return underlyingValue->formatToString(status); }
            const Formattable& unwrap() const override { return underlyingValue->unwrap(); }
            const FunctionOptions& getResolvedOptions() const override { return underlyingValue->getResolvedOptions(); }
            UMFDirectionality getDirection() const override { return underlyingValue->getDirection(); }
            UMFBidiOption getDirectionAnnotation() const override { return underlyingValue->getDirectionAnnotation(); }
            UBool isSelectable() const override { return underlyingValue->isSelectable(); }
            UBool isNullOperand() const override { return underlyingValue->isNullOperand(); }
            void selectKeys(const UnicodeString* keys,
                            int32_t keysLen,
                            int32_t* prefs,
                            int32_t& prefsLen,
                            UErrorCode& status) const override { return underlyingValue->selectKeys(keys, keysLen, prefs, prefsLen, status); }
            const UnicodeString& getFunctionName() const override { return underlyingValue->getFunctionName(); }
            const UnicodeString& getFallback() const override { return underlyingValue->getFallback(); }
            VariableValue() {}
            virtual ~VariableValue();
            VariableValue(VariableValue&&);
            VariableValue& operator=(VariableValue&&) noexcept;
        private:
            const FunctionValue* underlyingValue;

            VariableValue(const FunctionValue*);
    }; // class VariableValue

    // PrioritizedVariant

    // For how this class is used, see the references to (integer, variant) tuples
    // in https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#pattern-selection
    class PrioritizedVariant : public UObject {
    public:
        PrioritizedVariant() = default;
        PrioritizedVariant(PrioritizedVariant&&) = default;
        PrioritizedVariant& operator=(PrioritizedVariant&&) noexcept = default;
        UBool operator<(const PrioritizedVariant&) const;
        int32_t priority;
        /* const */ SelectorKeys keys;
        /* const */ Pattern pat;
        PrioritizedVariant(uint32_t p,
                           const SelectorKeys& k,
                           const Pattern& pattern) noexcept : priority(p), keys(k), pat(pattern) {}
        virtual ~PrioritizedVariant();
    }; // class PrioritizedVariant

    static inline int32_t comparePrioritizedVariants(UElement left, UElement right) {
        const PrioritizedVariant& tuple1 = *(static_cast<const PrioritizedVariant*>(left.pointer));
        const PrioritizedVariant& tuple2 = *(static_cast<const PrioritizedVariant*>(right.pointer));
        if (tuple1 < tuple2) {
            return -1;
        }
        if (tuple1.priority == tuple2.priority) {
            return 0;
        }
        return 1;
    }

    // Closures and environments
    // -------------------------

    class Environment;

    // A closure represents the right-hand side of a variable
    // declaration, along with an environment giving values
    // to its free variables
    class Closure : public UMemory {
    public:
        const Expression& getExpr() const {
            return expr;
        }
        Environment& getEnv() const {
            return env;
        }
        Closure(Closure&&) = default;
        static Closure* create(const Expression&, Environment&, UErrorCode&);

        virtual ~Closure();
    private:

        Closure(const Expression& expression, Environment& environment) : expr(expression), env(environment) {}

        // An unevaluated expression
        const Expression& expr;
        // The environment mapping names used in this
        // expression to other expressions
        Environment& env;
    };

    class NonEmptyEnvironment;

    // An environment is represented as a linked chain of
    // non-empty environments, terminating at an empty environment.
    // It's searched using linear search.
    class Environment : public UMemory {
        public:
            virtual bool has(const VariableName&) const = 0;
            virtual InternalValue& lookup(const VariableName&) = 0;
            virtual InternalValue& bogus() = 0;
            // For convenience so that InternalValue::getValue() can return a reference
            // in error cases
            FunctionValue& bogusFunctionValue() { return bogusFunctionVal; }
            virtual InternalValue& createFallback(const UnicodeString&, UErrorCode&) = 0;
            virtual InternalValue& createNull(UErrorCode&) = 0;
            virtual InternalValue& createUnnamed(InternalValue&&, UErrorCode&) = 0;
            static Environment* create(UErrorCode&);
            static Environment* create(const VariableName&, Closure*, const UnicodeString&,
                                       Environment*, UErrorCode&);
            virtual ~Environment();

        private:
            FunctionValue bogusFunctionVal;
    };

    // The empty environment includes a "bogus" value to use when an
    // InternalValue& is needed (e.g. error conditions),
    // and a vector of "unnamed" values, so that the environment can
    // own all InternalValues (even those arising from expressions
    // that appear directly in a pattern and are not named).
    class EmptyEnvironment : public Environment {
    public:
        EmptyEnvironment(UErrorCode& status);
        virtual ~EmptyEnvironment();

    private:
        friend class Environment;

        bool has(const VariableName&) const override;
        InternalValue& lookup(const VariableName&) override;
        InternalValue& bogus() override { return bogusValue; }
        static EmptyEnvironment* create(UErrorCode&);
        static NonEmptyEnvironment* create(const VariableName&, InternalValue,
                                           Environment*, UErrorCode&);

        // Creates a fallback value owned by this Environment
        InternalValue& createFallback(const UnicodeString&, UErrorCode&) override;
        // Creates a null operand owned by this Environment
        InternalValue& createNull(UErrorCode&) override;
        // Creates an arbitrary value owned by this Environment
        InternalValue& createUnnamed(InternalValue&&, UErrorCode&) override;

        InternalValue& addUnnamedValue(LocalPointer<InternalValue>, UErrorCode&);

        InternalValue bogusValue; // Used in place of `nullptr` in error conditions
        UVector unnamedValues;
    };

    class NonEmptyEnvironment : public Environment {
    public:
        InternalValue* update(const VariableName&, InternalValue&&);
    private:
        friend class Environment;

        bool has(const VariableName&) const override;
        InternalValue& lookup(const VariableName&) override;
        InternalValue& bogus() override { return parent->bogus(); }
        static NonEmptyEnvironment* create(const VariableName&, Closure&&, const Environment*, UErrorCode&);
        virtual ~NonEmptyEnvironment();
    private:
        friend class Environment;

        NonEmptyEnvironment(const VariableName& v, InternalValue c, Environment* e) : var(v), rhs(std::move(c)), parent(e) {}

        InternalValue& createFallback(const UnicodeString&, UErrorCode&) override;
        InternalValue& createNull(UErrorCode&) override;
        InternalValue& createUnnamed(InternalValue&&, UErrorCode&) override;

        // Maps VariableName onto Closure*
        // Chain of linked environments
        VariableName var;
        InternalValue rhs;
        const LocalPointer<Environment> parent;
    };

    // The context contains all the information needed to process
    // an entire message: arguments, formatter cache, and error list

    class MessageFormatter;

    class MessageContext : public UMemory {
    public:
        MessageContext(const MessageArguments&, const StaticErrors&, UErrorCode&);

        const Formattable* getGlobal(const VariableName&, UErrorCode&) const;

        // If any errors were set, update `status` accordingly
        void checkErrors(UErrorCode& status) const;
        DynamicErrors& getErrors() { return errors; }

        virtual ~MessageContext();

    private:

        const MessageArguments& arguments; // External message arguments
        // Errors accumulated during parsing/formatting
        DynamicErrors errors;

    }; // class MessageContext

} // namespace message2

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* #if !UCONFIG_NO_NORMALIZATION */

#endif /* U_SHOW_CPLUSPLUS_API */

#endif // MESSAGEFORMAT2_EVALUATION_H

#endif // U_HIDE_DEPRECATED_API
// eof

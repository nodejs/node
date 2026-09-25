// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "messageformat2_allocation.h"
#include "messageformat2_evaluation.h"
#include "messageformat2_function_registry_internal.h"
#include "messageformat2_macros.h"
#include "uvector.h" // U_ASSERT

U_NAMESPACE_BEGIN

// Auxiliary data structures used during formatting a message

namespace message2 {

using namespace data_model;

// Functions
// -------------

ResolvedFunctionOption::ResolvedFunctionOption(ResolvedFunctionOption&& other) {
    name = std::move(other.name);
    value = std::move(other.value);
    sourceIsLiteral = other.sourceIsLiteral;
}

ResolvedFunctionOption::~ResolvedFunctionOption() {}


const ResolvedFunctionOption* FunctionOptions::getResolvedFunctionOptions(int32_t& len) const {
    len = functionOptionsLen;
    U_ASSERT(len == 0 || options != nullptr);
    return options;
}

FunctionOptions::FunctionOptions(UVector&& optionsVector, UErrorCode& status) {
    CHECK_ERROR(status);

    functionOptionsLen = optionsVector.size();
    options = moveVectorToArray<ResolvedFunctionOption>(optionsVector, status);
}

// Returns false if option doesn't exist
UBool FunctionOptions::wasSetFromLiteral(const UnicodeString& key) const {
    if (options == nullptr) {
        U_ASSERT(functionOptionsLen == 0);
    }
    for (int32_t i = 0; i < functionOptionsLen; i++) {
        const ResolvedFunctionOption& opt = options[i];
        if (opt.getName() == key) {
            return opt.isLiteral();
        }
    }
    return false;
}

UBool FunctionOptions::getFunctionOption(std::u16string_view key, Formattable& option) const {
    if (options == nullptr) {
        U_ASSERT(functionOptionsLen == 0);
    }
    for (int32_t i = 0; i < functionOptionsLen; i++) {
        const ResolvedFunctionOption& opt = options[i];
        if (opt.getName() == key) {
            option = opt.getValue();
            return true;
        }
    }
    return false;
}

UnicodeString FunctionOptions::getStringFunctionOption(std::u16string_view key) const {
    Formattable option;
    if (getFunctionOption(key, option)) {
        if (option.getType() == UFMT_STRING) {
            UErrorCode localErrorCode = U_ZERO_ERROR;
            UnicodeString val = option.getString(localErrorCode);
            U_ASSERT(U_SUCCESS(localErrorCode));
            return val;
        }
    }
    // For anything else, including non-string values, return "".
    // Alternately, could try to stringify the non-string option.
    // (Currently, no tests require that.)
    return {};
}

FunctionOptions& FunctionOptions::operator=(FunctionOptions&& other) noexcept {
    functionOptionsLen = other.functionOptionsLen;
    options = other.options;
    other.functionOptionsLen = 0;
    other.options = nullptr;
    return *this;
}

FunctionOptions::FunctionOptions(FunctionOptions&& other) {
    *this = std::move(other);
}

FunctionOptions::~FunctionOptions() {
    if (options != nullptr) {
        delete[] options;
        options = nullptr;
    }
}

static bool containsOption(const UVector& opts, const ResolvedFunctionOption& opt) {
    for (int32_t i = 0; i < opts.size(); i++) {
        if (static_cast<ResolvedFunctionOption*>(opts[i])->getName()
            == opt.getName()) {
            return true;
        }
    }
    return false;
}

// Options in `this` take precedence
// `this` can't be used after mergeOptions is called
FunctionOptions FunctionOptions::mergeOptions(FunctionOptions&& other,
                                              UErrorCode& status) {
    UVector mergedOptions(status);
    mergedOptions.setDeleter(uprv_deleteUObject);

    if (U_FAILURE(status)) {
        return {};
    }

    // Create a new vector consisting of the options from this `FunctionOptions`
    for (int32_t i = 0; i < functionOptionsLen; i++) {
        mergedOptions.adoptElement(create<ResolvedFunctionOption>(std::move(options[i]), status),
                                 status);
    }

    // Add each option from `other` that doesn't appear in this `FunctionOptions`
    for (int i = 0; i < other.functionOptionsLen; i++) {
        // Note: this is quadratic in the length of `options`
        if (!containsOption(mergedOptions, other.options[i])) {
            mergedOptions.adoptElement(create<ResolvedFunctionOption>(std::move(other.options[i]),
                                                                    status),
                                     status);
        }
    }

    delete[] options;
    options = nullptr;
    functionOptionsLen = 0;

    return FunctionOptions(std::move(mergedOptions), status);
}

// PrioritizedVariant
// ------------------

UBool PrioritizedVariant::operator<(const PrioritizedVariant& other) const {
  if (priority < other.priority) {
      return true;
  }
  return false;
}

PrioritizedVariant::~PrioritizedVariant() {}

    // ---------------- Environments and closures

    Environment* Environment::create(const VariableName& var, Closure&& c, Environment* parent, UErrorCode& errorCode) {
        NULL_ON_ERROR(errorCode);
        Environment* result = new NonEmptyEnvironment(var, std::move(c), parent);
        if (result == nullptr) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        }
        return result;
    }

    Environment* Environment::create(UErrorCode& errorCode) {
        NULL_ON_ERROR(errorCode);
        Environment* result = new EmptyEnvironment();
        if (result == nullptr) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        }
        return result;
    }

    const Closure& EmptyEnvironment::lookup(const VariableName& v) const {
        (void) v;
        U_ASSERT(false);
        UPRV_UNREACHABLE_EXIT;
    }

    const Closure& NonEmptyEnvironment::lookup(const VariableName& v) const {
        if (v == var) {
            return rhs;
        }
        return parent->lookup(v);
    }

    bool EmptyEnvironment::has(const VariableName& v) const {
        (void) v;
        return false;
    }

    bool NonEmptyEnvironment::has(const VariableName& v) const {
        if (v == var) {
            return true;
        }
        return parent->has(v);
    }

    Environment::~Environment() {}
    NonEmptyEnvironment::~NonEmptyEnvironment() {}
    EmptyEnvironment::~EmptyEnvironment() {}

    Closure::~Closure() {}

    // MessageContext methods

    void MessageContext::checkErrors(UErrorCode& status) const {
        CHECK_ERROR(status);
        errors.checkErrors(status);
    }

    const Formattable* MessageContext::getGlobal(const VariableName& v,
                                                 UErrorCode& errorCode) const {
       return arguments.getArgument(v, errorCode);
    }

    MessageContext::MessageContext(const MessageArguments& args,
                                   const StaticErrors& e,
                                   UErrorCode& status) : arguments(args), errors(e, status) {}

    MessageContext::~MessageContext() {}

    // InternalValue
    // -------------

    bool InternalValue::isFallback() const {
        return std::holds_alternative<FormattedPlaceholder>(argument)
            && std::get_if<FormattedPlaceholder>(&argument)->isFallback();
    }

    bool InternalValue::hasNullOperand() const {
        return std::holds_alternative<FormattedPlaceholder>(argument)
            && std::get_if<FormattedPlaceholder>(&argument)->isNullOperand();
    }

    FormattedPlaceholder InternalValue::takeArgument(UErrorCode& errorCode) {
        if (U_FAILURE(errorCode)) {
            return {};
        }

        if (std::holds_alternative<FormattedPlaceholder>(argument)) {
            return std::move(*std::get_if<FormattedPlaceholder>(&argument));
        }
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return {};
    }

    const UnicodeString& InternalValue::getFallback() const {
        if (std::holds_alternative<FormattedPlaceholder>(argument)) {
            return std::get_if<FormattedPlaceholder>(&argument)->getFallback();
        }
        return (*std::get_if<InternalValue*>(&argument))->getFallback();
    }

    const Selector* InternalValue::getSelector(UErrorCode& errorCode) const {
        if (U_FAILURE(errorCode)) {
            return nullptr;
        }

        if (selector == nullptr) {
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        }
        return selector;
    }

    InternalValue::InternalValue(FormattedPlaceholder&& arg) {
        argument = std::move(arg);
        selector = nullptr;
        formatter = nullptr;
    }

    InternalValue::InternalValue(InternalValue* operand,
                                 FunctionOptions&& opts,
                                 const FunctionName& functionName,
                                 const Formatter* f,
                                 const Selector* s) {
        argument = operand;
        options = std::move(opts);
        name = functionName;
        selector = s;
        formatter = f;
        U_ASSERT(selector != nullptr || formatter != nullptr);
    }

    // `this` cannot be used after calling this method
    void InternalValue::forceSelection(DynamicErrors& errs,
                                       const UnicodeString* keys,
                                       int32_t keysLen,
                                       UnicodeString* prefs,
                                       int32_t& prefsLen,
                                       UErrorCode& errorCode) {
        if (U_FAILURE(errorCode)) {
            return;
        }

        if (!canSelect()) {
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        // Find the argument and complete set of options by traversing `argument`
        FunctionOptions opts;
        InternalValue* p = this;
        FunctionName selectorName = name;

        bool operandSelect = false;
        while (std::holds_alternative<InternalValue*>(p->argument)) {
            if (p->name != selectorName) {
                // Can only compose calls to the same selector
                errorCode = U_ILLEGAL_ARGUMENT_ERROR;
                return;
            }
            // Very special case to detect something like:
            // .local $sel = {1 :integer select=exact} .local $bad = {$sel :integer} .match $bad 1 {{ONE}} * {{operand select {$bad}}}
            // This can be done better once function composition is fully implemented.
            if (p != this &&
                !p->options.getStringFunctionOption(options::SELECT).isEmpty()
                && (selectorName == functions::NUMBER || selectorName == functions::INTEGER)) {
                // In this case, we want to call the selector normally but emit a
                // `bad-option` error, possibly with the outcome of normal-looking output (with relaxed
                // error handling) and an error (with strict error handling).
                operandSelect = true;
            }
            // First argument to mergeOptions takes precedence
            opts = opts.mergeOptions(std::move(p->options), errorCode);
            if (U_FAILURE(errorCode)) {
                return;
            }
            InternalValue* next = *std::get_if<InternalValue*>(&p->argument);
            p = next;
        }
        FormattedPlaceholder arg = std::move(*std::get_if<FormattedPlaceholder>(&p->argument));

        // This condition can't be checked in the selector.
        // Effectively, there are two different kinds of "bad option" errors:
        // one that can be recovered from (used for select=$var) and one that
        // can't (used for bad digit size options and other cases).
        // The checking of the recoverable error has to be done here; otherwise,
        // the "bad option" signaled by the selector implementation would cause
        // fallback output to be used when formatting the `*` pattern.
        bool badSelectOption = !checkSelectOption();

        selector->selectKey(std::move(arg), std::move(opts),
                            keys, keysLen,
                            prefs, prefsLen, errorCode);
        if (errorCode == U_MF_SELECTOR_ERROR) {
            errorCode = U_ZERO_ERROR;
            errs.setSelectorError(selectorName, errorCode);
        } else if (errorCode == U_MF_BAD_OPTION) {
            errorCode = U_ZERO_ERROR;
            errs.setBadOption(selectorName, errorCode);
        } else if (operandSelect || badSelectOption) {
            errs.setRecoverableBadOption(selectorName, errorCode);
            // In this case, only the `*` variant should match
            prefsLen = 0;
        }
    }

    bool InternalValue::checkSelectOption() const {
        if (name != UnicodeString("number") && name != UnicodeString("integer")) {
            return true;
        }

        // Per the spec, if the "select" option is present, it must have been
        // set from a literal

        Formattable opt;
        // Returns false if the `select` option is present and it was not set from a literal

        // OK if the option wasn't present
        if (!options.getFunctionOption(UnicodeString("select"), opt)) {
            return true;
        }
        // Otherwise, return true if the option was set from a literal
        return options.wasSetFromLiteral(UnicodeString("select"));
    }

    FormattedPlaceholder InternalValue::forceFormatting(DynamicErrors& errs, UErrorCode& errorCode) {
        if (U_FAILURE(errorCode)) {
            return {};
        }

        if (formatter == nullptr && selector == nullptr) {
            U_ASSERT(std::holds_alternative<FormattedPlaceholder>(argument));
            return std::move(*std::get_if<FormattedPlaceholder>(&argument));
        }
        if (formatter == nullptr) {
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            return {};
        }

        FormattedPlaceholder arg;

        if (std::holds_alternative<FormattedPlaceholder>(argument)) {
            arg = std::move(*std::get_if<FormattedPlaceholder>(&argument));
        } else {
            arg = (*std::get_if<InternalValue*>(&argument))->forceFormatting(errs,
                                                                             errorCode);
        }

        if (U_FAILURE(errorCode)) {
            return {};
        }

        if (arg.isFallback()) {
            return arg;
        }

        // The fallback for a nullary function call is the function name
        UnicodeString fallback;
        if (arg.isNullOperand()) {
            fallback = u":";
            fallback += name;
        } else {
            fallback = arg.getFallback();
        }

        // Very special case for :number select=foo and :integer select=foo
        // This check can't be done inside the function implementation because
        // it doesn't have a way to both signal an error and return usable output,
        // and the spec stipulates that fallback output shouldn't be used in the
        // case of a bad `select` option to a formatting call.
        bool badSelect = !checkSelectOption();

        // Call the function with the argument
        FormattedPlaceholder result = formatter->format(std::move(arg), std::move(options), errorCode);
        if (U_SUCCESS(errorCode) && errorCode == U_USING_DEFAULT_WARNING) {
            // Ignore this warning
            errorCode = U_ZERO_ERROR;
        }
        if (U_FAILURE(errorCode)) {
            if (errorCode == U_MF_OPERAND_MISMATCH_ERROR) {
                errorCode = U_ZERO_ERROR;
                errs.setOperandMismatchError(name, errorCode);
            } else if (errorCode == U_MF_BAD_OPTION) {
                errorCode = U_ZERO_ERROR;
                errs.setBadOption(name, errorCode);
            } else {
                errorCode = U_ZERO_ERROR;
                // Convey any other error generated by the formatter
                // as a formatting error
                errs.setFormattingError(name, errorCode);
            }
        }
        // Ignore the output if any error occurred
        // We don't ignore the output in the case of a Bad Option Error,
        // because of the select=bad case where we want both an error
        // and non-fallback output.
        if (errs.hasFormattingError() || errs.hasBadOptionError()) {
            return FormattedPlaceholder(fallback);
        }
        if (badSelect) {
            // In this case, we want to set an error but not replace
            // the output with a fallback
            errs.setRecoverableBadOption(name, errorCode);
        }
        return result;
    }

    InternalValue& InternalValue::operator=(InternalValue&& other) noexcept {
        argument = std::move(other.argument);
        other.argument = nullptr;
        options = std::move(other.options);
        name = other.name;
        selector = other.selector;
        formatter = other.formatter;
        other.selector = nullptr;
        other.formatter = nullptr;

        return *this;
    }

    InternalValue::~InternalValue() {
        delete selector;
        selector = nullptr;
        delete formatter;
        formatter = nullptr;
        if (std::holds_alternative<InternalValue*>(argument)) {
            delete *std::get_if<InternalValue*>(&argument);
            argument = nullptr;
        }
    }

} // namespace message2
U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* #if !UCONFIG_NO_NORMALIZATION */

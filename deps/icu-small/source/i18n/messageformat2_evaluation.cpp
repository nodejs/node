// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/ubidi.h"
#include "messageformat2_allocation.h"
#include "messageformat2_evaluation.h"
#include "messageformat2_function_registry_internal.h"
#include "messageformat2_macros.h"
#include "uvector.h" // U_ASSERT

U_NAMESPACE_BEGIN

// Auxiliary data structures used during formatting a message

namespace message2 {

using namespace data_model;

// BaseValue
// ---------

BaseValue::BaseValue(const Locale& loc, const UnicodeString& fb, const Formattable& source, bool wasCreatedFromLiteral)
    : locale(loc), fromLiteral(wasCreatedFromLiteral) {
    innerValue = source;
    fallback += LEFT_CURLY_BRACE;
    fallback += fb;
    fallback += RIGHT_CURLY_BRACE;
}

/* static */ BaseValue* BaseValue::create(const Locale& locale,
                                          const UnicodeString& fallback,
                                          const Formattable& source,
                                          bool wasCreatedFromLiteral,
                                          UErrorCode& errorCode) {
    return message2::create<BaseValue>(BaseValue(locale, fallback, source, wasCreatedFromLiteral), errorCode);
}

extern UnicodeString formattableToString(const Locale&, const Formattable&, UErrorCode&);

UnicodeString BaseValue::formatToString(UErrorCode& errorCode) const {
    return formattableToString(locale, innerValue, errorCode);
}

BaseValue& BaseValue::operator=(BaseValue&& other) noexcept {
    innerValue = std::move(other.innerValue);
    opts = std::move(other.opts);
    dir = other.dir;
    inputDir = other.inputDir;
    locale = other.locale;
    fallback = other.fallback;
    fromLiteral = other.fromLiteral;

    return *this;
}

BaseValue::BaseValue(BaseValue&& other) {
    *this = std::move(other);
}

// VariableValue
// -------------

VariableValue::VariableValue(const FunctionValue* v) : underlyingValue(std::move(v)) {}

/* static */ VariableValue* VariableValue::create(const FunctionValue* v,
                                                  UErrorCode& errorCode) {
    return message2::create<VariableValue>(VariableValue(std::move(v)), errorCode);
}

VariableValue& VariableValue::operator=(VariableValue&& other) noexcept {
    underlyingValue = other.underlyingValue;

    return *this;
}

VariableValue::VariableValue(VariableValue&& other) {
    *this = std::move(other);
}

VariableValue::~VariableValue() {
    underlyingValue = nullptr; // not owned
}

// Functions
// -------------

ResolvedFunctionOption::ResolvedFunctionOption(ResolvedFunctionOption&& other) {
    *this = std::move(other);
}

ResolvedFunctionOption::ResolvedFunctionOption(const UnicodeString& n,
                                               const FunctionValue& f,
                                               bool b) : name(n), value(&f), thisWasMerged(b) {}

ResolvedFunctionOption::~ResolvedFunctionOption() {
    value = nullptr; // value is not owned
}


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
UBool FunctionOptions::wasSetFromLiteral(const std::u16string_view key) const {
    if (options == nullptr) {
        U_ASSERT(functionOptionsLen == 0);
    }
    for (int32_t i = 0; i < functionOptionsLen; i++) {
        const ResolvedFunctionOption& opt = options[i];
        if (opt.getName() == key) {
            // Require both: - opt's value was created from a literal;
            // - opt does not originate from merging a previous options map
            // with this one
            return opt.getValue().wasCreatedFromLiteral()
                && (!opt.wasMerged());
        }
    }
    return false;
}

const FunctionValue*
FunctionOptions::getFunctionOption(std::u16string_view key,
                                   UErrorCode& status) const {
    if (options == nullptr) {
        U_ASSERT(functionOptionsLen == 0);
    }
    for (int32_t i = 0; i < functionOptionsLen; i++) {
        const ResolvedFunctionOption& opt = options[i];
        if (opt.getName() == key) {
            return &opt.getValue();
        }
    }
    status = U_ILLEGAL_ARGUMENT_ERROR;
    return nullptr;
}

UnicodeString
FunctionOptions::getStringFunctionOption(std::u16string_view k, UErrorCode& errorCode) const {
    const FunctionValue* option = getFunctionOption(k, errorCode);
    if (U_SUCCESS(errorCode)) {
        UnicodeString result = option->formatToString(errorCode);
        if (U_SUCCESS(errorCode)) {
            return result;
        }
    }
    return {};
}

UnicodeString FunctionOptions::getStringFunctionOption(std::u16string_view key) const {
    UErrorCode localStatus = U_ZERO_ERROR;

    UnicodeString result = getStringFunctionOption(key, localStatus);
    if (U_FAILURE(localStatus)) {
        return {};
    }
    return result;
}

FunctionOptions& FunctionOptions::operator=(FunctionOptions other) noexcept {
    swap(*this, other);
    return *this;
}

FunctionOptions::FunctionOptions(const FunctionOptions& other) {
    U_ASSERT(!other.bogus);
    functionOptionsLen = other.functionOptionsLen;
    options = nullptr;
    if (functionOptionsLen != 0) {
        UErrorCode localStatus = U_ZERO_ERROR;
        options = copyArray<ResolvedFunctionOption>(other.options, functionOptionsLen, localStatus);
        if (U_FAILURE(localStatus)) {
            bogus = true;
        }
    }
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
FunctionOptions FunctionOptions::mergeOptions(const FunctionOptions& other,
                                              UErrorCode& status) const {
    UVector mergedOptions(status);
    mergedOptions.setDeleter(uprv_deleteUObject);

    if (U_FAILURE(status)) {
        return {};
    }
    if (bogus || other.bogus) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return {};
    }

    // Create a new vector consisting of the options from this `FunctionOptions`
    for (int32_t i = 0; i < functionOptionsLen; i++) {
        mergedOptions.adoptElement(create<ResolvedFunctionOption>(options[i], status),
                                 status);
    }

    // Add each option from `other` that doesn't appear in this `FunctionOptions`
    for (int i = 0; i < other.functionOptionsLen; i++) {
        // Note: this is quadratic in the length of `options`
        if (!containsOption(mergedOptions, other.options[i])) {
            const ResolvedFunctionOption& oldOpt = other.options[i];
            ResolvedFunctionOption newOpt = ResolvedFunctionOption(oldOpt.name, *oldOpt.value, true);
            mergedOptions.adoptElement(create<ResolvedFunctionOption>(newOpt, status),
                                       status);
        }
    }

    return FunctionOptions(std::move(mergedOptions), status);
}

// InternalValue
// -------------


InternalValue::~InternalValue() {}

InternalValue& InternalValue::operator=(InternalValue&& other) {
    fallbackString = other.fallbackString;
    val = std::move(other.val);
    return *this;
}

InternalValue::InternalValue(InternalValue&& other) {
    *this = std::move(other);
}

InternalValue::InternalValue(UErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    LocalPointer<FunctionValue> nv(new NullValue());
    if (!nv.isValid()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    val = std::move(nv);
}

InternalValue::InternalValue(FunctionValue* v, const UnicodeString& fb)
    : fallbackString(fb) {
    U_ASSERT(v != nullptr);
    val = LocalPointer<FunctionValue>(v);
}

const FunctionValue* InternalValue::getValue(UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return nullptr;
    }
    // If this is a closure or fallback, error out
    if (!isEvaluated()) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return nullptr;
    }
    // Follow the indirection to get the value
    if (isIndirection()) {
        const InternalValue* other = *std::get_if<const InternalValue*>(&val);
        U_ASSERT(other != nullptr);
        return other->getValue(status);
    }
    // Otherwise, return the contained FunctionValue
    const LocalPointer<FunctionValue>* result = std::get_if<LocalPointer<FunctionValue>>(&val);
    U_ASSERT(result->isValid());
    return (*result).getAlias();
}

FunctionValue* InternalValue::wrapInVariableValue(UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return nullptr;
    }
    const FunctionValue* inner = getValue(status);
    if (U_FAILURE(status)) {
        return nullptr;
    }
    return VariableValue::create(inner, status);
}

bool InternalValue::isSelectable() const {
    UErrorCode localStatus = U_ZERO_ERROR;
    const FunctionValue* val = getValue(localStatus);
    if (U_FAILURE(localStatus)) {
        return false;
    }
    return val->isSelectable();
}

/* static */ LocalPointer<InternalValue> InternalValue::null(UErrorCode& status) {
    if (U_SUCCESS(status)) {
        InternalValue* result = new InternalValue(status);
        if (U_SUCCESS(status)) {
            return LocalPointer<InternalValue>(result);
        }
    }
    return LocalPointer<InternalValue>();
}

/* static */ LocalPointer<InternalValue> InternalValue::fallback(const UnicodeString& s,
                                                                 UErrorCode& status) {
    if (U_SUCCESS(status)) {
        InternalValue* result = new InternalValue(s);
        if (U_SUCCESS(status)) {
            return LocalPointer<InternalValue>(result);
        }
    }
    return LocalPointer<InternalValue>();
}

/* static */ InternalValue InternalValue::closure(Closure* c, const UnicodeString& fb) {
    U_ASSERT(c != nullptr);
    return InternalValue(c, fb);
}

bool InternalValue::isClosure() const {
    return std::holds_alternative<LocalPointer<Closure>>(val);
}

bool InternalValue::isEvaluated() const {
    return std::holds_alternative<LocalPointer<FunctionValue>>(val) || isIndirection();
}

bool InternalValue::isIndirection() const {
    return std::holds_alternative<const InternalValue*>(val);
}

bool InternalValue::isNullOperand() const {
    UErrorCode localStatus = U_ZERO_ERROR;
    const FunctionValue* val = getValue(localStatus);
    if (U_FAILURE(localStatus)) {
        return false;
    }
    return val->isNullOperand();
}

void InternalValue::update(InternalValue& newVal) {
    fallbackString = newVal.fallbackString;
    val = &newVal;
}

void InternalValue::update(LocalPointer<FunctionValue> newVal) {
    val = std::move(newVal);
}

void InternalValue::update(const UnicodeString& fb) {
    fallbackString = fb;
    val = fb;
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

    Environment* Environment::create(const VariableName& var, Closure* c,
                                     const UnicodeString& fallbackStr,
                                     Environment* parent, UErrorCode& errorCode) {
        NULL_ON_ERROR(errorCode);
        Environment* result = new NonEmptyEnvironment(var, InternalValue::closure(c, fallbackStr), parent);
        if (result == nullptr) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return parent;
        }
        return result;
    }

    Environment* Environment::create(UErrorCode& errorCode) {
        NULL_ON_ERROR(errorCode);
        Environment* result = new EmptyEnvironment(errorCode);
        if (U_SUCCESS(errorCode) && result == nullptr) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        }
        return result;
    }

    InternalValue& EmptyEnvironment::lookup(const VariableName&) {
        U_ASSERT(false);
        UPRV_UNREACHABLE_EXIT;
    }

    InternalValue& NonEmptyEnvironment::lookup(const VariableName& v) {
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

    InternalValue& EmptyEnvironment::createNull(UErrorCode& status) {
        if (U_FAILURE(status)) {
            return bogus();
        }
        LocalPointer<InternalValue> val(InternalValue::null(status));
        return addUnnamedValue(std::move(val), status);
    }

    InternalValue& EmptyEnvironment::createFallback(const UnicodeString& s, UErrorCode& status) {
        if (U_FAILURE(status)) {
            return bogus();
        }
        LocalPointer<InternalValue> val(InternalValue::fallback(s, status));
        return addUnnamedValue(std::move(val), status);
    }

    InternalValue& EmptyEnvironment::createUnnamed(InternalValue&& v, UErrorCode& status) {
        if (U_FAILURE(status)) {
            return bogus();
        }
        LocalPointer<InternalValue> val(new InternalValue(std::move(v)));
        if (!val.isValid()) {
            return bogus();
        }
        return addUnnamedValue(std::move(val), status);
    }

    InternalValue& NonEmptyEnvironment::createNull(UErrorCode& status) {
        return parent->createNull(status);
    }

    InternalValue& NonEmptyEnvironment::createFallback(const UnicodeString& s, UErrorCode& status) {
        return parent->createFallback(s, status);
    }

    InternalValue& NonEmptyEnvironment::createUnnamed(InternalValue&& v, UErrorCode& status) {
        return parent->createUnnamed(std::move(v), status);
    }

    InternalValue& EmptyEnvironment::addUnnamedValue(LocalPointer<InternalValue> val,
                                             UErrorCode& status) {
        if (U_FAILURE(status)) {
            return bogus();
        }
        U_ASSERT(val.isValid());
        InternalValue* v = val.orphan();
        unnamedValues.adoptElement(v, status);
        return *v;
    }

    EmptyEnvironment::EmptyEnvironment(UErrorCode& status) : unnamedValues(UVector(status)) {
        unnamedValues.setDeleter(uprv_deleteUObject);
    }

    Environment::~Environment() {}
    NonEmptyEnvironment::~NonEmptyEnvironment() {}
    EmptyEnvironment::~EmptyEnvironment() {}

    /* static */ Closure* Closure::create(const Expression& expr, Environment& env,
                                          UErrorCode& status) {
        NULL_ON_ERROR(status);

        Closure* result = new Closure(expr, env);
        if (result == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
        return result;
    }

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

} // namespace message2
U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* #if !UCONFIG_NO_NORMALIZATION */

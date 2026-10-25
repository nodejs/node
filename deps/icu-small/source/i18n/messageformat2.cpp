// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/messageformat2_arguments.h"
#include "unicode/messageformat2_data_model.h"
#include "unicode/messageformat2_formattable.h"
#include "unicode/messageformat2.h"
#include "unicode/normalizer2.h"
#include "unicode/ubidi.h"
#include "unicode/unistr.h"
#include "messageformat2_allocation.h"
#include "messageformat2_checker.h"
#include "messageformat2_evaluation.h"
#include "messageformat2_function_registry_internal.h"
#include "messageformat2_macros.h"


U_NAMESPACE_BEGIN

namespace message2 {

using namespace data_model;

// ------------------------------------------------------
// Formatting


// Arguments and literals
//-----------------------

static UnicodeString varFallback(const VariableName& var) {
    UnicodeString str(DOLLAR);
    str += var;
    return str;
}

static UnicodeString functionFallback(const InternalValue& operand,
                                      const FunctionName& functionName) {
    UnicodeString fallbackStr;
    // Create the fallback string for this function call
    if (operand.isNullOperand()) {
        fallbackStr = UnicodeString(COLON);
        fallbackStr += functionName;
    } else {
        fallbackStr = operand.asFallback();
    }
    return fallbackStr;
}

// Assumes that `var` is a message argument; returns the argument's value.
[[nodiscard]] InternalValue MessageFormatter::evalArgument(const UnicodeString& fallback,
                                                           const VariableName& var,
                                                           MessageContext& context,
                                                           UErrorCode& errorCode) const {
    if (U_SUCCESS(errorCode)) {
        // Look up the variable in the global environment
        const Formattable* val = context.getGlobal(var, errorCode);
        if (U_SUCCESS(errorCode)) {
            // Note: the fallback string has to be passed in because in a declaration like:
            // .local $foo = {$bar :number}
            // the fallback for $bar is "$foo".
            UnicodeString fallbackToUse = fallback;
            if (fallbackToUse.isEmpty()) {
                fallbackToUse += DOLLAR;
                fallbackToUse += var;
            }
            // If it exists, create a BaseValue (FunctionValue) for it
            LocalPointer<BaseValue> result(BaseValue::create(locale, fallbackToUse, *val, false, errorCode));
            // Add fallback and return an InternalValue
            if (U_SUCCESS(errorCode)) {
                return InternalValue(result.orphan(), fallbackToUse);
            }
        }
    }
    return {};
}

// Helper function to re-escape any escaped-char characters
static UnicodeString reserialize(const UnicodeString& s) {
    UnicodeString result(PIPE);
    for (int32_t i = 0; i < s.length(); i++) {
        switch(s[i]) {
        case BACKSLASH:
        case PIPE:
        case LEFT_CURLY_BRACE:
        case RIGHT_CURLY_BRACE: {
            result += BACKSLASH;
            break;
        }
        default:
            break;
        }
        result += s[i];
    }
    result += PIPE;
    return result;
}

// Returns the contents of the literal
[[nodiscard]] InternalValue MessageFormatter::evalLiteral(const UnicodeString& fallback,
                                                          const Literal& lit,
                                                          UErrorCode& errorCode) const {
    // The fallback for a literal is itself, unless another fallback is passed
    // in (same reasoning as evalArgument())
    UnicodeString fallbackToUse = !fallback.isEmpty() ? fallback : reserialize(lit.unquoted());

    // Create a BaseValue (FunctionValue) that wraps the literal
    LocalPointer<BaseValue> val(BaseValue::create(locale,
                                                  fallbackToUse,
                                                  Formattable(lit.unquoted()),
                                                  true,
                                                  errorCode));
    if (U_SUCCESS(errorCode)) {
        return InternalValue(val.orphan(), fallbackToUse);
    }
    return {};
}

// Operands
// --------

[[nodiscard]] InternalValue& MessageFormatter::evalVariableReference(const UnicodeString& fallback,
                                                                     Environment& env,
                                                                     const VariableName& var,
                                                                     MessageContext& context,
                                                                     UErrorCode &status) const {
    // Check if it's local or global
    // Note: there is no name shadowing; this is enforced by the parser

    // This code implements lazy call-by-need evaluation of locals.
    // That is, the environment binds names to a closure, not a resolved value.
    // The spec does not require either eager or lazy evaluation.

    // NFC-normalize the variable name. See
    // https://github.com/unicode-org/message-format-wg/blob/main/spec/syntax.md#names-and-identifiers
    const VariableName normalized = StandardFunctions::normalizeNFC(var);

    // Look up the variable in the environment
    if (env.has(normalized)) {
        // `var` is a local -- look it up
        InternalValue& rhs = env.lookup(normalized);
        // Evaluate the expression using the environment from the closure
        // The name of this local variable is the fallback for its RHS.
        UnicodeString newFallback(DOLLAR);
        newFallback += var;
        if (rhs.isClosure()) {
            Closure& c = rhs.asClosure();
            InternalValue& result = evalExpression(newFallback,
                                                   c.getEnv(),
                                                   c.getExpr(),
                                                   context,
                                                   status);
            // Overwrite the closure with the result of evaluation
            if (result.isFallback()) {
                rhs.update(result.asFallback());
            } else {
                U_ASSERT(result.isEvaluated());

                // The FunctionValue representing the right-hand side of this declaration
                // might have a wasSetFromLiteral() method that returns true (i.e. if it's a BaseValue);
                // But that value is being assigned to a variable here, so we need to
                // ensure that wasSetFromLiteral() returns false.
                // We accomplish this by wrapping it in a VariableValue.
                LocalPointer<FunctionValue> variableValue(result.wrapInVariableValue(status));
                if (U_FAILURE(status) || !variableValue.isValid()) {
                    return result;
                }

                InternalValue wrappedResult(variableValue.orphan(), result.asFallback());
                InternalValue& ref = env.createUnnamed(std::move(wrappedResult), status);
                if (U_FAILURE(status)) {
                    return result;
                }
                // Create an indirection to the result returned
                // by evalExpression()
                rhs.update(ref);
            }
        }
        // If it's already evaluated, just return the value
        return rhs;
    }
    // Variable wasn't found in locals -- check if it's global
    InternalValue result = evalArgument(fallback, normalized, context, status);
    if (status == U_ILLEGAL_ARGUMENT_ERROR) {
        status = U_ZERO_ERROR;
        // Unbound variable -- set a resolution error
        context.getErrors().setUnresolvedVariable(var, status);
        // Use fallback per
        // https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#fallback-resolution
        return env.createFallback(varFallback(var), status);
    }
    // Looking up the global variable succeeded; return it
    return env.createUnnamed(std::move(result), status);
}

// InternalValues are passed as references into a global environment object
// that is live for the duration of one formatter call.
// They are mutable references so that they can be updated with a new value
// (when a closure is overwritten with the result of evaluating it),
// which can be shared across different references to the corresponding MF2
// variable.
[[nodiscard]] InternalValue& MessageFormatter::evalOperand(const UnicodeString& fallback,
                                                           Environment& env,
                                                           const Operand& rand,
                                                           MessageContext& context,
                                                           UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return env.bogus();
    }

    // Three cases: absent operand; variable; or literal

    // Absent (null) operand
    if (rand.isNull()) {
        return env.createNull(status);
    }
    // Variable reference
    if (rand.isVariable()) {
        return evalVariableReference(fallback, env, rand.asVariable(), context, status);
    }
    // Literal
    else {
        U_ASSERT(rand.isLiteral());
        return env.createUnnamed(evalLiteral(fallback, rand.asLiteral(), status), status);
    }
}

// Function calls
// --------------

// Looks up `functionName` and applies it to an operand and options,
// handling errors if the function is unbound
[[nodiscard]] InternalValue& MessageFormatter::apply(Environment& env,
                                                     const FunctionName& functionName,
                                                     InternalValue& rand,
                                                     FunctionOptions&& options,
                                                     MessageContext& context,
                                                     UErrorCode& status) const {
    if (U_FAILURE(status))
        return env.bogus();

    // Create the fallback string to use in case of an error
    // calling the function
    UnicodeString fallbackStr = functionFallback(rand, functionName);

    // Look up the function name
    const Function* function = lookupFunction(functionName, status);

    if (U_FAILURE(status)) { // Handle unknown function
        // Set error and use the fallback value
        status = U_ZERO_ERROR;
        context.getErrors().setUnknownFunction(functionName, status);
        return env.createFallback(fallbackStr, status);
    } // `function` is now known to be non-null

    // Value is not a fallback (checked by the caller),
    // so we can safely call getValue()
    const FunctionValue* functionArg(rand.getValue(status));
    U_ASSERT(U_SUCCESS(status));
    // Call the function
    LocalPointer<FunctionValue>
        functionResult(function->call(makeFunctionContext(options, functionName),
                                      *functionArg,
                                      std::move(options),
                                      status));
    // Handle any errors signaled by the function
    // (and use the fallback value)
    UErrorCode savedStatus = status;
    status = U_ZERO_ERROR;
    if (U_MF_RECOVERABLE_ERROR(savedStatus)) {
        if (savedStatus == U_MF_OPERAND_MISMATCH_ERROR) {
            context.getErrors().setOperandMismatchError(functionName, status);
        } else if (savedStatus == U_MF_FORMATTING_ERROR) {
            context.getErrors().setFormattingError(functionName, status);
        } else {
            U_ASSERT(savedStatus == U_MF_BAD_OPTION);
            context.getErrors().setBadOption(functionName, status);
        }
        return env.createFallback(fallbackStr, status);
    }
    // Non-recoverable error
    if (U_FAILURE(savedStatus)) {
        status = savedStatus;
        return env.bogus();
    } // Success; return the result
    return env.createUnnamed(InternalValue(functionResult.orphan(), fallbackStr), status);
}

// Function options and context
// ----------------------------

FunctionContext MessageFormatter::makeFunctionContext(const FunctionOptions& options, const FunctionName& calledFunction) const {
    // Look up "u:dir", and "u:id" in the options

    UMFBidiOption dir = ubidi_getMFOption(options.getStringFunctionOption(options::U_DIR));
    UnicodeString id = options.getStringFunctionOption(options::U_ID);

    return FunctionContext(locale, dir, id, calledFunction);
}

// Resolves a function's options
FunctionOptions MessageFormatter::resolveOptions(Environment& env,
                                                 const OptionMap& options,
                                                 MessageContext& context,
                                                 UErrorCode& status) const {
    // Create a vector of options
    LocalPointer<UVector> optionsVector(createUVector(status));
    if (U_FAILURE(status)) {
        return {};
    }
    LocalPointer<ResolvedFunctionOption> resolvedOpt;
    // For each option...
    for (int i = 0; i < options.size(); i++) {
        const Option& opt = options.getOption(i, status);
        if (U_FAILURE(status)) {
            return {};
        }
        const UnicodeString& k = opt.getName();
        const Operand& v = opt.getValue();

        // ...evaluate its right-hand side...
        UErrorCode subStatus = U_ZERO_ERROR;
        InternalValue& rhsVal = evalOperand({}, env, v, context, subStatus);
        // ...giving a FunctionValue.
        const FunctionValue* optVal = rhsVal.getValue(subStatus);
        if (U_FAILURE(subStatus)) { // Ignore fallback values
            continue;
        } // The list of resolved options omits any fallback values

        // The option is resolved; add it to the vector
        ResolvedFunctionOption resolvedOpt(k, *optVal, false);
        LocalPointer<ResolvedFunctionOption>
            p(create<ResolvedFunctionOption>(std::move(resolvedOpt), status));
        EMPTY_ON_ERROR(status);
        optionsVector->adoptElement(p.orphan(), status);
    }
    // Return a new FunctionOptions constructed from the vector of options
    return FunctionOptions(std::move(*optionsVector), status);
}

// Expressions
// -----------
// Evaluates an expression using `globalEnv` for the values of variables
[[nodiscard]] InternalValue& MessageFormatter::evalExpression(const UnicodeString& fallback,
                                                              Environment& globalEnv,
                                                              const Expression& expr,
                                                              MessageContext& context,
                                                              UErrorCode &status) const {
    if (U_FAILURE(status))
        return globalEnv.bogus();

    // Evaluate the operand (evalOperand handles the case of a null operand)
    InternalValue& randVal = evalOperand(fallback, globalEnv, expr.getOperand(), context, status);

    // If there's no function, we check for an implicit formatter
    if (!expr.isFunctionCall()) {
        const FunctionValue* contained = randVal.getValue(status);
        if (U_FAILURE(status)) {
            // Fallback or null -- no implicit formatter
            status = U_ZERO_ERROR;
            return randVal;
        } // There might be an implicit formatter
        const Formattable& toFormat = contained->unwrap();
        // If it has an object type, there might be an implicit formatter for it...
        switch (toFormat.getType()) {
        case UFMT_OBJECT: {
            const FormattableObject* obj = toFormat.getObject(status);
            U_ASSERT(U_SUCCESS(status));
            U_ASSERT(obj != nullptr);
            const UnicodeString& type = obj->tag();
            FunctionName functionName;
            if (!getDefaultFormatterNameByType(type, functionName)) {
                // No formatter for this type -- follow default behavior
                return randVal;
            }
            // ... apply the implicit formatter
            return apply(globalEnv,
                         functionName,
                         randVal,
                         FunctionOptions(),
                         context,
                         status);
        } // No formatters for other types, so just return the evaluated operand
        default:
            return randVal;
        } // End of non-function-call case
    } else {
        // Don't call the function on error values
        if (randVal.isFallback())
            return randVal;
        const Operator* rator = expr.getOperator(status); // Get the operator from the expression
        U_ASSERT(U_SUCCESS(status)); // This must succeed since we checked that it's a function call
        const FunctionName& functionName = rator->getFunctionName(); // Get function name
        const OptionMap& options = rator->getOptionsInternal();      // Get options
        FunctionOptions resolvedOptions = resolveOptions(globalEnv, options, context, status); // Resolve options

        // Call the function with the operand and arguments
        return apply(globalEnv, functionName,
                     randVal, std::move(resolvedOptions), context, status);
    }
}

// Patterns
// --------

// Formats each text and expression part of a pattern, appending the results to `result`
void MessageFormatter::formatPattern(MessageContext& context,
                                     Environment& globalEnv,
                                     const Pattern& pat,
                                     UErrorCode &status, UnicodeString& result) const {
    CHECK_ERROR(status);

    for (int32_t i = 0; i < pat.numParts(); i++) {
        const PatternPart& part = pat.getPart(i);
        if (part.isText()) {
            result += part.asText();
        } else if (part.isMarkup()) {
            validateUOptionsOnMarkup(context, globalEnv, part.asMarkup(), status);
        } else {
	      // Format the expression
	      InternalValue& partVal = evalExpression({}, globalEnv, part.contents(), context, status);
              if (partVal.isFallback()) {
                  result += LEFT_CURLY_BRACE;
                  result += partVal.asFallback();
                  result += RIGHT_CURLY_BRACE;
              } else {
                  // Get the `FunctionValue` corresponding to this part
                  const FunctionValue* val = partVal.getValue(status);
                  // It shouldn't be null or a fallback
                  U_ASSERT(U_SUCCESS(status));

                  // See comment in matchSelectorKeys()
                  bool badSelectOption = !checkSelectOption(*val);

                  // Format the `FunctionValue` to a string
                  UnicodeString fmt = val->formatToString(status);

                  // Apply bidi isolation to the formatted result
                  UMFDirectionality dir = val->getDirection();
                  result += *(ubidi_isolate(bidiIsolationStrategy,
                                            msgdir,
                                            bidiIsolationStyle,
                                            val->getDirectionAnnotation(),
                                            dir,
                                            fmt));

                  if (badSelectOption) {
                      context.getErrors().setBadOption(val->getFunctionName(), status);
                      CHECK_ERROR(status);
                  }

                  // Handle formatting errors. `formatToString()` can't take a context and thus can't
                  // register an error directly
                  if (status == U_MF_FORMATTING_ERROR) {
                      status = U_ZERO_ERROR;
                      // The name of the formatter that failed is unavailable.
                      // Not ideal, but it's hard for `formatToString()`
                      // to pass along more detailed diagnostics
                      context.getErrors().setFormattingError(status);
                  } else if (status == U_MF_BAD_OPTION) {
                      status = U_ZERO_ERROR;
                      context.getErrors().setBadOption(status);
                  }
              }
        }
    }
}

// ------------------------------------------------------
// Selection

// See https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#resolve-selectors
// `res` is a vector of ResolvedSelectors
void MessageFormatter::resolveSelectors(MessageContext& context, Environment& env, UErrorCode &status, UVector& res) const {
    CHECK_ERROR(status);
    U_ASSERT(!dataModel.hasPattern());

    const VariableName* selectors = dataModel.getSelectorsInternal();
    // 1. Let res be a new empty list of resolved values that support selection.
    // (Implicit, since `res` is an out-parameter)
    // 2. For each expression exp of the message's selectors
    for (int32_t i = 0; i < dataModel.numSelectors(); i++) {
        // 2i. Let rv be the resolved value of exp.
        InternalValue& rv = evalVariableReference({}, env, selectors[i], context, status);
        if (rv.isSelectable()) {
            // 2ii. If selection is supported for rv:
            // (True if this code has been reached)
        } else {
            // 2iii. Else:
            // Let nomatch be a resolved value for which selection always fails.
            // Append nomatch as the last element of the list res.
            // Emit a Selection Error.
            // (Note: in this case, rv, being a fallback, serves as `nomatch`)
            context.getErrors().setSelectorError({}, status);
        }
        // 2ii(a). Append rv as the last element of the list res.
        // (Also fulfills 2iii)
        LocalPointer<InternalValue> v(create<InternalValue>(std::move(rv), status));
        CHECK_ERROR(status);
        res.adoptElement(v.orphan(), status);
    }
}

bool MessageFormatter::checkSelectOption(const FunctionValue& val) const {
    const UnicodeString& name = val.getFunctionName();

    if (name != functions::NUMBER && name != functions::INTEGER) {
        return true;
    }

    // Per the spec, if the "select" option is present, it must have been
    // set from a literal

    // Returns false if the `select` option is present and it was not set from a literal

    const FunctionOptions& opts = val.getResolvedOptions();

    // OK if the option wasn't present
    UErrorCode localErrorCode = U_ZERO_ERROR;
    opts.getFunctionOption(options::SELECT, localErrorCode);
    if (U_FAILURE(localErrorCode)) {
        return true;
    }
    // Otherwise, return true if the option was set from a literal
    return opts.wasSetFromLiteral(options::SELECT);
}

// See https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#resolve-preferences
// `keys` and `matches` are vectors of strings
void MessageFormatter::matchSelectorKeys(const UVector& keys,
                                         MessageContext& context,
					 InternalValue&& rv,
					 UVector& keysOut,
					 UErrorCode& status) const {
    CHECK_ERROR(status);

    if (!rv.isSelectable()) {
        return;
    }

    UErrorCode savedStatus = status;

    // Convert `keys` to an array
    int32_t keysLen = keys.size();
    UnicodeString* keysArr = new UnicodeString[keysLen];
    if (keysArr == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    for (int32_t i = 0; i < keysLen; i++) {
        const UnicodeString* k = static_cast<UnicodeString*>(keys[i]);
        U_ASSERT(k != nullptr);
        keysArr[i] = *k;
    }
    LocalArray<UnicodeString> adoptedKeys(keysArr);

    // Create an array to hold the output
    int32_t* prefsArr = static_cast<int32_t*>(uprv_malloc(keysLen * sizeof(int32_t)));
    if (prefsArr == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    int32_t prefsLen = 0;

    // Call the selector
    // Caller checked for fallback, so it's safe to call getValue()
    const FunctionValue* rvVal = rv.getValue(status);
    U_ASSERT(U_SUCCESS(status));

    // This condition can't be checked in the selector.
    bool badSelectOption = !checkSelectOption(*rvVal);

    rvVal->selectKeys(adoptedKeys.getAlias(), keysLen, prefsArr, prefsLen,
                      status);

    if (badSelectOption) {
        context.getErrors().setBadOption(rvVal->getFunctionName(), status);
        CHECK_ERROR(status);
        // In this case, only the `*` variant should match
        prefsLen = 0;
    }

    // Update errors
    if (savedStatus != status) {
        if (U_FAILURE(status)) {
            status = U_ZERO_ERROR;
            context.getErrors().setSelectorError({}, status);
        } else {
            // Ignore warnings
            status = savedStatus;
        }
    }

    CHECK_ERROR(status);

    // Copy the resulting keys (if there was no error)
    keysOut.removeAllElements();
    for (int32_t i = 0; i < prefsLen; i++) {
        UnicodeString* k =
            message2::create<UnicodeString>(std::move(keysArr[prefsArr[i]]), status);
        if (k == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        keysOut.adoptElement(k, status);
        CHECK_ERROR(status);
    }

    uprv_free(prefsArr);
}

// See https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#resolve-preferences
// `res` is a vector of FormattedPlaceholders;
// `pref` is a vector of vectors of strings
void MessageFormatter::resolvePreferences(MessageContext& context,
                                          UVector& res,
                                          UVector& pref,
                                          UErrorCode &status) const {
    CHECK_ERROR(status);

    // 1. Let pref be a new empty list of lists of strings.
    // (Implicit, since `pref` is an out-parameter)
    UnicodeString ks;
    LocalPointer<UnicodeString> ksP;
    int32_t numVariants = dataModel.numVariants();
    const Variant* variants = dataModel.getVariantsInternal();
    // 2. For each index i in res
    for (int32_t i = 0; i < res.size(); i++) {
        // 2i. Let keys be a new empty list of strings.
        LocalPointer<UVector> keys(createUVector(status));
        CHECK_ERROR(status);
        // 2ii. For each variant `var` of the message
        for (int32_t variantNum = 0; variantNum < numVariants; variantNum++) {
            const SelectorKeys& selectorKeys = variants[variantNum].getKeys();

            // Note: Here, `var` names the key list of `var`,
            // not a Variant itself
            const Key* var = selectorKeys.getKeysInternal();
            // 2ii(a). Let `key` be the `var` key at position i.
            U_ASSERT(i < selectorKeys.len); // established by semantic check in formatSelectors()
            const Key& key = var[i];
            // 2ii(b). If `key` is not the catch-all key '*'
            if (!key.isWildcard()) {
                // 2ii(b)(a) Assert that key is a literal.
                // (Not needed)
                // 2ii(b)(b) Let `ks` be the resolved value of `key` in Unicode Normalization Form C.
                ks = StandardFunctions::normalizeNFC(key.asLiteral().unquoted());
                // 2ii(b)(c) Append `ks` as the last element of the list `keys`.
                ksP.adoptInstead(create<UnicodeString>(std::move(ks), status));
                CHECK_ERROR(status);
                keys->adoptElement(ksP.orphan(), status);
            }
        }
        // 2iii. Let `rv` be the resolved value at index `i` of `res`.
        U_ASSERT(i < res.size());
        InternalValue rv = std::move(*(static_cast<InternalValue*>(res[i])));
        // 2iv. Let matches be the result of calling the method MatchSelectorKeys(rv, keys)
        LocalPointer<UVector> matches(createUVector(status));
        matchSelectorKeys(*keys, context, std::move(rv), *matches, status);
        // 2v. Append `matches` as the last element of the list `pref`
        pref.adoptElement(matches.orphan(), status);
    }
}

// `v` is assumed to be a vector of strings
static int32_t vectorFind(const UVector& v, const UnicodeString& k) {
    for (int32_t i = 0; i < v.size(); i++) {
        if (*static_cast<UnicodeString*>(v[i]) == k) {
            return i;
        }
    }
    return -1;
}

static UBool vectorContains(const UVector& v, const UnicodeString& k) {
    return (vectorFind(v, k) != -1);
}

// See https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#filter-variants
// `pref` is a vector of vectors of strings. `vars` is a vector of PrioritizedVariants
void MessageFormatter::filterVariants(const UVector& pref, UVector& vars, UErrorCode& status) const {
    const Variant* variants = dataModel.getVariantsInternal();

    // 1. Let `vars` be a new empty list of variants.
    // (Not needed since `vars` is an out-parameter)
    // 2. For each variant `var` of the message:
    for (int32_t j = 0; j < dataModel.numVariants(); j++) {
        const SelectorKeys& selectorKeys = variants[j].getKeys();
        const Pattern& p = variants[j].getPattern();

        // Note: Here, `var` names the key list of `var`,
        // not a Variant itself
        const Key* var = selectorKeys.getKeysInternal();
        // 2i. For each index `i` in `pref`:
        bool noMatch = false;
        for (int32_t i = 0; i < pref.size(); i++) {
            // 2i(a). Let `key` be the `var` key at position `i`.
            U_ASSERT(i < selectorKeys.len);
            const Key& key = var[i];
            // 2i(b). If key is the catch-all key '*':
            if (key.isWildcard()) {
                // 2i(b)(a). Continue the inner loop on pref.
                continue;
            }
            // 2i(c). Assert that `key` is a literal.
            // (Not needed)
            // 2i(d). Let `ks` be the resolved value of `key`.
            UnicodeString ks = StandardFunctions::normalizeNFC(key.asLiteral().unquoted());
            // 2i(e). Let `matches` be the list of strings at index `i` of `pref`.
            const UVector& matches = *(static_cast<UVector*>(pref[i])); // `matches` is a vector of strings
            // 2i(f). If `matches` includes `ks`
            if (vectorContains(matches, ks)) {
                // 2i(f)(a). Continue the inner loop on `pref`.
                continue;
            }
            // 2i(g). Else:
            // 2i(g)(a). Continue the outer loop on message variants.
            noMatch = true;
            break;
        }
        if (!noMatch) {
            // Append `var` as the last element of the list `vars`.
	    PrioritizedVariant* tuple = create<PrioritizedVariant>(PrioritizedVariant(-1, selectorKeys, p), status);
            CHECK_ERROR(status);
            vars.adoptElement(tuple, status);
        }
    }
}

// See https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#sort-variants
// Leaves the preferred variant as element 0 in `sortable`
// Note: this sorts in-place, so `sortable` is just `vars`
// `pref` is a vector of vectors of strings; `vars` is a vector of PrioritizedVariants
void MessageFormatter::sortVariants(const UVector& pref, UVector& vars, UErrorCode& status) const {
    CHECK_ERROR(status);

// Note: steps 1 and 2 are omitted since we use `vars` as `sortable` (we sort in-place)
    // 1. Let `sortable` be a new empty list of (integer, variant) tuples.
    // (Not needed since `sortable` is an out-parameter)
    // 2. For each variant `var` of `vars`
    // 2i. Let tuple be a new tuple (-1, var).
    // 2ii. Append `tuple` as the last element of the list `sortable`.

    // 3. Let `len` be the integer count of items in `pref`.
    int32_t len = pref.size();
    // 4. Let `i` be `len` - 1.
    int32_t i = len - 1;
    // 5. While i >= 0:
    while (i >= 0) {
        // 5i. Let `matches` be the list of strings at index `i` of `pref`.
        U_ASSERT(pref[i] != nullptr);
	const UVector& matches = *(static_cast<UVector*>(pref[i])); // `matches` is a vector of strings
        // 5ii. Let `minpref` be the integer count of items in `matches`.
        int32_t minpref = matches.size();
        // 5iii. For each tuple `tuple` of `sortable`:
        for (int32_t j = 0; j < vars.size(); j++) {
            U_ASSERT(vars[j] != nullptr);
            PrioritizedVariant& tuple = *(static_cast<PrioritizedVariant*>(vars[j]));
            // 5iii(a). Let matchpref be an integer with the value minpref.
            int32_t matchpref = minpref;
            // 5iii(b). Let `key` be the tuple variant key at position `i`.
            const Key* tupleVariantKeys = tuple.keys.getKeysInternal();
            U_ASSERT(i < tuple.keys.len); // Given by earlier semantic checking
            const Key& key = tupleVariantKeys[i];
            // 5iii(c) If `key` is not the catch-all key '*':
            if (!key.isWildcard()) {
                // 5iii(c)(a). Assert that `key` is a literal.
                // (Not needed)
                // 5iii(c)(b). Let `ks` be the resolved value of `key`.
                UnicodeString ks = StandardFunctions::normalizeNFC(key.asLiteral().unquoted());
                // 5iii(c)(c) Let matchpref be the integer position of ks in `matches`.
                matchpref = vectorFind(matches, ks);
                U_ASSERT(matchpref >= 0);
            }
            // 5iii(d) Set the `tuple` integer value as matchpref.
            tuple.priority = matchpref;
        }
        // 5iv. Set `sortable` to be the result of calling the method SortVariants(`sortable`)
        vars.sort(comparePrioritizedVariants, status);
        CHECK_ERROR(status);
        // 5v. Set `i` to be `i` - 1.
        i--;
    }
    // The caller is responsible for steps 6 and 7
    // 6. Let `var` be the `variant` element of the first element of `sortable`.
    // 7. Select the pattern of `var`
}

void MessageFormatter::formatSelectors(MessageContext& context,
                                       Environment& env,
                                       UErrorCode &status,
                                       UnicodeString& result) const {
    CHECK_ERROR(status);

    // See https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#pattern-selection

    // Resolve Selectors
    // res is a vector of InternalValues
    LocalPointer<UVector> res(createUVector(status));
    CHECK_ERROR(status);
    resolveSelectors(context, env, status, *res);

    // Resolve Preferences
    // pref is a vector of vectors of strings
    LocalPointer<UVector> pref(createUVector(status));
    CHECK_ERROR(status);
    resolvePreferences(context, *res, *pref, status);

    // Filter Variants
    // vars is a vector of PrioritizedVariants
    LocalPointer<UVector> vars(createUVector(status));
    CHECK_ERROR(status);
    filterVariants(*pref, *vars, status);

    // Sort Variants and select the final pattern
    // Note: `sortable` in the spec is just `vars` here,
    // which is sorted in-place
    sortVariants(*pref, *vars, status);

    CHECK_ERROR(status);

    // 6. Let `var` be the `variant` element of the first element of `sortable`.
    U_ASSERT(vars->size() > 0); // This should have been checked earlier (having 0 variants would be a data model error)
    const PrioritizedVariant& var = *(static_cast<PrioritizedVariant*>(vars->elementAt(0)));
    // 7. Select the pattern of `var`
    const Pattern& pat = var.pat;

    // Format the pattern
    formatPattern(context, env, pat, status, result);
}

// Formatting to string
// --------------------

UnicodeString MessageFormatter::formatToString(const MessageArguments& arguments, UErrorCode &status) {
    EMPTY_ON_ERROR(status);

    // Create a new environment that will store closures for all local variables
    Environment* env = Environment::create(status);

    // Create a new context with the given arguments and the `errors` structure
    MessageContext context(arguments, *errors, status);

    // Check for unresolved variable errors
    checkDeclarations(context, env, status);
    LocalPointer<Environment> globalEnv(env);

    DynamicErrors& err = context.getErrors();
    UnicodeString result;

    if (!(err.hasSyntaxError() || err.hasDataModelError())) {
        if (dataModel.hasPattern()) {
            formatPattern(context, *globalEnv, dataModel.getPattern(), status, result);
        } else {
            // Check for errors/warnings -- if so, then the result of pattern selection is the fallback value
            // See https://www.unicode.org/reports/tr35/tr35-messageFormat.html#pattern-selection
            const DynamicErrors& err = context.getErrors();
            if (err.hasSyntaxError() || err.hasDataModelError()) {
                result += REPLACEMENT;
            } else {
                formatSelectors(context, *globalEnv, status, result);
            }
        }
    }

    // Update status according to all errors seen while formatting
    if (signalErrors) {
        context.checkErrors(status);
    }
    if (U_FAILURE(status)) {
        result.remove();
    }
    return result;
}

// Markup
// ------

// Evaluates `rand` and requires the value to be a string, setting `result` to it
// if so, and setting a bad option error if not
bool MessageFormatter::operandToStringWithBadOptionError(MessageContext& context,
                                                         Environment& globalEnv,
                                                         const Operand& rand,
                                                         UnicodeString& result,
                                                         UErrorCode& status) const {
    EMPTY_ON_ERROR(status);

    InternalValue& iVal = evalOperand({}, globalEnv, rand, context, status);
    EMPTY_ON_ERROR(status);
    const FunctionValue* val = iVal.getValue(status);
    U_ASSERT(U_SUCCESS(status));

    result = val->unwrap().getString(status);
    if (U_FAILURE(status)) {
        status = U_ZERO_ERROR;
        context.getErrors().setBadOption({}, status);
        return false;
    }
    return true;
}

// Validates u: options on markup parts -- see
// https://github.com/unicode-org/message-format-wg/blob/main/spec/u-namespace.md
void MessageFormatter::validateUOptionsOnMarkup(MessageContext& context,
                                                Environment& globalEnv,
                                                const Markup& markupPart,
                                                UErrorCode& status) const {
    CHECK_ERROR(status);

    const OptionMap& opts = markupPart.getOptionsInternal();
    for (int32_t i = 0; i < opts.len; i++) {
        const Option& opt = opts.options[i];
        const UnicodeString& optionName = opt.getName();
        const Operand& optionValue = opt.getValue();

        if (optionName == options::U_ID) {
            UnicodeString ignore;
            operandToStringWithBadOptionError(context, globalEnv, optionValue, ignore, status);
        } else if (optionName == options::U_DIR) {
            // Can't be set on markup
            context.getErrors().setBadOption({}, status);
        }
        // Any other options are ignored
    }
}

// ----------------------------------------
// Checking for resolution errors

void MessageFormatter::check(MessageContext& context, const Environment& localEnv, const OptionMap& options, UErrorCode& status) const {
    // Check the RHS of each option
    for (int32_t i = 0; i < options.size(); i++) {
        const Option& opt = options.getOption(i, status);
        CHECK_ERROR(status);
        check(context, localEnv, opt.getValue(), status);
    }
}

void MessageFormatter::check(MessageContext& context, const Environment& localEnv, const Operand& rand, UErrorCode& status) const {
    // Nothing to check for literals
    if (rand.isLiteral() || rand.isNull()) {
        return;
    }

    // Check that variable is in scope
    const VariableName& var = rand.asVariable();
    UnicodeString normalized = StandardFunctions::normalizeNFC(var);

    // Check local scope
    if (localEnv.has(normalized)) {
        return;
    }
    // Check global scope
    context.getGlobal(normalized, status);
    if (status == U_ILLEGAL_ARGUMENT_ERROR) {
        status = U_ZERO_ERROR;
        context.getErrors().setUnresolvedVariable(var, status);
    }
    // Either `var` is a global, or some other error occurred.
    // Nothing more to do either way
    return;
}

void MessageFormatter::check(MessageContext& context, const Environment& localEnv, const Expression& expr, UErrorCode& status) const {
    // Check for unresolved variable errors
    if (expr.isFunctionCall()) {
        const Operator* rator = expr.getOperator(status);
        U_ASSERT(U_SUCCESS(status));
        const Operand& rand = expr.getOperand();
        check(context, localEnv, rand, status);
        check(context, localEnv, rator->getOptionsInternal(), status);
    }
}

// Check for resolution errors
void MessageFormatter::checkDeclarations(MessageContext& context, Environment*& env, UErrorCode &status) const {
    CHECK_ERROR(status);

    const Binding* decls = getDataModel().getLocalVariablesInternal();
    U_ASSERT(env != nullptr && (decls != nullptr || getDataModel().bindingsLen == 0));

    for (int32_t i = 0; i < getDataModel().bindingsLen; i++) {
        const Binding& decl = decls[i];
        const Expression& rhs = decl.getValue();
        check(context, *env, rhs, status);

        // Add a closure to the global environment,
        // memoizing the value of localEnv up to this point

        // Add the LHS to the environment for checking the next declaration
        const VariableName& lhs = decl.getVariable();
        env = Environment::create(StandardFunctions::normalizeNFC(lhs),
                                  Closure::create(rhs, *env, status),
                                  varFallback(lhs),
                                  env,
                                  status);
        CHECK_ERROR(status);
    }
}
} // namespace message2

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* #if !UCONFIG_NO_NORMALIZATION */

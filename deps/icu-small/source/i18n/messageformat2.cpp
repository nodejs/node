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

// The result of formatting a literal is just itself.
static Formattable evalLiteral(const Literal& lit) {
    return Formattable(lit.unquoted());
}

// Assumes that `var` is a message argument; returns the argument's value.
[[nodiscard]] FormattedPlaceholder MessageFormatter::evalArgument(const UnicodeString& fallback,
                                                                  const VariableName& var,
                                                                  MessageContext& context,
                                                                  UErrorCode& errorCode) const {
    if (U_SUCCESS(errorCode)) {
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
            return (FormattedPlaceholder(*val, fallbackToUse));
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
[[nodiscard]] FormattedPlaceholder MessageFormatter::formatLiteral(const UnicodeString& fallback,
                                                                   const Literal& lit) const {
    // The fallback for a literal is itself, unless another fallback is passed in
    // (same reasoning as evalArgument())
    UnicodeString fallbackToUse = fallback.isEmpty() ? reserialize(lit.unquoted()) : fallback;
    return FormattedPlaceholder(evalLiteral(lit), fallbackToUse);
}

[[nodiscard]] InternalValue* MessageFormatter::formatOperand(const UnicodeString& fallback,
                                                             const Environment& env,
                                                             const Operand& rand,
                                                             MessageContext& context,
                                                             UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return {};
    }

    if (rand.isNull()) {
        return create<InternalValue>(InternalValue(FormattedPlaceholder()), status);
    }
    if (rand.isVariable()) {
        // Check if it's local or global
        // Note: there is no name shadowing; this is enforced by the parser
        const VariableName& var = rand.asVariable();
        // TODO: Currently, this code implements lazy evaluation of locals.
        // That is, the environment binds names to a closure, not a resolved value.
        // Eager vs. lazy evaluation is an open issue:
        // see https://github.com/unicode-org/message-format-wg/issues/299

        // NFC-normalize the variable name. See
        // https://github.com/unicode-org/message-format-wg/blob/main/spec/syntax.md#names-and-identifiers
        const VariableName normalized = StandardFunctions::normalizeNFC(var);

        // Look up the variable in the environment
        if (env.has(normalized)) {
          // `var` is a local -- look it up
          const Closure& rhs = env.lookup(normalized);
          // Format the expression using the environment from the closure
          // The name of this local variable is the fallback for its RHS.
          UnicodeString newFallback(DOLLAR);
          newFallback += var;
          return formatExpression(newFallback, rhs.getEnv(), rhs.getExpr(), context, status);
        }
        // Variable wasn't found in locals -- check if it's global
        FormattedPlaceholder result = evalArgument(fallback, normalized, context, status);
        if (status == U_ILLEGAL_ARGUMENT_ERROR) {
            status = U_ZERO_ERROR;
            // Unbound variable -- set a resolution error
            context.getErrors().setUnresolvedVariable(var, status);
            // Use fallback per
            // https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#fallback-resolution
            UnicodeString str(DOLLAR);
            str += var;
            return create<InternalValue>(InternalValue(FormattedPlaceholder(str)), status);
        }
        return create<InternalValue>(InternalValue(std::move(result)), status);
    } else {
        U_ASSERT(rand.isLiteral());
        return create<InternalValue>(InternalValue(formatLiteral(fallback, rand.asLiteral())), status);
    }
}

// Resolves a function's options
FunctionOptions MessageFormatter::resolveOptions(const Environment& env, const OptionMap& options, MessageContext& context, UErrorCode& status) const {
    LocalPointer<UVector> optionsVector(createUVector(status));
    if (U_FAILURE(status)) {
        return {};
    }
    LocalPointer<ResolvedFunctionOption> resolvedOpt;
    for (int i = 0; i < options.size(); i++) {
        const Option& opt = options.getOption(i, status);
        if (U_FAILURE(status)) {
            return {};
        }
        const UnicodeString& k = opt.getName();
        const Operand& v = opt.getValue();

        // Options are fully evaluated before calling the function
        // Format the operand
        LocalPointer<InternalValue> rhsVal(formatOperand({}, env, v, context, status));
        if (U_FAILURE(status)) {
            return {};
        }
        // Note: this means option values are "eagerly" evaluated.
        // Currently, options don't have options. This will be addressed by the
        // full FormattedPlaceholder redesign.
        FormattedPlaceholder optValue = rhsVal->forceFormatting(context.getErrors(), status);
        resolvedOpt.adoptInstead(create<ResolvedFunctionOption>
                                 (ResolvedFunctionOption(k,
                                                         optValue.asFormattable(),
                                                         v.isLiteral()),
                                  status));
        if (U_FAILURE(status)) {
            return {};
        }
        optionsVector->adoptElement(resolvedOpt.orphan(), status);
    }
    return FunctionOptions(std::move(*optionsVector), status);
}

// Overload that dispatches on argument type. Syntax doesn't provide for options in this case.
[[nodiscard]] InternalValue* MessageFormatter::evalFunctionCall(FormattedPlaceholder&& argument,
                                                                MessageContext& context,
                                                                UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return nullptr;
    }

    // These cases should have been checked for already
    U_ASSERT(!argument.isFallback() && !argument.isNullOperand());

    const Formattable& toFormat = argument.asFormattable();
    switch (toFormat.getType()) {
    case UFMT_OBJECT: {
        const FormattableObject* obj = toFormat.getObject(status);
        U_ASSERT(U_SUCCESS(status));
        U_ASSERT(obj != nullptr);
        const UnicodeString& type = obj->tag();
        FunctionName functionName;
        if (!getDefaultFormatterNameByType(type, functionName)) {
            // No formatter for this type -- follow default behavior
            break;
        }
        return evalFunctionCall(functionName,
                                create<InternalValue>(std::move(argument), status),
                                FunctionOptions(),
                                context,
                                status);
    }
    default: {
        // TODO: The array case isn't handled yet; not sure whether it's desirable
        // to have a default list formatter
        break;
    }
    }
    // No formatter for this type, or it's a primitive type (which will be formatted later)
    // -- just return the argument itself
    return create<InternalValue>(std::move(argument), status);
}

// Overload that dispatches on function name
// Adopts `arg`
[[nodiscard]] InternalValue* MessageFormatter::evalFunctionCall(const FunctionName& functionName,
                                                                InternalValue* arg_,
                                                                FunctionOptions&& options,
                                                                MessageContext& context,
                                                                UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return {};
    }

    LocalPointer<InternalValue> arg(arg_);

    // Look up the formatter or selector
    LocalPointer<Formatter> formatterImpl(nullptr);
    LocalPointer<Selector> selectorImpl(nullptr);
    if (isFormatter(functionName)) {
        formatterImpl.adoptInstead(getFormatter(functionName, status));
        U_ASSERT(U_SUCCESS(status));
    }
    if (isSelector(functionName)) {
        selectorImpl.adoptInstead(getSelector(context, functionName, status));
        U_ASSERT(U_SUCCESS(status));
    }
    if (formatterImpl == nullptr && selectorImpl == nullptr) {
        // Unknown function error
        context.getErrors().setUnknownFunction(functionName, status);

        if (arg->hasNullOperand()) {
            // Non-selector used as selector; an error would have been recorded earlier
            UnicodeString fallback(COLON);
            fallback += functionName;
            return new InternalValue(FormattedPlaceholder(fallback));
        } else {
            return new InternalValue(FormattedPlaceholder(arg->getFallback()));
        }
    }
    return new InternalValue(arg.orphan(),
                             std::move(options),
                             functionName,
                             formatterImpl.isValid() ? formatterImpl.orphan() : nullptr,
                             selectorImpl.isValid() ? selectorImpl.orphan() : nullptr);
}

// Formats an expression using `globalEnv` for the values of variables
[[nodiscard]] InternalValue* MessageFormatter::formatExpression(const UnicodeString& fallback,
                                                                const Environment& globalEnv,
                                                                const Expression& expr,
                                                                MessageContext& context,
                                                                UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return {};
    }

    const Operand& rand = expr.getOperand();
    // Format the operand (formatOperand handles the case of a null operand)
    LocalPointer<InternalValue> randVal(formatOperand(fallback, globalEnv, rand, context, status));

    FormattedPlaceholder maybeRand = randVal->takeArgument(status);

    if (!expr.isFunctionCall() && U_SUCCESS(status)) {
        // Dispatch based on type of `randVal`
         if (maybeRand.isFallback()) {
            return randVal.orphan();
        }
        return evalFunctionCall(std::move(maybeRand), context, status);
    } else if (expr.isFunctionCall()) {
        status = U_ZERO_ERROR;
        const Operator* rator = expr.getOperator(status);
        U_ASSERT(U_SUCCESS(status));
        const FunctionName& functionName = rator->getFunctionName();
        const OptionMap& options = rator->getOptionsInternal();
        // Resolve the options
        FunctionOptions resolvedOptions = resolveOptions(globalEnv, options, context, status);

        // Call the formatter function
        return evalFunctionCall(functionName,
                                randVal.orphan(),
                                std::move(resolvedOptions),
                                context,
                                status);
    } else {
        status = U_ZERO_ERROR;
        return randVal.orphan();
    }
}

// Formats each text and expression part of a pattern, appending the results to `result`
void MessageFormatter::formatPattern(MessageContext& context, const Environment& globalEnv, const Pattern& pat, UErrorCode &status, UnicodeString& result) const {
    CHECK_ERROR(status);

    for (int32_t i = 0; i < pat.numParts(); i++) {
        const PatternPart& part = pat.getPart(i);
        if (part.isText()) {
            result += part.asText();
        } else if (part.isMarkup()) {
            // Markup is ignored
        } else {
	      // Format the expression
              LocalPointer<InternalValue> partVal(
                  formatExpression({}, globalEnv, part.contents(), context, status));
              FormattedPlaceholder partResult = partVal->forceFormatting(context.getErrors(),
                                                                         status);
              // Force full evaluation, e.g. applying default formatters to
	      // unformatted input (or formatting numbers as strings)
              result += partResult.formatToString(locale, status);
              // Handle formatting errors. `formatToString()` can't take a context and thus can't
              // register an error directly
              if (status == U_MF_FORMATTING_ERROR) {
                  status = U_ZERO_ERROR;
                  // TODO: The name of the formatter that failed is unavailable.
                  // Not ideal, but it's hard for `formatToString()`
                  // to pass along more detailed diagnostics
                  context.getErrors().setFormattingError(status);
              }
        }
    }
}

// ------------------------------------------------------
// Selection

// See https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#resolve-selectors
// `res` is a vector of ResolvedSelectors
void MessageFormatter::resolveSelectors(MessageContext& context, const Environment& env, UErrorCode &status, UVector& res) const {
    CHECK_ERROR(status);
    U_ASSERT(!dataModel.hasPattern());

    const VariableName* selectors = dataModel.getSelectorsInternal();
    // 1. Let res be a new empty list of resolved values that support selection.
    // (Implicit, since `res` is an out-parameter)
    // 2. For each expression exp of the message's selectors
    for (int32_t i = 0; i < dataModel.numSelectors(); i++) {
        // 2i. Let rv be the resolved value of exp.
        LocalPointer<InternalValue> rv(formatOperand({}, env, Operand(selectors[i]), context, status));
        if (rv->canSelect()) {
            // 2ii. If selection is supported for rv:
            // (True if this code has been reached)
        } else {
            // 2iii. Else:
            // Let nomatch be a resolved value for which selection always fails.
            // Append nomatch as the last element of the list res.
            // Emit a Selection Error.
            // (Note: in this case, rv, being a fallback, serves as `nomatch`)
            DynamicErrors& err = context.getErrors();
            err.setSelectorError(rv->getFunctionName(), status);
            rv.adoptInstead(new InternalValue(FormattedPlaceholder(rv->getFallback())));
            if (!rv.isValid()) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
        }
        // 2ii(a). Append rv as the last element of the list res.
        // (Also fulfills 2iii)
        res.adoptElement(rv.orphan(), status);
    }
}

// See https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#resolve-preferences
// `keys` and `matches` are vectors of strings
void MessageFormatter::matchSelectorKeys(const UVector& keys,
                                         MessageContext& context,
					 InternalValue* rv, // Does not adopt `rv`
					 UVector& keysOut,
					 UErrorCode& status) const {
    CHECK_ERROR(status);

    if (U_FAILURE(status)) {
        // Return an empty list of matches
        status = U_ZERO_ERROR;
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
    UnicodeString* prefsArr = new UnicodeString[keysLen];
    if (prefsArr == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    LocalArray<UnicodeString> adoptedPrefs(prefsArr);
    int32_t prefsLen = 0;

    // Call the selector
    FunctionName name = rv->getFunctionName();
    rv->forceSelection(context.getErrors(),
                       adoptedKeys.getAlias(), keysLen,
                       adoptedPrefs.getAlias(), prefsLen,
                       status);

    // Update errors
    if (savedStatus != status) {
        if (U_FAILURE(status)) {
            status = U_ZERO_ERROR;
            context.getErrors().setSelectorError(name, status);
        } else {
            // Ignore warnings
            status = savedStatus;
        }
    }

    CHECK_ERROR(status);

    // Copy the resulting keys (if there was no error)
    keysOut.removeAllElements();
    for (int32_t i = 0; i < prefsLen; i++) {
        UnicodeString* k = message2::create<UnicodeString>(std::move(prefsArr[i]), status);
        if (k == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        keysOut.adoptElement(k, status);
        CHECK_ERROR(status);
    }
}

// See https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md#resolve-preferences
// `res` is a vector of FormattedPlaceholders;
// `pref` is a vector of vectors of strings
void MessageFormatter::resolvePreferences(MessageContext& context, UVector& res, UVector& pref, UErrorCode &status) const {
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
        InternalValue* rv = static_cast<InternalValue*>(res[i]);
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

void MessageFormatter::formatSelectors(MessageContext& context, const Environment& env, UErrorCode &status, UnicodeString& result) const {
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

// Note: this is non-const due to the function registry being non-const, which is in turn
// due to the values (`FormatterFactory` objects in the map) having mutable state.
// In other words, formatting a message can mutate the underlying `MessageFormatter` by changing
// state within the factory objects that represent custom formatters.
UnicodeString MessageFormatter::formatToString(const MessageArguments& arguments, UErrorCode &status) {
    EMPTY_ON_ERROR(status);

    // Create a new context with the given arguments and the `errors` structure
    MessageContext context(arguments, *errors, status);
    UnicodeString result;

    if (!(errors->hasSyntaxError() || errors->hasDataModelError())) {
        // Create a new environment that will store closures for all local variables
        // Check for unresolved variable errors
        // checkDeclarations needs a reference to the pointer to the environment
        // since it uses its `env` argument as an out-parameter. So it needs to be
        // temporarily not a LocalPointer...
        Environment* env(Environment::create(status));
        checkDeclarations(context, env, status);
        // ...and then it's adopted to avoid leaks
        LocalPointer<Environment> globalEnv(env);

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
        env = Environment::create(StandardFunctions::normalizeNFC(decl.getVariable()),
                                  Closure(rhs, *env),
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

/*
******************************************************************************
* Copyright (C) 2014-2016, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* simpleformatter.h
*/

#ifndef __SIMPLEFORMATTER_H__
#define __SIMPLEFORMATTER_H__

/**
 * \file
 * \brief C++ API: Simple formatter, minimal subset of MessageFormat.
 */

#include "unicode/utypes.h"
#include "unicode/unistr.h"

#ifndef U_HIDE_DRAFT_API

U_NAMESPACE_BEGIN

/**
 * Formats simple patterns like "{1} was born in {0}".
 * Minimal subset of MessageFormat; fast, simple, minimal dependencies.
 * Supports only numbered arguments with no type nor style parameters,
 * and formats only string values.
 * Quoting via ASCII apostrophe compatible with ICU MessageFormat default behavior.
 *
 * Factory methods set error codes for syntax errors
 * and for too few or too many arguments/placeholders.
 *
 * SimpleFormatter objects are thread-safe except for assignment and applying new patterns.
 *
 * Example:
 * <pre>
 * UErrorCode errorCode = U_ZERO_ERROR;
 * SimpleFormatter fmt("{1} '{born}' in {0}", errorCode);
 * UnicodeString result;
 *
 * // Output: "paul {born} in england"
 * fmt.format("england", "paul", result, errorCode);
 * </pre>
 *
 * This class is not intended for public subclassing.
 *
 * @see MessageFormat
 * @see UMessagePatternApostropheMode
 * @draft ICU 57
 */
class U_COMMON_API SimpleFormatter U_FINAL : public UMemory {
public:
    /**
     * Default constructor.
     * @draft ICU 57
     */
    SimpleFormatter() : compiledPattern((UChar)0) {}

    /**
     * Constructs a formatter from the pattern string.
     *
     * @param pattern The pattern string.
     * @param errorCode ICU error code in/out parameter.
     *                  Must fulfill U_SUCCESS before the function call.
     *                  Set to U_ILLEGAL_ARGUMENT_ERROR for bad argument syntax.
     * @draft ICU 57
     */
    SimpleFormatter(const UnicodeString& pattern, UErrorCode &errorCode) {
        applyPattern(pattern, errorCode);
    }

    /**
     * Constructs a formatter from the pattern string.
     * The number of arguments checked against the given limits is the
     * highest argument number plus one, not the number of occurrences of arguments.
     *
     * @param pattern The pattern string.
     * @param min The pattern must have at least this many arguments.
     * @param max The pattern must have at most this many arguments.
     * @param errorCode ICU error code in/out parameter.
     *                  Must fulfill U_SUCCESS before the function call.
     *                  Set to U_ILLEGAL_ARGUMENT_ERROR for bad argument syntax and
     *                  too few or too many arguments.
     * @draft ICU 57
     */
    SimpleFormatter(const UnicodeString& pattern, int32_t min, int32_t max,
                    UErrorCode &errorCode) {
        applyPatternMinMaxArguments(pattern, min, max, errorCode);
    }

    /**
     * Copy constructor.
     * @draft ICU 57
     */
    SimpleFormatter(const SimpleFormatter& other)
            : compiledPattern(other.compiledPattern) {}

    /**
     * Assignment operator.
     * @draft ICU 57
     */
    SimpleFormatter &operator=(const SimpleFormatter& other);

    /**
     * Destructor.
     * @draft ICU 57
     */
    ~SimpleFormatter();

    /**
     * Changes this object according to the new pattern.
     *
     * @param pattern The pattern string.
     * @param errorCode ICU error code in/out parameter.
     *                  Must fulfill U_SUCCESS before the function call.
     *                  Set to U_ILLEGAL_ARGUMENT_ERROR for bad argument syntax.
     * @return TRUE if U_SUCCESS(errorCode).
     * @draft ICU 57
     */
    UBool applyPattern(const UnicodeString &pattern, UErrorCode &errorCode) {
        return applyPatternMinMaxArguments(pattern, 0, INT32_MAX, errorCode);
    }

    /**
     * Changes this object according to the new pattern.
     * The number of arguments checked against the given limits is the
     * highest argument number plus one, not the number of occurrences of arguments.
     *
     * @param pattern The pattern string.
     * @param min The pattern must have at least this many arguments.
     * @param max The pattern must have at most this many arguments.
     * @param errorCode ICU error code in/out parameter.
     *                  Must fulfill U_SUCCESS before the function call.
     *                  Set to U_ILLEGAL_ARGUMENT_ERROR for bad argument syntax and
     *                  too few or too many arguments.
     * @return TRUE if U_SUCCESS(errorCode).
     * @draft ICU 57
     */
    UBool applyPatternMinMaxArguments(const UnicodeString &pattern,
                                      int32_t min, int32_t max, UErrorCode &errorCode);

    /**
     * @return The max argument number + 1.
     * @draft ICU 57
     */
    int32_t getArgumentLimit() const {
        return getArgumentLimit(compiledPattern.getBuffer(), compiledPattern.length());
    }

    /**
     * Formats the given value, appending to the appendTo builder.
     * The argument value must not be the same object as appendTo.
     * getArgumentLimit() must be at most 1.
     *
     * @param value0 Value for argument {0}.
     * @param appendTo Gets the formatted pattern and value appended.
     * @param errorCode ICU error code in/out parameter.
     *                  Must fulfill U_SUCCESS before the function call.
     * @return appendTo
     * @draft ICU 57
     */
    UnicodeString &format(
            const UnicodeString &value0,
            UnicodeString &appendTo, UErrorCode &errorCode) const;

    /**
     * Formats the given values, appending to the appendTo builder.
     * An argument value must not be the same object as appendTo.
     * getArgumentLimit() must be at most 2.
     *
     * @param value0 Value for argument {0}.
     * @param value1 Value for argument {1}.
     * @param appendTo Gets the formatted pattern and values appended.
     * @param errorCode ICU error code in/out parameter.
     *                  Must fulfill U_SUCCESS before the function call.
     * @return appendTo
     * @draft ICU 57
     */
    UnicodeString &format(
            const UnicodeString &value0,
            const UnicodeString &value1,
            UnicodeString &appendTo, UErrorCode &errorCode) const;

    /**
     * Formats the given values, appending to the appendTo builder.
     * An argument value must not be the same object as appendTo.
     * getArgumentLimit() must be at most 3.
     *
     * @param value0 Value for argument {0}.
     * @param value1 Value for argument {1}.
     * @param value2 Value for argument {2}.
     * @param appendTo Gets the formatted pattern and values appended.
     * @param errorCode ICU error code in/out parameter.
     *                  Must fulfill U_SUCCESS before the function call.
     * @return appendTo
     * @draft ICU 57
     */
    UnicodeString &format(
            const UnicodeString &value0,
            const UnicodeString &value1,
            const UnicodeString &value2,
            UnicodeString &appendTo, UErrorCode &errorCode) const;

    /**
     * Formats the given values, appending to the appendTo string.
     *
     * @param values The argument values.
     *               An argument value must not be the same object as appendTo.
     *               Can be NULL if valuesLength==getArgumentLimit()==0.
     * @param valuesLength The length of the values array.
     *                     Must be at least getArgumentLimit().
     * @param appendTo Gets the formatted pattern and values appended.
     * @param offsets offsets[i] receives the offset of where
     *                values[i] replaced pattern argument {i}.
     *                Can be shorter or longer than values. Can be NULL if offsetsLength==0.
     *                If there is no {i} in the pattern, then offsets[i] is set to -1.
     * @param offsetsLength The length of the offsets array.
     * @param errorCode ICU error code in/out parameter.
     *                  Must fulfill U_SUCCESS before the function call.
     * @return appendTo
     * @draft ICU 57
     */
    UnicodeString &formatAndAppend(
            const UnicodeString *const *values, int32_t valuesLength,
            UnicodeString &appendTo,
            int32_t *offsets, int32_t offsetsLength, UErrorCode &errorCode) const;

    /**
     * Formats the given values, replacing the contents of the result string.
     * May optimize by actually appending to the result if it is the same object
     * as the value corresponding to the initial argument in the pattern.
     *
     * @param values The argument values.
     *               An argument value may be the same object as result.
     *               Can be NULL if valuesLength==getArgumentLimit()==0.
     * @param valuesLength The length of the values array.
     *                     Must be at least getArgumentLimit().
     * @param result Gets its contents replaced by the formatted pattern and values.
     * @param offsets offsets[i] receives the offset of where
     *                values[i] replaced pattern argument {i}.
     *                Can be shorter or longer than values. Can be NULL if offsetsLength==0.
     *                If there is no {i} in the pattern, then offsets[i] is set to -1.
     * @param offsetsLength The length of the offsets array.
     * @param errorCode ICU error code in/out parameter.
     *                  Must fulfill U_SUCCESS before the function call.
     * @return result
     * @draft ICU 57
     */
    UnicodeString &formatAndReplace(
            const UnicodeString *const *values, int32_t valuesLength,
            UnicodeString &result,
            int32_t *offsets, int32_t offsetsLength, UErrorCode &errorCode) const;

    /**
     * Returns the pattern text with none of the arguments.
     * Like formatting with all-empty string values.
     * @draft ICU 57
     */
    UnicodeString getTextWithNoArguments() const {
        return getTextWithNoArguments(compiledPattern.getBuffer(), compiledPattern.length());
    }

private:
    /**
     * Binary representation of the compiled pattern.
     * Index 0: One more than the highest argument number.
     * Followed by zero or more arguments or literal-text segments.
     *
     * An argument is stored as its number, less than ARG_NUM_LIMIT.
     * A literal-text segment is stored as its length (at least 1) offset by ARG_NUM_LIMIT,
     * followed by that many chars.
     */
    UnicodeString compiledPattern;

    static inline int32_t getArgumentLimit(const UChar *compiledPattern,
                                              int32_t compiledPatternLength) {
        return compiledPatternLength == 0 ? 0 : compiledPattern[0];
    }

    static UnicodeString getTextWithNoArguments(const UChar *compiledPattern, int32_t compiledPatternLength);

    static UnicodeString &format(
            const UChar *compiledPattern, int32_t compiledPatternLength,
            const UnicodeString *const *values,
            UnicodeString &result, const UnicodeString *resultCopy, UBool forbidResultAsValue,
            int32_t *offsets, int32_t offsetsLength,
            UErrorCode &errorCode);
};

U_NAMESPACE_END

#endif /* U_HIDE_DRAFT_API */

#endif  // __SIMPLEFORMATTER_H__

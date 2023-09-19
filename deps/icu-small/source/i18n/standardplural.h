// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 * Copyright (C) 2015, International Business Machines Corporation
 * and others. All Rights Reserved.
 *******************************************************************************
 * standardplural.h
 *
 * created on: 2015dec14
 * created by: Markus W. Scherer
 */

#ifndef __STANDARDPLURAL_H__
#define __STANDARDPLURAL_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

class UnicodeString;

/**
 * Standard CLDR plural form/category constants.
 * See http://www.unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules
 */
class U_I18N_API StandardPlural {
public:
    enum Form {
        ZERO,
        ONE,
        TWO,
        FEW,
        MANY,
        OTHER,
        EQ_0,
        EQ_1,
        COUNT
    };

    /**
     * @return the lowercase CLDR keyword string for the plural form
     */
    static const char *getKeyword(Form p);

    /**
     * @param keyword for example "few" or "other"
     * @return the plural form corresponding to the keyword, or OTHER
     */
    static Form orOtherFromString(const char *keyword) {
        return static_cast<Form>(indexOrOtherIndexFromString(keyword));
    }

    /**
     * @param keyword for example "few" or "other"
     * @return the plural form corresponding to the keyword, or OTHER
     */
    static Form orOtherFromString(const UnicodeString &keyword) {
        return static_cast<Form>(indexOrOtherIndexFromString(keyword));
    }

    /**
     * Sets U_ILLEGAL_ARGUMENT_ERROR if the keyword is not a plural form.
     *
     * @param keyword for example "few" or "other"
     * @return the plural form corresponding to the keyword
     */
    static Form fromString(const char *keyword, UErrorCode &errorCode) {
        return static_cast<Form>(indexFromString(keyword, errorCode));
    }

    /**
     * Sets U_ILLEGAL_ARGUMENT_ERROR if the keyword is not a plural form.
     *
     * @param keyword for example "few" or "other"
     * @return the plural form corresponding to the keyword
     */
    static Form fromString(const UnicodeString &keyword, UErrorCode &errorCode) {
        return static_cast<Form>(indexFromString(keyword, errorCode));
    }

    /**
     * @param keyword for example "few" or "other"
     * @return the index of the plural form corresponding to the keyword, or a negative value
     */
    static int32_t indexOrNegativeFromString(const char *keyword);

    /**
     * @param keyword for example "few" or "other"
     * @return the index of the plural form corresponding to the keyword, or a negative value
     */
    static int32_t indexOrNegativeFromString(const UnicodeString &keyword);

    /**
     * @param keyword for example "few" or "other"
     * @return the index of the plural form corresponding to the keyword, or OTHER
     */
    static int32_t indexOrOtherIndexFromString(const char *keyword) {
        int32_t i = indexOrNegativeFromString(keyword);
        return i >= 0 ? i : OTHER;
    }

    /**
     * @param keyword for example "few" or "other"
     * @return the index of the plural form corresponding to the keyword, or OTHER
     */
    static int32_t indexOrOtherIndexFromString(const UnicodeString &keyword) {
        int32_t i = indexOrNegativeFromString(keyword);
        return i >= 0 ? i : OTHER;
    }

    /**
     * Sets U_ILLEGAL_ARGUMENT_ERROR if the keyword is not a plural form.
     *
     * @param keyword for example "few" or "other"
     * @return the index of the plural form corresponding to the keyword
     */
    static int32_t indexFromString(const char *keyword, UErrorCode &errorCode);

    /**
     * Sets U_ILLEGAL_ARGUMENT_ERROR if the keyword is not a plural form.
     *
     * @param keyword for example "few" or "other"
     * @return the index of the plural form corresponding to the keyword
     */
    static int32_t indexFromString(const UnicodeString &keyword, UErrorCode &errorCode);
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_FORMATTING
#endif  // __STANDARDPLURAL_H__

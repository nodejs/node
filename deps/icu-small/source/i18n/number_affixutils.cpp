// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "number_affixutils.h"
#include "unicode/utf16.h"
#include "unicode/uniset.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

TokenConsumer::~TokenConsumer() = default;
SymbolProvider::~SymbolProvider() = default;

int32_t AffixUtils::estimateLength(const UnicodeString &patternString, UErrorCode &status) {
    AffixPatternState state = STATE_BASE;
    int32_t offset = 0;
    int32_t length = 0;
    for (; offset < patternString.length();) {
        UChar32 cp = patternString.char32At(offset);

        switch (state) {
            case STATE_BASE:
                if (cp == u'\'') {
                    // First quote
                    state = STATE_FIRST_QUOTE;
                } else {
                    // Unquoted symbol
                    length++;
                }
                break;
            case STATE_FIRST_QUOTE:
                if (cp == u'\'') {
                    // Repeated quote
                    length++;
                    state = STATE_BASE;
                } else {
                    // Quoted code point
                    length++;
                    state = STATE_INSIDE_QUOTE;
                }
                break;
            case STATE_INSIDE_QUOTE:
                if (cp == u'\'') {
                    // End of quoted sequence
                    state = STATE_AFTER_QUOTE;
                } else {
                    // Quoted code point
                    length++;
                }
                break;
            case STATE_AFTER_QUOTE:
                if (cp == u'\'') {
                    // Double quote inside of quoted sequence
                    length++;
                    state = STATE_INSIDE_QUOTE;
                } else {
                    // Unquoted symbol
                    length++;
                }
                break;
            default:
                U_ASSERT(false);
        }

        offset += U16_LENGTH(cp);
    }

    switch (state) {
        case STATE_FIRST_QUOTE:
        case STATE_INSIDE_QUOTE:
            status = U_ILLEGAL_ARGUMENT_ERROR;
            break;
        default:
            break;
    }

    return length;
}

UnicodeString AffixUtils::escape(const UnicodeString &input) {
    AffixPatternState state = STATE_BASE;
    int32_t offset = 0;
    UnicodeString output;
    for (; offset < input.length();) {
        UChar32 cp = input.char32At(offset);

        switch (cp) {
            case u'\'':
                output.append(u"''", -1);
                break;

            case u'-':
            case u'+':
            case u'%':
            case u'‰':
            case u'¤':
                if (state == STATE_BASE) {
                    output.append(u'\'');
                    output.append(cp);
                    state = STATE_INSIDE_QUOTE;
                } else {
                    output.append(cp);
                }
                break;

            default:
                if (state == STATE_INSIDE_QUOTE) {
                    output.append(u'\'');
                    output.append(cp);
                    state = STATE_BASE;
                } else {
                    output.append(cp);
                }
                break;
        }
        offset += U16_LENGTH(cp);
    }

    if (state == STATE_INSIDE_QUOTE) {
        output.append(u'\'');
    }

    return output;
}

Field AffixUtils::getFieldForType(AffixPatternType type) {
    switch (type) {
        case TYPE_MINUS_SIGN:
            return Field::UNUM_SIGN_FIELD;
        case TYPE_PLUS_SIGN:
            return Field::UNUM_SIGN_FIELD;
        case TYPE_PERCENT:
            return Field::UNUM_PERCENT_FIELD;
        case TYPE_PERMILLE:
            return Field::UNUM_PERMILL_FIELD;
        case TYPE_CURRENCY_SINGLE:
            return Field::UNUM_CURRENCY_FIELD;
        case TYPE_CURRENCY_DOUBLE:
            return Field::UNUM_CURRENCY_FIELD;
        case TYPE_CURRENCY_TRIPLE:
            return Field::UNUM_CURRENCY_FIELD;
        case TYPE_CURRENCY_QUAD:
            return Field::UNUM_CURRENCY_FIELD;
        case TYPE_CURRENCY_QUINT:
            return Field::UNUM_CURRENCY_FIELD;
        case TYPE_CURRENCY_OVERFLOW:
            return Field::UNUM_CURRENCY_FIELD;
        default:
            U_ASSERT(false);
            return Field::UNUM_FIELD_COUNT; // suppress "control reaches end of non-void function"
    }
}

int32_t
AffixUtils::unescape(const UnicodeString &affixPattern, NumberStringBuilder &output, int32_t position,
                     const SymbolProvider &provider, UErrorCode &status) {
    int32_t length = 0;
    AffixTag tag;
    while (hasNext(tag, affixPattern)) {
        tag = nextToken(tag, affixPattern, status);
        if (U_FAILURE(status)) { return length; }
        if (tag.type == TYPE_CURRENCY_OVERFLOW) {
            // Don't go to the provider for this special case
            length += output.insertCodePoint(position + length, 0xFFFD, UNUM_CURRENCY_FIELD, status);
        } else if (tag.type < 0) {
            length += output.insert(
                    position + length, provider.getSymbol(tag.type), getFieldForType(tag.type), status);
        } else {
            length += output.insertCodePoint(position + length, tag.codePoint, UNUM_FIELD_COUNT, status);
        }
    }
    return length;
}

int32_t AffixUtils::unescapedCodePointCount(const UnicodeString &affixPattern,
                                            const SymbolProvider &provider, UErrorCode &status) {
    int32_t length = 0;
    AffixTag tag;
    while (hasNext(tag, affixPattern)) {
        tag = nextToken(tag, affixPattern, status);
        if (U_FAILURE(status)) { return length; }
        if (tag.type == TYPE_CURRENCY_OVERFLOW) {
            length += 1;
        } else if (tag.type < 0) {
            length += provider.getSymbol(tag.type).length();
        } else {
            length += U16_LENGTH(tag.codePoint);
        }
    }
    return length;
}

bool
AffixUtils::containsType(const UnicodeString &affixPattern, AffixPatternType type, UErrorCode &status) {
    if (affixPattern.length() == 0) {
        return false;
    }
    AffixTag tag;
    while (hasNext(tag, affixPattern)) {
        tag = nextToken(tag, affixPattern, status);
        if (U_FAILURE(status)) { return false; }
        if (tag.type == type) {
            return true;
        }
    }
    return false;
}

bool AffixUtils::hasCurrencySymbols(const UnicodeString &affixPattern, UErrorCode &status) {
    if (affixPattern.length() == 0) {
        return false;
    }
    AffixTag tag;
    while (hasNext(tag, affixPattern)) {
        tag = nextToken(tag, affixPattern, status);
        if (U_FAILURE(status)) { return false; }
        if (tag.type < 0 && getFieldForType(tag.type) == UNUM_CURRENCY_FIELD) {
            return true;
        }
    }
    return false;
}

UnicodeString AffixUtils::replaceType(const UnicodeString &affixPattern, AffixPatternType type,
                                      char16_t replacementChar, UErrorCode &status) {
    UnicodeString output(affixPattern); // copy
    if (affixPattern.length() == 0) {
        return output;
    };
    AffixTag tag;
    while (hasNext(tag, affixPattern)) {
        tag = nextToken(tag, affixPattern, status);
        if (U_FAILURE(status)) { return output; }
        if (tag.type == type) {
            output.replace(tag.offset - 1, 1, replacementChar);
        }
    }
    return output;
}

bool AffixUtils::containsOnlySymbolsAndIgnorables(const UnicodeString& affixPattern,
                                                  const UnicodeSet& ignorables, UErrorCode& status) {
    if (affixPattern.length() == 0) {
        return true;
    };
    AffixTag tag;
    while (hasNext(tag, affixPattern)) {
        tag = nextToken(tag, affixPattern, status);
        if (U_FAILURE(status)) { return false; }
        if (tag.type == TYPE_CODEPOINT && !ignorables.contains(tag.codePoint)) {
            return false;
        }
    }
    return true;
}

void AffixUtils::iterateWithConsumer(const UnicodeString& affixPattern, TokenConsumer& consumer,
                                     UErrorCode& status) {
    if (affixPattern.length() == 0) {
        return;
    };
    AffixTag tag;
    while (hasNext(tag, affixPattern)) {
        tag = nextToken(tag, affixPattern, status);
        if (U_FAILURE(status)) { return; }
        consumer.consumeToken(tag.type, tag.codePoint, status);
        if (U_FAILURE(status)) { return; }
    }
}

AffixTag AffixUtils::nextToken(AffixTag tag, const UnicodeString &patternString, UErrorCode &status) {
    int32_t offset = tag.offset;
    int32_t state = tag.state;
    for (; offset < patternString.length();) {
        UChar32 cp = patternString.char32At(offset);
        int32_t count = U16_LENGTH(cp);

        switch (state) {
            case STATE_BASE:
                switch (cp) {
                    case u'\'':
                        state = STATE_FIRST_QUOTE;
                        offset += count;
                        // continue to the next code point
                        break;
                    case u'-':
                        return makeTag(offset + count, TYPE_MINUS_SIGN, STATE_BASE, 0);
                    case u'+':
                        return makeTag(offset + count, TYPE_PLUS_SIGN, STATE_BASE, 0);
                    case u'%':
                        return makeTag(offset + count, TYPE_PERCENT, STATE_BASE, 0);
                    case u'‰':
                        return makeTag(offset + count, TYPE_PERMILLE, STATE_BASE, 0);
                    case u'¤':
                        state = STATE_FIRST_CURR;
                        offset += count;
                        // continue to the next code point
                        break;
                    default:
                        return makeTag(offset + count, TYPE_CODEPOINT, STATE_BASE, cp);
                }
                break;
            case STATE_FIRST_QUOTE:
                if (cp == u'\'') {
                    return makeTag(offset + count, TYPE_CODEPOINT, STATE_BASE, cp);
                } else {
                    return makeTag(offset + count, TYPE_CODEPOINT, STATE_INSIDE_QUOTE, cp);
                }
            case STATE_INSIDE_QUOTE:
                if (cp == u'\'') {
                    state = STATE_AFTER_QUOTE;
                    offset += count;
                    // continue to the next code point
                    break;
                } else {
                    return makeTag(offset + count, TYPE_CODEPOINT, STATE_INSIDE_QUOTE, cp);
                }
            case STATE_AFTER_QUOTE:
                if (cp == u'\'') {
                    return makeTag(offset + count, TYPE_CODEPOINT, STATE_INSIDE_QUOTE, cp);
                } else {
                    state = STATE_BASE;
                    // re-evaluate this code point
                    break;
                }
            case STATE_FIRST_CURR:
                if (cp == u'¤') {
                    state = STATE_SECOND_CURR;
                    offset += count;
                    // continue to the next code point
                    break;
                } else {
                    return makeTag(offset, TYPE_CURRENCY_SINGLE, STATE_BASE, 0);
                }
            case STATE_SECOND_CURR:
                if (cp == u'¤') {
                    state = STATE_THIRD_CURR;
                    offset += count;
                    // continue to the next code point
                    break;
                } else {
                    return makeTag(offset, TYPE_CURRENCY_DOUBLE, STATE_BASE, 0);
                }
            case STATE_THIRD_CURR:
                if (cp == u'¤') {
                    state = STATE_FOURTH_CURR;
                    offset += count;
                    // continue to the next code point
                    break;
                } else {
                    return makeTag(offset, TYPE_CURRENCY_TRIPLE, STATE_BASE, 0);
                }
            case STATE_FOURTH_CURR:
                if (cp == u'¤') {
                    state = STATE_FIFTH_CURR;
                    offset += count;
                    // continue to the next code point
                    break;
                } else {
                    return makeTag(offset, TYPE_CURRENCY_QUAD, STATE_BASE, 0);
                }
            case STATE_FIFTH_CURR:
                if (cp == u'¤') {
                    state = STATE_OVERFLOW_CURR;
                    offset += count;
                    // continue to the next code point
                    break;
                } else {
                    return makeTag(offset, TYPE_CURRENCY_QUINT, STATE_BASE, 0);
                }
            case STATE_OVERFLOW_CURR:
                if (cp == u'¤') {
                    offset += count;
                    // continue to the next code point and loop back to this state
                    break;
                } else {
                    return makeTag(offset, TYPE_CURRENCY_OVERFLOW, STATE_BASE, 0);
                }
            default:
                U_ASSERT(false);
        }
    }
    // End of string
    switch (state) {
        case STATE_BASE:
            // No more tokens in string.
            return {-1};
        case STATE_FIRST_QUOTE:
        case STATE_INSIDE_QUOTE:
            // For consistent behavior with the JDK and ICU 58, set an error here.
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return {-1};
        case STATE_AFTER_QUOTE:
            // No more tokens in string.
            return {-1};
        case STATE_FIRST_CURR:
            return makeTag(offset, TYPE_CURRENCY_SINGLE, STATE_BASE, 0);
        case STATE_SECOND_CURR:
            return makeTag(offset, TYPE_CURRENCY_DOUBLE, STATE_BASE, 0);
        case STATE_THIRD_CURR:
            return makeTag(offset, TYPE_CURRENCY_TRIPLE, STATE_BASE, 0);
        case STATE_FOURTH_CURR:
            return makeTag(offset, TYPE_CURRENCY_QUAD, STATE_BASE, 0);
        case STATE_FIFTH_CURR:
            return makeTag(offset, TYPE_CURRENCY_QUINT, STATE_BASE, 0);
        case STATE_OVERFLOW_CURR:
            return makeTag(offset, TYPE_CURRENCY_OVERFLOW, STATE_BASE, 0);
        default:
            U_ASSERT(false);
            return {-1}; // suppress "control reaches end of non-void function"
    }
}

bool AffixUtils::hasNext(const AffixTag &tag, const UnicodeString &string) {
    // First check for the {-1} and default initializer syntax.
    if (tag.offset < 0) {
        return false;
    } else if (tag.offset == 0) {
        return string.length() > 0;
    }
    // The rest of the fields are safe to use now.
    // Special case: the last character in string is an end quote.
    if (tag.state == STATE_INSIDE_QUOTE && tag.offset == string.length() - 1 &&
        string.charAt(tag.offset) == u'\'') {
        return false;
    } else if (tag.state != STATE_BASE) {
        return true;
    } else {
        return tag.offset < string.length();
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */

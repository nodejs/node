// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/inspector_protocol/Parser.h"

#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/Values.h"

namespace blink {
namespace protocol {

namespace {

const int stackLimit = 1000;

enum Token {
    ObjectBegin,
    ObjectEnd,
    ArrayBegin,
    ArrayEnd,
    StringLiteral,
    Number,
    BoolTrue,
    BoolFalse,
    NullToken,
    ListSeparator,
    ObjectPairSeparator,
    InvalidToken,
};

const char* const nullString = "null";
const char* const trueString = "true";
const char* const falseString = "false";

bool parseConstToken(const UChar* start, const UChar* end, const UChar** tokenEnd, const char* token)
{
    while (start < end && *token != '\0' && *start++ == *token++) { }
    if (*token != '\0')
        return false;
    *tokenEnd = start;
    return true;
}

bool readInt(const UChar* start, const UChar* end, const UChar** tokenEnd, bool canHaveLeadingZeros)
{
    if (start == end)
        return false;
    bool haveLeadingZero = '0' == *start;
    int length = 0;
    while (start < end && '0' <= *start && *start <= '9') {
        ++start;
        ++length;
    }
    if (!length)
        return false;
    if (!canHaveLeadingZeros && length > 1 && haveLeadingZero)
        return false;
    *tokenEnd = start;
    return true;
}

bool parseNumberToken(const UChar* start, const UChar* end, const UChar** tokenEnd)
{
    // We just grab the number here. We validate the size in DecodeNumber.
    // According to RFC4627, a valid number is: [minus] int [frac] [exp]
    if (start == end)
        return false;
    UChar c = *start;
    if ('-' == c)
        ++start;

    if (!readInt(start, end, &start, false))
        return false;
    if (start == end) {
        *tokenEnd = start;
        return true;
    }

    // Optional fraction part
    c = *start;
    if ('.' == c) {
        ++start;
        if (!readInt(start, end, &start, true))
            return false;
        if (start == end) {
            *tokenEnd = start;
            return true;
        }
        c = *start;
    }

    // Optional exponent part
    if ('e' == c || 'E' == c) {
        ++start;
        if (start == end)
            return false;
        c = *start;
        if ('-' == c || '+' == c) {
            ++start;
            if (start == end)
                return false;
        }
        if (!readInt(start, end, &start, true))
            return false;
    }

    *tokenEnd = start;
    return true;
}

bool readHexDigits(const UChar* start, const UChar* end, const UChar** tokenEnd, int digits)
{
    if (end - start < digits)
        return false;
    for (int i = 0; i < digits; ++i) {
        UChar c = *start++;
        if (!(('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F')))
            return false;
    }
    *tokenEnd = start;
    return true;
}

bool parseStringToken(const UChar* start, const UChar* end, const UChar** tokenEnd)
{
    while (start < end) {
        UChar c = *start++;
        if ('\\' == c) {
            c = *start++;
            // Make sure the escaped char is valid.
            switch (c) {
            case 'x':
                if (!readHexDigits(start, end, &start, 2))
                    return false;
                break;
            case 'u':
                if (!readHexDigits(start, end, &start, 4))
                    return false;
                break;
            case '\\':
            case '/':
            case 'b':
            case 'f':
            case 'n':
            case 'r':
            case 't':
            case 'v':
            case '"':
                break;
            default:
                return false;
            }
        } else if ('"' == c) {
            *tokenEnd = start;
            return true;
        }
    }
    return false;
}

bool skipComment(const UChar* start, const UChar* end, const UChar** commentEnd)
{
    if (start == end)
        return false;

    if (*start != '/' || start + 1 >= end)
        return false;
    ++start;

    if (*start == '/') {
        // Single line comment, read to newline.
        for (++start; start < end; ++start) {
            if (*start == '\n' || *start == '\r') {
                *commentEnd = start + 1;
                return true;
            }
        }
        *commentEnd = end;
        // Comment reaches end-of-input, which is fine.
        return true;
    }

    if (*start == '*') {
        UChar previous = '\0';
        // Block comment, read until end marker.
        for (++start; start < end; previous = *start++) {
            if (previous == '*' && *start == '/') {
                *commentEnd = start + 1;
                return true;
            }
        }
        // Block comment must close before end-of-input.
        return false;
    }

    return false;
}

void skipWhitespaceAndComments(const UChar* start, const UChar* end, const UChar** whitespaceEnd)
{
    while (start < end) {
        if (isSpaceOrNewline(*start)) {
            ++start;
        } else if (*start == '/') {
            const UChar* commentEnd;
            if (!skipComment(start, end, &commentEnd))
                break;
            start = commentEnd;
        } else {
            break;
        }
    }
    *whitespaceEnd = start;
}

Token parseToken(const UChar* start, const UChar* end, const UChar** tokenStart, const UChar** tokenEnd)
{
    skipWhitespaceAndComments(start, end, tokenStart);
    start = *tokenStart;

    if (start == end)
        return InvalidToken;

    switch (*start) {
    case 'n':
        if (parseConstToken(start, end, tokenEnd, nullString))
            return NullToken;
        break;
    case 't':
        if (parseConstToken(start, end, tokenEnd, trueString))
            return BoolTrue;
        break;
    case 'f':
        if (parseConstToken(start, end, tokenEnd, falseString))
            return BoolFalse;
        break;
    case '[':
        *tokenEnd = start + 1;
        return ArrayBegin;
    case ']':
        *tokenEnd = start + 1;
        return ArrayEnd;
    case ',':
        *tokenEnd = start + 1;
        return ListSeparator;
    case '{':
        *tokenEnd = start + 1;
        return ObjectBegin;
    case '}':
        *tokenEnd = start + 1;
        return ObjectEnd;
    case ':':
        *tokenEnd = start + 1;
        return ObjectPairSeparator;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
        if (parseNumberToken(start, end, tokenEnd))
            return Number;
        break;
    case '"':
        if (parseStringToken(start + 1, end, tokenEnd))
            return StringLiteral;
        break;
    }
    return InvalidToken;
}

inline int hexToInt(UChar c)
{
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('A' <= c && c <= 'F')
        return c - 'A' + 10;
    if ('a' <= c && c <= 'f')
        return c - 'a' + 10;
    NOTREACHED();
    return 0;
}

bool decodeString(const UChar* start, const UChar* end, String16Builder* output)
{
    while (start < end) {
        UChar c = *start++;
        if ('\\' != c) {
            output->append(c);
            continue;
        }
        c = *start++;

        if (c == 'x') {
            // \x is not supported.
            return false;
        }

        switch (c) {
        case '"':
        case '/':
        case '\\':
            break;
        case 'b':
            c = '\b';
            break;
        case 'f':
            c = '\f';
            break;
        case 'n':
            c = '\n';
            break;
        case 'r':
            c = '\r';
            break;
        case 't':
            c = '\t';
            break;
        case 'v':
            c = '\v';
            break;
        case 'u':
            c = (hexToInt(*start) << 12) +
                (hexToInt(*(start + 1)) << 8) +
                (hexToInt(*(start + 2)) << 4) +
                hexToInt(*(start + 3));
            start += 4;
            break;
        default:
            return false;
        }
        output->append(c);
    }
    return true;
}

bool decodeString(const UChar* start, const UChar* end, String16* output)
{
    if (start == end) {
        *output = "";
        return true;
    }
    if (start > end)
        return false;
    String16Builder buffer;
    buffer.reserveCapacity(end - start);
    if (!decodeString(start, end, &buffer))
        return false;
    *output = buffer.toString();
    return true;
}

std::unique_ptr<Value> buildValue(const UChar* start, const UChar* end, const UChar** valueTokenEnd, int depth)
{
    if (depth > stackLimit)
        return nullptr;

    std::unique_ptr<Value> result;
    const UChar* tokenStart;
    const UChar* tokenEnd;
    Token token = parseToken(start, end, &tokenStart, &tokenEnd);
    switch (token) {
    case InvalidToken:
        return nullptr;
    case NullToken:
        result = Value::null();
        break;
    case BoolTrue:
        result = FundamentalValue::create(true);
        break;
    case BoolFalse:
        result = FundamentalValue::create(false);
        break;
    case Number: {
        bool ok;
        double value = String16::charactersToDouble(tokenStart, tokenEnd - tokenStart, &ok);
        if (!ok)
            return nullptr;
        int number = static_cast<int>(value);
        if (number == value)
            result = FundamentalValue::create(number);
        else
            result = FundamentalValue::create(value);
        break;
    }
    case StringLiteral: {
        String16 value;
        bool ok = decodeString(tokenStart + 1, tokenEnd - 1, &value);
        if (!ok)
            return nullptr;
        result = StringValue::create(value);
        break;
    }
    case ArrayBegin: {
        std::unique_ptr<ListValue> array = ListValue::create();
        start = tokenEnd;
        token = parseToken(start, end, &tokenStart, &tokenEnd);
        while (token != ArrayEnd) {
            std::unique_ptr<Value> arrayNode = buildValue(start, end, &tokenEnd, depth + 1);
            if (!arrayNode)
                return nullptr;
            array->pushValue(std::move(arrayNode));

            // After a list value, we expect a comma or the end of the list.
            start = tokenEnd;
            token = parseToken(start, end, &tokenStart, &tokenEnd);
            if (token == ListSeparator) {
                start = tokenEnd;
                token = parseToken(start, end, &tokenStart, &tokenEnd);
                if (token == ArrayEnd)
                    return nullptr;
            } else if (token != ArrayEnd) {
                // Unexpected value after list value. Bail out.
                return nullptr;
            }
        }
        if (token != ArrayEnd)
            return nullptr;
        result = std::move(array);
        break;
    }
    case ObjectBegin: {
        std::unique_ptr<DictionaryValue> object = DictionaryValue::create();
        start = tokenEnd;
        token = parseToken(start, end, &tokenStart, &tokenEnd);
        while (token != ObjectEnd) {
            if (token != StringLiteral)
                return nullptr;
            String16 key;
            if (!decodeString(tokenStart + 1, tokenEnd - 1, &key))
                return nullptr;
            start = tokenEnd;

            token = parseToken(start, end, &tokenStart, &tokenEnd);
            if (token != ObjectPairSeparator)
                return nullptr;
            start = tokenEnd;

            std::unique_ptr<Value> value = buildValue(start, end, &tokenEnd, depth + 1);
            if (!value)
                return nullptr;
            object->setValue(key, std::move(value));
            start = tokenEnd;

            // After a key/value pair, we expect a comma or the end of the
            // object.
            token = parseToken(start, end, &tokenStart, &tokenEnd);
            if (token == ListSeparator) {
                start = tokenEnd;
                token = parseToken(start, end, &tokenStart, &tokenEnd);
                if (token == ObjectEnd)
                    return nullptr;
            } else if (token != ObjectEnd) {
                // Unexpected value after last object value. Bail out.
                return nullptr;
            }
        }
        if (token != ObjectEnd)
            return nullptr;
        result = std::move(object);
        break;
    }

    default:
        // We got a token that's not a value.
        return nullptr;
    }

    skipWhitespaceAndComments(tokenEnd, end, valueTokenEnd);
    return result;
}

std::unique_ptr<Value> parseJSONInternal(const UChar* start, unsigned length)
{
    const UChar* end = start + length;
    const UChar *tokenEnd;
    std::unique_ptr<Value> value = buildValue(start, end, &tokenEnd, 0);
    if (!value || tokenEnd != end)
        return nullptr;
    return value;
}

} // anonymous namespace

std::unique_ptr<Value> parseJSON(const String16& json)
{
    if (json.isEmpty())
        return nullptr;
    return parseJSONInternal(json.characters16(), json.length());
}

} // namespace protocol
} // namespace blink

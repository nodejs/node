// Copyright 2007-2011 Baptiste Lepilleur and The JsonCpp Authors
// Copyright (C) 2016 InfoTeCS JSC. All rights reserved.
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#if !defined(JSON_IS_AMALGAMATION)
#include "json_tool.h"
#include <json/assertions.h>
#include <json/reader.h>
#include <json/value.h>
#endif // if !defined(JSON_IS_AMALGAMATION)
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <istream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

#include <cstdio>
#if __cplusplus >= 201103L

#if !defined(sscanf)
#define sscanf std::sscanf
#endif

#endif //__cplusplus

#if defined(_MSC_VER)
#if !defined(_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES)
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#endif //_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#endif //_MSC_VER

#if defined(_MSC_VER)
// Disable warning about strdup being deprecated.
#pragma warning(disable : 4996)
#endif

// Define JSONCPP_DEPRECATED_STACK_LIMIT as an appropriate integer at compile
// time to change the stack limit
#if !defined(JSONCPP_DEPRECATED_STACK_LIMIT)
#define JSONCPP_DEPRECATED_STACK_LIMIT 1000
#endif

static size_t const stackLimit_g =
    JSONCPP_DEPRECATED_STACK_LIMIT; // see readValue()

namespace Json {

#if __cplusplus >= 201103L || (defined(_CPPLIB_VER) && _CPPLIB_VER >= 520)
using CharReaderPtr = std::unique_ptr<CharReader>;
#else
using CharReaderPtr = std::auto_ptr<CharReader>;
#endif

// Implementation of class Features
// ////////////////////////////////

Features::Features() = default;

Features Features::all() { return {}; }

Features Features::strictMode() {
  Features features;
  features.allowComments_ = false;
  features.strictRoot_ = true;
  features.allowDroppedNullPlaceholders_ = false;
  features.allowNumericKeys_ = false;
  return features;
}

// Implementation of class Reader
// ////////////////////////////////

bool Reader::containsNewLine(Reader::Location begin, Reader::Location end) {
  return std::any_of(begin, end, [](char b) { return b == '\n' || b == '\r'; });
}

// Class Reader
// //////////////////////////////////////////////////////////////////

Reader::Reader() : features_(Features::all()) {}

Reader::Reader(const Features& features) : features_(features) {}

bool Reader::parse(const std::string& document, Value& root,
                   bool collectComments) {
  document_.assign(document.begin(), document.end());
  const char* begin = document_.c_str();
  const char* end = begin + document_.length();
  return parse(begin, end, root, collectComments);
}

bool Reader::parse(std::istream& is, Value& root, bool collectComments) {
  // std::istream_iterator<char> begin(is);
  // std::istream_iterator<char> end;
  // Those would allow streamed input from a file, if parse() were a
  // template function.

  // Since String is reference-counted, this at least does not
  // create an extra copy.
  String doc;
  std::getline(is, doc, static_cast<char> EOF);
  return parse(doc.data(), doc.data() + doc.size(), root, collectComments);
}

bool Reader::parse(const char* beginDoc, const char* endDoc, Value& root,
                   bool collectComments) {
  if (!features_.allowComments_) {
    collectComments = false;
  }

  begin_ = beginDoc;
  end_ = endDoc;
  collectComments_ = collectComments;
  current_ = begin_;
  lastValueEnd_ = nullptr;
  lastValue_ = nullptr;
  commentsBefore_.clear();
  errors_.clear();
  while (!nodes_.empty())
    nodes_.pop();
  nodes_.push(&root);

  bool successful = readValue();
  Token token;
  skipCommentTokens(token);
  if (collectComments_ && !commentsBefore_.empty())
    root.setComment(commentsBefore_, commentAfter);
  if (features_.strictRoot_) {
    if (!root.isArray() && !root.isObject()) {
      // Set error location to start of doc, ideally should be first token found
      // in doc
      token.type_ = tokenError;
      token.start_ = beginDoc;
      token.end_ = endDoc;
      addError(
          "A valid JSON document must be either an array or an object value.",
          token);
      return false;
    }
  }
  return successful;
}

bool Reader::readValue() {
  // readValue() may call itself only if it calls readObject() or ReadArray().
  // These methods execute nodes_.push() just before and nodes_.pop)() just
  // after calling readValue(). parse() executes one nodes_.push(), so > instead
  // of >=.
  if (nodes_.size() > stackLimit_g)
    throwRuntimeError("Exceeded stackLimit in readValue().");

  Token token;
  skipCommentTokens(token);
  bool successful = true;

  if (collectComments_ && !commentsBefore_.empty()) {
    currentValue().setComment(commentsBefore_, commentBefore);
    commentsBefore_.clear();
  }

  switch (token.type_) {
  case tokenObjectBegin:
    successful = readObject(token);
    currentValue().setOffsetLimit(current_ - begin_);
    break;
  case tokenArrayBegin:
    successful = readArray(token);
    currentValue().setOffsetLimit(current_ - begin_);
    break;
  case tokenNumber:
    successful = decodeNumber(token);
    break;
  case tokenString:
    successful = decodeString(token);
    break;
  case tokenTrue: {
    Value v(true);
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenFalse: {
    Value v(false);
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenNull: {
    Value v;
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenArraySeparator:
  case tokenObjectEnd:
  case tokenArrayEnd:
    if (features_.allowDroppedNullPlaceholders_) {
      // "Un-read" the current token and mark the current value as a null
      // token.
      current_--;
      Value v;
      currentValue().swapPayload(v);
      currentValue().setOffsetStart(current_ - begin_ - 1);
      currentValue().setOffsetLimit(current_ - begin_);
      break;
    } // Else, fall through...
  default:
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    return addError("Syntax error: value, object or array expected.", token);
  }

  if (collectComments_) {
    lastValueEnd_ = current_;
    lastValue_ = &currentValue();
  }

  return successful;
}

void Reader::skipCommentTokens(Token& token) {
  if (features_.allowComments_) {
    do {
      readToken(token);
    } while (token.type_ == tokenComment);
  } else {
    readToken(token);
  }
}

bool Reader::readToken(Token& token) {
  skipSpaces();
  token.start_ = current_;
  Char c = getNextChar();
  bool ok = true;
  switch (c) {
  case '{':
    token.type_ = tokenObjectBegin;
    break;
  case '}':
    token.type_ = tokenObjectEnd;
    break;
  case '[':
    token.type_ = tokenArrayBegin;
    break;
  case ']':
    token.type_ = tokenArrayEnd;
    break;
  case '"':
    token.type_ = tokenString;
    ok = readString();
    break;
  case '/':
    token.type_ = tokenComment;
    ok = readComment();
    break;
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
    token.type_ = tokenNumber;
    readNumber();
    break;
  case 't':
    token.type_ = tokenTrue;
    ok = match("rue", 3);
    break;
  case 'f':
    token.type_ = tokenFalse;
    ok = match("alse", 4);
    break;
  case 'n':
    token.type_ = tokenNull;
    ok = match("ull", 3);
    break;
  case ',':
    token.type_ = tokenArraySeparator;
    break;
  case ':':
    token.type_ = tokenMemberSeparator;
    break;
  case 0:
    token.type_ = tokenEndOfStream;
    break;
  default:
    ok = false;
    break;
  }
  if (!ok)
    token.type_ = tokenError;
  token.end_ = current_;
  return ok;
}

void Reader::skipSpaces() {
  while (current_ != end_) {
    Char c = *current_;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      ++current_;
    else
      break;
  }
}

bool Reader::match(const Char* pattern, int patternLength) {
  if (end_ - current_ < patternLength)
    return false;
  int index = patternLength;
  while (index--)
    if (current_[index] != pattern[index])
      return false;
  current_ += patternLength;
  return true;
}

bool Reader::readComment() {
  Location commentBegin = current_ - 1;
  Char c = getNextChar();
  bool successful = false;
  if (c == '*')
    successful = readCStyleComment();
  else if (c == '/')
    successful = readCppStyleComment();
  if (!successful)
    return false;

  if (collectComments_) {
    CommentPlacement placement = commentBefore;
    if (lastValueEnd_ && !containsNewLine(lastValueEnd_, commentBegin)) {
      if (c != '*' || !containsNewLine(commentBegin, current_))
        placement = commentAfterOnSameLine;
    }

    addComment(commentBegin, current_, placement);
  }
  return true;
}

String Reader::normalizeEOL(Reader::Location begin, Reader::Location end) {
  String normalized;
  normalized.reserve(static_cast<size_t>(end - begin));
  Reader::Location current = begin;
  while (current != end) {
    char c = *current++;
    if (c == '\r') {
      if (current != end && *current == '\n')
        // convert dos EOL
        ++current;
      // convert Mac EOL
      normalized += '\n';
    } else {
      normalized += c;
    }
  }
  return normalized;
}

void Reader::addComment(Location begin, Location end,
                        CommentPlacement placement) {
  assert(collectComments_);
  const String& normalized = normalizeEOL(begin, end);
  if (placement == commentAfterOnSameLine) {
    assert(lastValue_ != nullptr);
    lastValue_->setComment(normalized, placement);
  } else {
    commentsBefore_ += normalized;
  }
}

bool Reader::readCStyleComment() {
  while ((current_ + 1) < end_) {
    Char c = getNextChar();
    if (c == '*' && *current_ == '/')
      break;
  }
  return getNextChar() == '/';
}

bool Reader::readCppStyleComment() {
  while (current_ != end_) {
    Char c = getNextChar();
    if (c == '\n')
      break;
    if (c == '\r') {
      // Consume DOS EOL. It will be normalized in addComment.
      if (current_ != end_ && *current_ == '\n')
        getNextChar();
      // Break on Moc OS 9 EOL.
      break;
    }
  }
  return true;
}

void Reader::readNumber() {
  Location p = current_;
  char c = '0'; // stopgap for already consumed character
  // integral part
  while (c >= '0' && c <= '9')
    c = (current_ = p) < end_ ? *p++ : '\0';
  // fractional part
  if (c == '.') {
    c = (current_ = p) < end_ ? *p++ : '\0';
    while (c >= '0' && c <= '9')
      c = (current_ = p) < end_ ? *p++ : '\0';
  }
  // exponential part
  if (c == 'e' || c == 'E') {
    c = (current_ = p) < end_ ? *p++ : '\0';
    if (c == '+' || c == '-')
      c = (current_ = p) < end_ ? *p++ : '\0';
    while (c >= '0' && c <= '9')
      c = (current_ = p) < end_ ? *p++ : '\0';
  }
}

bool Reader::readString() {
  Char c = '\0';
  while (current_ != end_) {
    c = getNextChar();
    if (c == '\\')
      getNextChar();
    else if (c == '"')
      break;
  }
  return c == '"';
}

bool Reader::readObject(Token& token) {
  Token tokenName;
  String name;
  Value init(objectValue);
  currentValue().swapPayload(init);
  currentValue().setOffsetStart(token.start_ - begin_);
  while (readToken(tokenName)) {
    bool initialTokenOk = true;
    while (tokenName.type_ == tokenComment && initialTokenOk)
      initialTokenOk = readToken(tokenName);
    if (!initialTokenOk)
      break;
    if (tokenName.type_ == tokenObjectEnd && name.empty()) // empty object
      return true;
    name.clear();
    if (tokenName.type_ == tokenString) {
      if (!decodeString(tokenName, name))
        return recoverFromError(tokenObjectEnd);
    } else if (tokenName.type_ == tokenNumber && features_.allowNumericKeys_) {
      Value numberName;
      if (!decodeNumber(tokenName, numberName))
        return recoverFromError(tokenObjectEnd);
      name = numberName.asString();
    } else {
      break;
    }

    Token colon;
    if (!readToken(colon) || colon.type_ != tokenMemberSeparator) {
      return addErrorAndRecover("Missing ':' after object member name", colon,
                                tokenObjectEnd);
    }
    Value& value = currentValue()[name];
    nodes_.push(&value);
    bool ok = readValue();
    nodes_.pop();
    if (!ok) // error already set
      return recoverFromError(tokenObjectEnd);

    Token comma;
    if (!readToken(comma) ||
        (comma.type_ != tokenObjectEnd && comma.type_ != tokenArraySeparator &&
         comma.type_ != tokenComment)) {
      return addErrorAndRecover("Missing ',' or '}' in object declaration",
                                comma, tokenObjectEnd);
    }
    bool finalizeTokenOk = true;
    while (comma.type_ == tokenComment && finalizeTokenOk)
      finalizeTokenOk = readToken(comma);
    if (comma.type_ == tokenObjectEnd)
      return true;
  }
  return addErrorAndRecover("Missing '}' or object member name", tokenName,
                            tokenObjectEnd);
}

bool Reader::readArray(Token& token) {
  Value init(arrayValue);
  currentValue().swapPayload(init);
  currentValue().setOffsetStart(token.start_ - begin_);
  skipSpaces();
  if (current_ != end_ && *current_ == ']') // empty array
  {
    Token endArray;
    readToken(endArray);
    return true;
  }
  int index = 0;
  for (;;) {
    Value& value = currentValue()[index++];
    nodes_.push(&value);
    bool ok = readValue();
    nodes_.pop();
    if (!ok) // error already set
      return recoverFromError(tokenArrayEnd);

    Token currentToken;
    // Accept Comment after last item in the array.
    ok = readToken(currentToken);
    while (currentToken.type_ == tokenComment && ok) {
      ok = readToken(currentToken);
    }
    bool badTokenType = (currentToken.type_ != tokenArraySeparator &&
                         currentToken.type_ != tokenArrayEnd);
    if (!ok || badTokenType) {
      return addErrorAndRecover("Missing ',' or ']' in array declaration",
                                currentToken, tokenArrayEnd);
    }
    if (currentToken.type_ == tokenArrayEnd)
      break;
  }
  return true;
}

bool Reader::decodeNumber(Token& token) {
  Value decoded;
  if (!decodeNumber(token, decoded))
    return false;
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

bool Reader::decodeNumber(Token& token, Value& decoded) {
  // Attempts to parse the number as an integer. If the number is
  // larger than the maximum supported value of an integer then
  // we decode the number as a double.
  Location current = token.start_;
  bool isNegative = *current == '-';
  if (isNegative)
    ++current;
  // TODO: Help the compiler do the div and mod at compile time or get rid of
  // them.
  Value::LargestUInt maxIntegerValue =
      isNegative ? Value::LargestUInt(Value::maxLargestInt) + 1
                 : Value::maxLargestUInt;
  Value::LargestUInt threshold = maxIntegerValue / 10;
  Value::LargestUInt value = 0;
  while (current < token.end_) {
    Char c = *current++;
    if (c < '0' || c > '9')
      return decodeDouble(token, decoded);
    auto digit(static_cast<Value::UInt>(c - '0'));
    if (value >= threshold) {
      // We've hit or exceeded the max value divided by 10 (rounded down). If
      // a) we've only just touched the limit, b) this is the last digit, and
      // c) it's small enough to fit in that rounding delta, we're okay.
      // Otherwise treat this number as a double to avoid overflow.
      if (value > threshold || current != token.end_ ||
          digit > maxIntegerValue % 10) {
        return decodeDouble(token, decoded);
      }
    }
    value = value * 10 + digit;
  }
  if (isNegative && value == maxIntegerValue)
    decoded = Value::minLargestInt;
  else if (isNegative)
    decoded = -Value::LargestInt(value);
  else if (value <= Value::LargestUInt(Value::maxInt))
    decoded = Value::LargestInt(value);
  else
    decoded = value;
  return true;
}

bool Reader::decodeDouble(Token& token) {
  Value decoded;
  if (!decodeDouble(token, decoded))
    return false;
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

bool Reader::decodeDouble(Token& token, Value& decoded) {
  double value = 0;
  String buffer(token.start_, token.end_);
  IStringStream is(buffer);
  if (!(is >> value))
    return addError(
        "'" + String(token.start_, token.end_) + "' is not a number.", token);
  decoded = value;
  return true;
}

bool Reader::decodeString(Token& token) {
  String decoded_string;
  if (!decodeString(token, decoded_string))
    return false;
  Value decoded(decoded_string);
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

bool Reader::decodeString(Token& token, String& decoded) {
  decoded.reserve(static_cast<size_t>(token.end_ - token.start_ - 2));
  Location current = token.start_ + 1; // skip '"'
  Location end = token.end_ - 1;       // do not include '"'
  while (current != end) {
    Char c = *current++;
    if (c == '"')
      break;
    if (c == '\\') {
      if (current == end)
        return addError("Empty escape sequence in string", token, current);
      Char escape = *current++;
      switch (escape) {
      case '"':
        decoded += '"';
        break;
      case '/':
        decoded += '/';
        break;
      case '\\':
        decoded += '\\';
        break;
      case 'b':
        decoded += '\b';
        break;
      case 'f':
        decoded += '\f';
        break;
      case 'n':
        decoded += '\n';
        break;
      case 'r':
        decoded += '\r';
        break;
      case 't':
        decoded += '\t';
        break;
      case 'u': {
        unsigned int unicode;
        if (!decodeUnicodeCodePoint(token, current, end, unicode))
          return false;
        decoded += codePointToUTF8(unicode);
      } break;
      default:
        return addError("Bad escape sequence in string", token, current);
      }
    } else {
      decoded += c;
    }
  }
  return true;
}

bool Reader::decodeUnicodeCodePoint(Token& token, Location& current,
                                    Location end, unsigned int& unicode) {

  if (!decodeUnicodeEscapeSequence(token, current, end, unicode))
    return false;
  if (unicode >= 0xD800 && unicode <= 0xDBFF) {
    // surrogate pairs
    if (end - current < 6)
      return addError(
          "additional six characters expected to parse unicode surrogate pair.",
          token, current);
    if (*(current++) == '\\' && *(current++) == 'u') {
      unsigned int surrogatePair;
      if (decodeUnicodeEscapeSequence(token, current, end, surrogatePair)) {
        unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
      } else
        return false;
    } else
      return addError("expecting another \\u token to begin the second half of "
                      "a unicode surrogate pair",
                      token, current);
  }
  return true;
}

bool Reader::decodeUnicodeEscapeSequence(Token& token, Location& current,
                                         Location end,
                                         unsigned int& ret_unicode) {
  if (end - current < 4)
    return addError(
        "Bad unicode escape sequence in string: four digits expected.", token,
        current);
  int unicode = 0;
  for (int index = 0; index < 4; ++index) {
    Char c = *current++;
    unicode *= 16;
    if (c >= '0' && c <= '9')
      unicode += c - '0';
    else if (c >= 'a' && c <= 'f')
      unicode += c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      unicode += c - 'A' + 10;
    else
      return addError(
          "Bad unicode escape sequence in string: hexadecimal digit expected.",
          token, current);
  }
  ret_unicode = static_cast<unsigned int>(unicode);
  return true;
}

bool Reader::addError(const String& message, Token& token, Location extra) {
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = extra;
  errors_.push_back(info);
  return false;
}

bool Reader::recoverFromError(TokenType skipUntilToken) {
  size_t const errorCount = errors_.size();
  Token skip;
  for (;;) {
    if (!readToken(skip))
      errors_.resize(errorCount); // discard errors caused by recovery
    if (skip.type_ == skipUntilToken || skip.type_ == tokenEndOfStream)
      break;
  }
  errors_.resize(errorCount);
  return false;
}

bool Reader::addErrorAndRecover(const String& message, Token& token,
                                TokenType skipUntilToken) {
  addError(message, token);
  return recoverFromError(skipUntilToken);
}

Value& Reader::currentValue() { return *(nodes_.top()); }

Reader::Char Reader::getNextChar() {
  if (current_ == end_)
    return 0;
  return *current_++;
}

void Reader::getLocationLineAndColumn(Location location, int& line,
                                      int& column) const {
  Location current = begin_;
  Location lastLineStart = current;
  line = 0;
  while (current < location && current != end_) {
    Char c = *current++;
    if (c == '\r') {
      if (*current == '\n')
        ++current;
      lastLineStart = current;
      ++line;
    } else if (c == '\n') {
      lastLineStart = current;
      ++line;
    }
  }
  // column & line start at 1
  column = int(location - lastLineStart) + 1;
  ++line;
}

String Reader::getLocationLineAndColumn(Location location) const {
  int line, column;
  getLocationLineAndColumn(location, line, column);
  char buffer[18 + 16 + 16 + 1];
  jsoncpp_snprintf(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
  return buffer;
}

// Deprecated. Preserved for backward compatibility
String Reader::getFormatedErrorMessages() const {
  return getFormattedErrorMessages();
}

String Reader::getFormattedErrorMessages() const {
  String formattedMessage;
  for (const auto& error : errors_) {
    formattedMessage +=
        "* " + getLocationLineAndColumn(error.token_.start_) + "\n";
    formattedMessage += "  " + error.message_ + "\n";
    if (error.extra_)
      formattedMessage +=
          "See " + getLocationLineAndColumn(error.extra_) + " for detail.\n";
  }
  return formattedMessage;
}

std::vector<Reader::StructuredError> Reader::getStructuredErrors() const {
  std::vector<Reader::StructuredError> allErrors;
  for (const auto& error : errors_) {
    Reader::StructuredError structured;
    structured.offset_start = error.token_.start_ - begin_;
    structured.offset_limit = error.token_.end_ - begin_;
    structured.message = error.message_;
    allErrors.push_back(structured);
  }
  return allErrors;
}

bool Reader::pushError(const Value& value, const String& message) {
  ptrdiff_t const length = end_ - begin_;
  if (value.getOffsetStart() > length || value.getOffsetLimit() > length)
    return false;
  Token token;
  token.type_ = tokenError;
  token.start_ = begin_ + value.getOffsetStart();
  token.end_ = begin_ + value.getOffsetLimit();
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = nullptr;
  errors_.push_back(info);
  return true;
}

bool Reader::pushError(const Value& value, const String& message,
                       const Value& extra) {
  ptrdiff_t const length = end_ - begin_;
  if (value.getOffsetStart() > length || value.getOffsetLimit() > length ||
      extra.getOffsetLimit() > length)
    return false;
  Token token;
  token.type_ = tokenError;
  token.start_ = begin_ + value.getOffsetStart();
  token.end_ = begin_ + value.getOffsetLimit();
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = begin_ + extra.getOffsetStart();
  errors_.push_back(info);
  return true;
}

bool Reader::good() const { return errors_.empty(); }

// Originally copied from the Features class (now deprecated), used internally
// for features implementation.
class OurFeatures {
public:
  static OurFeatures all();
  bool allowComments_;
  bool allowTrailingCommas_;
  bool strictRoot_;
  bool allowDroppedNullPlaceholders_;
  bool allowNumericKeys_;
  bool allowSingleQuotes_;
  bool failIfExtra_;
  bool rejectDupKeys_;
  bool allowSpecialFloats_;
  bool skipBom_;
  size_t stackLimit_;
}; // OurFeatures

OurFeatures OurFeatures::all() { return {}; }

// Implementation of class Reader
// ////////////////////////////////

// Originally copied from the Reader class (now deprecated), used internally
// for implementing JSON reading.
class OurReader {
public:
  using Char = char;
  using Location = const Char*;
  struct StructuredError {
    ptrdiff_t offset_start;
    ptrdiff_t offset_limit;
    String message;
  };

  explicit OurReader(OurFeatures const& features);
  bool parse(const char* beginDoc, const char* endDoc, Value& root,
             bool collectComments = true);
  String getFormattedErrorMessages() const;
  std::vector<StructuredError> getStructuredErrors() const;

private:
  OurReader(OurReader const&);      // no impl
  void operator=(OurReader const&); // no impl

  enum TokenType {
    tokenEndOfStream = 0,
    tokenObjectBegin,
    tokenObjectEnd,
    tokenArrayBegin,
    tokenArrayEnd,
    tokenString,
    tokenNumber,
    tokenTrue,
    tokenFalse,
    tokenNull,
    tokenNaN,
    tokenPosInf,
    tokenNegInf,
    tokenArraySeparator,
    tokenMemberSeparator,
    tokenComment,
    tokenError
  };

  class Token {
  public:
    TokenType type_;
    Location start_;
    Location end_;
  };

  class ErrorInfo {
  public:
    Token token_;
    String message_;
    Location extra_;
  };

  using Errors = std::deque<ErrorInfo>;

  bool readToken(Token& token);
  void skipSpaces();
  void skipBom(bool skipBom);
  bool match(const Char* pattern, int patternLength);
  bool readComment();
  bool readCStyleComment(bool* containsNewLineResult);
  bool readCppStyleComment();
  bool readString();
  bool readStringSingleQuote();
  bool readNumber(bool checkInf);
  bool readValue();
  bool readObject(Token& token);
  bool readArray(Token& token);
  bool decodeNumber(Token& token);
  bool decodeNumber(Token& token, Value& decoded);
  bool decodeString(Token& token);
  bool decodeString(Token& token, String& decoded);
  bool decodeDouble(Token& token);
  bool decodeDouble(Token& token, Value& decoded);
  bool decodeUnicodeCodePoint(Token& token, Location& current, Location end,
                              unsigned int& unicode);
  bool decodeUnicodeEscapeSequence(Token& token, Location& current,
                                   Location end, unsigned int& unicode);
  bool addError(const String& message, Token& token, Location extra = nullptr);
  bool recoverFromError(TokenType skipUntilToken);
  bool addErrorAndRecover(const String& message, Token& token,
                          TokenType skipUntilToken);
  void skipUntilSpace();
  Value& currentValue();
  Char getNextChar();
  void getLocationLineAndColumn(Location location, int& line,
                                int& column) const;
  String getLocationLineAndColumn(Location location) const;
  void addComment(Location begin, Location end, CommentPlacement placement);
  void skipCommentTokens(Token& token);

  static String normalizeEOL(Location begin, Location end);
  static bool containsNewLine(Location begin, Location end);

  using Nodes = std::stack<Value*>;

  Nodes nodes_{};
  Errors errors_{};
  String document_{};
  Location begin_ = nullptr;
  Location end_ = nullptr;
  Location current_ = nullptr;
  Location lastValueEnd_ = nullptr;
  Value* lastValue_ = nullptr;
  bool lastValueHasAComment_ = false;
  String commentsBefore_{};

  OurFeatures const features_;
  bool collectComments_ = false;
}; // OurReader

// complete copy of Read impl, for OurReader

bool OurReader::containsNewLine(OurReader::Location begin,
                                OurReader::Location end) {
  return std::any_of(begin, end, [](char b) { return b == '\n' || b == '\r'; });
}

OurReader::OurReader(OurFeatures const& features) : features_(features) {}

bool OurReader::parse(const char* beginDoc, const char* endDoc, Value& root,
                      bool collectComments) {
  if (!features_.allowComments_) {
    collectComments = false;
  }

  begin_ = beginDoc;
  end_ = endDoc;
  collectComments_ = collectComments;
  current_ = begin_;
  lastValueEnd_ = nullptr;
  lastValue_ = nullptr;
  commentsBefore_.clear();
  errors_.clear();
  while (!nodes_.empty())
    nodes_.pop();
  nodes_.push(&root);

  // skip byte order mark if it exists at the beginning of the UTF-8 text.
  skipBom(features_.skipBom_);
  bool successful = readValue();
  nodes_.pop();
  Token token;
  skipCommentTokens(token);
  if (features_.failIfExtra_ && (token.type_ != tokenEndOfStream)) {
    addError("Extra non-whitespace after JSON value.", token);
    return false;
  }
  if (collectComments_ && !commentsBefore_.empty())
    root.setComment(commentsBefore_, commentAfter);
  if (features_.strictRoot_) {
    if (!root.isArray() && !root.isObject()) {
      // Set error location to start of doc, ideally should be first token found
      // in doc
      token.type_ = tokenError;
      token.start_ = beginDoc;
      token.end_ = endDoc;
      addError(
          "A valid JSON document must be either an array or an object value.",
          token);
      return false;
    }
  }
  return successful;
}

bool OurReader::readValue() {
  //  To preserve the old behaviour we cast size_t to int.
  if (nodes_.size() > features_.stackLimit_)
    throwRuntimeError("Exceeded stackLimit in readValue().");
  Token token;
  skipCommentTokens(token);
  bool successful = true;

  if (collectComments_ && !commentsBefore_.empty()) {
    currentValue().setComment(commentsBefore_, commentBefore);
    commentsBefore_.clear();
  }

  switch (token.type_) {
  case tokenObjectBegin:
    successful = readObject(token);
    currentValue().setOffsetLimit(current_ - begin_);
    break;
  case tokenArrayBegin:
    successful = readArray(token);
    currentValue().setOffsetLimit(current_ - begin_);
    break;
  case tokenNumber:
    successful = decodeNumber(token);
    break;
  case tokenString:
    successful = decodeString(token);
    break;
  case tokenTrue: {
    Value v(true);
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenFalse: {
    Value v(false);
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenNull: {
    Value v;
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenNaN: {
    Value v(std::numeric_limits<double>::quiet_NaN());
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenPosInf: {
    Value v(std::numeric_limits<double>::infinity());
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenNegInf: {
    Value v(-std::numeric_limits<double>::infinity());
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenArraySeparator:
  case tokenObjectEnd:
  case tokenArrayEnd:
    if (features_.allowDroppedNullPlaceholders_) {
      // "Un-read" the current token and mark the current value as a null
      // token.
      current_--;
      Value v;
      currentValue().swapPayload(v);
      currentValue().setOffsetStart(current_ - begin_ - 1);
      currentValue().setOffsetLimit(current_ - begin_);
      break;
    } // else, fall through ...
  default:
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    return addError("Syntax error: value, object or array expected.", token);
  }

  if (collectComments_) {
    lastValueEnd_ = current_;
    lastValueHasAComment_ = false;
    lastValue_ = &currentValue();
  }

  return successful;
}

void OurReader::skipCommentTokens(Token& token) {
  if (features_.allowComments_) {
    do {
      readToken(token);
    } while (token.type_ == tokenComment);
  } else {
    readToken(token);
  }
}

bool OurReader::readToken(Token& token) {
  skipSpaces();
  token.start_ = current_;
  Char c = getNextChar();
  bool ok = true;
  switch (c) {
  case '{':
    token.type_ = tokenObjectBegin;
    break;
  case '}':
    token.type_ = tokenObjectEnd;
    break;
  case '[':
    token.type_ = tokenArrayBegin;
    break;
  case ']':
    token.type_ = tokenArrayEnd;
    break;
  case '"':
    token.type_ = tokenString;
    ok = readString();
    break;
  case '\'':
    if (features_.allowSingleQuotes_) {
      token.type_ = tokenString;
      ok = readStringSingleQuote();
      break;
    } // else fall through
  case '/':
    token.type_ = tokenComment;
    ok = readComment();
    break;
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
    token.type_ = tokenNumber;
    readNumber(false);
    break;
  case '-':
    if (readNumber(true)) {
      token.type_ = tokenNumber;
    } else {
      token.type_ = tokenNegInf;
      ok = features_.allowSpecialFloats_ && match("nfinity", 7);
    }
    break;
  case '+':
    if (readNumber(true)) {
      token.type_ = tokenNumber;
    } else {
      token.type_ = tokenPosInf;
      ok = features_.allowSpecialFloats_ && match("nfinity", 7);
    }
    break;
  case 't':
    token.type_ = tokenTrue;
    ok = match("rue", 3);
    break;
  case 'f':
    token.type_ = tokenFalse;
    ok = match("alse", 4);
    break;
  case 'n':
    token.type_ = tokenNull;
    ok = match("ull", 3);
    break;
  case 'N':
    if (features_.allowSpecialFloats_) {
      token.type_ = tokenNaN;
      ok = match("aN", 2);
    } else {
      ok = false;
    }
    break;
  case 'I':
    if (features_.allowSpecialFloats_) {
      token.type_ = tokenPosInf;
      ok = match("nfinity", 7);
    } else {
      ok = false;
    }
    break;
  case ',':
    token.type_ = tokenArraySeparator;
    break;
  case ':':
    token.type_ = tokenMemberSeparator;
    break;
  case 0:
    token.type_ = tokenEndOfStream;
    break;
  default:
    ok = false;
    break;
  }
  if (!ok)
    token.type_ = tokenError;
  token.end_ = current_;
  return ok;
}

void OurReader::skipSpaces() {
  while (current_ != end_) {
    Char c = *current_;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      ++current_;
    else
      break;
  }
}

void OurReader::skipBom(bool skipBom) {
  // The default behavior is to skip BOM.
  if (skipBom) {
    if ((end_ - begin_) >= 3 && strncmp(begin_, "\xEF\xBB\xBF", 3) == 0) {
      begin_ += 3;
      current_ = begin_;
    }
  }
}

bool OurReader::match(const Char* pattern, int patternLength) {
  if (end_ - current_ < patternLength)
    return false;
  int index = patternLength;
  while (index--)
    if (current_[index] != pattern[index])
      return false;
  current_ += patternLength;
  return true;
}

bool OurReader::readComment() {
  const Location commentBegin = current_ - 1;
  const Char c = getNextChar();
  bool successful = false;
  bool cStyleWithEmbeddedNewline = false;

  const bool isCStyleComment = (c == '*');
  const bool isCppStyleComment = (c == '/');
  if (isCStyleComment) {
    successful = readCStyleComment(&cStyleWithEmbeddedNewline);
  } else if (isCppStyleComment) {
    successful = readCppStyleComment();
  }

  if (!successful)
    return false;

  if (collectComments_) {
    CommentPlacement placement = commentBefore;

    if (!lastValueHasAComment_) {
      if (lastValueEnd_ && !containsNewLine(lastValueEnd_, commentBegin)) {
        if (isCppStyleComment || !cStyleWithEmbeddedNewline) {
          placement = commentAfterOnSameLine;
          lastValueHasAComment_ = true;
        }
      }
    }

    addComment(commentBegin, current_, placement);
  }
  return true;
}

String OurReader::normalizeEOL(OurReader::Location begin,
                               OurReader::Location end) {
  String normalized;
  normalized.reserve(static_cast<size_t>(end - begin));
  OurReader::Location current = begin;
  while (current != end) {
    char c = *current++;
    if (c == '\r') {
      if (current != end && *current == '\n')
        // convert dos EOL
        ++current;
      // convert Mac EOL
      normalized += '\n';
    } else {
      normalized += c;
    }
  }
  return normalized;
}

void OurReader::addComment(Location begin, Location end,
                           CommentPlacement placement) {
  assert(collectComments_);
  const String& normalized = normalizeEOL(begin, end);
  if (placement == commentAfterOnSameLine) {
    assert(lastValue_ != nullptr);
    lastValue_->setComment(normalized, placement);
  } else {
    commentsBefore_ += normalized;
  }
}

bool OurReader::readCStyleComment(bool* containsNewLineResult) {
  *containsNewLineResult = false;

  while ((current_ + 1) < end_) {
    Char c = getNextChar();
    if (c == '*' && *current_ == '/')
      break;
    if (c == '\n')
      *containsNewLineResult = true;
  }

  return getNextChar() == '/';
}

bool OurReader::readCppStyleComment() {
  while (current_ != end_) {
    Char c = getNextChar();
    if (c == '\n')
      break;
    if (c == '\r') {
      // Consume DOS EOL. It will be normalized in addComment.
      if (current_ != end_ && *current_ == '\n')
        getNextChar();
      // Break on Moc OS 9 EOL.
      break;
    }
  }
  return true;
}

bool OurReader::readNumber(bool checkInf) {
  Location p = current_;
  if (checkInf && p != end_ && *p == 'I') {
    current_ = ++p;
    return false;
  }
  char c = '0'; // stopgap for already consumed character
  // integral part
  while (c >= '0' && c <= '9')
    c = (current_ = p) < end_ ? *p++ : '\0';
  // fractional part
  if (c == '.') {
    c = (current_ = p) < end_ ? *p++ : '\0';
    while (c >= '0' && c <= '9')
      c = (current_ = p) < end_ ? *p++ : '\0';
  }
  // exponential part
  if (c == 'e' || c == 'E') {
    c = (current_ = p) < end_ ? *p++ : '\0';
    if (c == '+' || c == '-')
      c = (current_ = p) < end_ ? *p++ : '\0';
    while (c >= '0' && c <= '9')
      c = (current_ = p) < end_ ? *p++ : '\0';
  }
  return true;
}
bool OurReader::readString() {
  Char c = 0;
  while (current_ != end_) {
    c = getNextChar();
    if (c == '\\')
      getNextChar();
    else if (c == '"')
      break;
  }
  return c == '"';
}

bool OurReader::readStringSingleQuote() {
  Char c = 0;
  while (current_ != end_) {
    c = getNextChar();
    if (c == '\\')
      getNextChar();
    else if (c == '\'')
      break;
  }
  return c == '\'';
}

bool OurReader::readObject(Token& token) {
  Token tokenName;
  String name;
  Value init(objectValue);
  currentValue().swapPayload(init);
  currentValue().setOffsetStart(token.start_ - begin_);
  while (readToken(tokenName)) {
    bool initialTokenOk = true;
    while (tokenName.type_ == tokenComment && initialTokenOk)
      initialTokenOk = readToken(tokenName);
    if (!initialTokenOk)
      break;
    if (tokenName.type_ == tokenObjectEnd &&
        (name.empty() ||
         features_.allowTrailingCommas_)) // empty object or trailing comma
      return true;
    name.clear();
    if (tokenName.type_ == tokenString) {
      if (!decodeString(tokenName, name))
        return recoverFromError(tokenObjectEnd);
    } else if (tokenName.type_ == tokenNumber && features_.allowNumericKeys_) {
      Value numberName;
      if (!decodeNumber(tokenName, numberName))
        return recoverFromError(tokenObjectEnd);
      name = numberName.asString();
    } else {
      break;
    }
    if (name.length() >= (1U << 30))
      throwRuntimeError("keylength >= 2^30");
    if (features_.rejectDupKeys_ && currentValue().isMember(name)) {
      String msg = "Duplicate key: '" + name + "'";
      return addErrorAndRecover(msg, tokenName, tokenObjectEnd);
    }

    Token colon;
    if (!readToken(colon) || colon.type_ != tokenMemberSeparator) {
      return addErrorAndRecover("Missing ':' after object member name", colon,
                                tokenObjectEnd);
    }
    Value& value = currentValue()[name];
    nodes_.push(&value);
    bool ok = readValue();
    nodes_.pop();
    if (!ok) // error already set
      return recoverFromError(tokenObjectEnd);

    Token comma;
    if (!readToken(comma) ||
        (comma.type_ != tokenObjectEnd && comma.type_ != tokenArraySeparator &&
         comma.type_ != tokenComment)) {
      return addErrorAndRecover("Missing ',' or '}' in object declaration",
                                comma, tokenObjectEnd);
    }
    bool finalizeTokenOk = true;
    while (comma.type_ == tokenComment && finalizeTokenOk)
      finalizeTokenOk = readToken(comma);
    if (comma.type_ == tokenObjectEnd)
      return true;
  }
  return addErrorAndRecover("Missing '}' or object member name", tokenName,
                            tokenObjectEnd);
}

bool OurReader::readArray(Token& token) {
  Value init(arrayValue);
  currentValue().swapPayload(init);
  currentValue().setOffsetStart(token.start_ - begin_);
  int index = 0;
  for (;;) {
    skipSpaces();
    if (current_ != end_ && *current_ == ']' &&
        (index == 0 ||
         (features_.allowTrailingCommas_ &&
          !features_.allowDroppedNullPlaceholders_))) // empty array or trailing
                                                      // comma
    {
      Token endArray;
      readToken(endArray);
      return true;
    }
    Value& value = currentValue()[index++];
    nodes_.push(&value);
    bool ok = readValue();
    nodes_.pop();
    if (!ok) // error already set
      return recoverFromError(tokenArrayEnd);

    Token currentToken;
    // Accept Comment after last item in the array.
    ok = readToken(currentToken);
    while (currentToken.type_ == tokenComment && ok) {
      ok = readToken(currentToken);
    }
    bool badTokenType = (currentToken.type_ != tokenArraySeparator &&
                         currentToken.type_ != tokenArrayEnd);
    if (!ok || badTokenType) {
      return addErrorAndRecover("Missing ',' or ']' in array declaration",
                                currentToken, tokenArrayEnd);
    }
    if (currentToken.type_ == tokenArrayEnd)
      break;
  }
  return true;
}

bool OurReader::decodeNumber(Token& token) {
  Value decoded;
  if (!decodeNumber(token, decoded))
    return false;
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

bool OurReader::decodeNumber(Token& token, Value& decoded) {
  // Attempts to parse the number as an integer. If the number is
  // larger than the maximum supported value of an integer then
  // we decode the number as a double.
  Location current = token.start_;
  const bool isNegative = *current == '-';
  if (isNegative) {
    ++current;
  }

  // We assume we can represent the largest and smallest integer types as
  // unsigned integers with separate sign. This is only true if they can fit
  // into an unsigned integer.
  static_assert(Value::maxLargestInt <= Value::maxLargestUInt,
                "Int must be smaller than UInt");

  // We need to convert minLargestInt into a positive number. The easiest way
  // to do this conversion is to assume our "threshold" value of minLargestInt
  // divided by 10 can fit in maxLargestInt when absolute valued. This should
  // be a safe assumption.
  static_assert(Value::minLargestInt <= -Value::maxLargestInt,
                "The absolute value of minLargestInt must be greater than or "
                "equal to maxLargestInt");
  static_assert(Value::minLargestInt / 10 >= -Value::maxLargestInt,
                "The absolute value of minLargestInt must be only 1 magnitude "
                "larger than maxLargest Int");

  static constexpr Value::LargestUInt positive_threshold =
      Value::maxLargestUInt / 10;
  static constexpr Value::UInt positive_last_digit = Value::maxLargestUInt % 10;

  // For the negative values, we have to be more careful. Since typically
  // -Value::minLargestInt will cause an overflow, we first divide by 10 and
  // then take the inverse. This assumes that minLargestInt is only a single
  // power of 10 different in magnitude, which we check above. For the last
  // digit, we take the modulus before negating for the same reason.
  static constexpr auto negative_threshold =
      Value::LargestUInt(-(Value::minLargestInt / 10));
  static constexpr auto negative_last_digit =
      Value::UInt(-(Value::minLargestInt % 10));

  const Value::LargestUInt threshold =
      isNegative ? negative_threshold : positive_threshold;
  const Value::UInt max_last_digit =
      isNegative ? negative_last_digit : positive_last_digit;

  Value::LargestUInt value = 0;
  while (current < token.end_) {
    Char c = *current++;
    if (c < '0' || c > '9')
      return decodeDouble(token, decoded);

    const auto digit(static_cast<Value::UInt>(c - '0'));
    if (value >= threshold) {
      // We've hit or exceeded the max value divided by 10 (rounded down). If
      // a) we've only just touched the limit, meaing value == threshold,
      // b) this is the last digit, or
      // c) it's small enough to fit in that rounding delta, we're okay.
      // Otherwise treat this number as a double to avoid overflow.
      if (value > threshold || current != token.end_ ||
          digit > max_last_digit) {
        return decodeDouble(token, decoded);
      }
    }
    value = value * 10 + digit;
  }

  if (isNegative) {
    // We use the same magnitude assumption here, just in case.
    const auto last_digit = static_cast<Value::UInt>(value % 10);
    decoded = -Value::LargestInt(value / 10) * 10 - last_digit;
  } else if (value <= Value::LargestUInt(Value::maxLargestInt)) {
    decoded = Value::LargestInt(value);
  } else {
    decoded = value;
  }

  return true;
}

bool OurReader::decodeDouble(Token& token) {
  Value decoded;
  if (!decodeDouble(token, decoded))
    return false;
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

bool OurReader::decodeDouble(Token& token, Value& decoded) {
  double value = 0;
  const String buffer(token.start_, token.end_);
  IStringStream is(buffer);
  if (!(is >> value)) {
    return addError(
        "'" + String(token.start_, token.end_) + "' is not a number.", token);
  }
  decoded = value;
  return true;
}

bool OurReader::decodeString(Token& token) {
  String decoded_string;
  if (!decodeString(token, decoded_string))
    return false;
  Value decoded(decoded_string);
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

bool OurReader::decodeString(Token& token, String& decoded) {
  decoded.reserve(static_cast<size_t>(token.end_ - token.start_ - 2));
  Location current = token.start_ + 1; // skip '"'
  Location end = token.end_ - 1;       // do not include '"'
  while (current != end) {
    Char c = *current++;
    if (c == '"')
      break;
    if (c == '\\') {
      if (current == end)
        return addError("Empty escape sequence in string", token, current);
      Char escape = *current++;
      switch (escape) {
      case '"':
        decoded += '"';
        break;
      case '/':
        decoded += '/';
        break;
      case '\\':
        decoded += '\\';
        break;
      case 'b':
        decoded += '\b';
        break;
      case 'f':
        decoded += '\f';
        break;
      case 'n':
        decoded += '\n';
        break;
      case 'r':
        decoded += '\r';
        break;
      case 't':
        decoded += '\t';
        break;
      case 'u': {
        unsigned int unicode;
        if (!decodeUnicodeCodePoint(token, current, end, unicode))
          return false;
        decoded += codePointToUTF8(unicode);
      } break;
      default:
        return addError("Bad escape sequence in string", token, current);
      }
    } else {
      decoded += c;
    }
  }
  return true;
}

bool OurReader::decodeUnicodeCodePoint(Token& token, Location& current,
                                       Location end, unsigned int& unicode) {

  if (!decodeUnicodeEscapeSequence(token, current, end, unicode))
    return false;
  if (unicode >= 0xD800 && unicode <= 0xDBFF) {
    // surrogate pairs
    if (end - current < 6)
      return addError(
          "additional six characters expected to parse unicode surrogate pair.",
          token, current);
    if (*(current++) == '\\' && *(current++) == 'u') {
      unsigned int surrogatePair;
      if (decodeUnicodeEscapeSequence(token, current, end, surrogatePair)) {
        unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
      } else
        return false;
    } else
      return addError("expecting another \\u token to begin the second half of "
                      "a unicode surrogate pair",
                      token, current);
  }
  return true;
}

bool OurReader::decodeUnicodeEscapeSequence(Token& token, Location& current,
                                            Location end,
                                            unsigned int& ret_unicode) {
  if (end - current < 4)
    return addError(
        "Bad unicode escape sequence in string: four digits expected.", token,
        current);
  int unicode = 0;
  for (int index = 0; index < 4; ++index) {
    Char c = *current++;
    unicode *= 16;
    if (c >= '0' && c <= '9')
      unicode += c - '0';
    else if (c >= 'a' && c <= 'f')
      unicode += c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      unicode += c - 'A' + 10;
    else
      return addError(
          "Bad unicode escape sequence in string: hexadecimal digit expected.",
          token, current);
  }
  ret_unicode = static_cast<unsigned int>(unicode);
  return true;
}

bool OurReader::addError(const String& message, Token& token, Location extra) {
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = extra;
  errors_.push_back(info);
  return false;
}

bool OurReader::recoverFromError(TokenType skipUntilToken) {
  size_t errorCount = errors_.size();
  Token skip;
  for (;;) {
    if (!readToken(skip))
      errors_.resize(errorCount); // discard errors caused by recovery
    if (skip.type_ == skipUntilToken || skip.type_ == tokenEndOfStream)
      break;
  }
  errors_.resize(errorCount);
  return false;
}

bool OurReader::addErrorAndRecover(const String& message, Token& token,
                                   TokenType skipUntilToken) {
  addError(message, token);
  return recoverFromError(skipUntilToken);
}

Value& OurReader::currentValue() { return *(nodes_.top()); }

OurReader::Char OurReader::getNextChar() {
  if (current_ == end_)
    return 0;
  return *current_++;
}

void OurReader::getLocationLineAndColumn(Location location, int& line,
                                         int& column) const {
  Location current = begin_;
  Location lastLineStart = current;
  line = 0;
  while (current < location && current != end_) {
    Char c = *current++;
    if (c == '\r') {
      if (*current == '\n')
        ++current;
      lastLineStart = current;
      ++line;
    } else if (c == '\n') {
      lastLineStart = current;
      ++line;
    }
  }
  // column & line start at 1
  column = int(location - lastLineStart) + 1;
  ++line;
}

String OurReader::getLocationLineAndColumn(Location location) const {
  int line, column;
  getLocationLineAndColumn(location, line, column);
  char buffer[18 + 16 + 16 + 1];
  jsoncpp_snprintf(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
  return buffer;
}

String OurReader::getFormattedErrorMessages() const {
  String formattedMessage;
  for (const auto& error : errors_) {
    formattedMessage +=
        "* " + getLocationLineAndColumn(error.token_.start_) + "\n";
    formattedMessage += "  " + error.message_ + "\n";
    if (error.extra_)
      formattedMessage +=
          "See " + getLocationLineAndColumn(error.extra_) + " for detail.\n";
  }
  return formattedMessage;
}

std::vector<OurReader::StructuredError> OurReader::getStructuredErrors() const {
  std::vector<OurReader::StructuredError> allErrors;
  for (const auto& error : errors_) {
    OurReader::StructuredError structured;
    structured.offset_start = error.token_.start_ - begin_;
    structured.offset_limit = error.token_.end_ - begin_;
    structured.message = error.message_;
    allErrors.push_back(structured);
  }
  return allErrors;
}

class OurCharReader : public CharReader {
  bool const collectComments_;
  OurReader reader_;

public:
  OurCharReader(bool collectComments, OurFeatures const& features)
      : collectComments_(collectComments), reader_(features) {}
  bool parse(char const* beginDoc, char const* endDoc, Value* root,
             String* errs) override {
    bool ok = reader_.parse(beginDoc, endDoc, *root, collectComments_);
    if (errs) {
      *errs = reader_.getFormattedErrorMessages();
    }
    return ok;
  }
};

CharReaderBuilder::CharReaderBuilder() { setDefaults(&settings_); }
CharReaderBuilder::~CharReaderBuilder() = default;
CharReader* CharReaderBuilder::newCharReader() const {
  bool collectComments = settings_["collectComments"].asBool();
  OurFeatures features = OurFeatures::all();
  features.allowComments_ = settings_["allowComments"].asBool();
  features.allowTrailingCommas_ = settings_["allowTrailingCommas"].asBool();
  features.strictRoot_ = settings_["strictRoot"].asBool();
  features.allowDroppedNullPlaceholders_ =
      settings_["allowDroppedNullPlaceholders"].asBool();
  features.allowNumericKeys_ = settings_["allowNumericKeys"].asBool();
  features.allowSingleQuotes_ = settings_["allowSingleQuotes"].asBool();

  // Stack limit is always a size_t, so we get this as an unsigned int
  // regardless of it we have 64-bit integer support enabled.
  features.stackLimit_ = static_cast<size_t>(settings_["stackLimit"].asUInt());
  features.failIfExtra_ = settings_["failIfExtra"].asBool();
  features.rejectDupKeys_ = settings_["rejectDupKeys"].asBool();
  features.allowSpecialFloats_ = settings_["allowSpecialFloats"].asBool();
  features.skipBom_ = settings_["skipBom"].asBool();
  return new OurCharReader(collectComments, features);
}

bool CharReaderBuilder::validate(Json::Value* invalid) const {
  static const auto& valid_keys = *new std::set<String>{
      "collectComments",
      "allowComments",
      "allowTrailingCommas",
      "strictRoot",
      "allowDroppedNullPlaceholders",
      "allowNumericKeys",
      "allowSingleQuotes",
      "stackLimit",
      "failIfExtra",
      "rejectDupKeys",
      "allowSpecialFloats",
      "skipBom",
  };
  for (auto si = settings_.begin(); si != settings_.end(); ++si) {
    auto key = si.name();
    if (valid_keys.count(key))
      continue;
    if (invalid)
      (*invalid)[std::move(key)] = *si;
    else
      return false;
  }
  return invalid ? invalid->empty() : true;
}

Value& CharReaderBuilder::operator[](const String& key) {
  return settings_[key];
}
// static
void CharReaderBuilder::strictMode(Json::Value* settings) {
  //! [CharReaderBuilderStrictMode]
  (*settings)["allowComments"] = false;
  (*settings)["allowTrailingCommas"] = false;
  (*settings)["strictRoot"] = true;
  (*settings)["allowDroppedNullPlaceholders"] = false;
  (*settings)["allowNumericKeys"] = false;
  (*settings)["allowSingleQuotes"] = false;
  (*settings)["stackLimit"] = 1000;
  (*settings)["failIfExtra"] = true;
  (*settings)["rejectDupKeys"] = true;
  (*settings)["allowSpecialFloats"] = false;
  (*settings)["skipBom"] = true;
  //! [CharReaderBuilderStrictMode]
}
// static
void CharReaderBuilder::setDefaults(Json::Value* settings) {
  //! [CharReaderBuilderDefaults]
  (*settings)["collectComments"] = true;
  (*settings)["allowComments"] = true;
  (*settings)["allowTrailingCommas"] = true;
  (*settings)["strictRoot"] = false;
  (*settings)["allowDroppedNullPlaceholders"] = false;
  (*settings)["allowNumericKeys"] = false;
  (*settings)["allowSingleQuotes"] = false;
  (*settings)["stackLimit"] = 1000;
  (*settings)["failIfExtra"] = false;
  (*settings)["rejectDupKeys"] = false;
  (*settings)["allowSpecialFloats"] = false;
  (*settings)["skipBom"] = true;
  //! [CharReaderBuilderDefaults]
}

//////////////////////////////////
// global functions

bool parseFromStream(CharReader::Factory const& fact, IStream& sin, Value* root,
                     String* errs) {
  OStringStream ssin;
  ssin << sin.rdbuf();
  String doc = ssin.str();
  char const* begin = doc.data();
  char const* end = begin + doc.size();
  // Note that we do not actually need a null-terminator.
  CharReaderPtr const reader(fact.newCharReader());
  return reader->parse(begin, end, root, errs);
}

IStream& operator>>(IStream& sin, Value& root) {
  CharReaderBuilder b;
  String errs;
  bool ok = parseFromStream(b, sin, &root, &errs);
  if (!ok) {
    throwRuntimeError(errs);
  }
  return sin;
}

} // namespace Json

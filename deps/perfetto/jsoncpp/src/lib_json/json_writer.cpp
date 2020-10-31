// Copyright 2011 Baptiste Lepilleur and The JsonCpp Authors
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#if !defined(JSON_IS_AMALGAMATION)
#include "json_tool.h"
#include <json/writer.h>
#endif // if !defined(JSON_IS_AMALGAMATION)
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

#if __cplusplus >= 201103L
#include <cmath>
#include <cstdio>

#if !defined(isnan)
#define isnan std::isnan
#endif

#if !defined(isfinite)
#define isfinite std::isfinite
#endif

#else
#include <cmath>
#include <cstdio>

#if defined(_MSC_VER)
#if !defined(isnan)
#include <float.h>
#define isnan _isnan
#endif

#if !defined(isfinite)
#include <float.h>
#define isfinite _finite
#endif

#if !defined(_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES)
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#endif //_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES

#endif //_MSC_VER

#if defined(__sun) && defined(__SVR4) // Solaris
#if !defined(isfinite)
#include <ieeefp.h>
#define isfinite finite
#endif
#endif

#if defined(__hpux)
#if !defined(isfinite)
#if defined(__ia64) && !defined(finite)
#define isfinite(x)                                                            \
  ((sizeof(x) == sizeof(float) ? _Isfinitef(x) : _IsFinite(x)))
#endif
#endif
#endif

#if !defined(isnan)
// IEEE standard states that NaN values will not compare to themselves
#define isnan(x) (x != x)
#endif

#if !defined(__APPLE__)
#if !defined(isfinite)
#define isfinite finite
#endif
#endif
#endif

#if defined(_MSC_VER)
// Disable warning about strdup being deprecated.
#pragma warning(disable : 4996)
#endif

namespace Json {

#if __cplusplus >= 201103L || (defined(_CPPLIB_VER) && _CPPLIB_VER >= 520)
using StreamWriterPtr = std::unique_ptr<StreamWriter>;
#else
using StreamWriterPtr = std::auto_ptr<StreamWriter>;
#endif

String valueToString(LargestInt value) {
  UIntToStringBuffer buffer;
  char* current = buffer + sizeof(buffer);
  if (value == Value::minLargestInt) {
    uintToString(LargestUInt(Value::maxLargestInt) + 1, current);
    *--current = '-';
  } else if (value < 0) {
    uintToString(LargestUInt(-value), current);
    *--current = '-';
  } else {
    uintToString(LargestUInt(value), current);
  }
  assert(current >= buffer);
  return current;
}

String valueToString(LargestUInt value) {
  UIntToStringBuffer buffer;
  char* current = buffer + sizeof(buffer);
  uintToString(value, current);
  assert(current >= buffer);
  return current;
}

#if defined(JSON_HAS_INT64)

String valueToString(Int value) { return valueToString(LargestInt(value)); }

String valueToString(UInt value) { return valueToString(LargestUInt(value)); }

#endif // # if defined(JSON_HAS_INT64)

namespace {
String valueToString(double value, bool useSpecialFloats,
                     unsigned int precision, PrecisionType precisionType) {
  // Print into the buffer. We need not request the alternative representation
  // that always has a decimal point because JSON doesn't distinguish the
  // concepts of reals and integers.
  if (!isfinite(value)) {
    static const char* const reps[2][3] = {{"NaN", "-Infinity", "Infinity"},
                                           {"null", "-1e+9999", "1e+9999"}};
    return reps[useSpecialFloats ? 0 : 1]
               [isnan(value) ? 0 : (value < 0) ? 1 : 2];
  }

  String buffer(size_t(36), '\0');
  while (true) {
    int len = jsoncpp_snprintf(
        &*buffer.begin(), buffer.size(),
        (precisionType == PrecisionType::significantDigits) ? "%.*g" : "%.*f",
        precision, value);
    assert(len >= 0);
    auto wouldPrint = static_cast<size_t>(len);
    if (wouldPrint >= buffer.size()) {
      buffer.resize(wouldPrint + 1);
      continue;
    }
    buffer.resize(wouldPrint);
    break;
  }

  buffer.erase(fixNumericLocale(buffer.begin(), buffer.end()), buffer.end());

  // strip the zero padding from the right
  if (precisionType == PrecisionType::decimalPlaces) {
    buffer.erase(fixZerosInTheEnd(buffer.begin(), buffer.end()), buffer.end());
  }

  // try to ensure we preserve the fact that this was given to us as a double on
  // input
  if (buffer.find('.') == buffer.npos && buffer.find('e') == buffer.npos) {
    buffer += ".0";
  }
  return buffer;
}
} // namespace

String valueToString(double value, unsigned int precision,
                     PrecisionType precisionType) {
  return valueToString(value, false, precision, precisionType);
}

String valueToString(bool value) { return value ? "true" : "false"; }

static bool isAnyCharRequiredQuoting(char const* s, size_t n) {
  assert(s || !n);

  return std::any_of(s, s + n, [](unsigned char c) {
    return c == '\\' || c == '"' || !std::isprint(c);
  });
}

static unsigned int utf8ToCodepoint(const char*& s, const char* e) {
  const unsigned int REPLACEMENT_CHARACTER = 0xFFFD;

  unsigned int firstByte = static_cast<unsigned char>(*s);

  if (firstByte < 0x80)
    return firstByte;

  if (firstByte < 0xE0) {
    if (e - s < 2)
      return REPLACEMENT_CHARACTER;

    unsigned int calculated =
        ((firstByte & 0x1F) << 6) | (static_cast<unsigned int>(s[1]) & 0x3F);
    s += 1;
    // oversized encoded characters are invalid
    return calculated < 0x80 ? REPLACEMENT_CHARACTER : calculated;
  }

  if (firstByte < 0xF0) {
    if (e - s < 3)
      return REPLACEMENT_CHARACTER;

    unsigned int calculated = ((firstByte & 0x0F) << 12) |
                              ((static_cast<unsigned int>(s[1]) & 0x3F) << 6) |
                              (static_cast<unsigned int>(s[2]) & 0x3F);
    s += 2;
    // surrogates aren't valid codepoints itself
    // shouldn't be UTF-8 encoded
    if (calculated >= 0xD800 && calculated <= 0xDFFF)
      return REPLACEMENT_CHARACTER;
    // oversized encoded characters are invalid
    return calculated < 0x800 ? REPLACEMENT_CHARACTER : calculated;
  }

  if (firstByte < 0xF8) {
    if (e - s < 4)
      return REPLACEMENT_CHARACTER;

    unsigned int calculated = ((firstByte & 0x07) << 18) |
                              ((static_cast<unsigned int>(s[1]) & 0x3F) << 12) |
                              ((static_cast<unsigned int>(s[2]) & 0x3F) << 6) |
                              (static_cast<unsigned int>(s[3]) & 0x3F);
    s += 3;
    // oversized encoded characters are invalid
    return calculated < 0x10000 ? REPLACEMENT_CHARACTER : calculated;
  }

  return REPLACEMENT_CHARACTER;
}

static const char hex2[] = "000102030405060708090a0b0c0d0e0f"
                           "101112131415161718191a1b1c1d1e1f"
                           "202122232425262728292a2b2c2d2e2f"
                           "303132333435363738393a3b3c3d3e3f"
                           "404142434445464748494a4b4c4d4e4f"
                           "505152535455565758595a5b5c5d5e5f"
                           "606162636465666768696a6b6c6d6e6f"
                           "707172737475767778797a7b7c7d7e7f"
                           "808182838485868788898a8b8c8d8e8f"
                           "909192939495969798999a9b9c9d9e9f"
                           "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
                           "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
                           "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
                           "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
                           "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
                           "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

static String toHex16Bit(unsigned int x) {
  const unsigned int hi = (x >> 8) & 0xff;
  const unsigned int lo = x & 0xff;
  String result(4, ' ');
  result[0] = hex2[2 * hi];
  result[1] = hex2[2 * hi + 1];
  result[2] = hex2[2 * lo];
  result[3] = hex2[2 * lo + 1];
  return result;
}

static void appendRaw(String& result, unsigned ch) {
  result += static_cast<char>(ch);
}

static void appendHex(String& result, unsigned ch) {
  result.append("\\u").append(toHex16Bit(ch));
}

static String valueToQuotedStringN(const char* value, unsigned length,
                                   bool emitUTF8 = false) {
  if (value == nullptr)
    return "";

  if (!isAnyCharRequiredQuoting(value, length))
    return String("\"") + value + "\"";
  // We have to walk value and escape any special characters.
  // Appending to String is not efficient, but this should be rare.
  // (Note: forward slashes are *not* rare, but I am not escaping them.)
  String::size_type maxsize = length * 2 + 3; // allescaped+quotes+NULL
  String result;
  result.reserve(maxsize); // to avoid lots of mallocs
  result += "\"";
  char const* end = value + length;
  for (const char* c = value; c != end; ++c) {
    switch (*c) {
    case '\"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    // case '/':
    // Even though \/ is considered a legal escape in JSON, a bare
    // slash is also legal, so I see no reason to escape it.
    // (I hope I am not misunderstanding something.)
    // blep notes: actually escaping \/ may be useful in javascript to avoid </
    // sequence.
    // Should add a flag to allow this compatibility mode and prevent this
    // sequence from occurring.
    default: {
      if (emitUTF8) {
        unsigned codepoint = static_cast<unsigned char>(*c);
        if (codepoint < 0x20) {
          appendHex(result, codepoint);
        } else {
          appendRaw(result, codepoint);
        }
      } else {
        unsigned codepoint = utf8ToCodepoint(c, end); // modifies `c`
        if (codepoint < 0x20) {
          appendHex(result, codepoint);
        } else if (codepoint < 0x80) {
          appendRaw(result, codepoint);
        } else if (codepoint < 0x10000) {
          // Basic Multilingual Plane
          appendHex(result, codepoint);
        } else {
          // Extended Unicode. Encode 20 bits as a surrogate pair.
          codepoint -= 0x10000;
          appendHex(result, 0xd800 + ((codepoint >> 10) & 0x3ff));
          appendHex(result, 0xdc00 + (codepoint & 0x3ff));
        }
      }
    } break;
    }
  }
  result += "\"";
  return result;
}

String valueToQuotedString(const char* value) {
  return valueToQuotedStringN(value, static_cast<unsigned int>(strlen(value)));
}

// Class Writer
// //////////////////////////////////////////////////////////////////
Writer::~Writer() = default;

// Class FastWriter
// //////////////////////////////////////////////////////////////////

FastWriter::FastWriter()

    = default;

void FastWriter::enableYAMLCompatibility() { yamlCompatibilityEnabled_ = true; }

void FastWriter::dropNullPlaceholders() { dropNullPlaceholders_ = true; }

void FastWriter::omitEndingLineFeed() { omitEndingLineFeed_ = true; }

String FastWriter::write(const Value& root) {
  document_.clear();
  writeValue(root);
  if (!omitEndingLineFeed_)
    document_ += '\n';
  return document_;
}

void FastWriter::writeValue(const Value& value) {
  switch (value.type()) {
  case nullValue:
    if (!dropNullPlaceholders_)
      document_ += "null";
    break;
  case intValue:
    document_ += valueToString(value.asLargestInt());
    break;
  case uintValue:
    document_ += valueToString(value.asLargestUInt());
    break;
  case realValue:
    document_ += valueToString(value.asDouble());
    break;
  case stringValue: {
    // Is NULL possible for value.string_? No.
    char const* str;
    char const* end;
    bool ok = value.getString(&str, &end);
    if (ok)
      document_ += valueToQuotedStringN(str, static_cast<unsigned>(end - str));
    break;
  }
  case booleanValue:
    document_ += valueToString(value.asBool());
    break;
  case arrayValue: {
    document_ += '[';
    ArrayIndex size = value.size();
    for (ArrayIndex index = 0; index < size; ++index) {
      if (index > 0)
        document_ += ',';
      writeValue(value[index]);
    }
    document_ += ']';
  } break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    document_ += '{';
    for (auto it = members.begin(); it != members.end(); ++it) {
      const String& name = *it;
      if (it != members.begin())
        document_ += ',';
      document_ += valueToQuotedStringN(name.data(),
                                        static_cast<unsigned>(name.length()));
      document_ += yamlCompatibilityEnabled_ ? ": " : ":";
      writeValue(value[name]);
    }
    document_ += '}';
  } break;
  }
}

// Class StyledWriter
// //////////////////////////////////////////////////////////////////

StyledWriter::StyledWriter() = default;

String StyledWriter::write(const Value& root) {
  document_.clear();
  addChildValues_ = false;
  indentString_.clear();
  writeCommentBeforeValue(root);
  writeValue(root);
  writeCommentAfterValueOnSameLine(root);
  document_ += '\n';
  return document_;
}

void StyledWriter::writeValue(const Value& value) {
  switch (value.type()) {
  case nullValue:
    pushValue("null");
    break;
  case intValue:
    pushValue(valueToString(value.asLargestInt()));
    break;
  case uintValue:
    pushValue(valueToString(value.asLargestUInt()));
    break;
  case realValue:
    pushValue(valueToString(value.asDouble()));
    break;
  case stringValue: {
    // Is NULL possible for value.string_? No.
    char const* str;
    char const* end;
    bool ok = value.getString(&str, &end);
    if (ok)
      pushValue(valueToQuotedStringN(str, static_cast<unsigned>(end - str)));
    else
      pushValue("");
    break;
  }
  case booleanValue:
    pushValue(valueToString(value.asBool()));
    break;
  case arrayValue:
    writeArrayValue(value);
    break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    if (members.empty())
      pushValue("{}");
    else {
      writeWithIndent("{");
      indent();
      auto it = members.begin();
      for (;;) {
        const String& name = *it;
        const Value& childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeWithIndent(valueToQuotedString(name.c_str()));
        document_ += " : ";
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        document_ += ',';
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("}");
    }
  } break;
  }
}

void StyledWriter::writeArrayValue(const Value& value) {
  unsigned size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isArrayMultiLine = isMultilineArray(value);
    if (isArrayMultiLine) {
      writeWithIndent("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      unsigned index = 0;
      for (;;) {
        const Value& childValue = value[index];
        writeCommentBeforeValue(childValue);
        if (hasChildValue)
          writeWithIndent(childValues_[index]);
        else {
          writeIndent();
          writeValue(childValue);
        }
        if (++index == size) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        document_ += ',';
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("]");
    } else // output on a single line
    {
      assert(childValues_.size() == size);
      document_ += "[ ";
      for (unsigned index = 0; index < size; ++index) {
        if (index > 0)
          document_ += ", ";
        document_ += childValues_[index];
      }
      document_ += " ]";
    }
  }
}

bool StyledWriter::isMultilineArray(const Value& value) {
  ArrayIndex const size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (ArrayIndex index = 0; index < size && !isMultiLine; ++index) {
    const Value& childValue = value[index];
    isMultiLine = ((childValue.isArray() || childValue.isObject()) &&
                   !childValue.empty());
  }
  if (!isMultiLine) // check if line length > max line length
  {
    childValues_.reserve(size);
    addChildValues_ = true;
    ArrayIndex lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
    for (ArrayIndex index = 0; index < size; ++index) {
      if (hasCommentForValue(value[index])) {
        isMultiLine = true;
      }
      writeValue(value[index]);
      lineLength += static_cast<ArrayIndex>(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

void StyledWriter::pushValue(const String& value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    document_ += value;
}

void StyledWriter::writeIndent() {
  if (!document_.empty()) {
    char last = document_[document_.length() - 1];
    if (last == ' ') // already indented
      return;
    if (last != '\n') // Comments may add new-line
      document_ += '\n';
  }
  document_ += indentString_;
}

void StyledWriter::writeWithIndent(const String& value) {
  writeIndent();
  document_ += value;
}

void StyledWriter::indent() { indentString_ += String(indentSize_, ' '); }

void StyledWriter::unindent() {
  assert(indentString_.size() >= indentSize_);
  indentString_.resize(indentString_.size() - indentSize_);
}

void StyledWriter::writeCommentBeforeValue(const Value& root) {
  if (!root.hasComment(commentBefore))
    return;

  document_ += '\n';
  writeIndent();
  const String& comment = root.getComment(commentBefore);
  String::const_iterator iter = comment.begin();
  while (iter != comment.end()) {
    document_ += *iter;
    if (*iter == '\n' && ((iter + 1) != comment.end() && *(iter + 1) == '/'))
      writeIndent();
    ++iter;
  }

  // Comments are stripped of trailing newlines, so add one here
  document_ += '\n';
}

void StyledWriter::writeCommentAfterValueOnSameLine(const Value& root) {
  if (root.hasComment(commentAfterOnSameLine))
    document_ += " " + root.getComment(commentAfterOnSameLine);

  if (root.hasComment(commentAfter)) {
    document_ += '\n';
    document_ += root.getComment(commentAfter);
    document_ += '\n';
  }
}

bool StyledWriter::hasCommentForValue(const Value& value) {
  return value.hasComment(commentBefore) ||
         value.hasComment(commentAfterOnSameLine) ||
         value.hasComment(commentAfter);
}

// Class StyledStreamWriter
// //////////////////////////////////////////////////////////////////

StyledStreamWriter::StyledStreamWriter(String indentation)
    : document_(nullptr), indentation_(std::move(indentation)),
      addChildValues_(), indented_(false) {}

void StyledStreamWriter::write(OStream& out, const Value& root) {
  document_ = &out;
  addChildValues_ = false;
  indentString_.clear();
  indented_ = true;
  writeCommentBeforeValue(root);
  if (!indented_)
    writeIndent();
  indented_ = true;
  writeValue(root);
  writeCommentAfterValueOnSameLine(root);
  *document_ << "\n";
  document_ = nullptr; // Forget the stream, for safety.
}

void StyledStreamWriter::writeValue(const Value& value) {
  switch (value.type()) {
  case nullValue:
    pushValue("null");
    break;
  case intValue:
    pushValue(valueToString(value.asLargestInt()));
    break;
  case uintValue:
    pushValue(valueToString(value.asLargestUInt()));
    break;
  case realValue:
    pushValue(valueToString(value.asDouble()));
    break;
  case stringValue: {
    // Is NULL possible for value.string_? No.
    char const* str;
    char const* end;
    bool ok = value.getString(&str, &end);
    if (ok)
      pushValue(valueToQuotedStringN(str, static_cast<unsigned>(end - str)));
    else
      pushValue("");
    break;
  }
  case booleanValue:
    pushValue(valueToString(value.asBool()));
    break;
  case arrayValue:
    writeArrayValue(value);
    break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    if (members.empty())
      pushValue("{}");
    else {
      writeWithIndent("{");
      indent();
      auto it = members.begin();
      for (;;) {
        const String& name = *it;
        const Value& childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeWithIndent(valueToQuotedString(name.c_str()));
        *document_ << " : ";
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *document_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("}");
    }
  } break;
  }
}

void StyledStreamWriter::writeArrayValue(const Value& value) {
  unsigned size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isArrayMultiLine = isMultilineArray(value);
    if (isArrayMultiLine) {
      writeWithIndent("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      unsigned index = 0;
      for (;;) {
        const Value& childValue = value[index];
        writeCommentBeforeValue(childValue);
        if (hasChildValue)
          writeWithIndent(childValues_[index]);
        else {
          if (!indented_)
            writeIndent();
          indented_ = true;
          writeValue(childValue);
          indented_ = false;
        }
        if (++index == size) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *document_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("]");
    } else // output on a single line
    {
      assert(childValues_.size() == size);
      *document_ << "[ ";
      for (unsigned index = 0; index < size; ++index) {
        if (index > 0)
          *document_ << ", ";
        *document_ << childValues_[index];
      }
      *document_ << " ]";
    }
  }
}

bool StyledStreamWriter::isMultilineArray(const Value& value) {
  ArrayIndex const size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (ArrayIndex index = 0; index < size && !isMultiLine; ++index) {
    const Value& childValue = value[index];
    isMultiLine = ((childValue.isArray() || childValue.isObject()) &&
                   !childValue.empty());
  }
  if (!isMultiLine) // check if line length > max line length
  {
    childValues_.reserve(size);
    addChildValues_ = true;
    ArrayIndex lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
    for (ArrayIndex index = 0; index < size; ++index) {
      if (hasCommentForValue(value[index])) {
        isMultiLine = true;
      }
      writeValue(value[index]);
      lineLength += static_cast<ArrayIndex>(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

void StyledStreamWriter::pushValue(const String& value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    *document_ << value;
}

void StyledStreamWriter::writeIndent() {
  // blep intended this to look at the so-far-written string
  // to determine whether we are already indented, but
  // with a stream we cannot do that. So we rely on some saved state.
  // The caller checks indented_.
  *document_ << '\n' << indentString_;
}

void StyledStreamWriter::writeWithIndent(const String& value) {
  if (!indented_)
    writeIndent();
  *document_ << value;
  indented_ = false;
}

void StyledStreamWriter::indent() { indentString_ += indentation_; }

void StyledStreamWriter::unindent() {
  assert(indentString_.size() >= indentation_.size());
  indentString_.resize(indentString_.size() - indentation_.size());
}

void StyledStreamWriter::writeCommentBeforeValue(const Value& root) {
  if (!root.hasComment(commentBefore))
    return;

  if (!indented_)
    writeIndent();
  const String& comment = root.getComment(commentBefore);
  String::const_iterator iter = comment.begin();
  while (iter != comment.end()) {
    *document_ << *iter;
    if (*iter == '\n' && ((iter + 1) != comment.end() && *(iter + 1) == '/'))
      // writeIndent();  // would include newline
      *document_ << indentString_;
    ++iter;
  }
  indented_ = false;
}

void StyledStreamWriter::writeCommentAfterValueOnSameLine(const Value& root) {
  if (root.hasComment(commentAfterOnSameLine))
    *document_ << ' ' << root.getComment(commentAfterOnSameLine);

  if (root.hasComment(commentAfter)) {
    writeIndent();
    *document_ << root.getComment(commentAfter);
  }
  indented_ = false;
}

bool StyledStreamWriter::hasCommentForValue(const Value& value) {
  return value.hasComment(commentBefore) ||
         value.hasComment(commentAfterOnSameLine) ||
         value.hasComment(commentAfter);
}

//////////////////////////
// BuiltStyledStreamWriter

/// Scoped enums are not available until C++11.
struct CommentStyle {
  /// Decide whether to write comments.
  enum Enum {
    None, ///< Drop all comments.
    Most, ///< Recover odd behavior of previous versions (not implemented yet).
    All   ///< Keep all comments.
  };
};

struct BuiltStyledStreamWriter : public StreamWriter {
  BuiltStyledStreamWriter(String indentation, CommentStyle::Enum cs,
                          String colonSymbol, String nullSymbol,
                          String endingLineFeedSymbol, bool useSpecialFloats,
                          bool emitUTF8, unsigned int precision,
                          PrecisionType precisionType);
  int write(Value const& root, OStream* sout) override;

private:
  void writeValue(Value const& value);
  void writeArrayValue(Value const& value);
  bool isMultilineArray(Value const& value);
  void pushValue(String const& value);
  void writeIndent();
  void writeWithIndent(String const& value);
  void indent();
  void unindent();
  void writeCommentBeforeValue(Value const& root);
  void writeCommentAfterValueOnSameLine(Value const& root);
  static bool hasCommentForValue(const Value& value);

  using ChildValues = std::vector<String>;

  ChildValues childValues_;
  String indentString_;
  unsigned int rightMargin_;
  String indentation_;
  CommentStyle::Enum cs_;
  String colonSymbol_;
  String nullSymbol_;
  String endingLineFeedSymbol_;
  bool addChildValues_ : 1;
  bool indented_ : 1;
  bool useSpecialFloats_ : 1;
  bool emitUTF8_ : 1;
  unsigned int precision_;
  PrecisionType precisionType_;
};
BuiltStyledStreamWriter::BuiltStyledStreamWriter(
    String indentation, CommentStyle::Enum cs, String colonSymbol,
    String nullSymbol, String endingLineFeedSymbol, bool useSpecialFloats,
    bool emitUTF8, unsigned int precision, PrecisionType precisionType)
    : rightMargin_(74), indentation_(std::move(indentation)), cs_(cs),
      colonSymbol_(std::move(colonSymbol)), nullSymbol_(std::move(nullSymbol)),
      endingLineFeedSymbol_(std::move(endingLineFeedSymbol)),
      addChildValues_(false), indented_(false),
      useSpecialFloats_(useSpecialFloats), emitUTF8_(emitUTF8),
      precision_(precision), precisionType_(precisionType) {}
int BuiltStyledStreamWriter::write(Value const& root, OStream* sout) {
  sout_ = sout;
  addChildValues_ = false;
  indented_ = true;
  indentString_.clear();
  writeCommentBeforeValue(root);
  if (!indented_)
    writeIndent();
  indented_ = true;
  writeValue(root);
  writeCommentAfterValueOnSameLine(root);
  *sout_ << endingLineFeedSymbol_;
  sout_ = nullptr;
  return 0;
}
void BuiltStyledStreamWriter::writeValue(Value const& value) {
  switch (value.type()) {
  case nullValue:
    pushValue(nullSymbol_);
    break;
  case intValue:
    pushValue(valueToString(value.asLargestInt()));
    break;
  case uintValue:
    pushValue(valueToString(value.asLargestUInt()));
    break;
  case realValue:
    pushValue(valueToString(value.asDouble(), useSpecialFloats_, precision_,
                            precisionType_));
    break;
  case stringValue: {
    // Is NULL is possible for value.string_? No.
    char const* str;
    char const* end;
    bool ok = value.getString(&str, &end);
    if (ok)
      pushValue(valueToQuotedStringN(str, static_cast<unsigned>(end - str),
                                     emitUTF8_));
    else
      pushValue("");
    break;
  }
  case booleanValue:
    pushValue(valueToString(value.asBool()));
    break;
  case arrayValue:
    writeArrayValue(value);
    break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    if (members.empty())
      pushValue("{}");
    else {
      writeWithIndent("{");
      indent();
      auto it = members.begin();
      for (;;) {
        String const& name = *it;
        Value const& childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeWithIndent(valueToQuotedStringN(
            name.data(), static_cast<unsigned>(name.length()), emitUTF8_));
        *sout_ << colonSymbol_;
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *sout_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("}");
    }
  } break;
  }
}

void BuiltStyledStreamWriter::writeArrayValue(Value const& value) {
  unsigned size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isMultiLine = (cs_ == CommentStyle::All) || isMultilineArray(value);
    if (isMultiLine) {
      writeWithIndent("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      unsigned index = 0;
      for (;;) {
        Value const& childValue = value[index];
        writeCommentBeforeValue(childValue);
        if (hasChildValue)
          writeWithIndent(childValues_[index]);
        else {
          if (!indented_)
            writeIndent();
          indented_ = true;
          writeValue(childValue);
          indented_ = false;
        }
        if (++index == size) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *sout_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("]");
    } else // output on a single line
    {
      assert(childValues_.size() == size);
      *sout_ << "[";
      if (!indentation_.empty())
        *sout_ << " ";
      for (unsigned index = 0; index < size; ++index) {
        if (index > 0)
          *sout_ << ((!indentation_.empty()) ? ", " : ",");
        *sout_ << childValues_[index];
      }
      if (!indentation_.empty())
        *sout_ << " ";
      *sout_ << "]";
    }
  }
}

bool BuiltStyledStreamWriter::isMultilineArray(Value const& value) {
  ArrayIndex const size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (ArrayIndex index = 0; index < size && !isMultiLine; ++index) {
    Value const& childValue = value[index];
    isMultiLine = ((childValue.isArray() || childValue.isObject()) &&
                   !childValue.empty());
  }
  if (!isMultiLine) // check if line length > max line length
  {
    childValues_.reserve(size);
    addChildValues_ = true;
    ArrayIndex lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
    for (ArrayIndex index = 0; index < size; ++index) {
      if (hasCommentForValue(value[index])) {
        isMultiLine = true;
      }
      writeValue(value[index]);
      lineLength += static_cast<ArrayIndex>(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

void BuiltStyledStreamWriter::pushValue(String const& value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    *sout_ << value;
}

void BuiltStyledStreamWriter::writeIndent() {
  // blep intended this to look at the so-far-written string
  // to determine whether we are already indented, but
  // with a stream we cannot do that. So we rely on some saved state.
  // The caller checks indented_.

  if (!indentation_.empty()) {
    // In this case, drop newlines too.
    *sout_ << '\n' << indentString_;
  }
}

void BuiltStyledStreamWriter::writeWithIndent(String const& value) {
  if (!indented_)
    writeIndent();
  *sout_ << value;
  indented_ = false;
}

void BuiltStyledStreamWriter::indent() { indentString_ += indentation_; }

void BuiltStyledStreamWriter::unindent() {
  assert(indentString_.size() >= indentation_.size());
  indentString_.resize(indentString_.size() - indentation_.size());
}

void BuiltStyledStreamWriter::writeCommentBeforeValue(Value const& root) {
  if (cs_ == CommentStyle::None)
    return;
  if (!root.hasComment(commentBefore))
    return;

  if (!indented_)
    writeIndent();
  const String& comment = root.getComment(commentBefore);
  String::const_iterator iter = comment.begin();
  while (iter != comment.end()) {
    *sout_ << *iter;
    if (*iter == '\n' && ((iter + 1) != comment.end() && *(iter + 1) == '/'))
      // writeIndent();  // would write extra newline
      *sout_ << indentString_;
    ++iter;
  }
  indented_ = false;
}

void BuiltStyledStreamWriter::writeCommentAfterValueOnSameLine(
    Value const& root) {
  if (cs_ == CommentStyle::None)
    return;
  if (root.hasComment(commentAfterOnSameLine))
    *sout_ << " " + root.getComment(commentAfterOnSameLine);

  if (root.hasComment(commentAfter)) {
    writeIndent();
    *sout_ << root.getComment(commentAfter);
  }
}

// static
bool BuiltStyledStreamWriter::hasCommentForValue(const Value& value) {
  return value.hasComment(commentBefore) ||
         value.hasComment(commentAfterOnSameLine) ||
         value.hasComment(commentAfter);
}

///////////////
// StreamWriter

StreamWriter::StreamWriter() : sout_(nullptr) {}
StreamWriter::~StreamWriter() = default;
StreamWriter::Factory::~Factory() = default;
StreamWriterBuilder::StreamWriterBuilder() { setDefaults(&settings_); }
StreamWriterBuilder::~StreamWriterBuilder() = default;
StreamWriter* StreamWriterBuilder::newStreamWriter() const {
  const String indentation = settings_["indentation"].asString();
  const String cs_str = settings_["commentStyle"].asString();
  const String pt_str = settings_["precisionType"].asString();
  const bool eyc = settings_["enableYAMLCompatibility"].asBool();
  const bool dnp = settings_["dropNullPlaceholders"].asBool();
  const bool usf = settings_["useSpecialFloats"].asBool();
  const bool emitUTF8 = settings_["emitUTF8"].asBool();
  unsigned int pre = settings_["precision"].asUInt();
  CommentStyle::Enum cs = CommentStyle::All;
  if (cs_str == "All") {
    cs = CommentStyle::All;
  } else if (cs_str == "None") {
    cs = CommentStyle::None;
  } else {
    throwRuntimeError("commentStyle must be 'All' or 'None'");
  }
  PrecisionType precisionType(significantDigits);
  if (pt_str == "significant") {
    precisionType = PrecisionType::significantDigits;
  } else if (pt_str == "decimal") {
    precisionType = PrecisionType::decimalPlaces;
  } else {
    throwRuntimeError("precisionType must be 'significant' or 'decimal'");
  }
  String colonSymbol = " : ";
  if (eyc) {
    colonSymbol = ": ";
  } else if (indentation.empty()) {
    colonSymbol = ":";
  }
  String nullSymbol = "null";
  if (dnp) {
    nullSymbol.clear();
  }
  if (pre > 17)
    pre = 17;
  String endingLineFeedSymbol;
  return new BuiltStyledStreamWriter(indentation, cs, colonSymbol, nullSymbol,
                                     endingLineFeedSymbol, usf, emitUTF8, pre,
                                     precisionType);
}

bool StreamWriterBuilder::validate(Json::Value* invalid) const {
  static const auto& valid_keys = *new std::set<String>{
      "indentation",
      "commentStyle",
      "enableYAMLCompatibility",
      "dropNullPlaceholders",
      "useSpecialFloats",
      "emitUTF8",
      "precision",
      "precisionType",
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

Value& StreamWriterBuilder::operator[](const String& key) {
  return settings_[key];
}
// static
void StreamWriterBuilder::setDefaults(Json::Value* settings) {
  //! [StreamWriterBuilderDefaults]
  (*settings)["commentStyle"] = "All";
  (*settings)["indentation"] = "\t";
  (*settings)["enableYAMLCompatibility"] = false;
  (*settings)["dropNullPlaceholders"] = false;
  (*settings)["useSpecialFloats"] = false;
  (*settings)["emitUTF8"] = false;
  (*settings)["precision"] = 17;
  (*settings)["precisionType"] = "significant";
  //! [StreamWriterBuilderDefaults]
}

String writeString(StreamWriter::Factory const& factory, Value const& root) {
  OStringStream sout;
  StreamWriterPtr const writer(factory.newStreamWriter());
  writer->write(root, &sout);
  return sout.str();
}

OStream& operator<<(OStream& sout, Value const& root) {
  StreamWriterBuilder builder;
  StreamWriterPtr const writer(builder.newStreamWriter());
  writer->write(root, &sout);
  return sout;
}

} // namespace Json

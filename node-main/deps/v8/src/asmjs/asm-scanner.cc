// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/asmjs/asm-scanner.h"

#include <cinttypes>

#include "src/base/iterator.h"
#include "src/flags/flags.h"
#include "src/numbers/conversions.h"
#include "src/parsing/scanner.h"
#include "src/strings/char-predicates-inl.h"

namespace v8 {
namespace internal {

namespace {
// Cap number of identifiers to ensure we can assign both global and
// local ones a token id in the range of an int32_t.
static const int kMaxIdentifierCount = 0xF000000;
}  // namespace

AsmJsScanner::AsmJsScanner(Utf16CharacterStream* stream)
    : token_(kUninitialized),
      preceding_token_(kUninitialized),
      next_token_(kUninitialized),
      position_(0),
      preceding_position_(0),
      next_position_(0),
      rewind_(false),
      in_local_scope_(false),
      global_count_(0),
      double_value_(0.0),
      unsigned_value_(0),
      preceded_by_newline_(false) {
#define V(name, _junk1, _junk2, _junk3) property_names_[#name] = kToken_##name;
  STDLIB_MATH_FUNCTION_LIST(V)
  STDLIB_ARRAY_TYPE_LIST(V)
#undef V
#define V(name, _junk1) property_names_[#name] = kToken_##name;
  STDLIB_MATH_VALUE_LIST(V)
#undef V
#define V(name) property_names_[#name] = kToken_##name;
  STDLIB_OTHER_LIST(V)
#undef V
#define V(name) global_names_[#name] = kToken_##name;
  KEYWORD_NAME_LIST(V)
#undef V

  // Fully read the stream to protect against concurrent modification of on-heap
  // data (e.g. strings).
  input_offset_ = stream->pos();
  static constexpr auto kEndOfInputU = static_cast<base::uc32>(kEndOfInput);
  for (base::uc32 ch; (ch = stream->Advance()) != kEndOfInputU;) {
    input_.push_back(ch);
  }

  Next();
}

void AsmJsScanner::Next() {
  if (rewind_) {
    preceding_token_ = token_;
    preceding_position_ = position_;
    token_ = next_token_;
    position_ = next_position_;
    next_token_ = kUninitialized;
    next_position_ = 0;
    rewind_ = false;
    return;
  }

  if (token_ == kEndOfInput || token_ == kParseError) return;

#if DEBUG
  if (v8_flags.trace_asm_scanner) {
    if (Token() == kDouble) {
      PrintF("%lf ", AsDouble());
    } else if (Token() == kUnsigned) {
      PrintF("%" PRIu32 " ", AsUnsigned());
    } else {
      std::string name = Name(Token());
      PrintF("%s ", name.c_str());
    }
  }
#endif

  preceded_by_newline_ = false;
  preceding_token_ = token_;
  preceding_position_ = position_;

  while (true) {
    position_ = input_offset_ + input_position_;
    if (!HasMoreChars()) {
      token_ = kEndOfInput;
      return;
    }

    base::uc32 ch = NextChar();
    switch (ch) {
      case ' ':
      case '\t':
      case '\r':
        // Ignore whitespace.
        break;

      case '\n':
        // Track when we've passed a newline for optional semicolon support,
        // but keep scanning.
        preceded_by_newline_ = true;
        break;

      case '\'':
      case '"':
        ConsumeString(ch);
        return;

      case '/':
        if (Consume('/')) {
          ConsumeCPPComment();
        } else if (Consume('*')) {
          if (!ConsumeCComment()) {
            token_ = kParseError;
            return;
          }
        } else {
          token_ = '/';
          return;
        }
        // Breaks out of switch, but loops again (i.e. the case when we parsed
        // a comment, but need to continue to look for the next token).
        break;

      case '<':
      case '>':
      case '=':
      case '!':
        ConsumeCompareOrShift(ch);
        return;

#define V(single_char_token) case single_char_token:
        SIMPLE_SINGLE_TOKEN_LIST(V)
#undef V
        // Use fixed token IDs for ASCII.
        token_ = ch;
        return;

      default:
        if (IsIdentifierStart(ch)) {
          ConsumeIdentifier(ch);
        } else if (IsNumberStart(ch)) {
          ConsumeNumber(ch);
        } else {
          // TODO(bradnelson): Support unicode (probably via UnicodeCache).
          token_ = kParseError;
        }
        return;
    }
  }
}

void AsmJsScanner::Rewind() {
  DCHECK_NE(kUninitialized, preceding_token_);
  // TODO(bradnelson): Currently rewinding needs to leave in place the
  // preceding newline state (in case a |0 ends a line).
  // This is weird and stateful, fix me.
  DCHECK(!rewind_);
  next_token_ = token_;
  next_position_ = position_;
  token_ = preceding_token_;
  position_ = preceding_position_;
  preceding_token_ = kUninitialized;
  preceding_position_ = 0;
  rewind_ = true;
  identifier_string_.clear();
}

void AsmJsScanner::ResetLocals() { local_names_.clear(); }

#if DEBUG
// Only used for debugging.
std::string AsmJsScanner::Name(token_t token) const {
  if (token >= 32 && token < 127) {
    return std::string(1, static_cast<char>(token));
  }
  for (auto& i : local_names_) {
    if (i.second == token) {
      return i.first;
    }
  }
  for (auto& i : global_names_) {
    if (i.second == token) {
      return i.first;
    }
  }
  for (auto& i : property_names_) {
    if (i.second == token) {
      return i.first;
    }
  }
  switch (token) {
#define V(rawname, name) \
  case kToken_##name:    \
    return rawname;
    LONG_SYMBOL_NAME_LIST(V)
#undef V
#define V(name, value, string_name) \
  case name:                        \
    return string_name;
    SPECIAL_TOKEN_LIST(V)
    default:
      break;
#undef V
  }
  UNREACHABLE();
}
#endif

void AsmJsScanner::Seek(size_t pos) {
  DCHECK_LE(input_offset_, pos);
  size_t pos_within_input = pos - input_offset_;
  DCHECK_LE(pos_within_input, input_.size());
  input_position_ = pos_within_input;
  preceding_token_ = kUninitialized;
  token_ = kUninitialized;
  next_token_ = kUninitialized;
  preceding_position_ = 0;
  position_ = 0;
  next_position_ = 0;
  rewind_ = false;
  Next();
}

void AsmJsScanner::ConsumeIdentifier(base::uc32 ch) {
  // Consume characters while still part of the identifier.
  identifier_string_.assign(1, ch);
  while (HasMoreChars() && IsIdentifierPart(PeekChar())) {
    identifier_string_ += NextChar();
  }

  // Decode what the identifier means.
  if (preceding_token_ == '.') {
    auto i = property_names_.find(identifier_string_);
    if (i != property_names_.end()) {
      token_ = i->second;
      return;
    }
  } else {
    {
      auto i = local_names_.find(identifier_string_);
      if (i != local_names_.end()) {
        token_ = i->second;
        return;
      }
    }
    if (!in_local_scope_) {
      auto i = global_names_.find(identifier_string_);
      if (i != global_names_.end()) {
        token_ = i->second;
        return;
      }
    }
  }
  if (preceding_token_ == '.') {
    CHECK_LT(global_count_, kMaxIdentifierCount);
    token_ = kGlobalsStart + global_count_++;
    property_names_[identifier_string_] = token_;
  } else if (in_local_scope_) {
    CHECK_LT(local_names_.size(), kMaxIdentifierCount);
    token_ = kLocalsStart - static_cast<token_t>(local_names_.size());
    local_names_[identifier_string_] = token_;
  } else {
    CHECK_LT(global_count_, kMaxIdentifierCount);
    token_ = kGlobalsStart + global_count_++;
    global_names_[identifier_string_] = token_;
  }
}

namespace {
bool IsValidImplicitOctal(std::string_view number) {
  DCHECK_EQ(number[0], '0');
  return std::all_of(number.begin() + 1, number.end(), IsOctalDigit);
}
}  // namespace

void AsmJsScanner::ConsumeNumber(base::uc32 ch) {
  std::string number;
  number.assign(1, ch);
  bool has_dot = ch == '.';
  bool has_prefix = false;
  while (HasMoreChars()) {
    ch = PeekChar();
    if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
        (ch >= 'A' && ch <= 'F') || ch == '.' || ch == 'b' || ch == 'o' ||
        ch == 'x' ||
        ((ch == '-' || ch == '+') && !has_prefix &&
         (number[number.size() - 1] == 'e' ||
          number[number.size() - 1] == 'E'))) {
      // TODO(bradnelson): Test weird cases ending in -.
      if (ch == '.') {
        has_dot = true;
      }
      if (ch == 'b' || ch == 'o' || ch == 'x') {
        has_prefix = true;
      }
      number.push_back(ch);
      Advance();
    } else {
      break;
    }
  }
  // Special case the most common number.
  if (number.size() == 1 && number[0] == '0') {
    unsigned_value_ = 0;
    token_ = kUnsigned;
    return;
  }
  // Pick out dot.
  if (number.size() == 1 && number[0] == '.') {
    token_ = '.';
    return;
  }
  // Decode numbers, with seperate paths for prefixes and implicit octals.
  if (has_prefix && number[0] == '0') {
    // "0[xob]" by itself is a parse error.
    if (number.size() <= 2) {
      token_ = kParseError;
      return;
    }
    switch (number[1]) {
      case 'b':
        double_value_ = BinaryStringToDouble(
            base::Vector<const uint8_t>::cast(base::VectorOf(number)));
        break;
      case 'o':
        double_value_ = OctalStringToDouble(
            base::Vector<const uint8_t>::cast(base::VectorOf(number)));
        break;
      case 'x':
        double_value_ = HexStringToDouble(
            base::Vector<const uint8_t>::cast(base::VectorOf(number)));
        break;
      default:
        // If there is a prefix character, but it's not the second character,
        // then there's a parse error somewhere.
        token_ = kParseError;
        break;
    }
  } else if (number[0] == '0' && !has_prefix && IsValidImplicitOctal(number)) {
    double_value_ = ImplicitOctalStringToDouble(
        base::Vector<const uint8_t>::cast(base::VectorOf(number)));
  } else {
    double_value_ = StringToDouble(
        base::Vector<const uint8_t>::cast(base::VectorOf(number)),
        NO_CONVERSION_FLAG);
  }
  if (std::isnan(double_value_)) {
    // Check if string to number conversion didn't consume all the characters.
    // This happens if the character filter let through something invalid
    // like: 0123ef for example.
    // TODO(bradnelson): Check if this happens often enough to be a perf
    // problem.
    if (number[0] == '.') {
      size_t rewind_by = number.size() - 1;
      DCHECK_LE(rewind_by, input_position_);
      input_position_ -= rewind_by;
      token_ = '.';
      return;
    }
    // Anything else that doesn't parse is an error.
    token_ = kParseError;
    return;
  }
  if (has_dot || trunc(double_value_) != double_value_) {
    token_ = kDouble;
  } else {
    // Exceeding safe integer range is an error.
    if (double_value_ > static_cast<double>(kMaxUInt32)) {
      token_ = kParseError;
      return;
    }
    unsigned_value_ = static_cast<uint32_t>(double_value_);
    token_ = kUnsigned;
  }
}

bool AsmJsScanner::ConsumeCComment() {
  while (HasMoreChars()) {
    while (Consume('*')) {
      if (Consume('/')) return true;
    }

    if (Consume('\n')) {
      preceded_by_newline_ = true;
    } else {
      Advance();
    }
  }
  return false;
}

void AsmJsScanner::ConsumeCPPComment() {
  while (HasMoreChars()) {
    if (Consume('\n')) {
      preceded_by_newline_ = true;
      return;
    }
    Advance();
  }
}

void AsmJsScanner::ConsumeString(base::uc32 quote) {
  // Only string allowed is 'use asm' / "use asm".
  const char* expected = "use asm";
  for (; *expected != '\0'; ++expected) {
    if (!Consume(static_cast<base::uc32>(*expected))) {
      token_ = kParseError;
      return;
    }
  }
  if (!Consume(quote)) {
    token_ = kParseError;
    return;
  }
  token_ = kToken_UseAsm;
}

void AsmJsScanner::ConsumeCompareOrShift(base::uc32 ch) {
  if (Consume('=')) {
    switch (ch) {
      case '<':
        token_ = kToken_LE;
        break;
      case '>':
        token_ = kToken_GE;
        break;
      case '=':
        token_ = kToken_EQ;
        break;
      case '!':
        token_ = kToken_NE;
        break;
      default:
        UNREACHABLE();
    }
  } else if (ch == '<' && Consume('<')) {
    token_ = kToken_SHL;
  } else if (ch == '>' && Consume('>')) {
    token_ = Consume('>') ? kToken_SHR : kToken_SAR;
  } else {
    token_ = ch;
  }
}

bool AsmJsScanner::IsIdentifierStart(base::uc32 ch) {
  return base::IsInRange(AsciiAlphaToLower(ch), 'a', 'z') || ch == '_' ||
         ch == '$';
}

bool AsmJsScanner::IsIdentifierPart(base::uc32 ch) {
  return IsAsciiIdentifier(ch);
}

bool AsmJsScanner::IsNumberStart(base::uc32 ch) {
  return ch == '.' || IsDecimalDigit(ch);
}

}  // namespace internal
}  // namespace v8

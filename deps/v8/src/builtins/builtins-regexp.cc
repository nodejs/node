// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"
#include "src/regexp/regexp-utils.h"
#include "src/regexp/regexp.h"
#include "src/strings/string-builder-inl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 21.2 RegExp Objects

BUILTIN(RegExpPrototypeToString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSReceiver, recv, "RegExp.prototype.toString");

  if (*recv == isolate->regexp_function()->prototype()) {
    isolate->CountUsage(v8::Isolate::kRegExpPrototypeToString);
  }

  IncrementalStringBuilder builder(isolate);

  builder.AppendCharacter('/');
  {
    Handle<Object> source;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, source,
        JSReceiver::GetProperty(isolate, recv,
                                isolate->factory()->source_string()));
    DirectHandle<String> source_str;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, source_str,
                                       Object::ToString(isolate, source));
    builder.AppendString(source_str);
  }

  builder.AppendCharacter('/');
  {
    Handle<Object> flags;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, flags,
        JSReceiver::GetProperty(isolate, recv,
                                isolate->factory()->flags_string()));
    DirectHandle<String> flags_str;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, flags_str,
                                       Object::ToString(isolate, flags));
    builder.AppendString(flags_str);
  }

  RETURN_RESULT_OR_FAILURE(isolate, builder.Finish());
}

// The properties $1..$9 are the first nine capturing substrings of the last
// successful match, or ''.  The function RegExpMakeCaptureGetter will be
// called with indices from 1 to 9.
#define DEFINE_CAPTURE_GETTER(i)                        \
  BUILTIN(RegExpCapture##i##Getter) {                   \
    HandleScope scope(isolate);                         \
    return *RegExpUtils::GenericCaptureGetter(          \
        isolate, isolate->regexp_last_match_info(), i); \
  }
DEFINE_CAPTURE_GETTER(1)
DEFINE_CAPTURE_GETTER(2)
DEFINE_CAPTURE_GETTER(3)
DEFINE_CAPTURE_GETTER(4)
DEFINE_CAPTURE_GETTER(5)
DEFINE_CAPTURE_GETTER(6)
DEFINE_CAPTURE_GETTER(7)
DEFINE_CAPTURE_GETTER(8)
DEFINE_CAPTURE_GETTER(9)
#undef DEFINE_CAPTURE_GETTER

// The properties `input` and `$_` are aliases for each other.  When this
// value is set, the value it is set to is coerced to a string.
// Getter and setter for the input.

BUILTIN(RegExpInputGetter) {
  HandleScope scope(isolate);
  DirectHandle<Object> obj(isolate->regexp_last_match_info()->last_input(),
                           isolate);
  return IsUndefined(*obj, isolate) ? ReadOnlyRoots(isolate).empty_string()
                                    : Cast<String>(*obj);
}

BUILTIN(RegExpInputSetter) {
  HandleScope scope(isolate);
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  DirectHandle<String> str;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, str,
                                     Object::ToString(isolate, value));
  isolate->regexp_last_match_info()->set_last_input(*str);
  return ReadOnlyRoots(isolate).undefined_value();
}

// Getters for the static properties lastMatch, lastParen, leftContext, and
// rightContext of the RegExp constructor.  The properties are computed based
// on the captures array of the last successful match and the subject string
// of the last successful match.
BUILTIN(RegExpLastMatchGetter) {
  HandleScope scope(isolate);
  return *RegExpUtils::GenericCaptureGetter(
      isolate, isolate->regexp_last_match_info(), 0);
}

BUILTIN(RegExpLastParenGetter) {
  HandleScope scope(isolate);
  DirectHandle<RegExpMatchInfo> match_info = isolate->regexp_last_match_info();
  const int length = match_info->number_of_capture_registers();
  if (length <= 2) {
    return ReadOnlyRoots(isolate).empty_string();  // No captures.
  }

  DCHECK_EQ(0, length % 2);
  const int last_capture = (length / 2) - 1;

  // We match the SpiderMonkey behavior: return the substring defined by the
  // last pair (after the first pair) of elements of the capture array even if
  // it is empty.
  return *RegExpUtils::GenericCaptureGetter(isolate, match_info, last_capture);
}

BUILTIN(RegExpLeftContextGetter) {
  HandleScope scope(isolate);
  DirectHandle<RegExpMatchInfo> match_info = isolate->regexp_last_match_info();
  const int start_index = match_info->capture(0);
  Handle<String> last_subject(match_info->last_subject(), isolate);
  return *isolate->factory()->NewSubString(last_subject, 0, start_index);
}

BUILTIN(RegExpRightContextGetter) {
  HandleScope scope(isolate);
  DirectHandle<RegExpMatchInfo> match_info = isolate->regexp_last_match_info();
  const int start_index = match_info->capture(1);
  Handle<String> last_subject(match_info->last_subject(), isolate);
  const int len = last_subject->length();
  return *isolate->factory()->NewSubString(last_subject, start_index, len);
}

namespace {

constexpr uint8_t kNoEscape = 0;
constexpr uint8_t kEscapeToHex = std::numeric_limits<uint8_t>::max();
constexpr uint8_t GetAsciiEscape(char c) {
  switch (c) {
    // SyntaxCharacter :: one of
    //   ^ $ \ . * + ? ( ) [ ] { } |
    //
    // SyntaxCharacter and U+002F (SOLIDUS) are escaped as-is.
    case '^':
    case '$':
    case '\\':
    case '.':
    case '*':
    case '+':
    case '?':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '|':
    case '/':
      return c;

    // ControlEscape :: one of
    //   f n r t v
    case '\f':
      return 'f';
    case '\n':
      return 'n';
    case '\r':
      return 'r';
    case '\t':
      return 't';
    case '\v':
      return 'v';

    // One of ",-=<>#&!%:;@~'`", the code unit 0x0022 (QUOTATION MARK), and
    // ASCII whitespace are escaped to hex.
    case ',':
    case '-':
    case '=':
    case '<':
    case '>':
    case '#':
    case '&':
    case '!':
    case '%':
    case ':':
    case ';':
    case '@':
    case '~':
    case '\'':
    case '`':
    case '"':
    case ' ':
      return kEscapeToHex;

    default:
      return kNoEscape;
  }
}

constexpr const uint8_t kAsciiEscapes[128]{
#define GET_ASCII_ESCAPE(c) GetAsciiEscape(c),
    INT_0_TO_127_LIST(GET_ASCII_ESCAPE)
#undef GET_ASCII_ESCAPE
};

template <typename CharT>
MaybeDirectHandle<String> RegExpEscapeImpl(Isolate* isolate,
                                           base::OwnedVector<CharT> source) {
  char double_to_radix_chars[kDoubleToRadixMaxChars];
  base::Vector<char> double_to_radix_buffer =
      base::ArrayVector(double_to_radix_chars);

  // 2. Let escaped be the empty String.
  IncrementalStringBuilder escaped_builder(isolate);
  if constexpr (sizeof(CharT) == 2) {
    escaped_builder.ChangeEncoding();
  }

  // 3. Let cpList be StringToCodePoints(S).
  // 4. For each code point c of cpList, do
  // (Done below.)

  size_t start;
  std::remove_const_t<CharT> first_c = source[0];
  if (IsAlphaNumeric(first_c)) {
    // a. If escaped is the empty String and c is matched by either
    //    DecimalDigit or AsciiLetter, then
    //   i. NOTE: Escaping a leading digit ensures that output corresponds
    //     with pattern text which may be used after a \0 character escape or
    //     a DecimalEscape such as \1 and still match S rather than be
    //     interpreted as an extension of the preceding escape sequence.
    //     Escaping a leading ASCII letter does the same for the context after
    //     \c.
    //   ii. Let numericValue be the numeric value of c.
    //   iii. Let hex be Number::toString(𝔽(numericValue), 16).
    //   iv. Assert: The length of hex is 2.
    //   v. Set escaped to the string-concatenation of the code unit 0x005C
    //      (REVERSE SOLIDUS), "x", and hex.
    start = 1;
    escaped_builder.AppendCStringLiteral("\\x");
    std::string_view hex =
        DoubleToRadixStringView(first_c, 16, double_to_radix_buffer);
    escaped_builder.AppendString(hex);
  } else {
    start = 0;
  }

  // EncodeForRegExpEscape
  //
  // 1. If c is matched by SyntaxCharacter or c is U+002F (SOLIDUS), then a.
  //    Return the string-concatenation of 0x005C (REVERSE SOLIDUS) and
  //    UTF16EncodeCodePoint(c).
  // 2. Else if c is the code point listed in some cell of the “Code Point”
  //    column of Table 63, then a. Return the string-concatenation of 0x005C
  //    (REVERSE SOLIDUS) and the string in the “ControlEscape” column of the
  //    row whose “Code Point” column contains c.
  // 3. Let otherPunctuators be the string-concatenation of ",-=<>#&!%:;@~'`"
  //    and the code unit 0x0022 (QUOTATION MARK).
  // 4. Let toEscape be StringToCodePoints(otherPunctuators).
  // 5. If toEscape contains c, c is matched by either WhiteSpace or
  //    LineTerminator, or c has the same numeric value as a leading surrogate
  //    or trailing surrogate, then a. Let cNum be the numeric value of c. b. If
  //    cNum ≤ 0xFF, then i. Let hex be Number::toString(𝔽(cNum), 16). ii.
  //    Return the string-concatenation of the code unit 0x005C (REVERSE
  //    SOLIDUS), "x", and StringPad(hex, 2, "0", start). c. Let escaped be the
  //    empty String. d. Let codeUnits be UTF16EncodeCodePoint(c). e. For each
  //    code unit cu of codeUnits, do i. Set escaped to the string-concatenation
  //    of escaped and UnicodeEscape(cu). f. Return escaped.
  // 6. Return UTF16EncodeCodePoint(c).
  //
  // Steps 1-2 above are done by table lookup in kAsciiEscapes. For step 3,
  // matching otherPuncatuators, quotation mark, and ASCII whitespace is done by
  // table lookup in kAsciiEscapes. Non-ASCII whitespace and line terminators in
  // step 5 are matched manually below.

  for (size_t i = start; i < source.size(); i++) {
    CharT cu = source[i];
    base::uc32 cp = cu;
    uint8_t cmd = kNoEscape;

    if (IsAscii(cu)) {
      cmd = kAsciiEscapes[cu];
    } else {
      if constexpr (sizeof(CharT) == 2) {
        if (unibrow::Utf16::IsLeadSurrogate(cu)) {
          if (i + 1 < source.size() &&
              unibrow::Utf16::IsTrailSurrogate(source[i + 1])) {
            // Surrogate pair. Combine them.
            cp = unibrow::Utf16::CombineSurrogatePair(cu, source[i + 1]);
            i++;
          } else {
            // Lone lead surrogate.
            cmd = kEscapeToHex;
          }
        } else if (unibrow::Utf16::IsTrailSurrogate(cu)) {
          // Lone trailing surrogate.
          cmd = kEscapeToHex;
        }
      }

      // ASCII whitespace and line terminators are hardcoded in the
      // kAsciiEscapes table.
      if (IsWhiteSpaceOrLineTerminator(cp)) {
        cmd = kEscapeToHex;
      }
    }

    if (cmd == kNoEscape) {
      // Code point does not need to be escaped.
      if (cp == cu) {
        escaped_builder.Append<CharT, CharT>(cp);
      } else {
        DCHECK_LT(i, source.size());
        DCHECK(unibrow::Utf16::IsSurrogatePair(cu, source[i]));
        DCHECK_EQ(cp, unibrow::Utf16::CombineSurrogatePair(cu, source[i]));
        escaped_builder.Append<CharT, CharT>(cu);
        escaped_builder.Append<CharT, CharT>(source[i]);
      }
    } else if (cmd == kEscapeToHex) {
      // An escape to hex. Output \x or \u depending on how many code units.
      escaped_builder.AppendCStringLiteral(cp <= 0xFF ? "\\x" : "\\u");
      std::string_view hex =
          DoubleToRadixStringView(cp, 16, double_to_radix_buffer);
      escaped_builder.AppendString(hex);
    } else {
      // A manual, non-hex escape. See table in kAsciiEscapes.
      escaped_builder.AppendCharacter('\\');
      escaped_builder.AppendCharacter(cmd);
    }
  }

  return escaped_builder.Finish();
}
}  // namespace

BUILTIN(RegExpEscape) {
  HandleScope scope(isolate);
  Handle<Object> value = args.atOrUndefined(isolate, 1);

  isolate->CountUsage(v8::Isolate::kRegExpEscape);

  // 1. If S is not a String, throw a TypeError exception.
  if (!IsString(*value)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kArgumentIsNonString,
                              isolate->factory()->input_string()));
  }
  Handle<String> str = Cast<String>(value);

  if (str->length() == 0) return ReadOnlyRoots(isolate).empty_string();

  DirectHandle<String> escaped;

  // A copy of the input characters is needed because RegExpEscapeImpl builds up
  // the escaped string using IncrementalStringBuilder, which may allocate.
  str = String::Flatten(isolate, str);
  if (str->IsOneByteRepresentation()) {
    base::OwnedVector<const uint8_t> copy;
    {
      DisallowGarbageCollection no_gc;
      copy = base::OwnedCopyOf(str->GetFlatContent(no_gc).ToOneByteVector());
    }
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, escaped, RegExpEscapeImpl(isolate, std::move(copy)));
  } else {
    base::OwnedVector<const base::uc16> copy;
    {
      DisallowGarbageCollection no_gc;
      copy = base::OwnedCopyOf(str->GetFlatContent(no_gc).ToUC16Vector());
    }
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, escaped, RegExpEscapeImpl(isolate, std::move(copy)));
  }

  return *escaped;
}

}  // namespace internal
}  // namespace v8

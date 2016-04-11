// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-parser.h"

#include "src/char-predicates-inl.h"
#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/regexp/jsregexp.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

RegExpParser::RegExpParser(FlatStringReader* in, Handle<String>* error,
                           bool multiline, bool unicode, Isolate* isolate,
                           Zone* zone)
    : isolate_(isolate),
      zone_(zone),
      error_(error),
      captures_(NULL),
      in_(in),
      current_(kEndMarker),
      next_pos_(0),
      captures_started_(0),
      capture_count_(0),
      has_more_(true),
      multiline_(multiline),
      unicode_(unicode),
      simple_(false),
      contains_anchor_(false),
      is_scanned_for_captures_(false),
      failed_(false) {
  Advance();
}


uc32 RegExpParser::Next() {
  if (has_next()) {
    return in()->Get(next_pos_);
  } else {
    return kEndMarker;
  }
}


void RegExpParser::Advance() {
  if (next_pos_ < in()->length()) {
    StackLimitCheck check(isolate());
    if (check.HasOverflowed()) {
      ReportError(CStrVector(Isolate::kStackOverflowMessage));
    } else if (zone()->excess_allocation()) {
      ReportError(CStrVector("Regular expression too large"));
    } else {
      current_ = in()->Get(next_pos_);
      next_pos_++;
      // Read the whole surrogate pair in case of unicode flag, if possible.
      if (unicode_ && next_pos_ < in()->length() &&
          unibrow::Utf16::IsLeadSurrogate(static_cast<uc16>(current_))) {
        uc16 trail = in()->Get(next_pos_);
        if (unibrow::Utf16::IsTrailSurrogate(trail)) {
          current_ = unibrow::Utf16::CombineSurrogatePair(
              static_cast<uc16>(current_), trail);
          next_pos_++;
        }
      }
    }
  } else {
    current_ = kEndMarker;
    // Advance so that position() points to 1-after-the-last-character. This is
    // important so that Reset() to this position works correctly.
    next_pos_ = in()->length() + 1;
    has_more_ = false;
  }
}


void RegExpParser::Reset(int pos) {
  next_pos_ = pos;
  has_more_ = (pos < in()->length());
  Advance();
}


void RegExpParser::Advance(int dist) {
  next_pos_ += dist - 1;
  Advance();
}


bool RegExpParser::simple() { return simple_; }


bool RegExpParser::IsSyntaxCharacter(uc32 c) {
  return c == '^' || c == '$' || c == '\\' || c == '.' || c == '*' ||
         c == '+' || c == '?' || c == '(' || c == ')' || c == '[' || c == ']' ||
         c == '{' || c == '}' || c == '|';
}


RegExpTree* RegExpParser::ReportError(Vector<const char> message) {
  failed_ = true;
  *error_ = isolate()->factory()->NewStringFromAscii(message).ToHandleChecked();
  // Zip to the end to make sure the no more input is read.
  current_ = kEndMarker;
  next_pos_ = in()->length();
  return NULL;
}


#define CHECK_FAILED /**/); \
  if (failed_) return NULL; \
  ((void)0


// Pattern ::
//   Disjunction
RegExpTree* RegExpParser::ParsePattern() {
  RegExpTree* result = ParseDisjunction(CHECK_FAILED);
  DCHECK(!has_more());
  // If the result of parsing is a literal string atom, and it has the
  // same length as the input, then the atom is identical to the input.
  if (result->IsAtom() && result->AsAtom()->length() == in()->length()) {
    simple_ = true;
  }
  return result;
}


// Disjunction ::
//   Alternative
//   Alternative | Disjunction
// Alternative ::
//   [empty]
//   Term Alternative
// Term ::
//   Assertion
//   Atom
//   Atom Quantifier
RegExpTree* RegExpParser::ParseDisjunction() {
  // Used to store current state while parsing subexpressions.
  RegExpParserState initial_state(NULL, INITIAL, RegExpLookaround::LOOKAHEAD, 0,
                                  zone());
  RegExpParserState* state = &initial_state;
  // Cache the builder in a local variable for quick access.
  RegExpBuilder* builder = initial_state.builder();
  while (true) {
    switch (current()) {
      case kEndMarker:
        if (state->IsSubexpression()) {
          // Inside a parenthesized group when hitting end of input.
          ReportError(CStrVector("Unterminated group") CHECK_FAILED);
        }
        DCHECK_EQ(INITIAL, state->group_type());
        // Parsing completed successfully.
        return builder->ToRegExp();
      case ')': {
        if (!state->IsSubexpression()) {
          ReportError(CStrVector("Unmatched ')'") CHECK_FAILED);
        }
        DCHECK_NE(INITIAL, state->group_type());

        Advance();
        // End disjunction parsing and convert builder content to new single
        // regexp atom.
        RegExpTree* body = builder->ToRegExp();

        int end_capture_index = captures_started();

        int capture_index = state->capture_index();
        SubexpressionType group_type = state->group_type();

        // Build result of subexpression.
        if (group_type == CAPTURE) {
          RegExpCapture* capture = GetCapture(capture_index);
          capture->set_body(body);
          body = capture;
        } else if (group_type != GROUPING) {
          DCHECK(group_type == POSITIVE_LOOKAROUND ||
                 group_type == NEGATIVE_LOOKAROUND);
          bool is_positive = (group_type == POSITIVE_LOOKAROUND);
          body = new (zone()) RegExpLookaround(
              body, is_positive, end_capture_index - capture_index,
              capture_index, state->lookaround_type());
        }

        // Restore previous state.
        state = state->previous_state();
        builder = state->builder();

        builder->AddAtom(body);
        // For compatability with JSC and ES3, we allow quantifiers after
        // lookaheads, and break in all cases.
        break;
      }
      case '|': {
        Advance();
        builder->NewAlternative();
        continue;
      }
      case '*':
      case '+':
      case '?':
        return ReportError(CStrVector("Nothing to repeat"));
      case '^': {
        Advance();
        if (multiline_) {
          builder->AddAssertion(
              new (zone()) RegExpAssertion(RegExpAssertion::START_OF_LINE));
        } else {
          builder->AddAssertion(
              new (zone()) RegExpAssertion(RegExpAssertion::START_OF_INPUT));
          set_contains_anchor();
        }
        continue;
      }
      case '$': {
        Advance();
        RegExpAssertion::AssertionType assertion_type =
            multiline_ ? RegExpAssertion::END_OF_LINE
                       : RegExpAssertion::END_OF_INPUT;
        builder->AddAssertion(new (zone()) RegExpAssertion(assertion_type));
        continue;
      }
      case '.': {
        Advance();
        // everything except \x0a, \x0d, \u2028 and \u2029
        ZoneList<CharacterRange>* ranges =
            new (zone()) ZoneList<CharacterRange>(2, zone());
        CharacterRange::AddClassEscape('.', ranges, zone());
        RegExpTree* atom = new (zone()) RegExpCharacterClass(ranges, false);
        builder->AddAtom(atom);
        break;
      }
      case '(': {
        SubexpressionType subexpr_type = CAPTURE;
        RegExpLookaround::Type lookaround_type = state->lookaround_type();
        Advance();
        if (current() == '?') {
          switch (Next()) {
            case ':':
              subexpr_type = GROUPING;
              break;
            case '=':
              lookaround_type = RegExpLookaround::LOOKAHEAD;
              subexpr_type = POSITIVE_LOOKAROUND;
              break;
            case '!':
              lookaround_type = RegExpLookaround::LOOKAHEAD;
              subexpr_type = NEGATIVE_LOOKAROUND;
              break;
            case '<':
              if (FLAG_harmony_regexp_lookbehind) {
                Advance();
                lookaround_type = RegExpLookaround::LOOKBEHIND;
                if (Next() == '=') {
                  subexpr_type = POSITIVE_LOOKAROUND;
                  break;
                } else if (Next() == '!') {
                  subexpr_type = NEGATIVE_LOOKAROUND;
                  break;
                }
              }
            // Fall through.
            default:
              ReportError(CStrVector("Invalid group") CHECK_FAILED);
              break;
          }
          Advance(2);
        } else {
          if (captures_started_ >= kMaxCaptures) {
            ReportError(CStrVector("Too many captures") CHECK_FAILED);
          }
          captures_started_++;
        }
        // Store current state and begin new disjunction parsing.
        state = new (zone()) RegExpParserState(
            state, subexpr_type, lookaround_type, captures_started_, zone());
        builder = state->builder();
        continue;
      }
      case '[': {
        RegExpTree* atom = ParseCharacterClass(CHECK_FAILED);
        builder->AddAtom(atom);
        break;
      }
      // Atom ::
      //   \ AtomEscape
      case '\\':
        switch (Next()) {
          case kEndMarker:
            return ReportError(CStrVector("\\ at end of pattern"));
          case 'b':
            Advance(2);
            builder->AddAssertion(
                new (zone()) RegExpAssertion(RegExpAssertion::BOUNDARY));
            continue;
          case 'B':
            Advance(2);
            builder->AddAssertion(
                new (zone()) RegExpAssertion(RegExpAssertion::NON_BOUNDARY));
            continue;
          // AtomEscape ::
          //   CharacterClassEscape
          //
          // CharacterClassEscape :: one of
          //   d D s S w W
          case 'd':
          case 'D':
          case 's':
          case 'S':
          case 'w':
          case 'W': {
            uc32 c = Next();
            Advance(2);
            ZoneList<CharacterRange>* ranges =
                new (zone()) ZoneList<CharacterRange>(2, zone());
            CharacterRange::AddClassEscape(c, ranges, zone());
            RegExpTree* atom = new (zone()) RegExpCharacterClass(ranges, false);
            builder->AddAtom(atom);
            break;
          }
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9': {
            int index = 0;
            if (ParseBackReferenceIndex(&index)) {
              if (state->IsInsideCaptureGroup(index)) {
                // The back reference is inside the capture group it refers to.
                // Nothing can possibly have been captured yet, so we use empty
                // instead. This ensures that, when checking a back reference,
                // the capture registers of the referenced capture are either
                // both set or both cleared.
                builder->AddEmpty();
              } else {
                RegExpCapture* capture = GetCapture(index);
                RegExpTree* atom = new (zone()) RegExpBackReference(capture);
                builder->AddAtom(atom);
              }
              break;
            }
            uc32 first_digit = Next();
            if (first_digit == '8' || first_digit == '9') {
              // If the 'u' flag is present, only syntax characters can be
              // escaped,
              // no other identity escapes are allowed. If the 'u' flag is not
              // present, all identity escapes are allowed.
              if (!unicode_) {
                builder->AddCharacter(first_digit);
                Advance(2);
              } else {
                return ReportError(CStrVector("Invalid escape"));
              }
              break;
            }
          }
          // FALLTHROUGH
          case '0': {
            Advance();
            uc32 octal = ParseOctalLiteral();
            builder->AddCharacter(octal);
            break;
          }
          // ControlEscape :: one of
          //   f n r t v
          case 'f':
            Advance(2);
            builder->AddCharacter('\f');
            break;
          case 'n':
            Advance(2);
            builder->AddCharacter('\n');
            break;
          case 'r':
            Advance(2);
            builder->AddCharacter('\r');
            break;
          case 't':
            Advance(2);
            builder->AddCharacter('\t');
            break;
          case 'v':
            Advance(2);
            builder->AddCharacter('\v');
            break;
          case 'c': {
            Advance();
            uc32 controlLetter = Next();
            // Special case if it is an ASCII letter.
            // Convert lower case letters to uppercase.
            uc32 letter = controlLetter & ~('a' ^ 'A');
            if (letter < 'A' || 'Z' < letter) {
              // controlLetter is not in range 'A'-'Z' or 'a'-'z'.
              // This is outside the specification. We match JSC in
              // reading the backslash as a literal character instead
              // of as starting an escape.
              builder->AddCharacter('\\');
            } else {
              Advance(2);
              builder->AddCharacter(controlLetter & 0x1f);
            }
            break;
          }
          case 'x': {
            Advance(2);
            uc32 value;
            if (ParseHexEscape(2, &value)) {
              builder->AddCharacter(value);
            } else if (!unicode_) {
              builder->AddCharacter('x');
            } else {
              // If the 'u' flag is present, invalid escapes are not treated as
              // identity escapes.
              return ReportError(CStrVector("Invalid escape"));
            }
            break;
          }
          case 'u': {
            Advance(2);
            uc32 value;
            if (ParseUnicodeEscape(&value)) {
              builder->AddUnicodeCharacter(value);
            } else if (!unicode_) {
              builder->AddCharacter('u');
            } else {
              // If the 'u' flag is present, invalid escapes are not treated as
              // identity escapes.
              return ReportError(CStrVector("Invalid unicode escape"));
            }
            break;
          }
          default:
            Advance();
            // If the 'u' flag is present, only syntax characters can be
            // escaped, no
            // other identity escapes are allowed. If the 'u' flag is not
            // present,
            // all identity escapes are allowed.
            if (!unicode_ || IsSyntaxCharacter(current())) {
              builder->AddCharacter(current());
              Advance();
            } else {
              return ReportError(CStrVector("Invalid escape"));
            }
            break;
        }
        break;
      case '{': {
        int dummy;
        if (ParseIntervalQuantifier(&dummy, &dummy)) {
          ReportError(CStrVector("Nothing to repeat") CHECK_FAILED);
        }
        // fallthrough
      }
      default:
        builder->AddUnicodeCharacter(current());
        Advance();
        break;
    }  // end switch(current())

    int min;
    int max;
    switch (current()) {
      // QuantifierPrefix ::
      //   *
      //   +
      //   ?
      //   {
      case '*':
        min = 0;
        max = RegExpTree::kInfinity;
        Advance();
        break;
      case '+':
        min = 1;
        max = RegExpTree::kInfinity;
        Advance();
        break;
      case '?':
        min = 0;
        max = 1;
        Advance();
        break;
      case '{':
        if (ParseIntervalQuantifier(&min, &max)) {
          if (max < min) {
            ReportError(CStrVector("numbers out of order in {} quantifier.")
                            CHECK_FAILED);
          }
          break;
        } else {
          continue;
        }
      default:
        continue;
    }
    RegExpQuantifier::QuantifierType quantifier_type = RegExpQuantifier::GREEDY;
    if (current() == '?') {
      quantifier_type = RegExpQuantifier::NON_GREEDY;
      Advance();
    } else if (FLAG_regexp_possessive_quantifier && current() == '+') {
      // FLAG_regexp_possessive_quantifier is a debug-only flag.
      quantifier_type = RegExpQuantifier::POSSESSIVE;
      Advance();
    }
    builder->AddQuantifierToAtom(min, max, quantifier_type);
  }
}


#ifdef DEBUG
// Currently only used in an DCHECK.
static bool IsSpecialClassEscape(uc32 c) {
  switch (c) {
    case 'd':
    case 'D':
    case 's':
    case 'S':
    case 'w':
    case 'W':
      return true;
    default:
      return false;
  }
}
#endif


// In order to know whether an escape is a backreference or not we have to scan
// the entire regexp and find the number of capturing parentheses.  However we
// don't want to scan the regexp twice unless it is necessary.  This mini-parser
// is called when needed.  It can see the difference between capturing and
// noncapturing parentheses and can skip character classes and backslash-escaped
// characters.
void RegExpParser::ScanForCaptures() {
  // Start with captures started previous to current position
  int capture_count = captures_started();
  // Add count of captures after this position.
  int n;
  while ((n = current()) != kEndMarker) {
    Advance();
    switch (n) {
      case '\\':
        Advance();
        break;
      case '[': {
        int c;
        while ((c = current()) != kEndMarker) {
          Advance();
          if (c == '\\') {
            Advance();
          } else {
            if (c == ']') break;
          }
        }
        break;
      }
      case '(':
        if (current() != '?') capture_count++;
        break;
    }
  }
  capture_count_ = capture_count;
  is_scanned_for_captures_ = true;
}


bool RegExpParser::ParseBackReferenceIndex(int* index_out) {
  DCHECK_EQ('\\', current());
  DCHECK('1' <= Next() && Next() <= '9');
  // Try to parse a decimal literal that is no greater than the total number
  // of left capturing parentheses in the input.
  int start = position();
  int value = Next() - '0';
  Advance(2);
  while (true) {
    uc32 c = current();
    if (IsDecimalDigit(c)) {
      value = 10 * value + (c - '0');
      if (value > kMaxCaptures) {
        Reset(start);
        return false;
      }
      Advance();
    } else {
      break;
    }
  }
  if (value > captures_started()) {
    if (!is_scanned_for_captures_) {
      int saved_position = position();
      ScanForCaptures();
      Reset(saved_position);
    }
    if (value > capture_count_) {
      Reset(start);
      return false;
    }
  }
  *index_out = value;
  return true;
}


RegExpCapture* RegExpParser::GetCapture(int index) {
  // The index for the capture groups are one-based. Its index in the list is
  // zero-based.
  int know_captures =
      is_scanned_for_captures_ ? capture_count_ : captures_started_;
  DCHECK(index <= know_captures);
  if (captures_ == NULL) {
    captures_ = new (zone()) ZoneList<RegExpCapture*>(know_captures, zone());
  }
  while (captures_->length() < know_captures) {
    captures_->Add(new (zone()) RegExpCapture(captures_->length() + 1), zone());
  }
  return captures_->at(index - 1);
}


bool RegExpParser::RegExpParserState::IsInsideCaptureGroup(int index) {
  for (RegExpParserState* s = this; s != NULL; s = s->previous_state()) {
    if (s->group_type() != CAPTURE) continue;
    // Return true if we found the matching capture index.
    if (index == s->capture_index()) return true;
    // Abort if index is larger than what has been parsed up till this state.
    if (index > s->capture_index()) return false;
  }
  return false;
}


// QuantifierPrefix ::
//   { DecimalDigits }
//   { DecimalDigits , }
//   { DecimalDigits , DecimalDigits }
//
// Returns true if parsing succeeds, and set the min_out and max_out
// values. Values are truncated to RegExpTree::kInfinity if they overflow.
bool RegExpParser::ParseIntervalQuantifier(int* min_out, int* max_out) {
  DCHECK_EQ(current(), '{');
  int start = position();
  Advance();
  int min = 0;
  if (!IsDecimalDigit(current())) {
    Reset(start);
    return false;
  }
  while (IsDecimalDigit(current())) {
    int next = current() - '0';
    if (min > (RegExpTree::kInfinity - next) / 10) {
      // Overflow. Skip past remaining decimal digits and return -1.
      do {
        Advance();
      } while (IsDecimalDigit(current()));
      min = RegExpTree::kInfinity;
      break;
    }
    min = 10 * min + next;
    Advance();
  }
  int max = 0;
  if (current() == '}') {
    max = min;
    Advance();
  } else if (current() == ',') {
    Advance();
    if (current() == '}') {
      max = RegExpTree::kInfinity;
      Advance();
    } else {
      while (IsDecimalDigit(current())) {
        int next = current() - '0';
        if (max > (RegExpTree::kInfinity - next) / 10) {
          do {
            Advance();
          } while (IsDecimalDigit(current()));
          max = RegExpTree::kInfinity;
          break;
        }
        max = 10 * max + next;
        Advance();
      }
      if (current() != '}') {
        Reset(start);
        return false;
      }
      Advance();
    }
  } else {
    Reset(start);
    return false;
  }
  *min_out = min;
  *max_out = max;
  return true;
}


uc32 RegExpParser::ParseOctalLiteral() {
  DCHECK(('0' <= current() && current() <= '7') || current() == kEndMarker);
  // For compatibility with some other browsers (not all), we parse
  // up to three octal digits with a value below 256.
  uc32 value = current() - '0';
  Advance();
  if ('0' <= current() && current() <= '7') {
    value = value * 8 + current() - '0';
    Advance();
    if (value < 32 && '0' <= current() && current() <= '7') {
      value = value * 8 + current() - '0';
      Advance();
    }
  }
  return value;
}


bool RegExpParser::ParseHexEscape(int length, uc32* value) {
  int start = position();
  uc32 val = 0;
  for (int i = 0; i < length; ++i) {
    uc32 c = current();
    int d = HexValue(c);
    if (d < 0) {
      Reset(start);
      return false;
    }
    val = val * 16 + d;
    Advance();
  }
  *value = val;
  return true;
}


bool RegExpParser::ParseUnicodeEscape(uc32* value) {
  // Accept both \uxxxx and \u{xxxxxx} (if harmony unicode escapes are
  // allowed). In the latter case, the number of hex digits between { } is
  // arbitrary. \ and u have already been read.
  if (current() == '{' && unicode_) {
    int start = position();
    Advance();
    if (ParseUnlimitedLengthHexNumber(0x10ffff, value)) {
      if (current() == '}') {
        Advance();
        return true;
      }
    }
    Reset(start);
    return false;
  }
  // \u but no {, or \u{...} escapes not allowed.
  return ParseHexEscape(4, value);
}


bool RegExpParser::ParseUnlimitedLengthHexNumber(int max_value, uc32* value) {
  uc32 x = 0;
  int d = HexValue(current());
  if (d < 0) {
    return false;
  }
  while (d >= 0) {
    x = x * 16 + d;
    if (x > max_value) {
      return false;
    }
    Advance();
    d = HexValue(current());
  }
  *value = x;
  return true;
}


uc32 RegExpParser::ParseClassCharacterEscape() {
  DCHECK(current() == '\\');
  DCHECK(has_next() && !IsSpecialClassEscape(Next()));
  Advance();
  switch (current()) {
    case 'b':
      Advance();
      return '\b';
    // ControlEscape :: one of
    //   f n r t v
    case 'f':
      Advance();
      return '\f';
    case 'n':
      Advance();
      return '\n';
    case 'r':
      Advance();
      return '\r';
    case 't':
      Advance();
      return '\t';
    case 'v':
      Advance();
      return '\v';
    case 'c': {
      uc32 controlLetter = Next();
      uc32 letter = controlLetter & ~('A' ^ 'a');
      // For compatibility with JSC, inside a character class
      // we also accept digits and underscore as control characters.
      if ((controlLetter >= '0' && controlLetter <= '9') ||
          controlLetter == '_' || (letter >= 'A' && letter <= 'Z')) {
        Advance(2);
        // Control letters mapped to ASCII control characters in the range
        // 0x00-0x1f.
        return controlLetter & 0x1f;
      }
      // We match JSC in reading the backslash as a literal
      // character instead of as starting an escape.
      return '\\';
    }
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      // For compatibility, we interpret a decimal escape that isn't
      // a back reference (and therefore either \0 or not valid according
      // to the specification) as a 1..3 digit octal character code.
      return ParseOctalLiteral();
    case 'x': {
      Advance();
      uc32 value;
      if (ParseHexEscape(2, &value)) {
        return value;
      }
      if (!unicode_) {
        // If \x is not followed by a two-digit hexadecimal, treat it
        // as an identity escape.
        return 'x';
      }
      // If the 'u' flag is present, invalid escapes are not treated as
      // identity escapes.
      ReportError(CStrVector("Invalid escape"));
      return 0;
    }
    case 'u': {
      Advance();
      uc32 value;
      if (ParseUnicodeEscape(&value)) {
        return value;
      }
      if (!unicode_) {
        return 'u';
      }
      // If the 'u' flag is present, invalid escapes are not treated as
      // identity escapes.
      ReportError(CStrVector("Invalid unicode escape"));
      return 0;
    }
    default: {
      uc32 result = current();
      // If the 'u' flag is present, only syntax characters can be escaped, no
      // other identity escapes are allowed. If the 'u' flag is not present, all
      // identity escapes are allowed.
      if (!unicode_ || IsSyntaxCharacter(result)) {
        Advance();
        return result;
      }
      ReportError(CStrVector("Invalid escape"));
      return 0;
    }
  }
  return 0;
}


CharacterRange RegExpParser::ParseClassAtom(uc16* char_class) {
  DCHECK_EQ(0, *char_class);
  uc32 first = current();
  if (first == '\\') {
    switch (Next()) {
      case 'w':
      case 'W':
      case 'd':
      case 'D':
      case 's':
      case 'S': {
        *char_class = Next();
        Advance(2);
        return CharacterRange::Singleton(0);  // Return dummy value.
      }
      case kEndMarker:
        return ReportError(CStrVector("\\ at end of pattern"));
      default:
        uc32 c = ParseClassCharacterEscape(CHECK_FAILED);
        return CharacterRange::Singleton(c);
    }
  } else {
    Advance();
    return CharacterRange::Singleton(first);
  }
}


static const uc16 kNoCharClass = 0;

// Adds range or pre-defined character class to character ranges.
// If char_class is not kInvalidClass, it's interpreted as a class
// escape (i.e., 's' means whitespace, from '\s').
static inline void AddRangeOrEscape(ZoneList<CharacterRange>* ranges,
                                    uc16 char_class, CharacterRange range,
                                    Zone* zone) {
  if (char_class != kNoCharClass) {
    CharacterRange::AddClassEscape(char_class, ranges, zone);
  } else {
    ranges->Add(range, zone);
  }
}


RegExpTree* RegExpParser::ParseCharacterClass() {
  static const char* kUnterminated = "Unterminated character class";
  static const char* kRangeOutOfOrder = "Range out of order in character class";

  DCHECK_EQ(current(), '[');
  Advance();
  bool is_negated = false;
  if (current() == '^') {
    is_negated = true;
    Advance();
  }
  ZoneList<CharacterRange>* ranges =
      new (zone()) ZoneList<CharacterRange>(2, zone());
  while (has_more() && current() != ']') {
    uc16 char_class = kNoCharClass;
    CharacterRange first = ParseClassAtom(&char_class CHECK_FAILED);
    if (current() == '-') {
      Advance();
      if (current() == kEndMarker) {
        // If we reach the end we break out of the loop and let the
        // following code report an error.
        break;
      } else if (current() == ']') {
        AddRangeOrEscape(ranges, char_class, first, zone());
        ranges->Add(CharacterRange::Singleton('-'), zone());
        break;
      }
      uc16 char_class_2 = kNoCharClass;
      CharacterRange next = ParseClassAtom(&char_class_2 CHECK_FAILED);
      if (char_class != kNoCharClass || char_class_2 != kNoCharClass) {
        // Either end is an escaped character class. Treat the '-' verbatim.
        AddRangeOrEscape(ranges, char_class, first, zone());
        ranges->Add(CharacterRange::Singleton('-'), zone());
        AddRangeOrEscape(ranges, char_class_2, next, zone());
        continue;
      }
      if (first.from() > next.to()) {
        return ReportError(CStrVector(kRangeOutOfOrder) CHECK_FAILED);
      }
      ranges->Add(CharacterRange::Range(first.from(), next.to()), zone());
    } else {
      AddRangeOrEscape(ranges, char_class, first, zone());
    }
  }
  if (!has_more()) {
    return ReportError(CStrVector(kUnterminated) CHECK_FAILED);
  }
  Advance();
  if (ranges->length() == 0) {
    ranges->Add(CharacterRange::Everything(), zone());
    is_negated = !is_negated;
  }
  return new (zone()) RegExpCharacterClass(ranges, is_negated);
}


#undef CHECK_FAILED


bool RegExpParser::ParseRegExp(Isolate* isolate, Zone* zone,
                               FlatStringReader* input, bool multiline,
                               bool unicode, RegExpCompileData* result) {
  DCHECK(result != NULL);
  RegExpParser parser(input, &result->error, multiline, unicode, isolate, zone);
  RegExpTree* tree = parser.ParsePattern();
  if (parser.failed()) {
    DCHECK(tree == NULL);
    DCHECK(!result->error.is_null());
  } else {
    DCHECK(tree != NULL);
    DCHECK(result->error.is_null());
    if (FLAG_trace_regexp_parser) {
      OFStream os(stdout);
      tree->Print(os, zone);
      os << "\n";
    }
    result->tree = tree;
    int capture_count = parser.captures_started();
    result->simple = tree->IsAtom() && parser.simple() && capture_count == 0;
    result->contains_anchor = parser.contains_anchor();
    result->capture_count = capture_count;
  }
  return !parser.failed();
}


RegExpBuilder::RegExpBuilder(Zone* zone)
    : zone_(zone),
      pending_empty_(false),
      characters_(NULL),
      terms_(),
      alternatives_()
#ifdef DEBUG
      ,
      last_added_(ADD_NONE)
#endif
{
}


void RegExpBuilder::FlushCharacters() {
  pending_empty_ = false;
  if (characters_ != NULL) {
    RegExpTree* atom = new (zone()) RegExpAtom(characters_->ToConstVector());
    characters_ = NULL;
    text_.Add(atom, zone());
    LAST(ADD_ATOM);
  }
}


void RegExpBuilder::FlushText() {
  FlushCharacters();
  int num_text = text_.length();
  if (num_text == 0) {
    return;
  } else if (num_text == 1) {
    terms_.Add(text_.last(), zone());
  } else {
    RegExpText* text = new (zone()) RegExpText(zone());
    for (int i = 0; i < num_text; i++) text_.Get(i)->AppendToText(text, zone());
    terms_.Add(text, zone());
  }
  text_.Clear();
}


void RegExpBuilder::AddCharacter(uc16 c) {
  pending_empty_ = false;
  if (characters_ == NULL) {
    characters_ = new (zone()) ZoneList<uc16>(4, zone());
  }
  characters_->Add(c, zone());
  LAST(ADD_CHAR);
}


void RegExpBuilder::AddUnicodeCharacter(uc32 c) {
  if (c > unibrow::Utf16::kMaxNonSurrogateCharCode) {
    ZoneList<uc16> surrogate_pair(2, zone());
    surrogate_pair.Add(unibrow::Utf16::LeadSurrogate(c), zone());
    surrogate_pair.Add(unibrow::Utf16::TrailSurrogate(c), zone());
    RegExpAtom* atom = new (zone()) RegExpAtom(surrogate_pair.ToConstVector());
    AddAtom(atom);
  } else {
    AddCharacter(static_cast<uc16>(c));
  }
}


void RegExpBuilder::AddEmpty() { pending_empty_ = true; }


void RegExpBuilder::AddAtom(RegExpTree* term) {
  if (term->IsEmpty()) {
    AddEmpty();
    return;
  }
  if (term->IsTextElement()) {
    FlushCharacters();
    text_.Add(term, zone());
  } else {
    FlushText();
    terms_.Add(term, zone());
  }
  LAST(ADD_ATOM);
}


void RegExpBuilder::AddAssertion(RegExpTree* assert) {
  FlushText();
  terms_.Add(assert, zone());
  LAST(ADD_ASSERT);
}


void RegExpBuilder::NewAlternative() { FlushTerms(); }


void RegExpBuilder::FlushTerms() {
  FlushText();
  int num_terms = terms_.length();
  RegExpTree* alternative;
  if (num_terms == 0) {
    alternative = new (zone()) RegExpEmpty();
  } else if (num_terms == 1) {
    alternative = terms_.last();
  } else {
    alternative = new (zone()) RegExpAlternative(terms_.GetList(zone()));
  }
  alternatives_.Add(alternative, zone());
  terms_.Clear();
  LAST(ADD_NONE);
}


RegExpTree* RegExpBuilder::ToRegExp() {
  FlushTerms();
  int num_alternatives = alternatives_.length();
  if (num_alternatives == 0) return new (zone()) RegExpEmpty();
  if (num_alternatives == 1) return alternatives_.last();
  return new (zone()) RegExpDisjunction(alternatives_.GetList(zone()));
}


void RegExpBuilder::AddQuantifierToAtom(
    int min, int max, RegExpQuantifier::QuantifierType quantifier_type) {
  if (pending_empty_) {
    pending_empty_ = false;
    return;
  }
  RegExpTree* atom;
  if (characters_ != NULL) {
    DCHECK(last_added_ == ADD_CHAR);
    // Last atom was character.
    Vector<const uc16> char_vector = characters_->ToConstVector();
    int num_chars = char_vector.length();
    if (num_chars > 1) {
      Vector<const uc16> prefix = char_vector.SubVector(0, num_chars - 1);
      text_.Add(new (zone()) RegExpAtom(prefix), zone());
      char_vector = char_vector.SubVector(num_chars - 1, num_chars);
    }
    characters_ = NULL;
    atom = new (zone()) RegExpAtom(char_vector);
    FlushText();
  } else if (text_.length() > 0) {
    DCHECK(last_added_ == ADD_ATOM);
    atom = text_.RemoveLast();
    FlushText();
  } else if (terms_.length() > 0) {
    DCHECK(last_added_ == ADD_ATOM);
    atom = terms_.RemoveLast();
    if (atom->max_match() == 0) {
      // Guaranteed to only match an empty string.
      LAST(ADD_TERM);
      if (min == 0) {
        return;
      }
      terms_.Add(atom, zone());
      return;
    }
  } else {
    // Only call immediately after adding an atom or character!
    UNREACHABLE();
    return;
  }
  terms_.Add(new (zone()) RegExpQuantifier(min, max, quantifier_type, atom),
             zone());
  LAST(ADD_TERM);
}

}  // namespace internal
}  // namespace v8

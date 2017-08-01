// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-parser.h"

#include "src/char-predicates-inl.h"
#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"
#include "src/regexp/jsregexp.h"
#include "src/utils.h"

#ifdef V8_INTL_SUPPORT
#include "unicode/uniset.h"
#endif  // V8_INTL_SUPPORT

namespace v8 {
namespace internal {

RegExpParser::RegExpParser(FlatStringReader* in, Handle<String>* error,
                           JSRegExp::Flags flags, Isolate* isolate, Zone* zone)
    : isolate_(isolate),
      zone_(zone),
      error_(error),
      captures_(NULL),
      named_captures_(NULL),
      named_back_references_(NULL),
      in_(in),
      current_(kEndMarker),
      dotall_(flags & JSRegExp::kDotAll),
      ignore_case_(flags & JSRegExp::kIgnoreCase),
      multiline_(flags & JSRegExp::kMultiline),
      unicode_(flags & JSRegExp::kUnicode),
      next_pos_(0),
      captures_started_(0),
      capture_count_(0),
      has_more_(true),
      simple_(false),
      contains_anchor_(false),
      is_scanned_for_captures_(false),
      has_named_captures_(false),
      failed_(false) {
  DCHECK_IMPLIES(dotall(), FLAG_harmony_regexp_dotall);
  Advance();
}

template <bool update_position>
inline uc32 RegExpParser::ReadNext() {
  int position = next_pos_;
  uc32 c0 = in()->Get(position);
  position++;
  // Read the whole surrogate pair in case of unicode flag, if possible.
  if (unicode() && position < in()->length() &&
      unibrow::Utf16::IsLeadSurrogate(static_cast<uc16>(c0))) {
    uc16 c1 = in()->Get(position);
    if (unibrow::Utf16::IsTrailSurrogate(c1)) {
      c0 = unibrow::Utf16::CombineSurrogatePair(static_cast<uc16>(c0), c1);
      position++;
    }
  }
  if (update_position) next_pos_ = position;
  return c0;
}


uc32 RegExpParser::Next() {
  if (has_next()) {
    return ReadNext<false>();
  } else {
    return kEndMarker;
  }
}

void RegExpParser::Advance() {
  if (has_next()) {
    StackLimitCheck check(isolate());
    if (check.HasOverflowed()) {
      if (FLAG_abort_on_stack_overflow) FATAL("Aborting on stack overflow");
      ReportError(CStrVector(
          MessageTemplate::TemplateString(MessageTemplate::kStackOverflow)));
    } else if (zone()->excess_allocation()) {
      ReportError(CStrVector("Regular expression too large"));
    } else {
      current_ = ReadNext<true>();
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

bool RegExpParser::IsSyntaxCharacterOrSlash(uc32 c) {
  switch (c) {
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
      return true;
    default:
      break;
  }
  return false;
}


RegExpTree* RegExpParser::ReportError(Vector<const char> message) {
  if (failed_) return NULL;  // Do not overwrite any existing error.
  failed_ = true;
  *error_ = isolate()
                ->factory()
                ->NewStringFromOneByte(Vector<const uint8_t>::cast(message))
                .ToHandleChecked();
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
  PatchNamedBackReferences(CHECK_FAILED);
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
                                  nullptr, ignore_case(), unicode(), zone());
  RegExpParserState* state = &initial_state;
  // Cache the builder in a local variable for quick access.
  RegExpBuilder* builder = initial_state.builder();
  while (true) {
    switch (current()) {
      case kEndMarker:
        if (state->IsSubexpression()) {
          // Inside a parenthesized group when hitting end of input.
          return ReportError(CStrVector("Unterminated group"));
        }
        DCHECK_EQ(INITIAL, state->group_type());
        // Parsing completed successfully.
        return builder->ToRegExp();
      case ')': {
        if (!state->IsSubexpression()) {
          return ReportError(CStrVector("Unmatched ')'"));
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
          if (state->IsNamedCapture()) {
            CreateNamedCaptureAtIndex(state->capture_name(),
                                      capture_index CHECK_FAILED);
          }
          RegExpCapture* capture = GetCapture(capture_index);
          capture->set_body(body);
          body = capture;
        } else if (group_type == GROUPING) {
          body = new (zone()) RegExpGroup(body);
        } else {
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
        if (multiline()) {
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
            multiline() ? RegExpAssertion::END_OF_LINE
                        : RegExpAssertion::END_OF_INPUT;
        builder->AddAssertion(new (zone()) RegExpAssertion(assertion_type));
        continue;
      }
      case '.': {
        Advance();
        ZoneList<CharacterRange>* ranges =
            new (zone()) ZoneList<CharacterRange>(2, zone());

        if (dotall()) {
          // Everything.
          DCHECK(FLAG_harmony_regexp_dotall);
          CharacterRange::AddClassEscape('*', ranges, false, zone());
        } else {
          // Everything except \x0a, \x0d, \u2028 and \u2029
          CharacterRange::AddClassEscape('.', ranges, false, zone());
        }

        RegExpCharacterClass* cc = new (zone()) RegExpCharacterClass(ranges);
        builder->AddCharacterClass(cc);
        break;
      }
      case '(': {
        SubexpressionType subexpr_type = CAPTURE;
        RegExpLookaround::Type lookaround_type = state->lookaround_type();
        bool is_named_capture = false;
        Advance();
        if (current() == '?') {
          switch (Next()) {
            case ':':
              subexpr_type = GROUPING;
              Advance(2);
              break;
            case '=':
              lookaround_type = RegExpLookaround::LOOKAHEAD;
              subexpr_type = POSITIVE_LOOKAROUND;
              Advance(2);
              break;
            case '!':
              lookaround_type = RegExpLookaround::LOOKAHEAD;
              subexpr_type = NEGATIVE_LOOKAROUND;
              Advance(2);
              break;
            case '<':
              Advance();
              if (FLAG_harmony_regexp_lookbehind) {
                if (Next() == '=') {
                  subexpr_type = POSITIVE_LOOKAROUND;
                  lookaround_type = RegExpLookaround::LOOKBEHIND;
                  Advance(2);
                  break;
                } else if (Next() == '!') {
                  subexpr_type = NEGATIVE_LOOKAROUND;
                  lookaround_type = RegExpLookaround::LOOKBEHIND;
                  Advance(2);
                  break;
                }
              }
              if (FLAG_harmony_regexp_named_captures) {
                has_named_captures_ = true;
                is_named_capture = true;
                Advance();
                break;
              }
            // Fall through.
            default:
              return ReportError(CStrVector("Invalid group"));
          }
        }

        const ZoneVector<uc16>* capture_name = nullptr;
        if (subexpr_type == CAPTURE) {
          if (captures_started_ >= kMaxCaptures) {
            return ReportError(CStrVector("Too many captures"));
          }
          captures_started_++;

          if (is_named_capture) {
            capture_name = ParseCaptureGroupName(CHECK_FAILED);
          }
        }
        // Store current state and begin new disjunction parsing.
        state = new (zone()) RegExpParserState(
            state, subexpr_type, lookaround_type, captures_started_,
            capture_name, ignore_case(), unicode(), zone());
        builder = state->builder();
        continue;
      }
      case '[': {
        RegExpTree* cc = ParseCharacterClass(CHECK_FAILED);
        builder->AddCharacterClass(cc->AsCharacterClass());
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
            CharacterRange::AddClassEscape(c, ranges,
                                           unicode() && ignore_case(), zone());
            RegExpCharacterClass* cc =
                new (zone()) RegExpCharacterClass(ranges);
            builder->AddCharacterClass(cc);
            break;
          }
          case 'p':
          case 'P': {
            uc32 p = Next();
            Advance(2);
            if (unicode()) {
              if (FLAG_harmony_regexp_property) {
                ZoneList<CharacterRange>* ranges =
                    new (zone()) ZoneList<CharacterRange>(2, zone());
                if (!ParsePropertyClass(ranges, p == 'P')) {
                  return ReportError(CStrVector("Invalid property name"));
                }
                RegExpCharacterClass* cc =
                    new (zone()) RegExpCharacterClass(ranges);
                builder->AddCharacterClass(cc);
              } else {
                // With /u, no identity escapes except for syntax characters
                // are allowed. Otherwise, all identity escapes are allowed.
                return ReportError(CStrVector("Invalid escape"));
              }
            } else {
              builder->AddCharacter(p);
            }
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
            bool is_backref = ParseBackReferenceIndex(&index CHECK_FAILED);
            if (is_backref) {
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
            // With /u, no identity escapes except for syntax characters
            // are allowed. Otherwise, all identity escapes are allowed.
            if (unicode()) {
              return ReportError(CStrVector("Invalid escape"));
            }
            uc32 first_digit = Next();
            if (first_digit == '8' || first_digit == '9') {
              builder->AddCharacter(first_digit);
              Advance(2);
              break;
            }
          }
          // Fall through.
          case '0': {
            Advance();
            if (unicode() && Next() >= '0' && Next() <= '9') {
              // With /u, decimal escape with leading 0 are not parsed as octal.
              return ReportError(CStrVector("Invalid decimal escape"));
            }
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
              // Read the backslash as a literal character instead of as
              // starting an escape.
              // ES#prod-annexB-ExtendedPatternCharacter
              if (unicode()) {
                // With /u, invalid escapes are not treated as identity escapes.
                return ReportError(CStrVector("Invalid unicode escape"));
              }
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
            } else if (!unicode()) {
              builder->AddCharacter('x');
            } else {
              // With /u, invalid escapes are not treated as identity escapes.
              return ReportError(CStrVector("Invalid escape"));
            }
            break;
          }
          case 'u': {
            Advance(2);
            uc32 value;
            if (ParseUnicodeEscape(&value)) {
              builder->AddEscapedUnicodeCharacter(value);
            } else if (!unicode()) {
              builder->AddCharacter('u');
            } else {
              // With /u, invalid escapes are not treated as identity escapes.
              return ReportError(CStrVector("Invalid unicode escape"));
            }
            break;
          }
          case 'k':
            // Either an identity escape or a named back-reference.  The two
            // interpretations are mutually exclusive: '\k' is interpreted as
            // an identity escape for non-unicode patterns without named
            // capture groups, and as the beginning of a named back-reference
            // in all other cases.
            if (FLAG_harmony_regexp_named_captures &&
                (unicode() || HasNamedCaptures())) {
              Advance(2);
              ParseNamedBackReference(builder, state CHECK_FAILED);
              break;
            }
          // Fall through.
          default:
            Advance();
            // With /u, no identity escapes except for syntax characters
            // are allowed. Otherwise, all identity escapes are allowed.
            if (!unicode() || IsSyntaxCharacterOrSlash(current())) {
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
        bool parsed = ParseIntervalQuantifier(&dummy, &dummy CHECK_FAILED);
        if (parsed) return ReportError(CStrVector("Nothing to repeat"));
        // Fall through.
      }
      case '}':
      case ']':
        if (unicode()) {
          return ReportError(CStrVector("Lone quantifier brackets"));
        }
      // Fall through.
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
            return ReportError(
                CStrVector("numbers out of order in {} quantifier"));
          }
          break;
        } else if (unicode()) {
          // With /u, incomplete quantifiers are not allowed.
          return ReportError(CStrVector("Incomplete quantifier"));
        }
        continue;
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
    if (!builder->AddQuantifierToAtom(min, max, quantifier_type)) {
      return ReportError(CStrVector("Invalid quantifier"));
    }
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
  DCHECK(!is_scanned_for_captures_);
  const int saved_position = position();
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
        if (current() == '?') {
          // At this point we could be in
          // * a non-capturing group '(:',
          // * a lookbehind assertion '(?<=' '(?<!'
          // * or a named capture '(?<'.
          //
          // Of these, only named captures are capturing groups.
          if (!FLAG_harmony_regexp_named_captures) break;

          Advance();
          if (current() != '<') break;

          if (FLAG_harmony_regexp_lookbehind) {
            Advance();
            if (current() == '=' || current() == '!') break;
          }

          // Found a possible named capture. It could turn out to be a syntax
          // error (e.g. an unterminated or invalid name), but that distinction
          // does not matter for our purposes.
          has_named_captures_ = true;
        }
        capture_count++;
        break;
    }
  }
  capture_count_ = capture_count;
  is_scanned_for_captures_ = true;
  Reset(saved_position);
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
    if (!is_scanned_for_captures_) ScanForCaptures();
    if (value > capture_count_) {
      Reset(start);
      return false;
    }
  }
  *index_out = value;
  return true;
}

static void push_code_unit(ZoneVector<uc16>* v, uint32_t code_unit) {
  if (code_unit <= unibrow::Utf16::kMaxNonSurrogateCharCode) {
    v->push_back(code_unit);
  } else {
    v->push_back(unibrow::Utf16::LeadSurrogate(code_unit));
    v->push_back(unibrow::Utf16::TrailSurrogate(code_unit));
  }
}

const ZoneVector<uc16>* RegExpParser::ParseCaptureGroupName() {
  DCHECK(FLAG_harmony_regexp_named_captures);

  ZoneVector<uc16>* name =
      new (zone()->New(sizeof(ZoneVector<uc16>))) ZoneVector<uc16>(zone());

  bool at_start = true;
  while (true) {
    uc32 c = current();
    Advance();

    // Convert unicode escapes.
    if (c == '\\' && current() == 'u') {
      Advance();
      if (!ParseUnicodeEscape(&c)) {
        ReportError(CStrVector("Invalid Unicode escape sequence"));
        return nullptr;
      }
    }

    // The backslash char is misclassified as both ID_Start and ID_Continue.
    if (c == '\\') {
      ReportError(CStrVector("Invalid capture group name"));
      return nullptr;
    }

    if (at_start) {
      if (!IdentifierStart::Is(c)) {
        ReportError(CStrVector("Invalid capture group name"));
        return nullptr;
      }
      push_code_unit(name, c);
      at_start = false;
    } else {
      if (c == '>') {
        break;
      } else if (IdentifierPart::Is(c)) {
        push_code_unit(name, c);
      } else {
        ReportError(CStrVector("Invalid capture group name"));
        return nullptr;
      }
    }
  }

  return name;
}

bool RegExpParser::CreateNamedCaptureAtIndex(const ZoneVector<uc16>* name,
                                             int index) {
  DCHECK(FLAG_harmony_regexp_named_captures);
  DCHECK(0 < index && index <= captures_started_);
  DCHECK_NOT_NULL(name);

  if (named_captures_ == nullptr) {
    named_captures_ = new (zone()) ZoneList<RegExpCapture*>(1, zone());
  } else {
    // Check for duplicates and bail if we find any.
    // TODO(jgruber): O(n^2).
    for (const auto& named_capture : *named_captures_) {
      if (*named_capture->name() == *name) {
        ReportError(CStrVector("Duplicate capture group name"));
        return false;
      }
    }
  }

  RegExpCapture* capture = GetCapture(index);
  DCHECK(capture->name() == nullptr);

  capture->set_name(name);
  named_captures_->Add(capture, zone());

  return true;
}

bool RegExpParser::ParseNamedBackReference(RegExpBuilder* builder,
                                           RegExpParserState* state) {
  // The parser is assumed to be on the '<' in \k<name>.
  if (current() != '<') {
    ReportError(CStrVector("Invalid named reference"));
    return false;
  }

  Advance();
  const ZoneVector<uc16>* name = ParseCaptureGroupName();
  if (name == nullptr) {
    return false;
  }

  if (state->IsInsideCaptureGroup(name)) {
    builder->AddEmpty();
  } else {
    RegExpBackReference* atom = new (zone()) RegExpBackReference();
    atom->set_name(name);

    builder->AddAtom(atom);

    if (named_back_references_ == nullptr) {
      named_back_references_ =
          new (zone()) ZoneList<RegExpBackReference*>(1, zone());
    }
    named_back_references_->Add(atom, zone());
  }

  return true;
}

void RegExpParser::PatchNamedBackReferences() {
  if (named_back_references_ == nullptr) return;

  if (named_captures_ == nullptr) {
    ReportError(CStrVector("Invalid named capture referenced"));
    return;
  }

  // Look up and patch the actual capture for each named back reference.
  // TODO(jgruber): O(n^2), optimize if necessary.

  for (int i = 0; i < named_back_references_->length(); i++) {
    RegExpBackReference* ref = named_back_references_->at(i);

    int index = -1;
    for (const auto& capture : *named_captures_) {
      if (*capture->name() == *ref->name()) {
        index = capture->index();
        break;
      }
    }

    if (index == -1) {
      ReportError(CStrVector("Invalid named capture referenced"));
      return;
    }

    ref->set_capture(GetCapture(index));
  }
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

Handle<FixedArray> RegExpParser::CreateCaptureNameMap() {
  if (named_captures_ == nullptr || named_captures_->is_empty())
    return Handle<FixedArray>();

  Factory* factory = isolate()->factory();

  int len = named_captures_->length() * 2;
  Handle<FixedArray> array = factory->NewFixedArray(len);

  for (int i = 0; i < named_captures_->length(); i++) {
    RegExpCapture* capture = named_captures_->at(i);
    MaybeHandle<String> name = factory->NewStringFromTwoByte(capture->name());
    array->set(i * 2, *name.ToHandleChecked());
    array->set(i * 2 + 1, Smi::FromInt(capture->index()));
  }

  return array;
}

bool RegExpParser::HasNamedCaptures() {
  if (has_named_captures_ || is_scanned_for_captures_) {
    return has_named_captures_;
  }

  ScanForCaptures();
  DCHECK(is_scanned_for_captures_);
  return has_named_captures_;
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

bool RegExpParser::RegExpParserState::IsInsideCaptureGroup(
    const ZoneVector<uc16>* name) {
  DCHECK_NOT_NULL(name);
  for (RegExpParserState* s = this; s != NULL; s = s->previous_state()) {
    if (s->capture_name() == nullptr) continue;
    if (*s->capture_name() == *name) return true;
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
  // ES#prod-annexB-LegacyOctalEscapeSequence
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

// This parses RegExpUnicodeEscapeSequence as described in ECMA262.
bool RegExpParser::ParseUnicodeEscape(uc32* value) {
  // Accept both \uxxxx and \u{xxxxxx} (if harmony unicode escapes are
  // allowed). In the latter case, the number of hex digits between { } is
  // arbitrary. \ and u have already been read.
  if (current() == '{' && unicode()) {
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
  bool result = ParseHexEscape(4, value);
  if (result && unicode() && unibrow::Utf16::IsLeadSurrogate(*value) &&
      current() == '\\') {
    // Attempt to read trail surrogate.
    int start = position();
    if (Next() == 'u') {
      Advance(2);
      uc32 trail;
      if (ParseHexEscape(4, &trail) &&
          unibrow::Utf16::IsTrailSurrogate(trail)) {
        *value = unibrow::Utf16::CombineSurrogatePair(static_cast<uc16>(*value),
                                                      static_cast<uc16>(trail));
        return true;
      }
    }
    Reset(start);
  }
  return result;
}

#ifdef V8_INTL_SUPPORT

namespace {

bool IsExactPropertyAlias(const char* property_name, UProperty property) {
  const char* short_name = u_getPropertyName(property, U_SHORT_PROPERTY_NAME);
  if (short_name != NULL && strcmp(property_name, short_name) == 0) return true;
  for (int i = 0;; i++) {
    const char* long_name = u_getPropertyName(
        property, static_cast<UPropertyNameChoice>(U_LONG_PROPERTY_NAME + i));
    if (long_name == NULL) break;
    if (strcmp(property_name, long_name) == 0) return true;
  }
  return false;
}

bool IsExactPropertyValueAlias(const char* property_value_name,
                               UProperty property, int32_t property_value) {
  const char* short_name =
      u_getPropertyValueName(property, property_value, U_SHORT_PROPERTY_NAME);
  if (short_name != NULL && strcmp(property_value_name, short_name) == 0) {
    return true;
  }
  for (int i = 0;; i++) {
    const char* long_name = u_getPropertyValueName(
        property, property_value,
        static_cast<UPropertyNameChoice>(U_LONG_PROPERTY_NAME + i));
    if (long_name == NULL) break;
    if (strcmp(property_value_name, long_name) == 0) return true;
  }
  return false;
}

bool LookupPropertyValueName(UProperty property,
                             const char* property_value_name, bool negate,
                             ZoneList<CharacterRange>* result, Zone* zone) {
  UProperty property_for_lookup = property;
  if (property_for_lookup == UCHAR_SCRIPT_EXTENSIONS) {
    // For the property Script_Extensions, we have to do the property value
    // name lookup as if the property is Script.
    property_for_lookup = UCHAR_SCRIPT;
  }
  int32_t property_value =
      u_getPropertyValueEnum(property_for_lookup, property_value_name);
  if (property_value == UCHAR_INVALID_CODE) return false;

  // We require the property name to match exactly to one of the property value
  // aliases. However, u_getPropertyValueEnum uses loose matching.
  if (!IsExactPropertyValueAlias(property_value_name, property_for_lookup,
                                 property_value)) {
    return false;
  }

  UErrorCode ec = U_ZERO_ERROR;
  icu::UnicodeSet set;
  set.applyIntPropertyValue(property, property_value, ec);
  bool success = ec == U_ZERO_ERROR && !set.isEmpty();

  if (success) {
    set.removeAllStrings();
    if (negate) set.complement();
    for (int i = 0; i < set.getRangeCount(); i++) {
      result->Add(
          CharacterRange::Range(set.getRangeStart(i), set.getRangeEnd(i)),
          zone);
    }
  }
  return success;
}

template <size_t N>
inline bool NameEquals(const char* name, const char (&literal)[N]) {
  return strncmp(name, literal, N + 1) == 0;
}

bool LookupSpecialPropertyValueName(const char* name,
                                    ZoneList<CharacterRange>* result,
                                    bool negate, Zone* zone) {
  if (NameEquals(name, "Any")) {
    if (!negate) result->Add(CharacterRange::Everything(), zone);
  } else if (NameEquals(name, "ASCII")) {
    result->Add(negate ? CharacterRange::Range(0x80, String::kMaxCodePoint)
                       : CharacterRange::Range(0x0, 0x7f),
                zone);
  } else if (NameEquals(name, "Assigned")) {
    return LookupPropertyValueName(UCHAR_GENERAL_CATEGORY, "Unassigned",
                                   !negate, result, zone);
  } else {
    return false;
  }
  return true;
}

// Explicitly whitelist supported binary properties. The spec forbids supporting
// properties outside of this set to ensure interoperability.
bool IsSupportedBinaryProperty(UProperty property) {
  switch (property) {
    case UCHAR_ALPHABETIC:
    // 'Any' is not supported by ICU. See LookupSpecialPropertyValueName.
    // 'ASCII' is not supported by ICU. See LookupSpecialPropertyValueName.
    case UCHAR_ASCII_HEX_DIGIT:
    // 'Assigned' is not supported by ICU. See LookupSpecialPropertyValueName.
    case UCHAR_BIDI_CONTROL:
    case UCHAR_BIDI_MIRRORED:
    case UCHAR_CASE_IGNORABLE:
    case UCHAR_CASED:
    case UCHAR_CHANGES_WHEN_CASEFOLDED:
    case UCHAR_CHANGES_WHEN_CASEMAPPED:
    case UCHAR_CHANGES_WHEN_LOWERCASED:
    case UCHAR_CHANGES_WHEN_NFKC_CASEFOLDED:
    case UCHAR_CHANGES_WHEN_TITLECASED:
    case UCHAR_CHANGES_WHEN_UPPERCASED:
    case UCHAR_DASH:
    case UCHAR_DEFAULT_IGNORABLE_CODE_POINT:
    case UCHAR_DEPRECATED:
    case UCHAR_DIACRITIC:
    case UCHAR_EMOJI:
    // TODO(yangguo): Uncomment this once we upgrade to ICU 60.
    //                See https://ssl.icu-project.org/trac/ticket/13062
    // case UCHAR_EMOJI_COMPONENT:
    case UCHAR_EMOJI_MODIFIER_BASE:
    case UCHAR_EMOJI_MODIFIER:
    case UCHAR_EMOJI_PRESENTATION:
    case UCHAR_EXTENDER:
    case UCHAR_GRAPHEME_BASE:
    case UCHAR_GRAPHEME_EXTEND:
    case UCHAR_HEX_DIGIT:
    case UCHAR_ID_CONTINUE:
    case UCHAR_ID_START:
    case UCHAR_IDEOGRAPHIC:
    case UCHAR_IDS_BINARY_OPERATOR:
    case UCHAR_IDS_TRINARY_OPERATOR:
    case UCHAR_JOIN_CONTROL:
    case UCHAR_LOGICAL_ORDER_EXCEPTION:
    case UCHAR_LOWERCASE:
    case UCHAR_MATH:
    case UCHAR_NONCHARACTER_CODE_POINT:
    case UCHAR_PATTERN_SYNTAX:
    case UCHAR_PATTERN_WHITE_SPACE:
    case UCHAR_QUOTATION_MARK:
    case UCHAR_RADICAL:
    case UCHAR_S_TERM:
    case UCHAR_SOFT_DOTTED:
    case UCHAR_TERMINAL_PUNCTUATION:
    case UCHAR_UNIFIED_IDEOGRAPH:
    case UCHAR_UPPERCASE:
    case UCHAR_VARIATION_SELECTOR:
    case UCHAR_WHITE_SPACE:
    case UCHAR_XID_CONTINUE:
    case UCHAR_XID_START:
      return true;
    default:
      break;
  }
  return false;
}

}  // anonymous namespace

bool RegExpParser::ParsePropertyClass(ZoneList<CharacterRange>* result,
                                      bool negate) {
  // Parse the property class as follows:
  // - In \p{name}, 'name' is interpreted
  //   - either as a general category property value name.
  //   - or as a binary property name.
  // - In \p{name=value}, 'name' is interpreted as an enumerated property name,
  //   and 'value' is interpreted as one of the available property value names.
  // - Aliases in PropertyAlias.txt and PropertyValueAlias.txt can be used.
  // - Loose matching is not applied.
  List<char> first_part;
  List<char> second_part;
  if (current() == '{') {
    // Parse \p{[PropertyName=]PropertyNameValue}
    for (Advance(); current() != '}' && current() != '='; Advance()) {
      if (!has_next()) return false;
      first_part.Add(static_cast<char>(current()));
    }
    if (current() == '=') {
      for (Advance(); current() != '}'; Advance()) {
        if (!has_next()) return false;
        second_part.Add(static_cast<char>(current()));
      }
      second_part.Add(0);  // null-terminate string.
    }
  } else {
    return false;
  }
  Advance();
  first_part.Add(0);  // null-terminate string.

  if (second_part.is_empty()) {
    // First attempt to interpret as general category property value name.
    const char* name = first_part.ToConstVector().start();
    if (LookupPropertyValueName(UCHAR_GENERAL_CATEGORY_MASK, name, negate,
                                result, zone())) {
      return true;
    }
    // Interpret "Any", "ASCII", and "Assigned".
    if (LookupSpecialPropertyValueName(name, result, negate, zone())) {
      return true;
    }
    // Then attempt to interpret as binary property name with value name 'Y'.
    UProperty property = u_getPropertyEnum(name);
    if (!IsSupportedBinaryProperty(property)) return false;
    if (!IsExactPropertyAlias(name, property)) return false;
    return LookupPropertyValueName(property, negate ? "N" : "Y", false, result,
                                   zone());
  } else {
    // Both property name and value name are specified. Attempt to interpret
    // the property name as enumerated property.
    const char* property_name = first_part.ToConstVector().start();
    const char* value_name = second_part.ToConstVector().start();
    UProperty property = u_getPropertyEnum(property_name);
    if (!IsExactPropertyAlias(property_name, property)) return false;
    if (property == UCHAR_GENERAL_CATEGORY) {
      // We want to allow aggregate value names such as "Letter".
      property = UCHAR_GENERAL_CATEGORY_MASK;
    } else if (property != UCHAR_SCRIPT &&
               property != UCHAR_SCRIPT_EXTENSIONS) {
      return false;
    }
    return LookupPropertyValueName(property, value_name, negate, result,
                                   zone());
  }
}

#else  // V8_INTL_SUPPORT

bool RegExpParser::ParsePropertyClass(ZoneList<CharacterRange>* result,
                                      bool negate) {
  return false;
}

#endif  // V8_INTL_SUPPORT

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
      // Inside a character class, we also accept digits and underscore as
      // control characters, unless with /u. See Annex B:
      // ES#prod-annexB-ClassControlLetter
      if (letter >= 'A' && letter <= 'Z') {
        Advance(2);
        // Control letters mapped to ASCII control characters in the range
        // 0x00-0x1f.
        return controlLetter & 0x1f;
      }
      if (unicode()) {
        // With /u, invalid escapes are not treated as identity escapes.
        ReportError(CStrVector("Invalid class escape"));
        return 0;
      }
      if ((controlLetter >= '0' && controlLetter <= '9') ||
          controlLetter == '_') {
        Advance(2);
        return controlLetter & 0x1f;
      }
      // We match JSC in reading the backslash as a literal
      // character instead of as starting an escape.
      // TODO(v8:6201): Not yet covered by the spec.
      return '\\';
    }
    case '0':
      // With /u, \0 is interpreted as NUL if not followed by another digit.
      if (unicode() && !(Next() >= '0' && Next() <= '9')) {
        Advance();
        return 0;
      }
    // Fall through.
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
      // ES#prod-annexB-LegacyOctalEscapeSequence
      if (unicode()) {
        // With /u, decimal escape is not interpreted as octal character code.
        ReportError(CStrVector("Invalid class escape"));
        return 0;
      }
      return ParseOctalLiteral();
    case 'x': {
      Advance();
      uc32 value;
      if (ParseHexEscape(2, &value)) return value;
      if (unicode()) {
        // With /u, invalid escapes are not treated as identity escapes.
        ReportError(CStrVector("Invalid escape"));
        return 0;
      }
      // If \x is not followed by a two-digit hexadecimal, treat it
      // as an identity escape.
      return 'x';
    }
    case 'u': {
      Advance();
      uc32 value;
      if (ParseUnicodeEscape(&value)) return value;
      if (unicode()) {
        // With /u, invalid escapes are not treated as identity escapes.
        ReportError(CStrVector("Invalid unicode escape"));
        return 0;
      }
      // If \u is not followed by a two-digit hexadecimal, treat it
      // as an identity escape.
      return 'u';
    }
    default: {
      uc32 result = current();
      // With /u, no identity escapes except for syntax characters and '-' are
      // allowed. Otherwise, all identity escapes are allowed.
      if (!unicode() || IsSyntaxCharacterOrSlash(result) || result == '-') {
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
        first = ParseClassCharacterEscape(CHECK_FAILED);
    }
  } else {
    Advance();
  }

  return CharacterRange::Singleton(first);
}

static const uc16 kNoCharClass = 0;

// Adds range or pre-defined character class to character ranges.
// If char_class is not kInvalidClass, it's interpreted as a class
// escape (i.e., 's' means whitespace, from '\s').
static inline void AddRangeOrEscape(ZoneList<CharacterRange>* ranges,
                                    uc16 char_class, CharacterRange range,
                                    bool add_unicode_case_equivalents,
                                    Zone* zone) {
  if (char_class != kNoCharClass) {
    CharacterRange::AddClassEscape(char_class, ranges,
                                   add_unicode_case_equivalents, zone);
  } else {
    ranges->Add(range, zone);
  }
}

bool RegExpParser::ParseClassProperty(ZoneList<CharacterRange>* ranges) {
  if (!FLAG_harmony_regexp_property) return false;
  if (!unicode()) return false;
  if (current() != '\\') return false;
  uc32 next = Next();
  bool parse_success = false;
  if (next == 'p') {
    Advance(2);
    parse_success = ParsePropertyClass(ranges, false);
  } else if (next == 'P') {
    Advance(2);
    parse_success = ParsePropertyClass(ranges, true);
  } else {
    return false;
  }
  if (!parse_success)
    ReportError(CStrVector("Invalid property name in character class"));
  return parse_success;
}

RegExpTree* RegExpParser::ParseCharacterClass() {
  static const char* kUnterminated = "Unterminated character class";
  static const char* kRangeInvalid = "Invalid character class";
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
  bool add_unicode_case_equivalents = unicode() && ignore_case();
  while (has_more() && current() != ']') {
    bool parsed_property = ParseClassProperty(ranges CHECK_FAILED);
    if (parsed_property) continue;
    uc16 char_class = kNoCharClass;
    CharacterRange first = ParseClassAtom(&char_class CHECK_FAILED);
    if (current() == '-') {
      Advance();
      if (current() == kEndMarker) {
        // If we reach the end we break out of the loop and let the
        // following code report an error.
        break;
      } else if (current() == ']') {
        AddRangeOrEscape(ranges, char_class, first,
                         add_unicode_case_equivalents, zone());
        ranges->Add(CharacterRange::Singleton('-'), zone());
        break;
      }
      uc16 char_class_2 = kNoCharClass;
      CharacterRange next = ParseClassAtom(&char_class_2 CHECK_FAILED);
      if (char_class != kNoCharClass || char_class_2 != kNoCharClass) {
        // Either end is an escaped character class. Treat the '-' verbatim.
        if (unicode()) {
          // ES2015 21.2.2.15.1 step 1.
          return ReportError(CStrVector(kRangeInvalid));
        }
        AddRangeOrEscape(ranges, char_class, first,
                         add_unicode_case_equivalents, zone());
        ranges->Add(CharacterRange::Singleton('-'), zone());
        AddRangeOrEscape(ranges, char_class_2, next,
                         add_unicode_case_equivalents, zone());
        continue;
      }
      // ES2015 21.2.2.15.1 step 6.
      if (first.from() > next.to()) {
        return ReportError(CStrVector(kRangeOutOfOrder));
      }
      ranges->Add(CharacterRange::Range(first.from(), next.to()), zone());
    } else {
      AddRangeOrEscape(ranges, char_class, first, add_unicode_case_equivalents,
                       zone());
    }
  }
  if (!has_more()) {
    return ReportError(CStrVector(kUnterminated));
  }
  Advance();
  if (ranges->length() == 0) {
    ranges->Add(CharacterRange::Everything(), zone());
    is_negated = !is_negated;
  }
  RegExpCharacterClass::Flags flags;
  if (is_negated) flags = RegExpCharacterClass::NEGATED;
  return new (zone()) RegExpCharacterClass(ranges, flags);
}


#undef CHECK_FAILED


bool RegExpParser::ParseRegExp(Isolate* isolate, Zone* zone,
                               FlatStringReader* input, JSRegExp::Flags flags,
                               RegExpCompileData* result) {
  DCHECK(result != NULL);
  RegExpParser parser(input, &result->error, flags, isolate, zone);
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
    result->capture_name_map = parser.CreateCaptureNameMap();
    result->capture_count = capture_count;
  }
  return !parser.failed();
}

RegExpBuilder::RegExpBuilder(Zone* zone, bool ignore_case, bool unicode)
    : zone_(zone),
      pending_empty_(false),
      ignore_case_(ignore_case),
      unicode_(unicode),
      characters_(NULL),
      pending_surrogate_(kNoPendingSurrogate),
      terms_(),
      alternatives_()
#ifdef DEBUG
      ,
      last_added_(ADD_NONE)
#endif
{
}


void RegExpBuilder::AddLeadSurrogate(uc16 lead_surrogate) {
  DCHECK(unibrow::Utf16::IsLeadSurrogate(lead_surrogate));
  FlushPendingSurrogate();
  // Hold onto the lead surrogate, waiting for a trail surrogate to follow.
  pending_surrogate_ = lead_surrogate;
}


void RegExpBuilder::AddTrailSurrogate(uc16 trail_surrogate) {
  DCHECK(unibrow::Utf16::IsTrailSurrogate(trail_surrogate));
  if (pending_surrogate_ != kNoPendingSurrogate) {
    uc16 lead_surrogate = pending_surrogate_;
    pending_surrogate_ = kNoPendingSurrogate;
    DCHECK(unibrow::Utf16::IsLeadSurrogate(lead_surrogate));
    uc32 combined =
        unibrow::Utf16::CombineSurrogatePair(lead_surrogate, trail_surrogate);
    if (NeedsDesugaringForIgnoreCase(combined)) {
      AddCharacterClassForDesugaring(combined);
    } else {
      ZoneList<uc16> surrogate_pair(2, zone());
      surrogate_pair.Add(lead_surrogate, zone());
      surrogate_pair.Add(trail_surrogate, zone());
      RegExpAtom* atom =
          new (zone()) RegExpAtom(surrogate_pair.ToConstVector());
      AddAtom(atom);
    }
  } else {
    pending_surrogate_ = trail_surrogate;
    FlushPendingSurrogate();
  }
}


void RegExpBuilder::FlushPendingSurrogate() {
  if (pending_surrogate_ != kNoPendingSurrogate) {
    DCHECK(unicode());
    uc32 c = pending_surrogate_;
    pending_surrogate_ = kNoPendingSurrogate;
    AddCharacterClassForDesugaring(c);
  }
}


void RegExpBuilder::FlushCharacters() {
  FlushPendingSurrogate();
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
  FlushPendingSurrogate();
  pending_empty_ = false;
  if (NeedsDesugaringForIgnoreCase(c)) {
    AddCharacterClassForDesugaring(c);
  } else {
    if (characters_ == NULL) {
      characters_ = new (zone()) ZoneList<uc16>(4, zone());
    }
    characters_->Add(c, zone());
    LAST(ADD_CHAR);
  }
}


void RegExpBuilder::AddUnicodeCharacter(uc32 c) {
  if (c > static_cast<uc32>(unibrow::Utf16::kMaxNonSurrogateCharCode)) {
    DCHECK(unicode());
    AddLeadSurrogate(unibrow::Utf16::LeadSurrogate(c));
    AddTrailSurrogate(unibrow::Utf16::TrailSurrogate(c));
  } else if (unicode() && unibrow::Utf16::IsLeadSurrogate(c)) {
    AddLeadSurrogate(c);
  } else if (unicode() && unibrow::Utf16::IsTrailSurrogate(c)) {
    AddTrailSurrogate(c);
  } else {
    AddCharacter(static_cast<uc16>(c));
  }
}

void RegExpBuilder::AddEscapedUnicodeCharacter(uc32 character) {
  // A lead or trail surrogate parsed via escape sequence will not
  // pair up with any preceding lead or following trail surrogate.
  FlushPendingSurrogate();
  AddUnicodeCharacter(character);
  FlushPendingSurrogate();
}

void RegExpBuilder::AddEmpty() { pending_empty_ = true; }


void RegExpBuilder::AddCharacterClass(RegExpCharacterClass* cc) {
  if (NeedsDesugaringForUnicode(cc)) {
    // With /u, character class needs to be desugared, so it
    // must be a standalone term instead of being part of a RegExpText.
    AddTerm(cc);
  } else {
    AddAtom(cc);
  }
}

void RegExpBuilder::AddCharacterClassForDesugaring(uc32 c) {
  AddTerm(new (zone()) RegExpCharacterClass(
      CharacterRange::List(zone(), CharacterRange::Singleton(c))));
}


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


void RegExpBuilder::AddTerm(RegExpTree* term) {
  FlushText();
  terms_.Add(term, zone());
  LAST(ADD_ATOM);
}


void RegExpBuilder::AddAssertion(RegExpTree* assert) {
  FlushText();
  if (terms_.length() > 0 && terms_.last()->IsAssertion()) {
    // Omit repeated assertions of the same type.
    RegExpAssertion* last = terms_.last()->AsAssertion();
    RegExpAssertion* next = assert->AsAssertion();
    if (last->assertion_type() == next->assertion_type()) return;
  }
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


bool RegExpBuilder::NeedsDesugaringForUnicode(RegExpCharacterClass* cc) {
  if (!unicode()) return false;
  // TODO(yangguo): we could be smarter than this. Case-insensitivity does not
  // necessarily mean that we need to desugar. It's probably nicer to have a
  // separate pass to figure out unicode desugarings.
  if (ignore_case()) return true;
  ZoneList<CharacterRange>* ranges = cc->ranges(zone());
  CharacterRange::Canonicalize(ranges);
  for (int i = ranges->length() - 1; i >= 0; i--) {
    uc32 from = ranges->at(i).from();
    uc32 to = ranges->at(i).to();
    // Check for non-BMP characters.
    if (to >= kNonBmpStart) return true;
    // Check for lone surrogates.
    if (from <= kTrailSurrogateEnd && to >= kLeadSurrogateStart) return true;
  }
  return false;
}


bool RegExpBuilder::NeedsDesugaringForIgnoreCase(uc32 c) {
#ifdef V8_INTL_SUPPORT
  if (unicode() && ignore_case()) {
    icu::UnicodeSet set(c, c);
    set.closeOver(USET_CASE_INSENSITIVE);
    set.removeAllStrings();
    return set.size() > 1;
  }
  // In the case where ICU is not included, we act as if the unicode flag is
  // not set, and do not desugar.
#endif  // V8_INTL_SUPPORT
  return false;
}


RegExpTree* RegExpBuilder::ToRegExp() {
  FlushTerms();
  int num_alternatives = alternatives_.length();
  if (num_alternatives == 0) return new (zone()) RegExpEmpty();
  if (num_alternatives == 1) return alternatives_.last();
  return new (zone()) RegExpDisjunction(alternatives_.GetList(zone()));
}

bool RegExpBuilder::AddQuantifierToAtom(
    int min, int max, RegExpQuantifier::QuantifierType quantifier_type) {
  FlushPendingSurrogate();
  if (pending_empty_) {
    pending_empty_ = false;
    return true;
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
    // With /u, lookarounds are not quantifiable.
    if (unicode() && atom->IsLookaround()) return false;
    if (atom->max_match() == 0) {
      // Guaranteed to only match an empty string.
      LAST(ADD_TERM);
      if (min == 0) {
        return true;
      }
      terms_.Add(atom, zone());
      return true;
    }
  } else {
    // Only call immediately after adding an atom or character!
    UNREACHABLE();
    return false;
  }
  terms_.Add(new (zone()) RegExpQuantifier(min, max, quantifier_type, atom),
             zone());
  LAST(ADD_TERM);
  return true;
}

}  // namespace internal
}  // namespace v8

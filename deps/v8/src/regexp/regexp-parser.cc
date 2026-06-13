// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-parser.h"

#include "src/execution/isolate.h"
#include "src/objects/string-inl.h"
#include "src/regexp/regexp-ast-printer.h"
#include "src/regexp/regexp-ast.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/regexp/regexp.h"
#include "src/strings/char-predicates-inl.h"
#include "src/utils/ostreams.h"
#include "src/utils/utils.h"
#include "src/zone/zone-allocator.h"
#include "src/zone/zone-list-inl.h"

#ifdef V8_INTL_SUPPORT
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/usetiter.h"
#include "unicode/utf16.h"  // For U16_NEXT
#endif                      // V8_INTL_SUPPORT

namespace v8 {
namespace internal {
namespace regexp {

namespace {

// Whether we're currently inside the ClassEscape production
// (tc39.es/ecma262/#prod-annexB-CharacterEscape).
enum class InClassEscapeState {
  kInClass,
  kNotInClass,
};

// The production used to derive ClassSetOperand.
enum class ClassSetOperandType {
  kClassSetCharacter,
  kClassStringDisjunction,
  kNestedClass,
  kCharacterClassEscape,  // \ CharacterClassEscape is a special nested class,
                          // as we can fold it directly into another range.
  kClassSetRange
};

class TextBuilder {
 public:
  using SmallTreeVector = SmallZoneVector<Tree*, 8>;

  TextBuilder(Zone* zone, SmallTreeVector* terms_storage, Flags flags)
      : zone_(zone), flags_(flags), terms_(terms_storage), text_(zone) {}
  void AddCharacter(base::uc16 character);
  void AddUnicodeCharacter(base::uc32 character);
  void AddEscapedUnicodeCharacter(base::uc32 character);
  void AddAtom(Tree* atom);
  void AddTerm(Tree* term);
  void AddClassRanges(ClassRanges* cc);
  void FlushPendingSurrogate();
  void FlushText();
  Tree* PopLastAtom();
  Tree* ToRegExp();

 private:
  static const base::uc16 kNoPendingSurrogate = 0;

  void AddLeadSurrogate(base::uc16 lead_surrogate);
  void AddTrailSurrogate(base::uc16 trail_surrogate);
  void FlushCharacters();
  bool NeedsDesugaringForUnicode(ClassRanges* cc);
  bool NeedsDesugaringForIgnoreCase(base::uc32 c);
  void AddClassRangesForDesugaring(base::uc32 c);
  bool ignore_case() const { return IsIgnoreCase(flags_); }
  bool IsUnicodeMode() const {
    // Either /v or /u enable UnicodeMode
    // https://tc39.es/ecma262/#sec-parsepattern
    return IsUnicode(flags_) || IsUnicodeSets(flags_);
  }
  Zone* zone() const { return zone_; }

  Zone* const zone_;
  const Flags flags_;
  ZoneList<base::uc16>* characters_ = nullptr;
  base::uc16 pending_surrogate_ = kNoPendingSurrogate;
  SmallTreeVector* terms_;
  SmallTreeVector text_;
};

void TextBuilder::AddLeadSurrogate(base::uc16 lead_surrogate) {
  DCHECK(unibrow::Utf16::IsLeadSurrogate(lead_surrogate));
  FlushPendingSurrogate();
  // Hold onto the lead surrogate, waiting for a trail surrogate to follow.
  pending_surrogate_ = lead_surrogate;
}

void TextBuilder::AddTrailSurrogate(base::uc16 trail_surrogate) {
  DCHECK(unibrow::Utf16::IsTrailSurrogate(trail_surrogate));
  if (pending_surrogate_ != kNoPendingSurrogate) {
    base::uc16 lead_surrogate = pending_surrogate_;
    pending_surrogate_ = kNoPendingSurrogate;
    DCHECK(unibrow::Utf16::IsLeadSurrogate(lead_surrogate));
    base::uc32 combined =
        unibrow::Utf16::CombineSurrogatePair(lead_surrogate, trail_surrogate);
    if (NeedsDesugaringForIgnoreCase(combined)) {
      AddClassRangesForDesugaring(combined);
    } else {
      ZoneList<base::uc16> surrogate_pair(2, zone());
      surrogate_pair.Add(lead_surrogate, zone());
      surrogate_pair.Add(trail_surrogate, zone());
      Atom* atom = zone()->New<Atom>(surrogate_pair.ToConstVector());
      AddAtom(atom);
    }
  } else {
    pending_surrogate_ = trail_surrogate;
    FlushPendingSurrogate();
  }
}

void TextBuilder::FlushPendingSurrogate() {
  if (pending_surrogate_ != kNoPendingSurrogate) {
    DCHECK(IsUnicodeMode());
    base::uc32 c = pending_surrogate_;
    pending_surrogate_ = kNoPendingSurrogate;
    AddClassRangesForDesugaring(c);
  }
}

void TextBuilder::FlushCharacters() {
  FlushPendingSurrogate();
  if (characters_ != nullptr) {
    Tree* atom = zone()->New<Atom>(characters_->ToConstVector());
    characters_ = nullptr;
    text_.emplace_back(atom);
  }
}

void TextBuilder::FlushText() {
  FlushCharacters();
  size_t num_text = text_.size();
  if (num_text == 0) {
    return;
  } else if (num_text == 1) {
    terms_->emplace_back(text_.back());
  } else {
    Text* text = zone()->New<Text>(zone());
    for (size_t i = 0; i < num_text; i++) {
      text_[i]->AppendToText(text, zone());
    }
    terms_->emplace_back(text);
  }
  text_.clear();
}

void TextBuilder::AddCharacter(base::uc16 c) {
  FlushPendingSurrogate();
  if (characters_ == nullptr) {
    characters_ = zone()->New<ZoneList<base::uc16>>(4, zone());
  }
  characters_->Add(c, zone());
}

void TextBuilder::AddUnicodeCharacter(base::uc32 c) {
  if (c > static_cast<base::uc32>(unibrow::Utf16::kMaxNonSurrogateCharCode)) {
    DCHECK(IsUnicodeMode());
    AddLeadSurrogate(unibrow::Utf16::LeadSurrogate(c));
    AddTrailSurrogate(unibrow::Utf16::TrailSurrogate(c));
  } else if (IsUnicodeMode() && unibrow::Utf16::IsLeadSurrogate(c)) {
    AddLeadSurrogate(c);
  } else if (IsUnicodeMode() && unibrow::Utf16::IsTrailSurrogate(c)) {
    AddTrailSurrogate(c);
  } else {
    AddCharacter(static_cast<base::uc16>(c));
  }
}

void TextBuilder::AddEscapedUnicodeCharacter(base::uc32 character) {
  // A lead or trail surrogate parsed via escape sequence will not
  // pair up with any preceding lead or following trail surrogate.
  FlushPendingSurrogate();
  AddUnicodeCharacter(character);
  FlushPendingSurrogate();
}

void TextBuilder::AddClassRanges(ClassRanges* cr) {
  if (NeedsDesugaringForUnicode(cr)) {
    // With /u or /v, character class needs to be desugared, so it
    // must be a standalone term instead of being part of a Text.
    AddTerm(cr);
  } else {
    AddAtom(cr);
  }
}

void TextBuilder::AddClassRangesForDesugaring(base::uc32 c) {
  AddTerm(zone()->New<ClassRanges>(
      zone(), CharacterRange::List(zone(), CharacterRange::Singleton(c))));
}

void TextBuilder::AddAtom(Tree* atom) {
  DCHECK(atom->IsTextElement());
  FlushCharacters();
  text_.emplace_back(atom);
}

void TextBuilder::AddTerm(Tree* term) {
  DCHECK(term->IsTextElement());
  FlushText();
  terms_->emplace_back(term);
}

bool TextBuilder::NeedsDesugaringForUnicode(ClassRanges* cc) {
  if (!IsUnicodeMode()) return false;
  // TODO(yangguo): we could be smarter than this. Case-insensitivity does not
  // necessarily mean that we need to desugar. It's probably nicer to have a
  // separate pass to figure out unicode desugarings.
  if (ignore_case()) return true;
  ZoneList<CharacterRange>* ranges = cc->ranges(zone());
  CharacterRange::Canonicalize(ranges);

  if (cc->is_negated()) {
    ZoneList<CharacterRange>* negated_ranges =
        zone()->New<ZoneList<CharacterRange>>(ranges->length(), zone());
    CharacterRange::Negate(ranges, negated_ranges, zone());
    ranges = negated_ranges;
  }

  for (int i = ranges->length() - 1; i >= 0; i--) {
    base::uc32 from = ranges->at(i).from();
    base::uc32 to = ranges->at(i).to();
    // Check for non-BMP characters.
    if (to >= kNonBmpStart) return true;
    // Check for lone surrogates.
    if (from <= kTrailSurrogateEnd && to >= kLeadSurrogateStart) return true;
  }
  return false;
}

// We only use this for characters made of surrogate pairs.  All other
// characters outside of character classes are made case independent in the
// code generation.
bool TextBuilder::NeedsDesugaringForIgnoreCase(base::uc32 c) {
#ifdef V8_INTL_SUPPORT
  if (IsUnicodeMode() && ignore_case()) {
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

Tree* TextBuilder::PopLastAtom() {
  FlushPendingSurrogate();
  Tree* atom;
  if (characters_ != nullptr) {
    base::Vector<const base::uc16> char_vector = characters_->ToConstVector();
    int num_chars = char_vector.length();
    if (num_chars > 1) {
      base::Vector<const base::uc16> prefix =
          char_vector.SubVector(0, num_chars - 1);
      text_.emplace_back(zone()->New<Atom>(prefix));
      char_vector = char_vector.SubVector(num_chars - 1, num_chars);
    }
    characters_ = nullptr;
    atom = zone()->New<Atom>(char_vector);
    return atom;
  } else if (!text_.empty()) {
    atom = text_.back();
    text_.pop_back();
    return atom;
  }
  return nullptr;
}

Tree* TextBuilder::ToRegExp() {
  FlushText();
  size_t number_of_terms = terms_->size();
  if (number_of_terms == 0) return zone()->New<Empty>();
  if (number_of_terms == 1) return terms_->back();
  return zone()->New<Alternative>(zone()->New<ZoneList<Tree*>>(
      base::VectorOf(terms_->begin(), terms_->size()), zone()));
}

// Accumulates RegExp atoms and assertions into lists of terms and alternatives.
class Builder {
 public:
  Builder(Zone* zone, Flags flags)
      : zone_(zone),
        flags_(flags),
        terms_(zone),
        alternatives_(zone),
        text_builder_(TextBuilder{zone, &terms_, flags}) {}
  void AddCharacter(base::uc16 character);
  void AddUnicodeCharacter(base::uc32 character);
  void AddEscapedUnicodeCharacter(base::uc32 character);
  // "Adds" an empty expression. Does nothing except consume a
  // following quantifier
  void AddEmpty();
  void AddClassRanges(ClassRanges* cc);
  void AddAtom(Tree* tree);
  void AddTerm(Tree* tree);
  void AddAssertion(Tree* tree);
  void NewAlternative();  // '|'
  bool AddQuantifierToAtom(int min, int max, int index,
                           Quantifier::QuantifierType type);
  void FlushText();
  Tree* ToRegExp();
  Flags flags() const { return flags_; }

  bool ignore_case() const { return IsIgnoreCase(flags_); }
  bool multiline() const { return IsMultiline(flags_); }
  bool dotall() const { return IsDotAll(flags_); }

 private:
  void FlushTerms();
  bool IsUnicodeMode() const {
    // Either /v or /u enable UnicodeMode
    // https://tc39.es/ecma262/#sec-parsepattern
    return IsUnicode(flags_) || IsUnicodeSets(flags_);
  }
  Zone* zone() const { return zone_; }
  TextBuilder& text_builder() { return text_builder_; }

  Zone* const zone_;
  bool pending_empty_ = false;
  const Flags flags_;

  using SmallTreeVector = SmallZoneVector<Tree*, 8>;
  SmallTreeVector terms_;
  SmallTreeVector alternatives_;
  TextBuilder text_builder_;
};

enum SubexpressionType {
  INITIAL,
  CAPTURE,  // All positive values represent captures.
  POSITIVE_LOOKAROUND,
  NEGATIVE_LOOKAROUND,
  GROUPING
};

class ParserState : public ZoneObject {
 public:
  // Push a state on the stack.
  ParserState(ParserState* previous_state, SubexpressionType group_type,
              Lookaround::Type lookaround_type, int disjunction_capture_index,
              const ZoneVector<base::uc16>* capture_name, Flags flags,
              Zone* zone)
      : previous_state_(previous_state),
        builder_(zone, flags),
        group_type_(group_type),
        lookaround_type_(lookaround_type),
        disjunction_capture_index_(disjunction_capture_index),
        capture_name_(capture_name),
        non_participating_capture_group_intervals_(zone) {
    if (previous_state != nullptr) {
      non_participating_capture_group_intervals_.insert(
          non_participating_capture_group_intervals_.begin(),
          previous_state->non_participating_capture_group_intervals_);
    }
  }
  using IntervalVector = SmallZoneVector<Interval, 1>;

  // Parser state of containing expression, if any.
  ParserState* previous_state() const { return previous_state_; }
  bool IsSubexpression() { return previous_state_ != nullptr; }
  // Builder building this regexp's AST.
  Builder* builder() { return &builder_; }
  // Type of regexp being parsed (parenthesized group or entire regexp).
  SubexpressionType group_type() const { return group_type_; }
  // Lookahead or Lookbehind.
  Lookaround::Type lookaround_type() const { return lookaround_type_; }
  // Index in captures array of first capture in this sub-expression, if any.
  // Also the capture index of this sub-expression itself, if group_type
  // is CAPTURE.
  int capture_index() const { return disjunction_capture_index_; }
  // The name of the current sub-expression, if group_type is CAPTURE. Only
  // used for named captures.
  const ZoneVector<base::uc16>* capture_name() const { return capture_name_; }
  const IntervalVector& non_participating_capture_group_intervals() const {
    return non_participating_capture_group_intervals_;
  }

  bool IsNamedCapture() const { return capture_name_ != nullptr; }

  // Check whether the parser is inside a capture group with the given index.
  bool IsInsideCaptureGroup(int index) const {
    for (const ParserState* s = this; s != nullptr; s = s->previous_state()) {
      if (s->group_type() != CAPTURE) continue;
      // Return true if we found the matching capture index.
      if (index == s->capture_index()) return true;
      // Abort if index is larger than what has been parsed up till this state.
      if (index > s->capture_index()) return false;
    }
    return false;
  }

  // Check whether the parser is inside a capture group with the given name.
  bool IsInsideCaptureGroup(const ZoneVector<base::uc16>* name) const {
    DCHECK_NOT_NULL(name);
    for (const ParserState* s = this; s != nullptr; s = s->previous_state()) {
      if (s->capture_name() == nullptr) continue;
      if (*s->capture_name() == *name) return true;
    }
    return false;
  }

  void NewAlternative(int captures_started) {
    // Nothing to do if there were no new captures started before the
    // alternative.
    if (capture_index() == captures_started) return;

    // +1 to create a closed interval (capture_index() is exclusive).
    int from = capture_index() + 1;
    int to = captures_started;
    DCHECK_LE(from, to);
    // Extend the last interval if we increase its range by exactly 1.
    if (!non_participating_capture_group_intervals().empty() &&
        non_participating_capture_group_intervals().back().to() + 1 == to) {
      Interval& interval = non_participating_capture_group_intervals_.back();
      DCHECK(!interval.is_empty());
      DCHECK_GE(from, interval.from());
      interval = interval.Union({from, to});
    } else {
      non_participating_capture_group_intervals_.push_back({from, to});
    }
  }

 private:
  // Linked list implementation of stack of states.
  ParserState* const previous_state_;
  // Builder for the stored disjunction.
  Builder builder_;
  // Stored disjunction type (capture, look-ahead or grouping), if any.
  const SubexpressionType group_type_;
  // Stored read direction.
  const Lookaround::Type lookaround_type_;
  // Stored disjunction's capture index (if any).
  const int disjunction_capture_index_;
  // Stored capture name (if any).
  const ZoneVector<base::uc16>* const capture_name_;
  // List of Intervals of (named) capture indices [from, to] that are not
  // participating in the current state (i.e. they cannot match).
  // Capture indices are not participating if they were created in a different
  // alternative.
  IntervalVector non_participating_capture_group_intervals_;
};

template <class CharT>
class ParserImpl final {
 private:
  ParserImpl(const CharT* input, int input_length, Flags flags,
             uintptr_t stack_limit, Zone* zone,
             const DisallowGarbageCollection& no_gc);

  bool Parse(CompileData* result);

  Tree* ParsePattern();
  Tree* ParseDisjunction();
  Tree* ParseGroup();

  // Parses a {...,...} quantifier and stores the range in the given
  // out parameters.
  bool ParseIntervalQuantifier(int* min_out, int* max_out);

  // Checks whether the following is a length-digit hexadecimal number,
  // and sets the value if it is.
  bool ParseHexEscape(int length, base::uc32* value);
  bool ParseUnicodeEscape(base::uc32* value);
  bool ParseUnlimitedLengthHexNumber(int max_value, base::uc32* value);

  bool ParsePropertyClassName(ZoneVector<char>* name_1,
                              ZoneVector<char>* name_2);
  bool AddPropertyClassRange(ZoneList<CharacterRange>* add_to_range,
                             CharacterClassStrings* add_to_strings, bool negate,
                             const ZoneVector<char>& name_1,
                             const ZoneVector<char>& name_2);

  Tree* ParseClassRanges(ZoneList<CharacterRange>* ranges,
                         bool add_unicode_case_equivalents);
  // Parse inside a class. Either add escaped class to the range, or return
  // false and pass parsed single character through |char_out|.
  void ParseClassEscape(ZoneList<CharacterRange>* ranges, Zone* zone,
                        bool add_unicode_case_equivalents, base::uc32* char_out,
                        bool* is_class_escape);
  // Returns true iff parsing was successful.
  bool TryParseCharacterClassEscape(base::uc32 next,
                                    InClassEscapeState in_class_escape_state,
                                    ZoneList<CharacterRange>* ranges,
                                    CharacterClassStrings* strings, Zone* zone,
                                    bool add_unicode_case_equivalents);
  Tree* ParseClassStringDisjunction(ZoneList<CharacterRange>* ranges,
                                    CharacterClassStrings* strings);
  Tree* ParseClassSetOperand(const Builder* builder,
                             ClassSetOperandType* type_out);
  Tree* ParseClassSetOperand(const Builder* builder,
                             ClassSetOperandType* type_out,
                             ZoneList<CharacterRange>* ranges,
                             CharacterClassStrings* strings,
                             base::uc32* character);
  base::uc32 ParseClassSetCharacter();
  // Parses and returns a single escaped character.
  base::uc32 ParseCharacterEscape(InClassEscapeState in_class_escape_state,
                                  bool* is_escaped_unicode_character);

  void AddMaybeSimpleCaseFoldedRange(ZoneList<CharacterRange>* ranges,
                                     CharacterRange new_range);

  Tree* ParseClassUnion(const Builder* builder, bool is_negated,
                        Tree* first_operand,
                        ClassSetOperandType first_operand_type,
                        ZoneList<CharacterRange>* ranges,
                        CharacterClassStrings* strings,
                        base::uc32 first_character);
  Tree* ParseClassIntersection(const Builder* builder, bool is_negated,
                               Tree* first_operand,
                               ClassSetOperandType first_operand_type);
  Tree* ParseClassSubtraction(const Builder* builder, bool is_negated,
                              Tree* first_operand,
                              ClassSetOperandType first_operand_type);
  Tree* ParseCharacterClass(const Builder* state);

  base::uc32 ParseOctalLiteral();

  // Tries to parse the input as a back reference.  If successful it
  // stores the result in the output parameter and returns true.  If
  // it fails it will push back the characters read so the same characters
  // can be reparsed.
  bool ParseBackReferenceIndex(int* index_out);

  Tree* ReportError(Error error);
  void Advance();
  void Advance(int dist);
  void RewindByOneCodepoint();  // Rewinds to before the previous Advance().
  void Reset(int pos);

  // Reports whether the pattern might be used as a literal search string.
  // Only use if the result of the parse is a single atom node.
  bool simple() const { return simple_; }
  bool contains_anchor() const { return contains_anchor_; }
  void set_contains_anchor() { contains_anchor_ = true; }
  int captures_started() const { return captures_started_; }
  int position() const {
    const bool current_is_surrogate =
        current() != kEndMarker &&
        current() > unibrow::Utf16::kMaxNonSurrogateCharCode;
    const int rewind_bytes = current_is_surrogate ? 2 : 1;
    return next_pos_ - rewind_bytes;
  }
  bool failed() const { return failed_; }
  Flags flags() const { return flags_; }
  bool IsUnicodeMode() const {
    // Either /v or /u enable UnicodeMode
    // https://tc39.es/ecma262/#sec-parsepattern
    return IsUnicode(flags()) || IsUnicodeSets(flags()) || force_unicode_;
  }
  bool unicode_sets() const { return IsUnicodeSets(flags()); }
  bool ignore_case() const { return IsIgnoreCase(flags()); }

  static bool IsSyntaxCharacterOrSlash(base::uc32 c);
  static bool IsClassSetSyntaxCharacter(base::uc32 c);
  static bool IsClassSetReservedPunctuator(base::uc32 c);
  bool IsClassSetReservedDoublePunctuator(base::uc32 c);

  static const base::uc32 kEndMarker = (1 << 21);

 private:
  // Return the 1-indexed Capture object, allocate if necessary.
  Capture* GetCapture(int index);

  // Creates a new named capture at the specified index. Must be called exactly
  // once for each named capture. Fails if a capture with the same name is
  // encountered.
  bool CreateNamedCaptureAtIndex(const ParserState* state, int index);

  // Parses the name of a capture group (?<name>pattern). The name must adhere
  // to IdentifierName in the ECMAScript standard.
  const ZoneVector<base::uc16>* ParseCaptureGroupName();

  bool ParseNamedBackReference(Builder* builder, ParserState* state);
  ParserState* ParseOpenParenthesis(ParserState* state);

  // After the initial parsing pass, patch corresponding Capture objects
  // into all RegExpBackReferences. This is done after initial parsing in order
  // to avoid complicating cases in which references comes before the capture.
  void PatchNamedBackReferences();

  ZoneVector<Capture*>* GetNamedCaptures();

  // Returns true iff the pattern contains named captures. May call
  // ScanForCaptures to look ahead at the remaining pattern.
  bool HasNamedCaptures(InClassEscapeState in_class_escape_state);

  Zone* zone() const { return zone_; }

  base::uc32 current() const { return current_; }
  bool has_more() const { return has_more_; }
  bool has_next() const { return next_pos_ < input_length(); }
  base::uc32 Next();
  template <bool update_position>
  base::uc32 ReadNext();
  CharT InputAt(int index) const {
    DCHECK(0 <= index && index < input_length());
    return input_[index];
  }
  int input_length() const { return input_length_; }
  void ScanForCaptures(InClassEscapeState in_class_escape_state);

  struct CaptureNameLess {
    bool operator()(const Capture* lhs, const Capture* rhs) const {
      DCHECK_NOT_NULL(lhs);
      DCHECK_NOT_NULL(rhs);
      return *lhs->name() < *rhs->name();
    }
  };

  class ForceUnicodeScope final {
   public:
    explicit ForceUnicodeScope(ParserImpl<CharT>* parser) : parser_(parser) {
      DCHECK(!parser_->force_unicode_);
      parser_->force_unicode_ = true;
    }
    ~ForceUnicodeScope() {
      DCHECK(parser_->force_unicode_);
      parser_->force_unicode_ = false;
    }

   private:
    ParserImpl<CharT>* const parser_;
  };

  const DisallowGarbageCollection no_gc_;
  Zone* const zone_;
  Error error_ = Error::kNone;
  int error_pos_ = 0;
  ZoneList<Capture*>* captures_;
  // Maps capture names to a list of capture indices with this name.
  ZoneMap<Capture*, ZoneList<int>*, CaptureNameLess>* named_captures_;
  ZoneList<BackReference*>* named_back_references_;
  const CharT* const input_;
  const int input_length_;
  base::uc32 current_;
  Flags flags_;
  bool force_unicode_ = false;  // Force parser to act as if unicode were set.
  int next_pos_;
  int captures_started_;
  int capture_count_;  // Only valid after we have scanned for captures.
  int quantifier_count_;
  int lookaround_count_;  // Only valid after we have scanned for lookbehinds.
  bool has_more_;
  bool simple_;
  bool contains_anchor_;
  bool is_scanned_for_captures_;
  bool has_named_captures_;  // Only valid after we have scanned for captures.
  bool failed_;
  const uintptr_t stack_limit_;

  friend class regexp::Parser;
};

template <class CharT>
ParserImpl<CharT>::ParserImpl(const CharT* input, int input_length, Flags flags,
                              uintptr_t stack_limit, Zone* zone,
                              const DisallowGarbageCollection& no_gc)
    : zone_(zone),
      captures_(nullptr),
      named_captures_(nullptr),
      named_back_references_(nullptr),
      input_(input),
      input_length_(input_length),
      current_(kEndMarker),
      flags_(flags),
      next_pos_(0),
      captures_started_(0),
      capture_count_(0),
      quantifier_count_(0),
      lookaround_count_(0),
      has_more_(true),
      simple_(false),
      contains_anchor_(false),
      is_scanned_for_captures_(false),
      has_named_captures_(false),
      failed_(false),
      stack_limit_(stack_limit) {
  Advance();
}

template <>
template <bool update_position>
inline base::uc32 ParserImpl<uint8_t>::ReadNext() {
  int position = next_pos_;
  base::uc16 c0 = InputAt(position);
  position++;
  DCHECK(!unibrow::Utf16::IsLeadSurrogate(c0));
  if (update_position) next_pos_ = position;
  return c0;
}

template <>
template <bool update_position>
inline base::uc32 ParserImpl<base::uc16>::ReadNext() {
  int position = next_pos_;
  base::uc16 c0 = InputAt(position);
  base::uc32 result = c0;
  position++;
  // Read the whole surrogate pair in case of unicode mode, if possible.
  if (IsUnicodeMode() && position < input_length() &&
      unibrow::Utf16::IsLeadSurrogate(c0)) {
    base::uc16 c1 = InputAt(position);
    if (unibrow::Utf16::IsTrailSurrogate(c1)) {
      result = unibrow::Utf16::CombineSurrogatePair(c0, c1);
      position++;
    }
  }
  if (update_position) next_pos_ = position;
  return result;
}

template <class CharT>
base::uc32 ParserImpl<CharT>::Next() {
  if (has_next()) {
    return ReadNext<false>();
  } else {
    return kEndMarker;
  }
}

template <class CharT>
void ParserImpl<CharT>::Advance() {
  if (has_next()) {
    if (GetCurrentStackPosition() < stack_limit_) {
      ReportError(Error::kStackOverflow);
    } else {
      current_ = ReadNext<true>();
    }
  } else {
    current_ = kEndMarker;
    // Advance so that position() points to 1-after-the-last-character. This is
    // important so that Reset() to this position works correctly.
    next_pos_ = input_length() + 1;
    has_more_ = false;
  }
}

template <class CharT>
void ParserImpl<CharT>::RewindByOneCodepoint() {
  if (!has_more()) return;
  // Rewinds by one code point, i.e.: two code units if `current` is outside
  // the basic multilingual plane (= composed of a lead and trail surrogate),
  // or one code unit otherwise.
  const int rewind_by =
      current() > unibrow::Utf16::kMaxNonSurrogateCharCode ? -2 : -1;
  Advance(rewind_by);  // Undo the last Advance.
}

template <class CharT>
void ParserImpl<CharT>::Reset(int pos) {
  next_pos_ = pos;
  has_more_ = (pos < input_length());
  Advance();
}

template <class CharT>
void ParserImpl<CharT>::Advance(int dist) {
  next_pos_ += dist - 1;
  Advance();
}

// static
template <class CharT>
bool ParserImpl<CharT>::IsSyntaxCharacterOrSlash(base::uc32 c) {
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

// static
template <class CharT>
bool ParserImpl<CharT>::IsClassSetSyntaxCharacter(base::uc32 c) {
  switch (c) {
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '/':
    case '-':
    case '\\':
    case '|':
      return true;
    default:
      break;
  }
  return false;
}

// static
template <class CharT>
bool ParserImpl<CharT>::IsClassSetReservedPunctuator(base::uc32 c) {
  switch (c) {
    case '&':
    case '-':
    case '!':
    case '#':
    case '%':
    case ',':
    case ':':
    case ';':
    case '<':
    case '=':
    case '>':
    case '@':
    case '`':
    case '~':
      return true;
    default:
      break;
  }
  return false;
}

template <class CharT>
bool ParserImpl<CharT>::IsClassSetReservedDoublePunctuator(base::uc32 c) {
#define DOUBLE_PUNCTUATOR_CASE(Char) \
  case Char:                         \
    return Next() == Char

  switch (c) {
    DOUBLE_PUNCTUATOR_CASE('&');
    DOUBLE_PUNCTUATOR_CASE('!');
    DOUBLE_PUNCTUATOR_CASE('#');
    DOUBLE_PUNCTUATOR_CASE('$');
    DOUBLE_PUNCTUATOR_CASE('%');
    DOUBLE_PUNCTUATOR_CASE('*');
    DOUBLE_PUNCTUATOR_CASE('+');
    DOUBLE_PUNCTUATOR_CASE(',');
    DOUBLE_PUNCTUATOR_CASE('.');
    DOUBLE_PUNCTUATOR_CASE(':');
    DOUBLE_PUNCTUATOR_CASE(';');
    DOUBLE_PUNCTUATOR_CASE('<');
    DOUBLE_PUNCTUATOR_CASE('=');
    DOUBLE_PUNCTUATOR_CASE('>');
    DOUBLE_PUNCTUATOR_CASE('?');
    DOUBLE_PUNCTUATOR_CASE('@');
    DOUBLE_PUNCTUATOR_CASE('^');
    DOUBLE_PUNCTUATOR_CASE('`');
    DOUBLE_PUNCTUATOR_CASE('~');
    default:
      break;
  }
#undef DOUBLE_PUNCTUATOR_CASE

  return false;
}

template <class CharT>
Tree* ParserImpl<CharT>::ReportError(Error error) {
  if (failed_) return nullptr;  // Do not overwrite any existing error.
  failed_ = true;
  error_ = error;
  error_pos_ = position();
  // Zip to the end to make sure no more input is read.
  current_ = kEndMarker;
  next_pos_ = input_length();
  has_more_ = false;
  return nullptr;
}

#define CHECK_FAILED /**/);    \
  if (failed_) return nullptr; \
  ((void)0

// Pattern ::
//   Disjunction
template <class CharT>
Tree* ParserImpl<CharT>::ParsePattern() {
  Tree* result = ParseDisjunction(CHECK_FAILED);
  PatchNamedBackReferences(CHECK_FAILED);
  DCHECK(!has_more());
  // If the result of parsing is a literal string atom, and it has the
  // same length as the input, then the atom is identical to the input.
  if (result->IsAtom() && result->AsAtom()->length() == input_length()) {
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
template <class CharT>
Tree* ParserImpl<CharT>::ParseDisjunction() {
  // Used to store current state while parsing subexpressions.
  ParserState initial_state(nullptr, INITIAL, Lookaround::LOOKAHEAD, 0, nullptr,
                            flags(), zone());
  ParserState* state = &initial_state;
  // Cache the builder in a local variable for quick access.
  Builder* builder = initial_state.builder();
  while (true) {
    switch (current()) {
      case kEndMarker:
        if (failed()) return nullptr;  // E.g. the initial Advance failed.
        if (state->IsSubexpression()) {
          // Inside a parenthesized group when hitting end of input.
          return ReportError(Error::kUnterminatedGroup);
        }
        DCHECK_EQ(INITIAL, state->group_type());
        // Parsing completed successfully.
        return builder->ToRegExp();
      case ')': {
        if (!state->IsSubexpression()) {
          return ReportError(Error::kUnmatchedParen);
        }
        DCHECK_NE(INITIAL, state->group_type());

        Advance();
        // End disjunction parsing and convert builder content to new single
        // regexp atom.
        Tree* body = builder->ToRegExp();

        int end_capture_index = captures_started();

        int capture_index = state->capture_index();
        SubexpressionType group_type = state->group_type();

        // Build result of subexpression.
        if (group_type == CAPTURE) {
          if (state->IsNamedCapture()) {
            CreateNamedCaptureAtIndex(state, capture_index CHECK_FAILED);
          }
          Capture* capture = GetCapture(capture_index);
          capture->set_body(body);
          body = capture;
        } else if (group_type == GROUPING) {
          body = zone()->template New<Group>(body, builder->flags());
        } else {
          DCHECK(group_type == POSITIVE_LOOKAROUND ||
                 group_type == NEGATIVE_LOOKAROUND);
          bool is_positive = (group_type == POSITIVE_LOOKAROUND);
          body = zone()->template New<Lookaround>(
              body, is_positive, end_capture_index - capture_index,
              capture_index, state->lookaround_type(), lookaround_count_);
          lookaround_count_++;
        }

        // Restore previous state.
        state = state->previous_state();
        builder = state->builder();

        builder->AddAtom(body);
        // For compatibility with JSC and ES3, we allow quantifiers after
        // lookaheads, and break in all cases.
        break;
      }
      case '|': {
        Advance();
        state->NewAlternative(captures_started());
        builder->NewAlternative();
        continue;
      }
      case '*':
      case '+':
      case '?':
        return ReportError(Error::kNothingToRepeat);
      case '^': {
        Advance();
        builder->AddAssertion(zone()->template New<Assertion>(
            builder->multiline() ? Assertion::Type::START_OF_LINE
                                 : Assertion::Type::START_OF_INPUT));
        set_contains_anchor();
        continue;
      }
      case '$': {
        Advance();
        Assertion::Type assertion_type = builder->multiline()
                                             ? Assertion::Type::END_OF_LINE
                                             : Assertion::Type::END_OF_INPUT;
        builder->AddAssertion(zone()->template New<Assertion>(assertion_type));
        continue;
      }
      case '.': {
        Advance();
        ZoneList<CharacterRange>* ranges =
            zone()->template New<ZoneList<CharacterRange>>(2, zone());

        if (builder->dotall()) {
          // Everything.
          CharacterRange::AddClassEscape(StandardCharacterSet::kEverything,
                                         ranges, false, zone());
        } else {
          // Everything except \x0A, \x0D, \u2028 and \u2029.
          CharacterRange::AddClassEscape(
              StandardCharacterSet::kNotLineTerminator, ranges, false, zone());
        }

        ClassRanges* cc = zone()->template New<ClassRanges>(zone(), ranges);
        builder->AddClassRanges(cc);
        break;
      }
      case '(': {
        state = ParseOpenParenthesis(state CHECK_FAILED);
        builder = state->builder();
        flags_ = builder->flags();
        continue;
      }
      case '[': {
        Tree* cc = ParseCharacterClass(builder CHECK_FAILED);
        if (cc->IsClassRanges()) {
          builder->AddClassRanges(cc->AsClassRanges());
        } else {
          DCHECK(cc->IsClassSetExpression());
          builder->AddTerm(cc);
        }
        break;
      }
      // Atom ::
      //   \ AtomEscape
      case '\\':
        switch (Next()) {
          case kEndMarker:
            return ReportError(Error::kEscapeAtEndOfPattern);
          // AtomEscape ::
          //   [+UnicodeMode] DecimalEscape
          //   [~UnicodeMode] DecimalEscape but only if the CapturingGroupNumber
          //                  of DecimalEscape is ≤ NcapturingParens
          //   CharacterEscape (some cases of this mixed in too)
          //
          // TODO(jgruber): It may make sense to disentangle all the different
          // cases and make the structure mirror the spec, e.g. for AtomEscape:
          //
          //  if (TryParseDecimalEscape(...)) return;
          //  if (TryParseCharacterClassEscape(...)) return;
          //  if (TryParseCharacterEscape(...)) return;
          //  if (TryParseGroupName(...)) return;
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
            const bool is_backref =
                ParseBackReferenceIndex(&index CHECK_FAILED);
            if (is_backref) {
              if (state->IsInsideCaptureGroup(index)) {
                // The back reference is inside the capture group it refers to.
                // Nothing can possibly have been captured yet, so we use empty
                // instead. This ensures that, when checking a back reference,
                // the capture registers of the referenced capture are either
                // both set or both cleared.
                builder->AddEmpty();
              } else {
                Capture* capture = GetCapture(index);
                Tree* atom =
                    zone()->template New<BackReference>(capture, zone());
                builder->AddAtom(atom);
              }
              break;
            }
            // With /u and /v, no identity escapes except for syntax characters
            // are allowed. Otherwise, all identity escapes are allowed.
            if (IsUnicodeMode()) {
              return ReportError(Error::kInvalidEscape);
            }
            base::uc32 first_digit = Next();
            if (first_digit == '8' || first_digit == '9') {
              builder->AddCharacter(first_digit);
              Advance(2);
              break;
            }
            [[fallthrough]];
          }
          case '0': {
            Advance();
            if (IsUnicodeMode() && Next() >= '0' && Next() <= '9') {
              // Decimal escape with leading 0 are not parsed as octal.
              return ReportError(Error::kInvalidDecimalEscape);
            }
            base::uc32 octal = ParseOctalLiteral();
            builder->AddCharacter(octal);
            break;
          }
          case 'b':
            Advance(2);
            builder->AddAssertion(
                zone()->template New<Assertion>(Assertion::Type::BOUNDARY));
            continue;
          case 'B':
            Advance(2);
            builder->AddAssertion(
                zone()->template New<Assertion>(Assertion::Type::NON_BOUNDARY));
            continue;
          // AtomEscape ::
          //   CharacterClassEscape
          case 'd':
          case 'D':
          case 's':
          case 'S':
          case 'w':
          case 'W': {
            base::uc32 next = Next();
            ZoneList<CharacterRange>* ranges =
                zone()->template New<ZoneList<CharacterRange>>(2, zone());
            bool add_unicode_case_equivalents =
                IsUnicodeMode() && ignore_case();
            bool parsed_character_class_escape = TryParseCharacterClassEscape(
                next, InClassEscapeState::kNotInClass, ranges, nullptr, zone(),
                add_unicode_case_equivalents CHECK_FAILED);

            if (parsed_character_class_escape) {
              ClassRanges* cc =
                  zone()->template New<ClassRanges>(zone(), ranges);
              builder->AddClassRanges(cc);
            } else {
              CHECK(!IsUnicodeMode());
              Advance(2);
              builder->AddCharacter(next);  // IdentityEscape.
            }
            break;
          }
          case 'p':
          case 'P': {
            base::uc32 next = Next();
            ZoneList<CharacterRange>* ranges =
                zone()->template New<ZoneList<CharacterRange>>(2, zone());
            CharacterClassStrings* strings = nullptr;
            if (unicode_sets()) {
              strings = zone()->template New<CharacterClassStrings>(zone());
            }
            bool add_unicode_case_equivalents = ignore_case();
            bool parsed_character_class_escape = TryParseCharacterClassEscape(
                next, InClassEscapeState::kNotInClass, ranges, strings, zone(),
                add_unicode_case_equivalents CHECK_FAILED);

            if (parsed_character_class_escape) {
              if (unicode_sets()) {
                ClassSetOperand* op =
                    zone()->template New<ClassSetOperand>(ranges, strings);
                builder->AddTerm(op);
              } else {
                ClassRanges* cc =
                    zone()->template New<ClassRanges>(zone(), ranges);
                builder->AddClassRanges(cc);
              }
            } else {
              CHECK(!IsUnicodeMode());
              Advance(2);
              builder->AddCharacter(next);  // IdentityEscape.
            }
            break;
          }
          // AtomEscape ::
          //   k GroupName
          case 'k': {
            // Either an identity escape or a named back-reference.  The two
            // interpretations are mutually exclusive: '\k' is interpreted as
            // an identity escape for non-Unicode patterns without named
            // capture groups, and as the beginning of a named back-reference
            // in all other cases.
            const bool has_named_captures =
                HasNamedCaptures(InClassEscapeState::kNotInClass CHECK_FAILED);
            if (IsUnicodeMode() || has_named_captures) {
              Advance(2);
              ParseNamedBackReference(builder, state CHECK_FAILED);
              break;
            }
          }
            [[fallthrough]];
          // AtomEscape ::
          //   CharacterEscape
          default: {
            bool is_escaped_unicode_character = false;
            base::uc32 c = ParseCharacterEscape(
                InClassEscapeState::kNotInClass,
                &is_escaped_unicode_character CHECK_FAILED);
            if (is_escaped_unicode_character) {
              builder->AddEscapedUnicodeCharacter(c);
            } else {
              builder->AddCharacter(c);
            }
            break;
          }
        }
        break;
      case '{': {
        int dummy;
        bool parsed = ParseIntervalQuantifier(&dummy, &dummy CHECK_FAILED);
        if (parsed) return ReportError(Error::kNothingToRepeat);
        [[fallthrough]];
      }
      case '}':
      case ']':
        if (IsUnicodeMode()) {
          return ReportError(Error::kLoneQuantifierBrackets);
        }
        [[fallthrough]];
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
        max = Tree::kInfinity;
        Advance();
        break;
      case '+':
        min = 1;
        max = Tree::kInfinity;
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
            return ReportError(Error::kRangeOutOfOrder);
          }
          break;
        } else if (IsUnicodeMode()) {
          // Incomplete quantifiers are not allowed.
          return ReportError(Error::kIncompleteQuantifier);
        }
        continue;
      default:
        continue;
    }
    Quantifier::QuantifierType quantifier_type = Quantifier::GREEDY;
    if (current() == '?') {
      quantifier_type = Quantifier::NON_GREEDY;
      Advance();
    } else if (v8_flags.regexp_possessive_quantifier && current() == '+') {
      // v8_flags.regexp_possessive_quantifier is a debug-only flag.
      quantifier_type = Quantifier::POSSESSIVE;
      Advance();
    }
    if (!builder->AddQuantifierToAtom(min, max, quantifier_count_,
                                      quantifier_type)) {
      return ReportError(Error::kInvalidQuantifier);
    }
    ++quantifier_count_;
  }
}

template <class CharT>
ParserState* ParserImpl<CharT>::ParseOpenParenthesis(ParserState* state) {
  Lookaround::Type lookaround_type = state->lookaround_type();
  bool is_named_capture = false;
  const ZoneVector<base::uc16>* capture_name = nullptr;
  SubexpressionType subexpr_type = CAPTURE;
  Flags flags = state->builder()->flags();
  bool parsing_modifiers = false;
  bool modifiers_polarity = true;
  Flags modifiers;
  Advance();
  if (current() == '?') {
    do {
      base::uc32 next = Next();
      switch (next) {
        case '-':
          if (!v8_flags.js_regexp_modifiers) {
            ReportError(Error::kInvalidGroup);
            return nullptr;
          }
          Advance();
          parsing_modifiers = true;
          if (modifiers_polarity == false) {
            ReportError(Error::kMultipleFlagDashes);
            return nullptr;
          }
          modifiers_polarity = false;
          break;
        case 'm':
        case 'i':
        case 's': {
          if (!v8_flags.js_regexp_modifiers) {
            ReportError(Error::kInvalidGroup);
            return nullptr;
          }
          Advance();
          parsing_modifiers = true;
          Flag flag = TryFlagFromChar(next).value();
          if ((modifiers & flag) != 0) {
            ReportError(Error::kRepeatedFlag);
            return nullptr;
          }
          modifiers |= flag;
          flags.set(flag, modifiers_polarity);
          break;
        }
        case ':':
          Advance(2);
          parsing_modifiers = false;
          subexpr_type = GROUPING;
          break;
        case '=':
          Advance(2);
          if (parsing_modifiers) {
            DCHECK(v8_flags.js_regexp_modifiers);
            ReportError(Error::kInvalidGroup);
            return nullptr;
          }
          lookaround_type = Lookaround::LOOKAHEAD;
          subexpr_type = POSITIVE_LOOKAROUND;
          break;
        case '!':
          Advance(2);
          if (parsing_modifiers) {
            DCHECK(v8_flags.js_regexp_modifiers);
            ReportError(Error::kInvalidGroup);
            return nullptr;
          }
          lookaround_type = Lookaround::LOOKAHEAD;
          subexpr_type = NEGATIVE_LOOKAROUND;
          break;
        case '<':
          Advance();
          if (parsing_modifiers) {
            DCHECK(v8_flags.js_regexp_modifiers);
            ReportError(Error::kInvalidGroup);
            return nullptr;
          }
          if (Next() == '=') {
            Advance(2);
            lookaround_type = Lookaround::LOOKBEHIND;
            subexpr_type = POSITIVE_LOOKAROUND;
            break;
          } else if (Next() == '!') {
            Advance(2);
            lookaround_type = Lookaround::LOOKBEHIND;
            subexpr_type = NEGATIVE_LOOKAROUND;
            break;
          }
          is_named_capture = true;
          has_named_captures_ = true;
          Advance();
          break;
        default:
          ReportError(Error::kInvalidGroup);
          return nullptr;
      }
    } while (parsing_modifiers);
  }
  if (modifiers_polarity == false) {
    // We encountered a dash.
    if (modifiers == 0) {
      ReportError(Error::kInvalidFlagGroup);
      return nullptr;
    }
  }
  if (subexpr_type == CAPTURE) {
    if (captures_started_ >= RegExpMacroAssembler::kMaxCaptures) {
      ReportError(Error::kTooManyCaptures);
      return nullptr;
    }
    captures_started_++;

    if (is_named_capture) {
      capture_name = ParseCaptureGroupName(CHECK_FAILED);
    }
  }
  // Store current state and begin new disjunction parsing.
  return zone()->template New<ParserState>(state, subexpr_type, lookaround_type,
                                           captures_started_, capture_name,
                                           flags, zone());
}

// In order to know whether an escape is a backreference or not we have to scan
// the entire regexp and find the number of capturing parentheses.  However we
// don't want to scan the regexp twice unless it is necessary.  This mini-parser
// is called when needed.  It can see the difference between capturing and
// noncapturing parentheses and can skip character classes and backslash-escaped
// characters.
//
// Important: The scanner has to be in a consistent state when calling
// ScanForCaptures, e.g. not in the middle of an escape sequence '\[' or while
// parsing a nested class.
template <class CharT>
void ParserImpl<CharT>::ScanForCaptures(
    InClassEscapeState in_class_escape_state) {
  DCHECK(!is_scanned_for_captures_);
  const int saved_position = position();
  // Start with captures started previous to current position
  int capture_count = captures_started();
  // When we start inside a character class, skip everything inside the class.
  if (in_class_escape_state == InClassEscapeState::kInClass) {
    // \k is always invalid within a class in unicode mode, thus we should never
    // call ScanForCaptures within a class.
    DCHECK(!IsUnicodeMode());
    int c;
    while ((c = current()) != kEndMarker) {
      Advance();
      if (c == '\\') {
        Advance();
      } else {
        if (c == ']') break;
      }
    }
  }
  // Add count of captures after this position.
  int n;
  while ((n = current()) != kEndMarker) {
    Advance();
    switch (n) {
      case '\\':
        Advance();
        break;
      case '[': {
        int class_nest_level = 0;
        int c;
        while ((c = current()) != kEndMarker) {
          Advance();
          if (c == '\\') {
            Advance();
          } else if (c == '[') {
            // With /v, '[' inside a class is treated as a nested class.
            // Without /v, '[' is a normal character.
            if (unicode_sets()) class_nest_level++;
          } else if (c == ']') {
            if (class_nest_level == 0) break;
            class_nest_level--;
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

          Advance();
          if (current() != '<') break;

          Advance();
          if (current() == '=' || current() == '!') break;

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

template <class CharT>
bool ParserImpl<CharT>::ParseBackReferenceIndex(int* index_out) {
  DCHECK_EQ('\\', current());
  DCHECK('1' <= Next() && Next() <= '9');
  // Try to parse a decimal literal that is no greater than the total number
  // of left capturing parentheses in the input.
  int start = position();
  int value = Next() - '0';
  Advance(2);
  while (true) {
    base::uc32 c = current();
    if (IsDecimalDigit(c)) {
      value = 10 * value + (c - '0');
      if (value > RegExpMacroAssembler::kMaxCaptures) {
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
      ScanForCaptures(InClassEscapeState::kNotInClass);
    }
    if (value > capture_count_) {
      Reset(start);
      return false;
    }
  }
  *index_out = value;
  return true;
}

namespace {

void push_code_unit(ZoneVector<base::uc16>* v, uint32_t code_unit) {
  if (code_unit <= unibrow::Utf16::kMaxNonSurrogateCharCode) {
    v->push_back(code_unit);
  } else {
    v->push_back(unibrow::Utf16::LeadSurrogate(code_unit));
    v->push_back(unibrow::Utf16::TrailSurrogate(code_unit));
  }
}

}  // namespace

template <class CharT>
const ZoneVector<base::uc16>* ParserImpl<CharT>::ParseCaptureGroupName() {
  // Due to special Advance requirements (see the next comment), rewind by one
  // such that names starting with a surrogate pair are parsed correctly for
  // patterns where the unicode flag is unset.
  //
  // Note that we use this odd pattern of rewinding the last advance in order
  // to adhere to the common parser behavior of expecting `current` to point at
  // the first candidate character for a function (e.g. when entering ParseFoo,
  // `current` should point at the first character of Foo).
  RewindByOneCodepoint();

  ZoneVector<base::uc16>* name =
      zone()->template New<ZoneVector<base::uc16>>(zone());

  {
    // Advance behavior inside this function is tricky since
    // RegExpIdentifierName explicitly enables unicode (in spec terms, sets +U)
    // and thus allows surrogate pairs and \u{}-style escapes even in
    // non-unicode patterns. Therefore Advance within the capture group name
    // has to force-enable unicode, and outside the name revert to default
    // behavior.
    ForceUnicodeScope force_unicode(this);

    bool at_start = true;
    while (true) {
      Advance();
      base::uc32 c = current();

      // Convert unicode escapes.
      if (c == '\\' && Next() == 'u') {
        Advance(2);
        if (!ParseUnicodeEscape(&c)) {
          ReportError(Error::kInvalidUnicodeEscape);
          return nullptr;
        }
        RewindByOneCodepoint();
      }

      // The backslash char is misclassified as both ID_Start and ID_Continue.
      if (c == '\\') {
        ReportError(Error::kInvalidCaptureGroupName);
        return nullptr;
      }

      if (at_start) {
        if (!IsIdentifierStart(c)) {
          ReportError(Error::kInvalidCaptureGroupName);
          return nullptr;
        }
        push_code_unit(name, c);
        at_start = false;
      } else {
        if (c == '>') {
          break;
        } else if (IsIdentifierPart(c)) {
          push_code_unit(name, c);
        } else {
          ReportError(Error::kInvalidCaptureGroupName);
          return nullptr;
        }
      }
    }
  }

  // This final advance goes back into the state of pointing at the next
  // relevant char, which the rest of the parser expects. See also the previous
  // comments in this function.
  Advance();
  return name;
}

template <class CharT>
bool ParserImpl<CharT>::CreateNamedCaptureAtIndex(const ParserState* state,
                                                  int index) {
  const ZoneVector<base::uc16>* name = state->capture_name();
  const auto& non_participating_capture_group_intervals =
      state->non_participating_capture_group_intervals();
  DCHECK(0 < index && index <= captures_started_);
  DCHECK_NOT_NULL(name);

  Capture* capture = GetCapture(index);
  DCHECK_NULL(capture->name());

  capture->set_name(name);

  if (named_captures_ == nullptr) {
    named_captures_ =
        zone_->template New<ZoneMap<Capture*, ZoneList<int>*, CaptureNameLess>>(
            zone());
  } else {
    // Check for duplicates and bail if we find any.
    const auto& named_capture_it = named_captures_->find(capture);
    if (named_capture_it != named_captures_->end()) {
      if (v8_flags.js_regexp_duplicate_named_groups) {
        ZoneList<int>* named_capture_indices = named_capture_it->second;
        DCHECK_NOT_NULL(named_capture_indices);
        DCHECK(!named_capture_indices->is_empty());
        for (int named_index : *named_capture_indices) {
          bool is_duplicate = true;
          for (Interval interval : non_participating_capture_group_intervals) {
            DCHECK(!interval.is_empty());
            // We can stop as soon as we are inside one non-participating
            // interval. There can't be a non-participating and participating
            // interval, as intervals are never decreasing.
            if (interval.Contains(named_index)) {
              is_duplicate = false;
              break;
            }
            // Intervals are ordered strictly increasing, so we can stop early
            // when the current interval is past the current index.
            if (named_index <= interval.from()) {
              break;
            }
          }
          if (is_duplicate) {
            ReportError(Error::kDuplicateCaptureGroupName);
            return false;
          }
        }
      } else {
        ReportError(Error::kDuplicateCaptureGroupName);
        return false;
      }
    }
  }
  if (v8_flags.js_regexp_duplicate_named_groups) {
    // Check for nested named captures. This is necessary to find duplicate
    // named captures within the same disjunct.
    ParserState* parent_state = state->previous_state();
    if (parent_state && parent_state->IsInsideCaptureGroup(name)) {
      ReportError(Error::kDuplicateCaptureGroupName);
      return false;
    }
  }

  auto entry = named_captures_->try_emplace(
      capture, zone()->template New<ZoneList<int>>(1, zone()));
  entry.first->second->Add(index, zone());
  return true;
}

template <class CharT>
bool ParserImpl<CharT>::ParseNamedBackReference(Builder* builder,
                                                ParserState* state) {
  // The parser is assumed to be on the '<' in \k<name>.
  if (current() != '<') {
    ReportError(Error::kInvalidNamedReference);
    return false;
  }

  Advance();
  const ZoneVector<base::uc16>* name = ParseCaptureGroupName();
  if (name == nullptr) {
    return false;
  }

  if (state->IsInsideCaptureGroup(name)) {
    builder->AddEmpty();
  } else {
    BackReference* atom = zone()->template New<BackReference>(zone());
    atom->set_name(name);

    builder->AddAtom(atom);

    if (named_back_references_ == nullptr) {
      named_back_references_ =
          zone()->template New<ZoneList<BackReference*>>(1, zone());
    }
    named_back_references_->Add(atom, zone());
  }

  return true;
}

template <class CharT>
void ParserImpl<CharT>::PatchNamedBackReferences() {
  if (named_back_references_ == nullptr) return;

  if (named_captures_ == nullptr) {
    ReportError(Error::kInvalidNamedCaptureReference);
    return;
  }

  // Look up and patch the actual capture for each named back reference.

  for (int i = 0; i < named_back_references_->length(); i++) {
    BackReference* ref = named_back_references_->at(i);

    // Capture used to search the named_captures_ by name, index of the
    // capture is never used.
    static const int kInvalidIndex = 0;
    Capture* search_capture = zone()->template New<Capture>(kInvalidIndex);
    DCHECK_NULL(search_capture->name());
    search_capture->set_name(ref->name());

    const auto& capture_it = named_captures_->find(search_capture);
    if (capture_it == named_captures_->end()) {
      ReportError(Error::kInvalidNamedCaptureReference);
      return;
    }

    DCHECK_IMPLIES(!v8_flags.js_regexp_duplicate_named_groups,
                   capture_it->second->length() == 1);
    for (int index : *capture_it->second) {
      ref->add_capture(GetCapture(index), zone());
    }
  }
}

template <class CharT>
Capture* ParserImpl<CharT>::GetCapture(int index) {
  // The index for the capture groups are one-based. Its index in the list is
  // zero-based.
  const int known_captures =
      is_scanned_for_captures_ ? capture_count_ : captures_started_;
  SBXCHECK(index >= 1 && index <= known_captures);
  if (captures_ == nullptr) {
    captures_ =
        zone()->template New<ZoneList<Capture*>>(known_captures, zone());
  }
  while (captures_->length() < known_captures) {
    captures_->Add(zone()->template New<Capture>(captures_->length() + 1),
                   zone());
  }
  return captures_->at(index - 1);
}

template <class CharT>
ZoneVector<Capture*>* ParserImpl<CharT>::GetNamedCaptures() {
  if (named_captures_ == nullptr) {
    return nullptr;
  }
  DCHECK(!named_captures_->empty());

  ZoneVector<Capture*>* flattened_named_captures =
      zone()->template New<ZoneVector<Capture*>>(zone());
  for (auto capture : *named_captures_) {
    DCHECK_IMPLIES(!v8_flags.js_regexp_duplicate_named_groups,
                   capture.second->length() == 1);
    for (int index : *capture.second) {
      flattened_named_captures->push_back(GetCapture(index));
    }
  }
  return flattened_named_captures;
}

template <class CharT>
bool ParserImpl<CharT>::HasNamedCaptures(
    InClassEscapeState in_class_escape_state) {
  if (has_named_captures_ || is_scanned_for_captures_) {
    return has_named_captures_;
  }

  ScanForCaptures(in_class_escape_state);
  DCHECK(is_scanned_for_captures_);
  return has_named_captures_;
}

// QuantifierPrefix ::
//   { DecimalDigits }
//   { DecimalDigits , }
//   { DecimalDigits , DecimalDigits }
//
// Returns true if parsing succeeds, and set the min_out and max_out
// values. Values are truncated to Tree::kInfinity if they overflow.
template <class CharT>
bool ParserImpl<CharT>::ParseIntervalQuantifier(int* min_out, int* max_out) {
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
    if (min > (Tree::kInfinity - next) / 10) {
      // Overflow. Skip past remaining decimal digits and return -1.
      do {
        Advance();
      } while (IsDecimalDigit(current()));
      min = Tree::kInfinity;
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
      max = Tree::kInfinity;
      Advance();
    } else {
      while (IsDecimalDigit(current())) {
        int next = current() - '0';
        if (max > (Tree::kInfinity - next) / 10) {
          do {
            Advance();
          } while (IsDecimalDigit(current()));
          max = Tree::kInfinity;
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

template <class CharT>
base::uc32 ParserImpl<CharT>::ParseOctalLiteral() {
  DCHECK(('0' <= current() && current() <= '7') || !has_more());
  // For compatibility with some other browsers (not all), we parse
  // up to three octal digits with a value below 256.
  // ES#prod-annexB-LegacyOctalEscapeSequence
  base::uc32 value = current() - '0';
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

template <class CharT>
bool ParserImpl<CharT>::ParseHexEscape(int length, base::uc32* value) {
  int start = position();
  base::uc32 val = 0;
  for (int i = 0; i < length; ++i) {
    base::uc32 c = current();
    int d = base::HexValue(c);
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
template <class CharT>
bool ParserImpl<CharT>::ParseUnicodeEscape(base::uc32* value) {
  // Accept both \uxxxx and \u{xxxxxx} (if harmony unicode escapes are
  // allowed). In the latter case, the number of hex digits between { } is
  // arbitrary. \ and u have already been read.
  if (current() == '{' && IsUnicodeMode()) {
    int start = position();
    Advance();
    if (ParseUnlimitedLengthHexNumber(0x10FFFF, value)) {
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
  if (result && IsUnicodeMode() && unibrow::Utf16::IsLeadSurrogate(*value) &&
      current() == '\\') {
    // Attempt to read trail surrogate.
    int start = position();
    if (Next() == 'u') {
      Advance(2);
      base::uc32 trail;
      if (ParseHexEscape(4, &trail) &&
          unibrow::Utf16::IsTrailSurrogate(trail)) {
        *value = unibrow::Utf16::CombineSurrogatePair(
            static_cast<base::uc16>(*value), static_cast<base::uc16>(trail));
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
  if (short_name != nullptr && strcmp(property_name, short_name) == 0)
    return true;
  for (int i = 0;; i++) {
    const char* long_name = u_getPropertyName(
        property, static_cast<UPropertyNameChoice>(U_LONG_PROPERTY_NAME + i));
    if (long_name == nullptr) break;
    if (strcmp(property_name, long_name) == 0) return true;
  }
  return false;
}

bool IsExactPropertyValueAlias(const char* property_value_name,
                               UProperty property, int32_t property_value) {
  const char* short_name =
      u_getPropertyValueName(property, property_value, U_SHORT_PROPERTY_NAME);
  if (short_name != nullptr && strcmp(property_value_name, short_name) == 0) {
    return true;
  }
  for (int i = 0;; i++) {
    const char* long_name = u_getPropertyValueName(
        property, property_value,
        static_cast<UPropertyNameChoice>(U_LONG_PROPERTY_NAME + i));
    if (long_name == nullptr) break;
    if (strcmp(property_value_name, long_name) == 0) return true;
  }
  return false;
}

void ExtractStringsFromUnicodeSet(const icu::UnicodeSet& set,
                                  CharacterClassStrings* strings, Flags flags,
                                  Zone* zone) {
  DCHECK(set.hasStrings());
  DCHECK(IsUnicodeSets(flags));
  DCHECK_NOT_NULL(strings);

  TextBuilder::SmallTreeVector string_storage(zone);
  TextBuilder string_builder(zone, &string_storage, flags);
  const bool needs_case_folding = IsIgnoreCase(flags);
  icu::UnicodeSetIterator iter(set);
  iter.skipToStrings();
  while (iter.next()) {
    const icu::UnicodeString& s = iter.getString();
    const char16_t* p = s.getBuffer();
    int32_t length = s.length();
    ZoneList<base::uc32>* string =
        zone->template New<ZoneList<base::uc32>>(length, zone);
    for (int32_t i = 0; i < length;) {
      UChar32 c;
      U16_NEXT(p, i, length, c);
      string_builder.AddUnicodeCharacter(c);
      if (needs_case_folding) {
        c = u_foldCase(c, U_FOLD_CASE_DEFAULT);
      }
      string->Add(c, zone);
    }
    strings->emplace(string->ToVector(), string_builder.ToRegExp());
    string_storage.clear();
  }
}

bool LookupPropertyValueName(UProperty property,
                             const char* property_value_name, bool negate,
                             ZoneList<CharacterRange>* result_ranges,
                             CharacterClassStrings* result_strings, Flags flags,
                             Zone* zone) {
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
    if (set.hasStrings()) {
      ExtractStringsFromUnicodeSet(set, result_strings, flags, zone);
    }
    const bool needs_case_folding = IsUnicodeSets(flags) && IsIgnoreCase(flags);
    if (needs_case_folding) set.closeOver(USET_SIMPLE_CASE_INSENSITIVE);
    set.removeAllStrings();
    if (negate) set.complement();
    for (int i = 0; i < set.getRangeCount(); i++) {
      result_ranges->Add(
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
                                    bool negate, Flags flags, Zone* zone) {
  if (NameEquals(name, "Any")) {
    if (negate) {
      // Leave the list of character ranges empty, since the negation of 'Any'
      // is the empty set.
    } else {
      result->Add(CharacterRange::Everything(), zone);
    }
  } else if (NameEquals(name, "ASCII")) {
    result->Add(negate ? CharacterRange::Range(0x80, String::kMaxCodePoint)
                       : CharacterRange::Range(0x0, 0x7F),
                zone);
  } else if (NameEquals(name, "Assigned")) {
    return LookupPropertyValueName(UCHAR_GENERAL_CATEGORY, "Unassigned",
                                   !negate, result, nullptr, flags, zone);
  } else {
    return false;
  }
  return true;
}

// Explicitly allowlist supported binary properties. The spec forbids supporting
// properties outside of this set to ensure interoperability.
bool IsSupportedBinaryProperty(UProperty property, bool unicode_sets) {
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
    case UCHAR_EMOJI_COMPONENT:
    case UCHAR_EMOJI_MODIFIER_BASE:
    case UCHAR_EMOJI_MODIFIER:
    case UCHAR_EMOJI_PRESENTATION:
    case UCHAR_EXTENDED_PICTOGRAPHIC:
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
    case UCHAR_REGIONAL_INDICATOR:
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
    case UCHAR_BASIC_EMOJI:
    case UCHAR_EMOJI_KEYCAP_SEQUENCE:
    case UCHAR_RGI_EMOJI_MODIFIER_SEQUENCE:
    case UCHAR_RGI_EMOJI_FLAG_SEQUENCE:
    case UCHAR_RGI_EMOJI_TAG_SEQUENCE:
    case UCHAR_RGI_EMOJI_ZWJ_SEQUENCE:
    case UCHAR_RGI_EMOJI:
      return unicode_sets;
    default:
      break;
  }
  return false;
}

bool IsBinaryPropertyOfStrings(UProperty property) {
  switch (property) {
    case UCHAR_BASIC_EMOJI:
    case UCHAR_EMOJI_KEYCAP_SEQUENCE:
    case UCHAR_RGI_EMOJI_MODIFIER_SEQUENCE:
    case UCHAR_RGI_EMOJI_FLAG_SEQUENCE:
    case UCHAR_RGI_EMOJI_TAG_SEQUENCE:
    case UCHAR_RGI_EMOJI_ZWJ_SEQUENCE:
    case UCHAR_RGI_EMOJI:
      return true;
    default:
      break;
  }
  return false;
}

bool IsUnicodePropertyValueCharacter(char c) {
  // https://tc39.es/proposal-regexp-unicode-property-escapes/
  //
  // Note that using this to validate each parsed char is quite conservative.
  // A possible alternative solution would be to only ensure the parsed
  // property name/value candidate string does not contain '\0' characters and
  // let ICU lookups trigger the final failure.
  if ('a' <= c && c <= 'z') return true;
  if ('A' <= c && c <= 'Z') return true;
  if ('0' <= c && c <= '9') return true;
  return (c == '_');
}

}  // namespace

template <class CharT>
bool ParserImpl<CharT>::ParsePropertyClassName(ZoneVector<char>* name_1,
                                               ZoneVector<char>* name_2) {
  DCHECK(name_1->empty());
  DCHECK(name_2->empty());
  // Parse the property class as follows:
  // - In \p{name}, 'name' is interpreted
  //   - either as a general category property value name.
  //   - or as a binary property name.
  // - In \p{name=value}, 'name' is interpreted as an enumerated property name,
  //   and 'value' is interpreted as one of the available property value names.
  // - Aliases in PropertyAlias.txt and PropertyValueAlias.txt can be used.
  // - Loose matching is not applied.
  if (current() == '{') {
    // Parse \p{[PropertyName=]PropertyNameValue}
    for (Advance(); current() != '}' && current() != '='; Advance()) {
      if (!IsUnicodePropertyValueCharacter(current())) return false;
      if (!has_next()) return false;
      name_1->push_back(static_cast<char>(current()));
    }
    if (current() == '=') {
      for (Advance(); current() != '}'; Advance()) {
        if (!IsUnicodePropertyValueCharacter(current())) return false;
        if (!has_next()) return false;
        name_2->push_back(static_cast<char>(current()));
      }
      name_2->push_back(0);  // null-terminate string.
    }
  } else {
    return false;
  }
  Advance();
  name_1->push_back(0);  // null-terminate string.

  DCHECK(name_1->size() - 1 == std::strlen(name_1->data()));
  DCHECK(name_2->empty() || name_2->size() - 1 == std::strlen(name_2->data()));
  return true;
}

template <class CharT>
bool ParserImpl<CharT>::AddPropertyClassRange(
    ZoneList<CharacterRange>* add_to_ranges,
    CharacterClassStrings* add_to_strings, bool negate,
    const ZoneVector<char>& name_1, const ZoneVector<char>& name_2) {
  if (name_2.empty()) {
    // First attempt to interpret as general category property value name.
    const char* name = name_1.data();
    if (LookupPropertyValueName(UCHAR_GENERAL_CATEGORY_MASK, name, negate,
                                add_to_ranges, add_to_strings, flags(),
                                zone())) {
      return true;
    }
    // Interpret "Any", "ASCII", and "Assigned".
    if (LookupSpecialPropertyValueName(name, add_to_ranges, negate, flags(),
                                       zone())) {
      return true;
    }
    // Then attempt to interpret as binary property name with value name 'Y'.
    UProperty property = u_getPropertyEnum(name);
    if (!IsSupportedBinaryProperty(property, unicode_sets())) return false;
    if (!IsExactPropertyAlias(name, property)) return false;
    // Negation of properties with strings is not allowed.
    // See
    // https://tc39.es/ecma262/#sec-static-semantics-maycontainstrings
    if (negate && IsBinaryPropertyOfStrings(property)) return false;
    if (unicode_sets()) {
      // In /v mode we can't simple lookup the "false" binary property values,
      // as the spec requires us to perform case folding before calculating the
      // complement.
      // See https://tc39.es/ecma262/#sec-compiletocharset
      // UnicodePropertyValueExpression :: LoneUnicodePropertyNameOrValue
      return LookupPropertyValueName(property, "Y", negate, add_to_ranges,
                                     add_to_strings, flags(), zone());
    } else {
      return LookupPropertyValueName(property, negate ? "N" : "Y", false,
                                     add_to_ranges, add_to_strings, flags(),
                                     zone());
    }
  } else {
    // Both property name and value name are specified. Attempt to interpret
    // the property name as enumerated property.
    const char* property_name = name_1.data();
    const char* value_name = name_2.data();
    UProperty property = u_getPropertyEnum(property_name);
    if (!IsExactPropertyAlias(property_name, property)) return false;
    if (property == UCHAR_GENERAL_CATEGORY) {
      // We want to allow aggregate value names such as "Letter".
      property = UCHAR_GENERAL_CATEGORY_MASK;
    } else if (property != UCHAR_SCRIPT &&
               property != UCHAR_SCRIPT_EXTENSIONS) {
      return false;
    }
    return LookupPropertyValueName(property, value_name, negate, add_to_ranges,
                                   add_to_strings, flags(), zone());
  }
}

#else  // V8_INTL_SUPPORT

template <class CharT>
bool ParserImpl<CharT>::ParsePropertyClassName(ZoneVector<char>* name_1,
                                               ZoneVector<char>* name_2) {
  return false;
}

template <class CharT>
bool ParserImpl<CharT>::AddPropertyClassRange(
    ZoneList<CharacterRange>* add_to_ranges,
    CharacterClassStrings* add_to_strings, bool negate,
    const ZoneVector<char>& name_1, const ZoneVector<char>& name_2) {
  return false;
}

#endif  // V8_INTL_SUPPORT

template <class CharT>
bool ParserImpl<CharT>::ParseUnlimitedLengthHexNumber(int max_value,
                                                      base::uc32* value) {
  base::uc32 x = 0;
  int d = base::HexValue(current());
  if (d < 0) {
    return false;
  }
  while (d >= 0) {
    x = x * 16 + d;
    if (x > static_cast<base::uc32>(max_value)) {
      return false;
    }
    Advance();
    d = base::HexValue(current());
  }
  *value = x;
  return true;
}

// https://tc39.es/ecma262/#prod-CharacterEscape
template <class CharT>
base::uc32 ParserImpl<CharT>::ParseCharacterEscape(
    InClassEscapeState in_class_escape_state,
    bool* is_escaped_unicode_character) {
  DCHECK_EQ('\\', current());
  DCHECK(has_next());

  Advance();

  const base::uc32 c = current();
  switch (c) {
    // CharacterEscape ::
    //   ControlEscape :: one of
    //     f n r t v
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
    // CharacterEscape ::
    //   c ControlLetter
    case 'c': {
      base::uc32 controlLetter = Next();
      base::uc32 letter = controlLetter & ~('A' ^ 'a');
      if (letter >= 'A' && letter <= 'Z') {
        Advance(2);
        // Control letters mapped to ASCII control characters in the range
        // 0x00-0x1F.
        return controlLetter & 0x1F;
      }
      if (IsUnicodeMode()) {
        // With /u and /v, invalid escapes are not treated as identity escapes.
        ReportError(Error::kInvalidUnicodeEscape);
        return 0;
      }
      if (in_class_escape_state == InClassEscapeState::kInClass) {
        // Inside a character class, we also accept digits and underscore as
        // control characters, unless with /u or /v. See Annex B:
        // ES#prod-annexB-ClassControlLetter
        if ((controlLetter >= '0' && controlLetter <= '9') ||
            controlLetter == '_') {
          Advance(2);
          return controlLetter & 0x1F;
        }
      }
      // We match JSC in reading the backslash as a literal
      // character instead of as starting an escape.
      return '\\';
    }
    // CharacterEscape ::
    //   0 [lookahead ∉ DecimalDigit]
    //   [~UnicodeMode] LegacyOctalEscapeSequence
    case '0':
      // \0 is interpreted as NUL if not followed by another digit.
      if (Next() < '0' || Next() > '9') {
        Advance();
        return 0;
      }
      [[fallthrough]];
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
      if (IsUnicodeMode()) {
        // With /u or /v, decimal escape is not interpreted as octal character
        // code.
        ReportError(Error::kInvalidDecimalEscape);
        return 0;
      }
      return ParseOctalLiteral();
    // CharacterEscape ::
    //   HexEscapeSequence
    case 'x': {
      Advance();
      base::uc32 value;
      if (ParseHexEscape(2, &value)) return value;
      if (IsUnicodeMode()) {
        // With /u or /v, invalid escapes are not treated as identity escapes.
        ReportError(Error::kInvalidEscape);
        return 0;
      }
      // If \x is not followed by a two-digit hexadecimal, treat it
      // as an identity escape.
      return 'x';
    }
    // CharacterEscape ::
    //   RegExpUnicodeEscapeSequence [?UnicodeMode]
    case 'u': {
      Advance();
      base::uc32 value;
      if (ParseUnicodeEscape(&value)) {
        *is_escaped_unicode_character = true;
        return value;
      }
      if (IsUnicodeMode()) {
        // With /u or /v, invalid escapes are not treated as identity escapes.
        ReportError(Error::kInvalidUnicodeEscape);
        return 0;
      }
      // If \u is not followed by a two-digit hexadecimal, treat it
      // as an identity escape.
      return 'u';
    }
    default:
      break;
  }

  // CharacterEscape ::
  //   IdentityEscape[?UnicodeMode, ?N]
  //
  // * With /u, no identity escapes except for syntax characters are
  //   allowed.
  // * With /v, no identity escapes except for syntax characters and
  //   ClassSetReservedPunctuators (if within a class) are allowed.
  // * Without /u or /v:
  //   * '\c' is not an IdentityEscape.
  //   * '\k' is not an IdentityEscape when named captures exist.
  //   * Otherwise, all identity escapes are allowed.
  if (unicode_sets() && in_class_escape_state == InClassEscapeState::kInClass) {
    if (IsClassSetReservedPunctuator(c)) {
      Advance();
      return c;
    }
  }
  if (IsUnicodeMode()) {
    if (!IsSyntaxCharacterOrSlash(c)) {
      ReportError(Error::kInvalidEscape);
      return 0;
    }
    Advance();
    return c;
  }
  DCHECK(!IsUnicodeMode());
  if (c == 'c') {
    ReportError(Error::kInvalidEscape);
    return 0;
  }
  Advance();
  // Note: It's important to Advance before the HasNamedCaptures call s.t. we
  // don't start scanning in the middle of an escape.
  if (c == 'k' && HasNamedCaptures(in_class_escape_state)) {
    ReportError(Error::kInvalidEscape);
    return 0;
  }
  return c;
}

// https://tc39.es/ecma262/#prod-ClassRanges
template <class CharT>
Tree* ParserImpl<CharT>::ParseClassRanges(ZoneList<CharacterRange>* ranges,
                                          bool add_unicode_case_equivalents) {
  base::uc32 char_1, char_2;
  bool is_class_1, is_class_2;
  while (has_more() && current() != ']') {
    ParseClassEscape(ranges, zone(), add_unicode_case_equivalents, &char_1,
                     &is_class_1 CHECK_FAILED);
    // ClassAtom
    if (current() == '-') {
      Advance();
      if (!has_more()) {
        // If we reach the end we break out of the loop and let the
        // following code report an error.
        break;
      } else if (current() == ']') {
        if (!is_class_1) ranges->Add(CharacterRange::Singleton(char_1), zone());
        ranges->Add(CharacterRange::Singleton('-'), zone());
        break;
      }
      ParseClassEscape(ranges, zone(), add_unicode_case_equivalents, &char_2,
                       &is_class_2 CHECK_FAILED);
      if (is_class_1 || is_class_2) {
        // Either end is an escaped character class. Treat the '-' verbatim.
        if (IsUnicodeMode()) {
          // ES2015 21.2.2.15.1 step 1.
          return ReportError(Error::kInvalidCharacterClass);
        }
        if (!is_class_1) ranges->Add(CharacterRange::Singleton(char_1), zone());
        ranges->Add(CharacterRange::Singleton('-'), zone());
        if (!is_class_2) ranges->Add(CharacterRange::Singleton(char_2), zone());
        continue;
      }
      // ES2015 21.2.2.15.1 step 6.
      if (char_1 > char_2) {
        return ReportError(Error::kOutOfOrderCharacterClass);
      }
      ranges->Add(CharacterRange::Range(char_1, char_2), zone());
    } else {
      if (!is_class_1) ranges->Add(CharacterRange::Singleton(char_1), zone());
    }
  }
  return nullptr;
}

// https://tc39.es/ecma262/#prod-ClassEscape
template <class CharT>
void ParserImpl<CharT>::ParseClassEscape(ZoneList<CharacterRange>* ranges,
                                         Zone* zone,
                                         bool add_unicode_case_equivalents,
                                         base::uc32* char_out,
                                         bool* is_class_escape) {
  *is_class_escape = false;

  if (current() != '\\') {
    // Not a ClassEscape.
    *char_out = current();
    Advance();
    return;
  }

  const base::uc32 next = Next();
  switch (next) {
    case 'b':
      *char_out = '\b';
      Advance(2);
      return;
    case '-':
      if (IsUnicodeMode()) {
        *char_out = next;
        Advance(2);
        return;
      }
      break;
    case kEndMarker:
      ReportError(Error::kEscapeAtEndOfPattern);
      return;
    default:
      break;
  }

  static constexpr InClassEscapeState kInClassEscape =
      InClassEscapeState::kInClass;
  *is_class_escape =
      TryParseCharacterClassEscape(next, kInClassEscape, ranges, nullptr, zone,
                                   add_unicode_case_equivalents);
  if (*is_class_escape) return;

  bool dummy = false;  // Unused.
  *char_out = ParseCharacterEscape(kInClassEscape, &dummy);
}

// https://tc39.es/ecma262/#prod-CharacterClassEscape
template <class CharT>
bool ParserImpl<CharT>::TryParseCharacterClassEscape(
    base::uc32 next, InClassEscapeState in_class_escape_state,
    ZoneList<CharacterRange>* ranges, CharacterClassStrings* strings,
    Zone* zone, bool add_unicode_case_equivalents) {
  DCHECK_EQ(current(), '\\');
  DCHECK_EQ(Next(), next);

  switch (next) {
    case 'd':
    case 'D':
    case 's':
    case 'S':
    case 'w':
    case 'W':
      CharacterRange::AddClassEscape(static_cast<StandardCharacterSet>(next),
                                     ranges, add_unicode_case_equivalents,
                                     zone);
      Advance(2);
      return true;
    case 'p':
    case 'P': {
      if (!IsUnicodeMode()) return false;
      bool negate = next == 'P';
      Advance(2);
      ZoneVector<char> name_1(zone);
      ZoneVector<char> name_2(zone);
      if (!ParsePropertyClassName(&name_1, &name_2) ||
          !AddPropertyClassRange(ranges, strings, negate, name_1, name_2)) {
        ReportError(in_class_escape_state == InClassEscapeState::kInClass
                        ? Error::kInvalidClassPropertyName
                        : Error::kInvalidPropertyName);
      }
      return true;
    }
    default:
      return false;
  }
}

namespace {

// Add |string| to |ranges| if length of |string| == 1, otherwise add |string|
// to |strings|.
void AddClassString(ZoneList<base::uc32>* normalized_string,
                    Tree* regexp_string, ZoneList<CharacterRange>* ranges,
                    CharacterClassStrings* strings, Zone* zone) {
  if (normalized_string->length() == 1) {
    ranges->Add(CharacterRange::Singleton(normalized_string->at(0)), zone);
  } else {
    strings->emplace(normalized_string->ToVector(), regexp_string);
  }
}

}  // namespace

// https://tc39.es/ecma262/#prod-ClassStringDisjunction
template <class CharT>
Tree* ParserImpl<CharT>::ParseClassStringDisjunction(
    ZoneList<CharacterRange>* ranges, CharacterClassStrings* strings) {
  DCHECK(unicode_sets());
  DCHECK_EQ(current(), '\\');
  DCHECK_EQ(Next(), 'q');
  Advance(2);
  if (current() != '{') {
    // Identity escape of 'q' is not allowed in unicode mode.
    return ReportError(Error::kInvalidEscape);
  }
  Advance();

  ZoneList<base::uc32>* string =
      zone()->template New<ZoneList<base::uc32>>(4, zone());
  TextBuilder::SmallTreeVector string_storage(zone());
  TextBuilder string_builder(zone(), &string_storage, flags());

  while (has_more() && current() != '}') {
    if (current() == '|') {
      AddClassString(string, string_builder.ToRegExp(), ranges, strings,
                     zone());
      string = zone()->template New<ZoneList<base::uc32>>(4, zone());
      string_storage.clear();
      Advance();
    } else {
      base::uc32 c = ParseClassSetCharacter(CHECK_FAILED);
      if (ignore_case()) {
#ifdef V8_INTL_SUPPORT
        c = u_foldCase(c, U_FOLD_CASE_DEFAULT);
#else
        c = AsciiAlphaToLower(c);
#endif
      }
      string->Add(c, zone());
      string_builder.AddUnicodeCharacter(c);
    }
  }

  AddClassString(string, string_builder.ToRegExp(), ranges, strings, zone());
  CharacterRange::Canonicalize(ranges);

  // We don't need to handle missing closing '}' here.
  // If the character class is correctly closed, ParseClassSetCharacter will
  // report an error.
  Advance();
  return nullptr;
}

// https://tc39.es/ecma262/#prod-ClassSetOperand
// Tree returned based on type_out:
//  * kNestedClass: ClassSetExpression
//  * For all other types: ClassSetOperand
template <class CharT>
Tree* ParserImpl<CharT>::ParseClassSetOperand(const Builder* builder,
                                              ClassSetOperandType* type_out) {
  ZoneList<CharacterRange>* ranges =
      zone()->template New<ZoneList<CharacterRange>>(1, zone());
  CharacterClassStrings* strings =
      zone()->template New<CharacterClassStrings>(zone());
  base::uc32 character;
  Tree* tree = ParseClassSetOperand(builder, type_out, ranges, strings,
                                    &character CHECK_FAILED);
  DCHECK_IMPLIES(*type_out != ClassSetOperandType::kNestedClass,
                 tree == nullptr);
  DCHECK_IMPLIES(*type_out == ClassSetOperandType::kClassSetCharacter,
                 ranges->is_empty());
  DCHECK_IMPLIES(*type_out == ClassSetOperandType::kClassSetCharacter,
                 strings->empty());
  DCHECK_IMPLIES(*type_out == ClassSetOperandType::kNestedClass,
                 ranges->is_empty());
  DCHECK_IMPLIES(*type_out == ClassSetOperandType::kNestedClass,
                 strings->empty());
  DCHECK_IMPLIES(*type_out == ClassSetOperandType::kNestedClass,
                 tree->IsClassSetExpression());
  // ClassSetRange is only used within ClassSetUnion().
  DCHECK_NE(*type_out, ClassSetOperandType::kClassSetRange);
  // There are no restrictions for kCharacterClassEscape.
  // CharacterClassEscape includes \p{}, which can contain ranges, strings or
  // both and \P{}, which could contain nothing (i.e. \P{Any}).
  if (tree == nullptr) {
    if (*type_out == ClassSetOperandType::kClassSetCharacter) {
      AddMaybeSimpleCaseFoldedRange(ranges,
                                    CharacterRange::Singleton(character));
    }
    tree = zone()->template New<ClassSetOperand>(ranges, strings);
  }
  return tree;
}

// https://tc39.es/ecma262/#prod-ClassSetOperand
// Based on |type_out| either a tree is returned or
// |ranges|/|strings|/|character| modified. If a tree is returned,
// ranges/strings are not modified. If |type_out| is kNestedClass, a tree of
// type ClassSetExpression is returned. If | type_out| is
// kClassSetCharacter, |character| is set and nullptr returned. For all other
// types, |ranges|/|strings|/|character| is modified and nullptr is returned.
template <class CharT>
Tree* ParserImpl<CharT>::ParseClassSetOperand(const Builder* builder,
                                              ClassSetOperandType* type_out,
                                              ZoneList<CharacterRange>* ranges,
                                              CharacterClassStrings* strings,
                                              base::uc32* character) {
  DCHECK(unicode_sets());
  base::uc32 c = current();
  if (c == '\\') {
    const base::uc32 next = Next();
    if (next == 'q') {
      *type_out = ClassSetOperandType::kClassStringDisjunction;
      ParseClassStringDisjunction(ranges, strings CHECK_FAILED);
      return nullptr;
    }
    static constexpr InClassEscapeState kInClassEscape =
        InClassEscapeState::kInClass;
    const bool add_unicode_case_equivalents = ignore_case();
    if (TryParseCharacterClassEscape(next, kInClassEscape, ranges, strings,
                                     zone(), add_unicode_case_equivalents)) {
      *type_out = ClassSetOperandType::kCharacterClassEscape;
      return nullptr;
    }
  }

  if (c == '[') {
    *type_out = ClassSetOperandType::kNestedClass;
    return ParseCharacterClass(builder);
  }

  *type_out = ClassSetOperandType::kClassSetCharacter;
  c = ParseClassSetCharacter(CHECK_FAILED);
  *character = c;
  return nullptr;
}

template <class CharT>
base::uc32 ParserImpl<CharT>::ParseClassSetCharacter() {
  DCHECK(unicode_sets());
  const base::uc32 c = current();
  if (c == '\\') {
    const base::uc32 next = Next();
    switch (next) {
      case 'b':
        Advance(2);
        return '\b';
      case kEndMarker:
        ReportError(Error::kEscapeAtEndOfPattern);
        return 0;
    }
    static constexpr InClassEscapeState kInClassEscape =
        InClassEscapeState::kInClass;

    bool dummy = false;  // Unused.
    return ParseCharacterEscape(kInClassEscape, &dummy);
  }
  if (IsClassSetSyntaxCharacter(c)) {
    ReportError(Error::kInvalidCharacterInClass);
    return 0;
  }
  if (IsClassSetReservedDoublePunctuator(c)) {
    ReportError(Error::kInvalidClassSetOperation);
    return 0;
  }
  Advance();
  return c;
}

namespace {

bool MayContainStrings(ClassSetOperandType type, Tree* operand) {
  switch (type) {
    case ClassSetOperandType::kClassSetCharacter:
    case ClassSetOperandType::kClassSetRange:
      return false;
    case ClassSetOperandType::kCharacterClassEscape:
    case ClassSetOperandType::kClassStringDisjunction:
      return operand->AsClassSetOperand()->has_strings();
    case ClassSetOperandType::kNestedClass:
      if (operand->IsClassRanges()) return false;
      return operand->AsClassSetExpression()->may_contain_strings();
  }
}

}  // namespace

template <class CharT>
void ParserImpl<CharT>::AddMaybeSimpleCaseFoldedRange(
    ZoneList<CharacterRange>* ranges, CharacterRange new_range) {
  DCHECK(unicode_sets());
  if (ignore_case()) {
    ZoneList<CharacterRange>* new_ranges =
        zone()->template New<ZoneList<CharacterRange>>(2, zone());
    new_ranges->Add(new_range, zone());
    CharacterRange::AddUnicodeCaseEquivalents(new_ranges, zone());
    ranges->AddAll(*new_ranges, zone());
  } else {
    ranges->Add(new_range, zone());
  }
  CharacterRange::Canonicalize(ranges);
}

// https://tc39.es/ecma262/#prod-ClassUnion
template <class CharT>
Tree* ParserImpl<CharT>::ParseClassUnion(const Builder* builder,
                                         bool is_negated, Tree* first_operand,
                                         ClassSetOperandType first_operand_type,
                                         ZoneList<CharacterRange>* ranges,
                                         CharacterClassStrings* strings,
                                         base::uc32 character) {
  DCHECK(unicode_sets());
  ZoneList<Tree*>* operands = zone()->template New<ZoneList<Tree*>>(2, zone());
  bool may_contain_strings = false;
  // Add the lhs to operands if necessary.
  // Either the lhs values were added to |ranges|/|strings| (in which case
  // |first_operand| is nullptr), or the lhs was evaluated to a tree and passed
  // as |first_operand| (in which case |ranges| and |strings| are empty).
  if (first_operand != nullptr) {
    may_contain_strings = MayContainStrings(first_operand_type, first_operand);
    operands->Add(first_operand, zone());
  }
  ClassSetOperandType last_type = first_operand_type;
  while (has_more() && current() != ']') {
    if (current() == '-') {
      // Mix of ClassSetRange and ClassSubtraction is not allowed.
      if (Next() == '-') {
        return ReportError(Error::kInvalidClassSetOperation);
      }
      Advance();
      if (!has_more()) {
        // If we reach the end we break out of the loop and let the
        // following code report an error.
        break;
      }
      // If the lhs and rhs around '-' are both ClassSetCharacters, they
      // represent a character range.
      // In case one of them is not a ClassSetCharacter, it is a syntax error,
      // as '-' can not be used unescaped within a class with /v.
      // See
      // https://tc39.es/ecma262/#prod-ClassSetRange
      if (last_type != ClassSetOperandType::kClassSetCharacter) {
        return ReportError(Error::kInvalidCharacterClass);
      }
      base::uc32 from = character;
      ParseClassSetOperand(builder, &last_type, ranges, strings,
                           &character CHECK_FAILED);
      if (last_type != ClassSetOperandType::kClassSetCharacter) {
        return ReportError(Error::kInvalidCharacterClass);
      }
      if (from > character) {
        return ReportError(Error::kOutOfOrderCharacterClass);
      }
      AddMaybeSimpleCaseFoldedRange(ranges,
                                    CharacterRange::Range(from, character));
      last_type = ClassSetOperandType::kClassSetRange;
    } else {
      DCHECK_NE(current(), '-');
      if (last_type == ClassSetOperandType::kClassSetCharacter) {
        AddMaybeSimpleCaseFoldedRange(ranges,
                                      CharacterRange::Singleton(character));
      }
      Tree* operand = ParseClassSetOperand(builder, &last_type, ranges, strings,
                                           &character CHECK_FAILED);
      if (operand != nullptr) {
        may_contain_strings |= MayContainStrings(last_type, operand);
        // Add the range we started building as operand and reset the current
        // range.
        if (!ranges->is_empty() || !strings->empty()) {
          may_contain_strings |= !strings->empty();
          operands->Add(zone()->template New<ClassSetOperand>(ranges, strings),
                        zone());
          ranges = zone()->template New<ZoneList<CharacterRange>>(2, zone());
          strings = zone()->template New<CharacterClassStrings>(zone());
        }
        operands->Add(operand, zone());
      }
    }
  }

  if (!has_more()) {
    return ReportError(Error::kUnterminatedCharacterClass);
  }

  if (last_type == ClassSetOperandType::kClassSetCharacter) {
    AddMaybeSimpleCaseFoldedRange(ranges, CharacterRange::Singleton(character));
  }

  // Add the range we started building as operand.
  if (!ranges->is_empty() || !strings->empty()) {
    may_contain_strings |= !strings->empty();
    operands->Add(zone()->template New<ClassSetOperand>(ranges, strings),
                  zone());
  }

  DCHECK_EQ(current(), ']');
  Advance();

  if (is_negated && may_contain_strings) {
    return ReportError(Error::kNegatedCharacterClassWithStrings);
  }

  if (operands->is_empty()) {
    // Return empty expression if no operands were added (e.g. [\P{Any}]
    // produces an empty range).
    DCHECK(ranges->is_empty());
    DCHECK(strings->empty());
    return ClassSetExpression::Empty(zone(), is_negated);
  }

  return zone()->template New<ClassSetExpression>(
      ClassSetExpression::OperationType::kUnion, is_negated,
      may_contain_strings, operands);
}

// https://tc39.es/ecma262/#prod-ClassIntersection
template <class CharT>
Tree* ParserImpl<CharT>::ParseClassIntersection(
    const Builder* builder, bool is_negated, Tree* first_operand,
    ClassSetOperandType first_operand_type) {
  DCHECK(unicode_sets());
  DCHECK(current() == '&' && Next() == '&');
  bool may_contain_strings =
      MayContainStrings(first_operand_type, first_operand);
  ZoneList<Tree*>* operands = zone()->template New<ZoneList<Tree*>>(2, zone());
  operands->Add(first_operand, zone());
  while (has_more() && current() != ']') {
    if (current() != '&' || Next() != '&') {
      return ReportError(Error::kInvalidClassSetOperation);
    }
    Advance(2);
    // [lookahead ≠ &]
    if (current() == '&') {
      return ReportError(Error::kInvalidCharacterInClass);
    }

    ClassSetOperandType operand_type;
    Tree* operand = ParseClassSetOperand(builder, &operand_type CHECK_FAILED);
    may_contain_strings &= MayContainStrings(operand_type, operand);
    operands->Add(operand, zone());
  }
  if (!has_more()) {
    return ReportError(Error::kUnterminatedCharacterClass);
  }
  if (is_negated && may_contain_strings) {
    return ReportError(Error::kNegatedCharacterClassWithStrings);
  }
  DCHECK_EQ(current(), ']');
  Advance();
  return zone()->template New<ClassSetExpression>(
      ClassSetExpression::OperationType::kIntersection, is_negated,
      may_contain_strings, operands);
}

// https://tc39.es/ecma262/#prod-ClassSubtraction
template <class CharT>
Tree* ParserImpl<CharT>::ParseClassSubtraction(
    const Builder* builder, bool is_negated, Tree* first_operand,
    ClassSetOperandType first_operand_type) {
  DCHECK(unicode_sets());
  DCHECK(current() == '-' && Next() == '-');
  const bool may_contain_strings =
      MayContainStrings(first_operand_type, first_operand);
  if (is_negated && may_contain_strings) {
    return ReportError(Error::kNegatedCharacterClassWithStrings);
  }
  ZoneList<Tree*>* operands = zone()->template New<ZoneList<Tree*>>(2, zone());
  operands->Add(first_operand, zone());
  while (has_more() && current() != ']') {
    if (current() != '-' || Next() != '-') {
      return ReportError(Error::kInvalidClassSetOperation);
    }
    Advance(2);
    ClassSetOperandType dummy;  // unused
    Tree* operand = ParseClassSetOperand(builder, &dummy CHECK_FAILED);
    operands->Add(operand, zone());
  }
  if (!has_more()) {
    return ReportError(Error::kUnterminatedCharacterClass);
  }
  DCHECK_EQ(current(), ']');
  Advance();
  return zone()->template New<ClassSetExpression>(
      ClassSetExpression::OperationType::kSubtraction, is_negated,
      may_contain_strings, operands);
}

// https://tc39.es/ecma262/#prod-CharacterClass
template <class CharT>
Tree* ParserImpl<CharT>::ParseCharacterClass(const Builder* builder) {
  DCHECK_EQ(current(), '[');
  Advance();
  bool is_negated = false;
  if (current() == '^') {
    is_negated = true;
    Advance();
  }
  ZoneList<CharacterRange>* ranges =
      zone()->template New<ZoneList<CharacterRange>>(2, zone());
  if (current() == ']') {
    Advance();
    if (unicode_sets()) {
      return ClassSetExpression::Empty(zone(), is_negated);
    } else {
      ClassRanges::ClassRangesFlags class_ranges_flags;
      if (is_negated) class_ranges_flags = ClassRanges::NEGATED;
      return zone()->template New<ClassRanges>(zone(), ranges,
                                               class_ranges_flags);
    }
  }

  if (!unicode_sets()) {
    bool add_unicode_case_equivalents = IsUnicodeMode() && ignore_case();
    ParseClassRanges(ranges, add_unicode_case_equivalents CHECK_FAILED);
    if (!has_more()) {
      return ReportError(Error::kUnterminatedCharacterClass);
    }
    DCHECK_EQ(current(), ']');
    Advance();
    ClassRanges::ClassRangesFlags character_class_flags;
    if (is_negated) character_class_flags = ClassRanges::NEGATED;
    if (!ignore_case()) {
      character_class_flags |= ClassRanges::NO_CASE_FOLDING_NEEDED;
    }
    if (sizeof(CharT) == 1 && !is_negated) {
      // No surrogate pairs.
      character_class_flags |= ClassRanges::IS_CERTAINLY_ONE_CODE_POINT;
    }
    return zone()->template New<ClassRanges>(zone(), ranges,
                                             character_class_flags);
  } else {
    ClassSetOperandType operand_type;
    CharacterClassStrings* strings =
        zone()->template New<CharacterClassStrings>(zone());
    base::uc32 character;
    Tree* operand = ParseClassSetOperand(builder, &operand_type, ranges,
                                         strings, &character CHECK_FAILED);
    switch (current()) {
      case '-':
        if (Next() == '-') {
          if (operand == nullptr) {
            if (operand_type == ClassSetOperandType::kClassSetCharacter) {
              AddMaybeSimpleCaseFoldedRange(
                  ranges, CharacterRange::Singleton(character));
            }
            operand = zone()->template New<ClassSetOperand>(ranges, strings);
          }
          return ParseClassSubtraction(builder, is_negated, operand,
                                       operand_type);
        }
        // ClassSetRange is handled in ParseClassUnion().
        break;
      case '&':
        if (Next() == '&') {
          if (operand == nullptr) {
            if (operand_type == ClassSetOperandType::kClassSetCharacter) {
              AddMaybeSimpleCaseFoldedRange(
                  ranges, CharacterRange::Singleton(character));
            }
            operand = zone()->template New<ClassSetOperand>(ranges, strings);
          }
          return ParseClassIntersection(builder, is_negated, operand,
                                        operand_type);
        }
    }
    return ParseClassUnion(builder, is_negated, operand, operand_type, ranges,
                           strings, character);
  }
}

#undef CHECK_FAILED

template <class CharT>
bool ParserImpl<CharT>::Parse(CompileData* result) {
  DCHECK_NOT_NULL(result);
  Tree* tree = ParsePattern();

  if (failed()) {
    DCHECK_NULL(tree);
    DCHECK_NE(error_, Error::kNone);
    result->error = error_;
    result->error_pos = error_pos_;
    return false;
  }

  DCHECK_NOT_NULL(tree);
  DCHECK_EQ(error_, Error::kNone);
#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS
  if (V8_UNLIKELY(v8_flags.trace_regexp_parser)) {
    StdoutStream os;
    AstNodePrinter printer(os, nullptr, zone());
    printer.Print(tree);
    os << "\n";
  }
#endif

  result->tree = tree;
  const int capture_count = captures_started();
  result->simple = tree->IsAtom() && simple() && capture_count == 0;
  result->contains_anchor = contains_anchor();
  result->capture_count = capture_count;
  result->named_captures = GetNamedCaptures();
  return true;
}

void Builder::FlushText() { text_builder().FlushText(); }

void Builder::AddCharacter(base::uc16 c) {
  pending_empty_ = false;
  text_builder().AddCharacter(c);
}

void Builder::AddUnicodeCharacter(base::uc32 c) {
  pending_empty_ = false;
  text_builder().AddUnicodeCharacter(c);
}

void Builder::AddEscapedUnicodeCharacter(base::uc32 character) {
  pending_empty_ = false;
  text_builder().AddEscapedUnicodeCharacter(character);
}

void Builder::AddEmpty() {
  text_builder().FlushPendingSurrogate();
  pending_empty_ = true;
}

void Builder::AddClassRanges(ClassRanges* cc) {
  pending_empty_ = false;
  text_builder().AddClassRanges(cc);
}

void Builder::AddAtom(Tree* term) {
  if (term->IsEmpty()) {
    AddEmpty();
    return;
  }
  pending_empty_ = false;
  if (term->IsTextElement()) {
    text_builder().AddAtom(term);
  } else {
    FlushText();
    terms_.emplace_back(term);
  }
}

void Builder::AddTerm(Tree* term) {
  DCHECK(!term->IsEmpty());
  pending_empty_ = false;
  if (term->IsTextElement()) {
    text_builder().AddTerm(term);
  } else {
    FlushText();
    terms_.emplace_back(term);
  }
}

void Builder::AddAssertion(Tree* assert) {
  FlushText();
  pending_empty_ = false;
  terms_.emplace_back(assert);
}

void Builder::NewAlternative() { FlushTerms(); }

void Builder::FlushTerms() {
  FlushText();
  size_t num_terms = terms_.size();
  Tree* alternative;
  if (num_terms == 0) {
    alternative = zone()->New<Empty>();
  } else if (num_terms == 1) {
    alternative = terms_.back();
  } else {
    alternative = zone()->New<Alternative>(zone()->New<ZoneList<Tree*>>(
        base::VectorOf(terms_.begin(), terms_.size()), zone()));
  }
  alternatives_.emplace_back(alternative);
  terms_.clear();
}

Tree* Builder::ToRegExp() {
  FlushTerms();
  size_t num_alternatives = alternatives_.size();
  if (num_alternatives == 0) return zone()->New<Empty>();
  if (num_alternatives == 1) return alternatives_.back();
  return zone()->New<Disjunction>(zone()->New<ZoneList<Tree*>>(
      base::VectorOf(alternatives_.begin(), alternatives_.size()), zone()));
}

bool Builder::AddQuantifierToAtom(int min, int max, int index,
                                  Quantifier::QuantifierType quantifier_type) {
  if (pending_empty_) {
    pending_empty_ = false;
    return true;
  }
  Tree* atom = text_builder().PopLastAtom();
  if (atom != nullptr) {
    FlushText();
  } else if (!terms_.empty()) {
    atom = terms_.back();
    terms_.pop_back();
    if (atom->IsLookaround()) {
      // With /u or /v, lookarounds are not quantifiable.
      if (IsUnicodeMode()) return false;
      // Lookbehinds are not quantifiable.
      if (atom->AsLookaround()->type() == Lookaround::LOOKBEHIND) {
        return false;
      }
    }
    if (atom->max_match() == 0) {
      // Guaranteed to only match an empty string.
      if (min == 0) {
        return true;
      }
      terms_.emplace_back(atom);
      return true;
    }
  } else {
    // Only call immediately after adding an atom or character!
    UNREACHABLE();
  }
  terms_.emplace_back(
      zone()->New<Quantifier>(min, max, quantifier_type, index, atom));
  return true;
}

template class ParserImpl<uint8_t>;
template class ParserImpl<base::uc16>;

}  // namespace

// static
bool Parser::ParseRegExpFromHeapString(Isolate* isolate, Zone* zone,
                                       DirectHandle<String> input, Flags flags,
                                       CompileData* result) {
  DisallowGarbageCollection no_gc;
  uintptr_t stack_limit = isolate->stack_guard()->real_climit();
  String::FlatContent content = input->GetFlatContent(no_gc);
  if (content.IsOneByte()) {
    base::Vector<const uint8_t> v = content.ToOneByteVector();
    return ParserImpl<uint8_t>{v.begin(),   v.length(), flags,
                               stack_limit, zone,       no_gc}
        .Parse(result);
  } else {
    base::Vector<const base::uc16> v = content.ToUC16Vector();
    return ParserImpl<base::uc16>{v.begin(),   v.length(), flags,
                                  stack_limit, zone,       no_gc}
        .Parse(result);
  }
}

// static
template <class CharT>
bool Parser::VerifyRegExpSyntax(Zone* zone, uintptr_t stack_limit,
                                const CharT* input, int input_length,
                                Flags flags, CompileData* result,
                                const DisallowGarbageCollection& no_gc) {
  return ParserImpl<CharT>{input, input_length, flags, stack_limit, zone, no_gc}
      .Parse(result);
}

template bool Parser::VerifyRegExpSyntax<uint8_t>(
    Zone*, uintptr_t, const uint8_t*, int, Flags, CompileData*,
    const DisallowGarbageCollection&);
template bool Parser::VerifyRegExpSyntax<base::uc16>(
    Zone*, uintptr_t, const base::uc16*, int, Flags, CompileData*,
    const DisallowGarbageCollection&);

}  // namespace regexp
}  // namespace internal
}  // namespace v8

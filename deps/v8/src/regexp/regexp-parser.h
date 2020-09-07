// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_PARSER_H_
#define V8_REGEXP_REGEXP_PARSER_H_

#include "src/objects/js-regexp.h"
#include "src/objects/objects.h"
#include "src/regexp/regexp-ast.h"
#include "src/regexp/regexp-error.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

struct RegExpCompileData;

// A BufferedZoneList is an automatically growing list, just like (and backed
// by) a ZoneList, that is optimized for the case of adding and removing
// a single element. The last element added is stored outside the backing list,
// and if no more than one element is ever added, the ZoneList isn't even
// allocated.
// Elements must not be nullptr pointers.
template <typename T, int initial_size>
class BufferedZoneList {
 public:
  BufferedZoneList() : list_(nullptr), last_(nullptr) {}

  // Adds element at end of list. This element is buffered and can
  // be read using last() or removed using RemoveLast until a new Add or until
  // RemoveLast or GetList has been called.
  void Add(T* value, Zone* zone) {
    if (last_ != nullptr) {
      if (list_ == nullptr) {
        list_ = new (zone) ZoneList<T*>(initial_size, zone);
      }
      list_->Add(last_, zone);
    }
    last_ = value;
  }

  T* last() {
    DCHECK(last_ != nullptr);
    return last_;
  }

  T* RemoveLast() {
    DCHECK(last_ != nullptr);
    T* result = last_;
    if ((list_ != nullptr) && (list_->length() > 0))
      last_ = list_->RemoveLast();
    else
      last_ = nullptr;
    return result;
  }

  T* Get(int i) {
    DCHECK((0 <= i) && (i < length()));
    if (list_ == nullptr) {
      DCHECK_EQ(0, i);
      return last_;
    } else {
      if (i == list_->length()) {
        DCHECK(last_ != nullptr);
        return last_;
      } else {
        return list_->at(i);
      }
    }
  }

  void Clear() {
    list_ = nullptr;
    last_ = nullptr;
  }

  int length() {
    int length = (list_ == nullptr) ? 0 : list_->length();
    return length + ((last_ == nullptr) ? 0 : 1);
  }

  ZoneList<T*>* GetList(Zone* zone) {
    if (list_ == nullptr) {
      list_ = new (zone) ZoneList<T*>(initial_size, zone);
    }
    if (last_ != nullptr) {
      list_->Add(last_, zone);
      last_ = nullptr;
    }
    return list_;
  }

 private:
  ZoneList<T*>* list_;
  T* last_;
};


// Accumulates RegExp atoms and assertions into lists of terms and alternatives.
class RegExpBuilder : public ZoneObject {
 public:
  RegExpBuilder(Zone* zone, JSRegExp::Flags flags);
  void AddCharacter(uc16 character);
  void AddUnicodeCharacter(uc32 character);
  void AddEscapedUnicodeCharacter(uc32 character);
  // "Adds" an empty expression. Does nothing except consume a
  // following quantifier
  void AddEmpty();
  void AddCharacterClass(RegExpCharacterClass* cc);
  void AddCharacterClassForDesugaring(uc32 c);
  void AddAtom(RegExpTree* tree);
  void AddTerm(RegExpTree* tree);
  void AddAssertion(RegExpTree* tree);
  void NewAlternative();  // '|'
  bool AddQuantifierToAtom(int min, int max,
                           RegExpQuantifier::QuantifierType type);
  void FlushText();
  RegExpTree* ToRegExp();
  JSRegExp::Flags flags() const { return flags_; }
  void set_flags(JSRegExp::Flags flags) { flags_ = flags; }

  bool ignore_case() const { return (flags_ & JSRegExp::kIgnoreCase) != 0; }
  bool multiline() const { return (flags_ & JSRegExp::kMultiline) != 0; }
  bool dotall() const { return (flags_ & JSRegExp::kDotAll) != 0; }

 private:
  static const uc16 kNoPendingSurrogate = 0;
  void AddLeadSurrogate(uc16 lead_surrogate);
  void AddTrailSurrogate(uc16 trail_surrogate);
  void FlushPendingSurrogate();
  void FlushCharacters();
  void FlushTerms();
  bool NeedsDesugaringForUnicode(RegExpCharacterClass* cc);
  bool NeedsDesugaringForIgnoreCase(uc32 c);
  Zone* zone() const { return zone_; }
  bool unicode() const { return (flags_ & JSRegExp::kUnicode) != 0; }

  Zone* zone_;
  bool pending_empty_;
  JSRegExp::Flags flags_;
  ZoneList<uc16>* characters_;
  uc16 pending_surrogate_;
  BufferedZoneList<RegExpTree, 2> terms_;
  BufferedZoneList<RegExpTree, 2> text_;
  BufferedZoneList<RegExpTree, 2> alternatives_;
#ifdef DEBUG
  enum { ADD_NONE, ADD_CHAR, ADD_TERM, ADD_ASSERT, ADD_ATOM } last_added_;
#define LAST(x) last_added_ = x;
#else
#define LAST(x)
#endif
};

class V8_EXPORT_PRIVATE RegExpParser {
 public:
  RegExpParser(FlatStringReader* in, JSRegExp::Flags flags, Isolate* isolate,
               Zone* zone);

  static bool ParseRegExp(Isolate* isolate, Zone* zone, FlatStringReader* input,
                          JSRegExp::Flags flags, RegExpCompileData* result);
  static bool VerifyRegExpSyntax(Isolate* isolate, Zone* zone,
                                 FlatStringReader* input, JSRegExp::Flags flags,
                                 RegExpCompileData* result,
                                 const DisallowHeapAllocation& no_gc);

 private:
  bool Parse(RegExpCompileData* result, const DisallowHeapAllocation&);

  RegExpTree* ParsePattern();
  RegExpTree* ParseDisjunction();
  RegExpTree* ParseGroup();

  // Parses a {...,...} quantifier and stores the range in the given
  // out parameters.
  bool ParseIntervalQuantifier(int* min_out, int* max_out);

  // Parses and returns a single escaped character.  The character
  // must not be 'b' or 'B' since they are usually handle specially.
  uc32 ParseClassCharacterEscape();

  // Checks whether the following is a length-digit hexadecimal number,
  // and sets the value if it is.
  bool ParseHexEscape(int length, uc32* value);
  bool ParseUnicodeEscape(uc32* value);
  bool ParseUnlimitedLengthHexNumber(int max_value, uc32* value);

  bool ParsePropertyClassName(ZoneVector<char>* name_1,
                              ZoneVector<char>* name_2);
  bool AddPropertyClassRange(ZoneList<CharacterRange>* add_to, bool negate,
                             const ZoneVector<char>& name_1,
                             const ZoneVector<char>& name_2);

  RegExpTree* GetPropertySequence(const ZoneVector<char>& name_1);
  RegExpTree* ParseCharacterClass(const RegExpBuilder* state);

  uc32 ParseOctalLiteral();

  // Tries to parse the input as a back reference.  If successful it
  // stores the result in the output parameter and returns true.  If
  // it fails it will push back the characters read so the same characters
  // can be reparsed.
  bool ParseBackReferenceIndex(int* index_out);

  // Parse inside a class. Either add escaped class to the range, or return
  // false and pass parsed single character through |char_out|.
  void ParseClassEscape(ZoneList<CharacterRange>* ranges, Zone* zone,
                        bool add_unicode_case_equivalents, uc32* char_out,
                        bool* is_class_escape);

  char ParseClassEscape();

  RegExpTree* ReportError(RegExpError error);
  void Advance();
  void Advance(int dist);
  void Reset(int pos);

  // Reports whether the pattern might be used as a literal search string.
  // Only use if the result of the parse is a single atom node.
  bool simple();
  bool contains_anchor() { return contains_anchor_; }
  void set_contains_anchor() { contains_anchor_ = true; }
  int captures_started() { return captures_started_; }
  int position() { return next_pos_ - 1; }
  bool failed() { return failed_; }
  // The Unicode flag can't be changed using in-regexp syntax, so it's OK to
  // just read the initial flag value here.
  bool unicode() const { return (top_level_flags_ & JSRegExp::kUnicode) != 0; }

  static bool IsSyntaxCharacterOrSlash(uc32 c);

  static const uc32 kEndMarker = (1 << 21);

 private:
  enum SubexpressionType {
    INITIAL,
    CAPTURE,  // All positive values represent captures.
    POSITIVE_LOOKAROUND,
    NEGATIVE_LOOKAROUND,
    GROUPING
  };

  class RegExpParserState : public ZoneObject {
   public:
    // Push a state on the stack.
    RegExpParserState(RegExpParserState* previous_state,
                      SubexpressionType group_type,
                      RegExpLookaround::Type lookaround_type,
                      int disjunction_capture_index,
                      const ZoneVector<uc16>* capture_name,
                      JSRegExp::Flags flags, Zone* zone)
        : previous_state_(previous_state),
          builder_(new (zone) RegExpBuilder(zone, flags)),
          group_type_(group_type),
          lookaround_type_(lookaround_type),
          disjunction_capture_index_(disjunction_capture_index),
          capture_name_(capture_name) {}
    // Parser state of containing expression, if any.
    RegExpParserState* previous_state() const { return previous_state_; }
    bool IsSubexpression() { return previous_state_ != nullptr; }
    // RegExpBuilder building this regexp's AST.
    RegExpBuilder* builder() const { return builder_; }
    // Type of regexp being parsed (parenthesized group or entire regexp).
    SubexpressionType group_type() const { return group_type_; }
    // Lookahead or Lookbehind.
    RegExpLookaround::Type lookaround_type() const { return lookaround_type_; }
    // Index in captures array of first capture in this sub-expression, if any.
    // Also the capture index of this sub-expression itself, if group_type
    // is CAPTURE.
    int capture_index() const { return disjunction_capture_index_; }
    // The name of the current sub-expression, if group_type is CAPTURE. Only
    // used for named captures.
    const ZoneVector<uc16>* capture_name() const { return capture_name_; }

    bool IsNamedCapture() const { return capture_name_ != nullptr; }

    // Check whether the parser is inside a capture group with the given index.
    bool IsInsideCaptureGroup(int index);
    // Check whether the parser is inside a capture group with the given name.
    bool IsInsideCaptureGroup(const ZoneVector<uc16>* name);

   private:
    // Linked list implementation of stack of states.
    RegExpParserState* const previous_state_;
    // Builder for the stored disjunction.
    RegExpBuilder* const builder_;
    // Stored disjunction type (capture, look-ahead or grouping), if any.
    const SubexpressionType group_type_;
    // Stored read direction.
    const RegExpLookaround::Type lookaround_type_;
    // Stored disjunction's capture index (if any).
    const int disjunction_capture_index_;
    // Stored capture name (if any).
    const ZoneVector<uc16>* const capture_name_;
  };

  // Return the 1-indexed RegExpCapture object, allocate if necessary.
  RegExpCapture* GetCapture(int index);

  // Creates a new named capture at the specified index. Must be called exactly
  // once for each named capture. Fails if a capture with the same name is
  // encountered.
  bool CreateNamedCaptureAtIndex(const ZoneVector<uc16>* name, int index);

  // Parses the name of a capture group (?<name>pattern). The name must adhere
  // to IdentifierName in the ECMAScript standard.
  const ZoneVector<uc16>* ParseCaptureGroupName();

  bool ParseNamedBackReference(RegExpBuilder* builder,
                               RegExpParserState* state);
  RegExpParserState* ParseOpenParenthesis(RegExpParserState* state);

  // After the initial parsing pass, patch corresponding RegExpCapture objects
  // into all RegExpBackReferences. This is done after initial parsing in order
  // to avoid complicating cases in which references comes before the capture.
  void PatchNamedBackReferences();

  Handle<FixedArray> CreateCaptureNameMap();

  // Returns true iff the pattern contains named captures. May call
  // ScanForCaptures to look ahead at the remaining pattern.
  bool HasNamedCaptures();

  Isolate* isolate() { return isolate_; }
  Zone* zone() const { return zone_; }

  uc32 current() { return current_; }
  bool has_more() { return has_more_; }
  bool has_next() { return next_pos_ < in()->length(); }
  uc32 Next();
  template <bool update_position>
  uc32 ReadNext();
  FlatStringReader* in() { return in_; }
  void ScanForCaptures();

  struct RegExpCaptureNameLess {
    bool operator()(const RegExpCapture* lhs, const RegExpCapture* rhs) const {
      DCHECK_NOT_NULL(lhs);
      DCHECK_NOT_NULL(rhs);
      return *lhs->name() < *rhs->name();
    }
  };

  Isolate* isolate_;
  Zone* zone_;
  RegExpError error_ = RegExpError::kNone;
  int error_pos_ = 0;
  ZoneList<RegExpCapture*>* captures_;
  ZoneSet<RegExpCapture*, RegExpCaptureNameLess>* named_captures_;
  ZoneList<RegExpBackReference*>* named_back_references_;
  FlatStringReader* in_;
  uc32 current_;
  // These are the flags specified outside the regexp syntax ie after the
  // terminating '/' or in the second argument to the constructor.  The current
  // flags are stored on the RegExpBuilder.
  JSRegExp::Flags top_level_flags_;
  int next_pos_;
  int captures_started_;
  int capture_count_;  // Only valid after we have scanned for captures.
  bool has_more_;
  bool simple_;
  bool contains_anchor_;
  bool is_scanned_for_captures_;
  bool has_named_captures_;  // Only valid after we have scanned for captures.
  bool failed_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_PARSER_H_

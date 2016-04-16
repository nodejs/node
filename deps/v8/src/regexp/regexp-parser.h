// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_PARSER_H_
#define V8_REGEXP_REGEXP_PARSER_H_

#include "src/objects.h"
#include "src/regexp/regexp-ast.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

struct RegExpCompileData;


// A BufferedZoneList is an automatically growing list, just like (and backed
// by) a ZoneList, that is optimized for the case of adding and removing
// a single element. The last element added is stored outside the backing list,
// and if no more than one element is ever added, the ZoneList isn't even
// allocated.
// Elements must not be NULL pointers.
template <typename T, int initial_size>
class BufferedZoneList {
 public:
  BufferedZoneList() : list_(NULL), last_(NULL) {}

  // Adds element at end of list. This element is buffered and can
  // be read using last() or removed using RemoveLast until a new Add or until
  // RemoveLast or GetList has been called.
  void Add(T* value, Zone* zone) {
    if (last_ != NULL) {
      if (list_ == NULL) {
        list_ = new (zone) ZoneList<T*>(initial_size, zone);
      }
      list_->Add(last_, zone);
    }
    last_ = value;
  }

  T* last() {
    DCHECK(last_ != NULL);
    return last_;
  }

  T* RemoveLast() {
    DCHECK(last_ != NULL);
    T* result = last_;
    if ((list_ != NULL) && (list_->length() > 0))
      last_ = list_->RemoveLast();
    else
      last_ = NULL;
    return result;
  }

  T* Get(int i) {
    DCHECK((0 <= i) && (i < length()));
    if (list_ == NULL) {
      DCHECK_EQ(0, i);
      return last_;
    } else {
      if (i == list_->length()) {
        DCHECK(last_ != NULL);
        return last_;
      } else {
        return list_->at(i);
      }
    }
  }

  void Clear() {
    list_ = NULL;
    last_ = NULL;
  }

  int length() {
    int length = (list_ == NULL) ? 0 : list_->length();
    return length + ((last_ == NULL) ? 0 : 1);
  }

  ZoneList<T*>* GetList(Zone* zone) {
    if (list_ == NULL) {
      list_ = new (zone) ZoneList<T*>(initial_size, zone);
    }
    if (last_ != NULL) {
      list_->Add(last_, zone);
      last_ = NULL;
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
  RegExpBuilder(Zone* zone, bool ignore_case, bool unicode);
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
  RegExpTree* ToRegExp();

 private:
  static const uc16 kNoPendingSurrogate = 0;
  void AddLeadSurrogate(uc16 lead_surrogate);
  void AddTrailSurrogate(uc16 trail_surrogate);
  void FlushPendingSurrogate();
  void FlushCharacters();
  void FlushText();
  void FlushTerms();
  bool NeedsDesugaringForUnicode(RegExpCharacterClass* cc);
  bool NeedsDesugaringForIgnoreCase(uc32 c);
  Zone* zone() const { return zone_; }
  bool ignore_case() const { return ignore_case_; }
  bool unicode() const { return unicode_; }

  Zone* zone_;
  bool pending_empty_;
  bool ignore_case_;
  bool unicode_;
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


class RegExpParser BASE_EMBEDDED {
 public:
  RegExpParser(FlatStringReader* in, Handle<String>* error,
               JSRegExp::Flags flags, Isolate* isolate, Zone* zone);

  static bool ParseRegExp(Isolate* isolate, Zone* zone, FlatStringReader* input,
                          JSRegExp::Flags flags, RegExpCompileData* result);

  RegExpTree* ParsePattern();
  RegExpTree* ParseDisjunction();
  RegExpTree* ParseGroup();
  RegExpTree* ParseCharacterClass();

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
  ZoneList<CharacterRange>* ParsePropertyClass();

  uc32 ParseOctalLiteral();

  // Tries to parse the input as a back reference.  If successful it
  // stores the result in the output parameter and returns true.  If
  // it fails it will push back the characters read so the same characters
  // can be reparsed.
  bool ParseBackReferenceIndex(int* index_out);

  CharacterRange ParseClassAtom(uc16* char_class);
  RegExpTree* ReportError(Vector<const char> message);
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
  bool ignore_case() const { return ignore_case_; }
  bool multiline() const { return multiline_; }
  bool unicode() const { return unicode_; }

  static bool IsSyntaxCharacterOrSlash(uc32 c);

  static const int kMaxCaptures = 1 << 16;
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
    RegExpParserState(RegExpParserState* previous_state,
                      SubexpressionType group_type,
                      RegExpLookaround::Type lookaround_type,
                      int disjunction_capture_index, bool ignore_case,
                      bool unicode, Zone* zone)
        : previous_state_(previous_state),
          builder_(new (zone) RegExpBuilder(zone, ignore_case, unicode)),
          group_type_(group_type),
          lookaround_type_(lookaround_type),
          disjunction_capture_index_(disjunction_capture_index) {}
    // Parser state of containing expression, if any.
    RegExpParserState* previous_state() { return previous_state_; }
    bool IsSubexpression() { return previous_state_ != NULL; }
    // RegExpBuilder building this regexp's AST.
    RegExpBuilder* builder() { return builder_; }
    // Type of regexp being parsed (parenthesized group or entire regexp).
    SubexpressionType group_type() { return group_type_; }
    // Lookahead or Lookbehind.
    RegExpLookaround::Type lookaround_type() { return lookaround_type_; }
    // Index in captures array of first capture in this sub-expression, if any.
    // Also the capture index of this sub-expression itself, if group_type
    // is CAPTURE.
    int capture_index() { return disjunction_capture_index_; }

    // Check whether the parser is inside a capture group with the given index.
    bool IsInsideCaptureGroup(int index);

   private:
    // Linked list implementation of stack of states.
    RegExpParserState* previous_state_;
    // Builder for the stored disjunction.
    RegExpBuilder* builder_;
    // Stored disjunction type (capture, look-ahead or grouping), if any.
    SubexpressionType group_type_;
    // Stored read direction.
    RegExpLookaround::Type lookaround_type_;
    // Stored disjunction's capture index (if any).
    int disjunction_capture_index_;
  };

  // Return the 1-indexed RegExpCapture object, allocate if necessary.
  RegExpCapture* GetCapture(int index);

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

  Isolate* isolate_;
  Zone* zone_;
  Handle<String>* error_;
  ZoneList<RegExpCapture*>* captures_;
  FlatStringReader* in_;
  uc32 current_;
  bool ignore_case_;
  bool multiline_;
  bool unicode_;
  int next_pos_;
  int captures_started_;
  // The capture count is only valid after we have scanned for captures.
  int capture_count_;
  bool has_more_;
  bool simple_;
  bool contains_anchor_;
  bool is_scanned_for_captures_;
  bool failed_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_PARSER_H_

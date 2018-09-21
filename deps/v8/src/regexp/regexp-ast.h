// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_AST_H_
#define V8_REGEXP_REGEXP_AST_H_

#include "src/objects.h"
#include "src/objects/js-regexp.h"
#include "src/objects/string.h"
#include "src/utils.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

#define FOR_EACH_REG_EXP_TREE_TYPE(VISIT) \
  VISIT(Disjunction)                      \
  VISIT(Alternative)                      \
  VISIT(Assertion)                        \
  VISIT(CharacterClass)                   \
  VISIT(Atom)                             \
  VISIT(Quantifier)                       \
  VISIT(Capture)                          \
  VISIT(Group)                            \
  VISIT(Lookaround)                       \
  VISIT(BackReference)                    \
  VISIT(Empty)                            \
  VISIT(Text)

#define FORWARD_DECLARE(Name) class RegExp##Name;
FOR_EACH_REG_EXP_TREE_TYPE(FORWARD_DECLARE)
#undef FORWARD_DECLARE

class RegExpCompiler;
class RegExpNode;
class RegExpTree;


class RegExpVisitor BASE_EMBEDDED {
 public:
  virtual ~RegExpVisitor() {}
#define MAKE_CASE(Name) \
  virtual void* Visit##Name(RegExp##Name*, void* data) = 0;
  FOR_EACH_REG_EXP_TREE_TYPE(MAKE_CASE)
#undef MAKE_CASE
};


// A simple closed interval.
class Interval {
 public:
  Interval() : from_(kNone), to_(kNone) {}
  Interval(int from, int to) : from_(from), to_(to) {}
  Interval Union(Interval that) {
    if (that.from_ == kNone)
      return *this;
    else if (from_ == kNone)
      return that;
    else
      return Interval(Min(from_, that.from_), Max(to_, that.to_));
  }
  bool Contains(int value) { return (from_ <= value) && (value <= to_); }
  bool is_empty() { return from_ == kNone; }
  int from() const { return from_; }
  int to() const { return to_; }
  static Interval Empty() { return Interval(); }
  static const int kNone = -1;

 private:
  int from_;
  int to_;
};


// Represents code units in the range from from_ to to_, both ends are
// inclusive.
class CharacterRange {
 public:
  CharacterRange() : from_(0), to_(0) {}
  // For compatibility with the CHECK_OK macro
  CharacterRange(void* null) { DCHECK_NULL(null); }  // NOLINT
  static void AddClassEscape(char type, ZoneList<CharacterRange>* ranges,
                             Zone* zone);
  // Add class escapes. Add case equivalent closure for \w and \W if necessary.
  static void AddClassEscape(char type, ZoneList<CharacterRange>* ranges,
                             bool add_unicode_case_equivalents, Zone* zone);
  static Vector<const int> GetWordBounds();
  static inline CharacterRange Singleton(uc32 value) {
    return CharacterRange(value, value);
  }
  static inline CharacterRange Range(uc32 from, uc32 to) {
    DCHECK(0 <= from && to <= String::kMaxCodePoint);
    DCHECK(static_cast<uint32_t>(from) <= static_cast<uint32_t>(to));
    return CharacterRange(from, to);
  }
  static inline CharacterRange Everything() {
    return CharacterRange(0, String::kMaxCodePoint);
  }
  static inline ZoneList<CharacterRange>* List(Zone* zone,
                                               CharacterRange range) {
    ZoneList<CharacterRange>* list =
        new (zone) ZoneList<CharacterRange>(1, zone);
    list->Add(range, zone);
    return list;
  }
  bool Contains(uc32 i) { return from_ <= i && i <= to_; }
  uc32 from() const { return from_; }
  void set_from(uc32 value) { from_ = value; }
  uc32 to() const { return to_; }
  void set_to(uc32 value) { to_ = value; }
  bool is_valid() { return from_ <= to_; }
  bool IsEverything(uc32 max) { return from_ == 0 && to_ >= max; }
  bool IsSingleton() { return (from_ == to_); }
  static void AddCaseEquivalents(Isolate* isolate, Zone* zone,
                                 ZoneList<CharacterRange>* ranges,
                                 bool is_one_byte);
  // Whether a range list is in canonical form: Ranges ordered by from value,
  // and ranges non-overlapping and non-adjacent.
  static bool IsCanonical(ZoneList<CharacterRange>* ranges);
  // Convert range list to canonical form. The characters covered by the ranges
  // will still be the same, but no character is in more than one range, and
  // adjacent ranges are merged. The resulting list may be shorter than the
  // original, but cannot be longer.
  static void Canonicalize(ZoneList<CharacterRange>* ranges);
  // Negate the contents of a character range in canonical form.
  static void Negate(ZoneList<CharacterRange>* src,
                     ZoneList<CharacterRange>* dst, Zone* zone);
  static const int kStartMarker = (1 << 24);
  static const int kPayloadMask = (1 << 24) - 1;

 private:
  CharacterRange(uc32 from, uc32 to) : from_(from), to_(to) {}

  uc32 from_;
  uc32 to_;
};


class CharacterSet final BASE_EMBEDDED {
 public:
  explicit CharacterSet(uc16 standard_set_type)
      : ranges_(nullptr), standard_set_type_(standard_set_type) {}
  explicit CharacterSet(ZoneList<CharacterRange>* ranges)
      : ranges_(ranges), standard_set_type_(0) {}
  ZoneList<CharacterRange>* ranges(Zone* zone);
  uc16 standard_set_type() const { return standard_set_type_; }
  void set_standard_set_type(uc16 special_set_type) {
    standard_set_type_ = special_set_type;
  }
  bool is_standard() { return standard_set_type_ != 0; }
  void Canonicalize();

 private:
  ZoneList<CharacterRange>* ranges_;
  // If non-zero, the value represents a standard set (e.g., all whitespace
  // characters) without having to expand the ranges.
  uc16 standard_set_type_;
};


class TextElement final BASE_EMBEDDED {
 public:
  enum TextType { ATOM, CHAR_CLASS };

  static TextElement Atom(RegExpAtom* atom);
  static TextElement CharClass(RegExpCharacterClass* char_class);

  int cp_offset() const { return cp_offset_; }
  void set_cp_offset(int cp_offset) { cp_offset_ = cp_offset; }
  int length() const;

  TextType text_type() const { return text_type_; }

  RegExpTree* tree() const { return tree_; }

  RegExpAtom* atom() const {
    DCHECK(text_type() == ATOM);
    return reinterpret_cast<RegExpAtom*>(tree());
  }

  RegExpCharacterClass* char_class() const {
    DCHECK(text_type() == CHAR_CLASS);
    return reinterpret_cast<RegExpCharacterClass*>(tree());
  }

 private:
  TextElement(TextType text_type, RegExpTree* tree)
      : cp_offset_(-1), text_type_(text_type), tree_(tree) {}

  int cp_offset_;
  TextType text_type_;
  RegExpTree* tree_;
};


class RegExpTree : public ZoneObject {
 public:
  static const int kInfinity = kMaxInt;
  virtual ~RegExpTree() {}
  virtual void* Accept(RegExpVisitor* visitor, void* data) = 0;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) = 0;
  virtual bool IsTextElement() { return false; }
  virtual bool IsAnchoredAtStart() { return false; }
  virtual bool IsAnchoredAtEnd() { return false; }
  virtual int min_match() = 0;
  virtual int max_match() = 0;
  // Returns the interval of registers used for captures within this
  // expression.
  virtual Interval CaptureRegisters() { return Interval::Empty(); }
  virtual void AppendToText(RegExpText* text, Zone* zone);
  std::ostream& Print(std::ostream& os, Zone* zone);  // NOLINT
#define MAKE_ASTYPE(Name)           \
  virtual RegExp##Name* As##Name(); \
  virtual bool Is##Name();
  FOR_EACH_REG_EXP_TREE_TYPE(MAKE_ASTYPE)
#undef MAKE_ASTYPE
};


class RegExpDisjunction final : public RegExpTree {
 public:
  explicit RegExpDisjunction(ZoneList<RegExpTree*>* alternatives);
  void* Accept(RegExpVisitor* visitor, void* data) override;
  RegExpNode* ToNode(RegExpCompiler* compiler, RegExpNode* on_success) override;
  RegExpDisjunction* AsDisjunction() override;
  Interval CaptureRegisters() override;
  bool IsDisjunction() override;
  bool IsAnchoredAtStart() override;
  bool IsAnchoredAtEnd() override;
  int min_match() override { return min_match_; }
  int max_match() override { return max_match_; }
  ZoneList<RegExpTree*>* alternatives() { return alternatives_; }

 private:
  bool SortConsecutiveAtoms(RegExpCompiler* compiler);
  void RationalizeConsecutiveAtoms(RegExpCompiler* compiler);
  void FixSingleCharacterDisjunctions(RegExpCompiler* compiler);
  ZoneList<RegExpTree*>* alternatives_;
  int min_match_;
  int max_match_;
};


class RegExpAlternative final : public RegExpTree {
 public:
  explicit RegExpAlternative(ZoneList<RegExpTree*>* nodes);
  void* Accept(RegExpVisitor* visitor, void* data) override;
  RegExpNode* ToNode(RegExpCompiler* compiler, RegExpNode* on_success) override;
  RegExpAlternative* AsAlternative() override;
  Interval CaptureRegisters() override;
  bool IsAlternative() override;
  bool IsAnchoredAtStart() override;
  bool IsAnchoredAtEnd() override;
  int min_match() override { return min_match_; }
  int max_match() override { return max_match_; }
  ZoneList<RegExpTree*>* nodes() { return nodes_; }

 private:
  ZoneList<RegExpTree*>* nodes_;
  int min_match_;
  int max_match_;
};


class RegExpAssertion final : public RegExpTree {
 public:
  enum AssertionType {
    START_OF_LINE,
    START_OF_INPUT,
    END_OF_LINE,
    END_OF_INPUT,
    BOUNDARY,
    NON_BOUNDARY
  };
  RegExpAssertion(AssertionType type, JSRegExp::Flags flags)
      : assertion_type_(type), flags_(flags) {}
  void* Accept(RegExpVisitor* visitor, void* data) override;
  RegExpNode* ToNode(RegExpCompiler* compiler, RegExpNode* on_success) override;
  RegExpAssertion* AsAssertion() override;
  bool IsAssertion() override;
  bool IsAnchoredAtStart() override;
  bool IsAnchoredAtEnd() override;
  int min_match() override { return 0; }
  int max_match() override { return 0; }
  AssertionType assertion_type() { return assertion_type_; }

 private:
  const AssertionType assertion_type_;
  const JSRegExp::Flags flags_;
};


class RegExpCharacterClass final : public RegExpTree {
 public:
  // NEGATED: The character class is negated and should match everything but
  //     the specified ranges.
  // CONTAINS_SPLIT_SURROGATE: The character class contains part of a split
  //     surrogate and should not be unicode-desugared (crbug.com/641091).
  enum Flag {
    NEGATED = 1 << 0,
    CONTAINS_SPLIT_SURROGATE = 1 << 1,
  };
  typedef base::Flags<Flag> CharacterClassFlags;

  RegExpCharacterClass(
      Zone* zone, ZoneList<CharacterRange>* ranges, JSRegExp::Flags flags,
      CharacterClassFlags character_class_flags = CharacterClassFlags())
      : set_(ranges),
        flags_(flags),
        character_class_flags_(character_class_flags) {
    // Convert the empty set of ranges to the negated Everything() range.
    if (ranges->is_empty()) {
      ranges->Add(CharacterRange::Everything(), zone);
      character_class_flags_ ^= NEGATED;
    }
  }
  RegExpCharacterClass(uc16 type, JSRegExp::Flags flags)
      : set_(type),
        flags_(flags),
        character_class_flags_(CharacterClassFlags()) {}
  void* Accept(RegExpVisitor* visitor, void* data) override;
  RegExpNode* ToNode(RegExpCompiler* compiler, RegExpNode* on_success) override;
  RegExpCharacterClass* AsCharacterClass() override;
  bool IsCharacterClass() override;
  bool IsTextElement() override { return true; }
  int min_match() override { return 1; }
  // The character class may match two code units for unicode regexps.
  // TODO(yangguo): we should split this class for usage in TextElement, and
  //                make max_match() dependent on the character class content.
  int max_match() override { return 2; }
  void AppendToText(RegExpText* text, Zone* zone) override;
  CharacterSet character_set() { return set_; }
  // TODO(lrn): Remove need for complex version if is_standard that
  // recognizes a mangled standard set and just do { return set_.is_special(); }
  bool is_standard(Zone* zone);
  // Returns a value representing the standard character set if is_standard()
  // returns true.
  // Currently used values are:
  // s : unicode whitespace
  // S : unicode non-whitespace
  // w : ASCII word character (digit, letter, underscore)
  // W : non-ASCII word character
  // d : ASCII digit
  // D : non-ASCII digit
  // . : non-newline
  // * : All characters, for advancing unanchored regexp
  uc16 standard_type() const { return set_.standard_set_type(); }
  ZoneList<CharacterRange>* ranges(Zone* zone) { return set_.ranges(zone); }
  bool is_negated() const { return (character_class_flags_ & NEGATED) != 0; }
  JSRegExp::Flags flags() const { return flags_; }
  bool contains_split_surrogate() const {
    return (character_class_flags_ & CONTAINS_SPLIT_SURROGATE) != 0;
  }

 private:
  CharacterSet set_;
  const JSRegExp::Flags flags_;
  CharacterClassFlags character_class_flags_;
};


class RegExpAtom final : public RegExpTree {
 public:
  explicit RegExpAtom(Vector<const uc16> data, JSRegExp::Flags flags)
      : data_(data), flags_(flags) {}
  void* Accept(RegExpVisitor* visitor, void* data) override;
  RegExpNode* ToNode(RegExpCompiler* compiler, RegExpNode* on_success) override;
  RegExpAtom* AsAtom() override;
  bool IsAtom() override;
  bool IsTextElement() override { return true; }
  int min_match() override { return data_.length(); }
  int max_match() override { return data_.length(); }
  void AppendToText(RegExpText* text, Zone* zone) override;
  Vector<const uc16> data() { return data_; }
  int length() { return data_.length(); }
  JSRegExp::Flags flags() const { return flags_; }
  bool ignore_case() const { return (flags_ & JSRegExp::kIgnoreCase) != 0; }

 private:
  Vector<const uc16> data_;
  const JSRegExp::Flags flags_;
};


class RegExpText final : public RegExpTree {
 public:
  explicit RegExpText(Zone* zone) : elements_(2, zone), length_(0) {}
  void* Accept(RegExpVisitor* visitor, void* data) override;
  RegExpNode* ToNode(RegExpCompiler* compiler, RegExpNode* on_success) override;
  RegExpText* AsText() override;
  bool IsText() override;
  bool IsTextElement() override { return true; }
  int min_match() override { return length_; }
  int max_match() override { return length_; }
  void AppendToText(RegExpText* text, Zone* zone) override;
  void AddElement(TextElement elm, Zone* zone) {
    elements_.Add(elm, zone);
    length_ += elm.length();
  }
  ZoneList<TextElement>* elements() { return &elements_; }

 private:
  ZoneList<TextElement> elements_;
  int length_;
};


class RegExpQuantifier final : public RegExpTree {
 public:
  enum QuantifierType { GREEDY, NON_GREEDY, POSSESSIVE };
  RegExpQuantifier(int min, int max, QuantifierType type, RegExpTree* body)
      : body_(body),
        min_(min),
        max_(max),
        min_match_(min * body->min_match()),
        quantifier_type_(type) {
    if (max > 0 && body->max_match() > kInfinity / max) {
      max_match_ = kInfinity;
    } else {
      max_match_ = max * body->max_match();
    }
  }
  void* Accept(RegExpVisitor* visitor, void* data) override;
  RegExpNode* ToNode(RegExpCompiler* compiler, RegExpNode* on_success) override;
  static RegExpNode* ToNode(int min, int max, bool is_greedy, RegExpTree* body,
                            RegExpCompiler* compiler, RegExpNode* on_success,
                            bool not_at_start = false);
  RegExpQuantifier* AsQuantifier() override;
  Interval CaptureRegisters() override;
  bool IsQuantifier() override;
  int min_match() override { return min_match_; }
  int max_match() override { return max_match_; }
  int min() { return min_; }
  int max() { return max_; }
  bool is_possessive() { return quantifier_type_ == POSSESSIVE; }
  bool is_non_greedy() { return quantifier_type_ == NON_GREEDY; }
  bool is_greedy() { return quantifier_type_ == GREEDY; }
  RegExpTree* body() { return body_; }

 private:
  RegExpTree* body_;
  int min_;
  int max_;
  int min_match_;
  int max_match_;
  QuantifierType quantifier_type_;
};


class RegExpCapture final : public RegExpTree {
 public:
  explicit RegExpCapture(int index)
      : body_(nullptr), index_(index), name_(nullptr) {}
  void* Accept(RegExpVisitor* visitor, void* data) override;
  RegExpNode* ToNode(RegExpCompiler* compiler, RegExpNode* on_success) override;
  static RegExpNode* ToNode(RegExpTree* body, int index,
                            RegExpCompiler* compiler, RegExpNode* on_success);
  RegExpCapture* AsCapture() override;
  bool IsAnchoredAtStart() override;
  bool IsAnchoredAtEnd() override;
  Interval CaptureRegisters() override;
  bool IsCapture() override;
  int min_match() override { return body_->min_match(); }
  int max_match() override { return body_->max_match(); }
  RegExpTree* body() { return body_; }
  void set_body(RegExpTree* body) { body_ = body; }
  int index() { return index_; }
  const ZoneVector<uc16>* name() const { return name_; }
  void set_name(const ZoneVector<uc16>* name) { name_ = name; }
  static int StartRegister(int index) { return index * 2; }
  static int EndRegister(int index) { return index * 2 + 1; }

 private:
  RegExpTree* body_;
  int index_;
  const ZoneVector<uc16>* name_;
};

class RegExpGroup final : public RegExpTree {
 public:
  explicit RegExpGroup(RegExpTree* body) : body_(body) {}
  void* Accept(RegExpVisitor* visitor, void* data) override;
  RegExpNode* ToNode(RegExpCompiler* compiler,
                     RegExpNode* on_success) override {
    return body_->ToNode(compiler, on_success);
  }
  RegExpGroup* AsGroup() override;
  bool IsAnchoredAtStart() override { return body_->IsAnchoredAtStart(); }
  bool IsAnchoredAtEnd() override { return body_->IsAnchoredAtEnd(); }
  bool IsGroup() override;
  int min_match() override { return body_->min_match(); }
  int max_match() override { return body_->max_match(); }
  Interval CaptureRegisters() override { return body_->CaptureRegisters(); }
  RegExpTree* body() { return body_; }

 private:
  RegExpTree* body_;
};

class RegExpLookaround final : public RegExpTree {
 public:
  enum Type { LOOKAHEAD, LOOKBEHIND };

  RegExpLookaround(RegExpTree* body, bool is_positive, int capture_count,
                   int capture_from, Type type)
      : body_(body),
        is_positive_(is_positive),
        capture_count_(capture_count),
        capture_from_(capture_from),
        type_(type) {}

  void* Accept(RegExpVisitor* visitor, void* data) override;
  RegExpNode* ToNode(RegExpCompiler* compiler, RegExpNode* on_success) override;
  RegExpLookaround* AsLookaround() override;
  Interval CaptureRegisters() override;
  bool IsLookaround() override;
  bool IsAnchoredAtStart() override;
  int min_match() override { return 0; }
  int max_match() override { return 0; }
  RegExpTree* body() { return body_; }
  bool is_positive() { return is_positive_; }
  int capture_count() { return capture_count_; }
  int capture_from() { return capture_from_; }
  Type type() { return type_; }

  class Builder {
   public:
    Builder(bool is_positive, RegExpNode* on_success,
            int stack_pointer_register, int position_register,
            int capture_register_count = 0, int capture_register_start = 0);
    RegExpNode* on_match_success() { return on_match_success_; }
    RegExpNode* ForMatch(RegExpNode* match);

   private:
    bool is_positive_;
    RegExpNode* on_match_success_;
    RegExpNode* on_success_;
    int stack_pointer_register_;
    int position_register_;
  };

 private:
  RegExpTree* body_;
  bool is_positive_;
  int capture_count_;
  int capture_from_;
  Type type_;
};


class RegExpBackReference final : public RegExpTree {
 public:
  explicit RegExpBackReference(JSRegExp::Flags flags)
      : capture_(nullptr), name_(nullptr), flags_(flags) {}
  RegExpBackReference(RegExpCapture* capture, JSRegExp::Flags flags)
      : capture_(capture), name_(nullptr), flags_(flags) {}
  void* Accept(RegExpVisitor* visitor, void* data) override;
  RegExpNode* ToNode(RegExpCompiler* compiler, RegExpNode* on_success) override;
  RegExpBackReference* AsBackReference() override;
  bool IsBackReference() override;
  int min_match() override { return 0; }
  // The back reference may be recursive, e.g. /(\2)(\1)/. To avoid infinite
  // recursion, we give up. Ignorance is bliss.
  int max_match() override { return kInfinity; }
  int index() { return capture_->index(); }
  RegExpCapture* capture() { return capture_; }
  void set_capture(RegExpCapture* capture) { capture_ = capture; }
  const ZoneVector<uc16>* name() const { return name_; }
  void set_name(const ZoneVector<uc16>* name) { name_ = name; }

 private:
  RegExpCapture* capture_;
  const ZoneVector<uc16>* name_;
  const JSRegExp::Flags flags_;
};


class RegExpEmpty final : public RegExpTree {
 public:
  RegExpEmpty() {}
  void* Accept(RegExpVisitor* visitor, void* data) override;
  RegExpNode* ToNode(RegExpCompiler* compiler, RegExpNode* on_success) override;
  RegExpEmpty* AsEmpty() override;
  bool IsEmpty() override;
  int min_match() override { return 0; }
  int max_match() override { return 0; }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_AST_H_

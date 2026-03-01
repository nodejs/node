// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-ast.h"

#include "src/execution/isolate-inl.h"
#include "src/regexp/regexp-ast.h"

namespace v8 {
namespace internal {

#define MAKE_ACCEPT(Name)                                          \
  void* RegExp##Name::Accept(RegExpVisitor* visitor, void* data) { \
    return visitor->Visit##Name(this, data);                       \
  }
FOR_EACH_REG_EXP_TREE_TYPE(MAKE_ACCEPT)
#undef MAKE_ACCEPT

#define MAKE_TYPE_CASE(Name)                               \
  RegExp##Name* RegExpTree::As##Name() { return nullptr; } \
  bool RegExpTree::Is##Name() { return false; }
FOR_EACH_REG_EXP_TREE_TYPE(MAKE_TYPE_CASE)
#undef MAKE_TYPE_CASE

#define MAKE_TYPE_CASE(Name)                              \
  RegExp##Name* RegExp##Name::As##Name() { return this; } \
  bool RegExp##Name::Is##Name() { return true; }
FOR_EACH_REG_EXP_TREE_TYPE(MAKE_TYPE_CASE)
#undef MAKE_TYPE_CASE

bool StackLimiter::IsOverflowed() {
  if (V8_LIKELY(budget_ > 0)) return false;
  // This can be a little slow so we don't do it until the soft budget has been
  // exhausted.
  return StackLimitCheck{Isolate::Current()}.HasOverflowed();
}

Interval RegExpGroup::CaptureRegisters(StackLimiter limiter) {
  if (limiter.IsOverflowed()) return Interval::Invalid();
  return body_->CaptureRegisters(limiter - 1);
}

namespace {

Interval ListCaptureRegisters(StackLimiter limiter,
                              ZoneList<RegExpTree*>* children) {
  if (limiter.IsOverflowed()) return Interval::Invalid();
  Interval result = Interval::Empty();
  for (int i = 0; i < children->length(); i++) {
    result = result.Union(children->at(i)->CaptureRegisters(limiter - 1));
  }
  return result;
}

}  // namespace

Interval RegExpAlternative::CaptureRegisters(StackLimiter limiter) {
  return ListCaptureRegisters(limiter - 1, nodes());
}

Interval RegExpDisjunction::CaptureRegisters(StackLimiter limiter) {
  return ListCaptureRegisters(limiter - 1, alternatives());
}

Interval RegExpLookaround::CaptureRegisters(StackLimiter limiter) {
  if (limiter.IsOverflowed()) return Interval::Invalid();
  return body()->CaptureRegisters(limiter - 1);
}

Interval RegExpCapture::CaptureRegisters(StackLimiter limiter) {
  if (limiter.IsOverflowed()) return Interval::Invalid();
  Interval self(StartRegister(index()), EndRegister(index()));
  return self.Union(body()->CaptureRegisters(limiter - 1));
}

Interval RegExpQuantifier::CaptureRegisters(StackLimiter limiter) {
  if (limiter.IsOverflowed()) return Interval::Invalid();
  return body()->CaptureRegisters(limiter - 1);
}

bool RegExpAssertion::IsCertainlyAnchoredAtStart(int budget) {
  return assertion_type() == RegExpAssertion::Type::START_OF_INPUT;
}

bool RegExpAssertion::IsCertainlyAnchoredAtEnd(int budget) {
  return assertion_type() == RegExpAssertion::Type::END_OF_INPUT;
}

bool RegExpAlternative::IsCertainlyAnchoredAtStart(int budget) {
  if (budget < 0) return false;
  ZoneList<RegExpTree*>* nodes = this->nodes();
  for (int i = 0; i < nodes->length(); i++) {
    RegExpTree* node = nodes->at(i);
    if (node->IsCertainlyAnchoredAtStart(budget - 1)) {
      return true;
    }
    if (node->max_match() > 0) {
      return false;
    }
  }
  return false;
}

bool RegExpAlternative::IsCertainlyAnchoredAtEnd(int budget) {
  if (budget < 0) return false;
  ZoneList<RegExpTree*>* nodes = this->nodes();
  for (int i = nodes->length() - 1; i >= 0; i--) {
    RegExpTree* node = nodes->at(i);
    if (node->IsCertainlyAnchoredAtEnd(budget - 1)) {
      return true;
    }
    if (node->max_match() > 0) {
      return false;
    }
  }
  return false;
}

bool RegExpDisjunction::IsCertainlyAnchoredAtStart(int budget) {
  if (budget < 0) return false;
  ZoneList<RegExpTree*>* alternatives = this->alternatives();
  for (int i = 0; i < alternatives->length(); i++) {
    if (!alternatives->at(i)->IsCertainlyAnchoredAtStart(budget - 1))
      return false;
  }
  return true;
}

bool RegExpDisjunction::IsCertainlyAnchoredAtEnd(int budget) {
  if (budget < 0) return false;
  ZoneList<RegExpTree*>* alternatives = this->alternatives();
  for (int i = 0; i < alternatives->length(); i++) {
    if (!alternatives->at(i)->IsCertainlyAnchoredAtEnd(budget - 1)) {
      return false;
    }
  }
  return true;
}

bool RegExpLookaround::IsCertainlyAnchoredAtStart(int budget) {
  if (budget < 0) return false;
  return is_positive() && type() == LOOKAHEAD &&
         body()->IsCertainlyAnchoredAtStart(budget - 1);
}

bool RegExpCapture::IsCertainlyAnchoredAtStart(int budget) {
  if (budget < 0) return false;
  return body()->IsCertainlyAnchoredAtStart(budget - 1);
}

bool RegExpCapture::IsCertainlyAnchoredAtEnd(int budget) {
  if (budget < 0) return false;
  return body()->IsCertainlyAnchoredAtEnd(budget - 1);
}

RegExpDisjunction::RegExpDisjunction(ZoneList<RegExpTree*>* alternatives)
    : alternatives_(alternatives) {
  DCHECK_LT(1, alternatives->length());
  RegExpTree* first_alternative = alternatives->at(0);
  min_match_ = first_alternative->min_match();
  max_match_ = first_alternative->max_match();
  for (int i = 1; i < alternatives->length(); i++) {
    RegExpTree* alternative = alternatives->at(i);
    min_match_ = std::min(min_match_, alternative->min_match());
    max_match_ = std::max(max_match_, alternative->max_match());
  }
}

namespace {

int IncreaseBy(int previous, int increase) {
  if (RegExpTree::kInfinity - previous < increase) {
    return RegExpTree::kInfinity;
  } else {
    return previous + increase;
  }
}

}  // namespace

RegExpAlternative::RegExpAlternative(ZoneList<RegExpTree*>* nodes)
    : nodes_(nodes) {
  DCHECK_LT(1, nodes->length());
  min_match_ = 0;
  max_match_ = 0;
  for (int i = 0; i < nodes->length(); i++) {
    RegExpTree* node = nodes->at(i);
    int node_min_match = node->min_match();
    min_match_ = IncreaseBy(min_match_, node_min_match);
    int node_max_match = node->max_match();
    max_match_ = IncreaseBy(max_match_, node_max_match);
  }
}

RegExpClassSetOperand::RegExpClassSetOperand(ZoneList<CharacterRange>* ranges,
                                             CharacterClassStrings* strings)
    : ranges_(ranges), strings_(strings) {
  DCHECK_NOT_NULL(ranges);
  min_match_ = 0;
  max_match_ = 0;
  if (!ranges->is_empty()) {
    min_match_ = 1;
    max_match_ = 2;
  }
  if (has_strings()) {
    for (auto string : *strings) {
      min_match_ = std::min(min_match_, string.second->min_match());
      max_match_ = std::max(max_match_, string.second->max_match());
    }
  }
}

RegExpClassSetExpression::RegExpClassSetExpression(
    OperationType op, bool is_negated, bool may_contain_strings,
    ZoneList<RegExpTree*>* operands)
    : operation_(op),
      is_negated_(is_negated),
      may_contain_strings_(may_contain_strings),
      operands_(operands) {
  DCHECK_NOT_NULL(operands);
  if (is_negated) {
    DCHECK(!may_contain_strings_);
    // We don't know anything about max matches for negated classes.
    // As there are no strings involved, assume that we can match a unicode
    // character (2 code points).
    max_match_ = 2;
  } else {
    max_match_ = 0;
    for (auto operand : *operands) {
      max_match_ = std::max(max_match_, operand->max_match());
    }
  }
}

// static
RegExpClassSetExpression* RegExpClassSetExpression::Empty(Zone* zone,
                                                          bool is_negated) {
  ZoneList<CharacterRange>* ranges =
      zone->template New<ZoneList<CharacterRange>>(0, zone);
  RegExpClassSetOperand* op =
      zone->template New<RegExpClassSetOperand>(ranges, nullptr);
  ZoneList<RegExpTree*>* operands =
      zone->template New<ZoneList<RegExpTree*>>(1, zone);
  operands->Add(op, zone);
  return zone->template New<RegExpClassSetExpression>(
      RegExpClassSetExpression::OperationType::kUnion, is_negated, false,
      operands);
}

bool RegExpText::StartsWithAtom() const {
  if (elements_.length() == 0) return false;
  return elements_.at(0).text_type() == TextElement::ATOM;
}

RegExpAtom* RegExpText::FirstAtom() const { return elements_.at(0).atom(); }

RegExpClassRanges::RegExpClassRanges(
    Zone* zone, ZoneList<CharacterRange>* ranges,
    RegExpClassRanges::ClassRangesFlags class_ranges_flags)
    : set_(ranges), class_ranges_flags_(class_ranges_flags) {
  // Convert the empty set of ranges to the negated Everything() range.
  if (ranges->is_empty()) {
    ranges->Add(CharacterRange::Everything(), zone);
    class_ranges_flags_ ^= NEGATED;
  }
  if (!is_negated() && !is_certainly_two_code_points() &&
      no_case_folding_needed()) {
    // Perhaps we can detect that it is always two code points.
    bool found_basic_plane = false;
    for (int i = 0; i < ranges->length(); i++) {
      if (ranges->at(i).from() < 0x10000) {
        found_basic_plane = true;
        break;
      }
    }
    if (!found_basic_plane) {
      class_ranges_flags_ |= IS_CERTAINLY_TWO_CODE_POINTS;
    }
  }
}

}  // namespace internal
}  // namespace v8

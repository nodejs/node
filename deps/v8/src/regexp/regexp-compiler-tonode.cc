// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-compiler.h"

#include "src/execution/isolate.h"
#include "src/regexp/regexp.h"
#ifdef V8_INTL_SUPPORT
#include "src/regexp/special-case.h"
#endif  // V8_INTL_SUPPORT
#include "src/strings/unicode-inl.h"
#include "src/zone/zone-list-inl.h"

#ifdef V8_INTL_SUPPORT
#include "unicode/locid.h"
#include "unicode/uniset.h"
#include "unicode/utypes.h"
#endif  // V8_INTL_SUPPORT

namespace v8 {
namespace internal {

using namespace regexp_compiler_constants;  // NOLINT(build/namespaces)

// -------------------------------------------------------------------
// Tree to graph conversion

RegExpNode* RegExpAtom::ToNode(RegExpCompiler* compiler,
                               RegExpNode* on_success) {
  ZoneList<TextElement>* elms =
      new (compiler->zone()) ZoneList<TextElement>(1, compiler->zone());
  elms->Add(TextElement::Atom(this), compiler->zone());
  return new (compiler->zone())
      TextNode(elms, compiler->read_backward(), on_success);
}

RegExpNode* RegExpText::ToNode(RegExpCompiler* compiler,
                               RegExpNode* on_success) {
  return new (compiler->zone())
      TextNode(elements(), compiler->read_backward(), on_success);
}

static bool CompareInverseRanges(ZoneList<CharacterRange>* ranges,
                                 const int* special_class, int length) {
  length--;  // Remove final marker.
  DCHECK_EQ(kRangeEndMarker, special_class[length]);
  DCHECK_NE(0, ranges->length());
  DCHECK_NE(0, length);
  DCHECK_NE(0, special_class[0]);
  if (ranges->length() != (length >> 1) + 1) {
    return false;
  }
  CharacterRange range = ranges->at(0);
  if (range.from() != 0) {
    return false;
  }
  for (int i = 0; i < length; i += 2) {
    if (special_class[i] != (range.to() + 1)) {
      return false;
    }
    range = ranges->at((i >> 1) + 1);
    if (special_class[i + 1] != range.from()) {
      return false;
    }
  }
  if (range.to() != String::kMaxCodePoint) {
    return false;
  }
  return true;
}

static bool CompareRanges(ZoneList<CharacterRange>* ranges,
                          const int* special_class, int length) {
  length--;  // Remove final marker.
  DCHECK_EQ(kRangeEndMarker, special_class[length]);
  if (ranges->length() * 2 != length) {
    return false;
  }
  for (int i = 0; i < length; i += 2) {
    CharacterRange range = ranges->at(i >> 1);
    if (range.from() != special_class[i] ||
        range.to() != special_class[i + 1] - 1) {
      return false;
    }
  }
  return true;
}

bool RegExpCharacterClass::is_standard(Zone* zone) {
  // TODO(lrn): Remove need for this function, by not throwing away information
  // along the way.
  if (is_negated()) {
    return false;
  }
  if (set_.is_standard()) {
    return true;
  }
  if (CompareRanges(set_.ranges(zone), kSpaceRanges, kSpaceRangeCount)) {
    set_.set_standard_set_type('s');
    return true;
  }
  if (CompareInverseRanges(set_.ranges(zone), kSpaceRanges, kSpaceRangeCount)) {
    set_.set_standard_set_type('S');
    return true;
  }
  if (CompareInverseRanges(set_.ranges(zone), kLineTerminatorRanges,
                           kLineTerminatorRangeCount)) {
    set_.set_standard_set_type('.');
    return true;
  }
  if (CompareRanges(set_.ranges(zone), kLineTerminatorRanges,
                    kLineTerminatorRangeCount)) {
    set_.set_standard_set_type('n');
    return true;
  }
  if (CompareRanges(set_.ranges(zone), kWordRanges, kWordRangeCount)) {
    set_.set_standard_set_type('w');
    return true;
  }
  if (CompareInverseRanges(set_.ranges(zone), kWordRanges, kWordRangeCount)) {
    set_.set_standard_set_type('W');
    return true;
  }
  return false;
}

UnicodeRangeSplitter::UnicodeRangeSplitter(ZoneList<CharacterRange>* base) {
  // The unicode range splitter categorizes given character ranges into:
  // - Code points from the BMP representable by one code unit.
  // - Code points outside the BMP that need to be split into surrogate pairs.
  // - Lone lead surrogates.
  // - Lone trail surrogates.
  // Lone surrogates are valid code points, even though no actual characters.
  // They require special matching to make sure we do not split surrogate pairs.

  for (int i = 0; i < base->length(); i++) AddRange(base->at(i));
}

void UnicodeRangeSplitter::AddRange(CharacterRange range) {
  static constexpr uc32 kBmp1Start = 0;
  static constexpr uc32 kBmp1End = kLeadSurrogateStart - 1;
  static constexpr uc32 kBmp2Start = kTrailSurrogateEnd + 1;
  static constexpr uc32 kBmp2End = kNonBmpStart - 1;

  // Ends are all inclusive.
  STATIC_ASSERT(kBmp1Start == 0);
  STATIC_ASSERT(kBmp1Start < kBmp1End);
  STATIC_ASSERT(kBmp1End + 1 == kLeadSurrogateStart);
  STATIC_ASSERT(kLeadSurrogateStart < kLeadSurrogateEnd);
  STATIC_ASSERT(kLeadSurrogateEnd + 1 == kTrailSurrogateStart);
  STATIC_ASSERT(kTrailSurrogateStart < kTrailSurrogateEnd);
  STATIC_ASSERT(kTrailSurrogateEnd + 1 == kBmp2Start);
  STATIC_ASSERT(kBmp2Start < kBmp2End);
  STATIC_ASSERT(kBmp2End + 1 == kNonBmpStart);
  STATIC_ASSERT(kNonBmpStart < kNonBmpEnd);

  static constexpr uc32 kStarts[] = {
      kBmp1Start, kLeadSurrogateStart, kTrailSurrogateStart,
      kBmp2Start, kNonBmpStart,
  };

  static constexpr uc32 kEnds[] = {
      kBmp1End, kLeadSurrogateEnd, kTrailSurrogateEnd, kBmp2End, kNonBmpEnd,
  };

  CharacterRangeVector* const kTargets[] = {
      &bmp_, &lead_surrogates_, &trail_surrogates_, &bmp_, &non_bmp_,
  };

  static constexpr int kCount = arraysize(kStarts);
  STATIC_ASSERT(kCount == arraysize(kEnds));
  STATIC_ASSERT(kCount == arraysize(kTargets));

  for (int i = 0; i < kCount; i++) {
    if (kStarts[i] > range.to()) break;
    const uc32 from = std::max(kStarts[i], range.from());
    const uc32 to = std::min(kEnds[i], range.to());
    if (from > to) continue;
    kTargets[i]->emplace_back(CharacterRange::Range(from, to));
  }
}

namespace {

// Translates between new and old V8-isms (SmallVector, ZoneList).
ZoneList<CharacterRange>* ToCanonicalZoneList(
    const UnicodeRangeSplitter::CharacterRangeVector* v, Zone* zone) {
  if (v->empty()) return nullptr;

  ZoneList<CharacterRange>* result =
      new (zone) ZoneList<CharacterRange>(static_cast<int>(v->size()), zone);
  for (size_t i = 0; i < v->size(); i++) {
    result->Add(v->at(i), zone);
  }

  CharacterRange::Canonicalize(result);
  return result;
}

void AddBmpCharacters(RegExpCompiler* compiler, ChoiceNode* result,
                      RegExpNode* on_success, UnicodeRangeSplitter* splitter) {
  ZoneList<CharacterRange>* bmp =
      ToCanonicalZoneList(splitter->bmp(), compiler->zone());
  if (bmp == nullptr) return;
  JSRegExp::Flags default_flags = JSRegExp::Flags();
  result->AddAlternative(GuardedAlternative(TextNode::CreateForCharacterRanges(
      compiler->zone(), bmp, compiler->read_backward(), on_success,
      default_flags)));
}

void AddNonBmpSurrogatePairs(RegExpCompiler* compiler, ChoiceNode* result,
                             RegExpNode* on_success,
                             UnicodeRangeSplitter* splitter) {
  ZoneList<CharacterRange>* non_bmp =
      ToCanonicalZoneList(splitter->non_bmp(), compiler->zone());
  if (non_bmp == nullptr) return;
  DCHECK(!compiler->one_byte());
  Zone* zone = compiler->zone();
  JSRegExp::Flags default_flags = JSRegExp::Flags();
  CharacterRange::Canonicalize(non_bmp);
  for (int i = 0; i < non_bmp->length(); i++) {
    // Match surrogate pair.
    // E.g. [\u10005-\u11005] becomes
    //      \ud800[\udc05-\udfff]|
    //      [\ud801-\ud803][\udc00-\udfff]|
    //      \ud804[\udc00-\udc05]
    uc32 from = non_bmp->at(i).from();
    uc32 to = non_bmp->at(i).to();
    uc16 from_l = unibrow::Utf16::LeadSurrogate(from);
    uc16 from_t = unibrow::Utf16::TrailSurrogate(from);
    uc16 to_l = unibrow::Utf16::LeadSurrogate(to);
    uc16 to_t = unibrow::Utf16::TrailSurrogate(to);
    if (from_l == to_l) {
      // The lead surrogate is the same.
      result->AddAlternative(
          GuardedAlternative(TextNode::CreateForSurrogatePair(
              zone, CharacterRange::Singleton(from_l),
              CharacterRange::Range(from_t, to_t), compiler->read_backward(),
              on_success, default_flags)));
    } else {
      if (from_t != kTrailSurrogateStart) {
        // Add [from_l][from_t-\udfff]
        result->AddAlternative(
            GuardedAlternative(TextNode::CreateForSurrogatePair(
                zone, CharacterRange::Singleton(from_l),
                CharacterRange::Range(from_t, kTrailSurrogateEnd),
                compiler->read_backward(), on_success, default_flags)));
        from_l++;
      }
      if (to_t != kTrailSurrogateEnd) {
        // Add [to_l][\udc00-to_t]
        result->AddAlternative(
            GuardedAlternative(TextNode::CreateForSurrogatePair(
                zone, CharacterRange::Singleton(to_l),
                CharacterRange::Range(kTrailSurrogateStart, to_t),
                compiler->read_backward(), on_success, default_flags)));
        to_l--;
      }
      if (from_l <= to_l) {
        // Add [from_l-to_l][\udc00-\udfff]
        result->AddAlternative(
            GuardedAlternative(TextNode::CreateForSurrogatePair(
                zone, CharacterRange::Range(from_l, to_l),
                CharacterRange::Range(kTrailSurrogateStart, kTrailSurrogateEnd),
                compiler->read_backward(), on_success, default_flags)));
      }
    }
  }
}

RegExpNode* NegativeLookaroundAgainstReadDirectionAndMatch(
    RegExpCompiler* compiler, ZoneList<CharacterRange>* lookbehind,
    ZoneList<CharacterRange>* match, RegExpNode* on_success, bool read_backward,
    JSRegExp::Flags flags) {
  Zone* zone = compiler->zone();
  RegExpNode* match_node = TextNode::CreateForCharacterRanges(
      zone, match, read_backward, on_success, flags);
  int stack_register = compiler->UnicodeLookaroundStackRegister();
  int position_register = compiler->UnicodeLookaroundPositionRegister();
  RegExpLookaround::Builder lookaround(false, match_node, stack_register,
                                       position_register);
  RegExpNode* negative_match = TextNode::CreateForCharacterRanges(
      zone, lookbehind, !read_backward, lookaround.on_match_success(), flags);
  return lookaround.ForMatch(negative_match);
}

RegExpNode* MatchAndNegativeLookaroundInReadDirection(
    RegExpCompiler* compiler, ZoneList<CharacterRange>* match,
    ZoneList<CharacterRange>* lookahead, RegExpNode* on_success,
    bool read_backward, JSRegExp::Flags flags) {
  Zone* zone = compiler->zone();
  int stack_register = compiler->UnicodeLookaroundStackRegister();
  int position_register = compiler->UnicodeLookaroundPositionRegister();
  RegExpLookaround::Builder lookaround(false, on_success, stack_register,
                                       position_register);
  RegExpNode* negative_match = TextNode::CreateForCharacterRanges(
      zone, lookahead, read_backward, lookaround.on_match_success(), flags);
  return TextNode::CreateForCharacterRanges(
      zone, match, read_backward, lookaround.ForMatch(negative_match), flags);
}

void AddLoneLeadSurrogates(RegExpCompiler* compiler, ChoiceNode* result,
                           RegExpNode* on_success,
                           UnicodeRangeSplitter* splitter) {
  JSRegExp::Flags default_flags = JSRegExp::Flags();
  ZoneList<CharacterRange>* lead_surrogates =
      ToCanonicalZoneList(splitter->lead_surrogates(), compiler->zone());
  if (lead_surrogates == nullptr) return;
  Zone* zone = compiler->zone();
  // E.g. \ud801 becomes \ud801(?![\udc00-\udfff]).
  ZoneList<CharacterRange>* trail_surrogates = CharacterRange::List(
      zone, CharacterRange::Range(kTrailSurrogateStart, kTrailSurrogateEnd));

  RegExpNode* match;
  if (compiler->read_backward()) {
    // Reading backward. Assert that reading forward, there is no trail
    // surrogate, and then backward match the lead surrogate.
    match = NegativeLookaroundAgainstReadDirectionAndMatch(
        compiler, trail_surrogates, lead_surrogates, on_success, true,
        default_flags);
  } else {
    // Reading forward. Forward match the lead surrogate and assert that
    // no trail surrogate follows.
    match = MatchAndNegativeLookaroundInReadDirection(
        compiler, lead_surrogates, trail_surrogates, on_success, false,
        default_flags);
  }
  result->AddAlternative(GuardedAlternative(match));
}

void AddLoneTrailSurrogates(RegExpCompiler* compiler, ChoiceNode* result,
                            RegExpNode* on_success,
                            UnicodeRangeSplitter* splitter) {
  JSRegExp::Flags default_flags = JSRegExp::Flags();
  ZoneList<CharacterRange>* trail_surrogates =
      ToCanonicalZoneList(splitter->trail_surrogates(), compiler->zone());
  if (trail_surrogates == nullptr) return;
  Zone* zone = compiler->zone();
  // E.g. \udc01 becomes (?<![\ud800-\udbff])\udc01
  ZoneList<CharacterRange>* lead_surrogates = CharacterRange::List(
      zone, CharacterRange::Range(kLeadSurrogateStart, kLeadSurrogateEnd));

  RegExpNode* match;
  if (compiler->read_backward()) {
    // Reading backward. Backward match the trail surrogate and assert that no
    // lead surrogate precedes it.
    match = MatchAndNegativeLookaroundInReadDirection(
        compiler, trail_surrogates, lead_surrogates, on_success, true,
        default_flags);
  } else {
    // Reading forward. Assert that reading backward, there is no lead
    // surrogate, and then forward match the trail surrogate.
    match = NegativeLookaroundAgainstReadDirectionAndMatch(
        compiler, lead_surrogates, trail_surrogates, on_success, false,
        default_flags);
  }
  result->AddAlternative(GuardedAlternative(match));
}

RegExpNode* UnanchoredAdvance(RegExpCompiler* compiler,
                              RegExpNode* on_success) {
  // This implements ES2015 21.2.5.2.3, AdvanceStringIndex.
  DCHECK(!compiler->read_backward());
  Zone* zone = compiler->zone();
  // Advance any character. If the character happens to be a lead surrogate and
  // we advanced into the middle of a surrogate pair, it will work out, as
  // nothing will match from there. We will have to advance again, consuming
  // the associated trail surrogate.
  ZoneList<CharacterRange>* range = CharacterRange::List(
      zone, CharacterRange::Range(0, String::kMaxUtf16CodeUnit));
  JSRegExp::Flags default_flags = JSRegExp::Flags();
  return TextNode::CreateForCharacterRanges(zone, range, false, on_success,
                                            default_flags);
}

void AddUnicodeCaseEquivalents(ZoneList<CharacterRange>* ranges, Zone* zone) {
#ifdef V8_INTL_SUPPORT
  DCHECK(CharacterRange::IsCanonical(ranges));

  // Micro-optimization to avoid passing large ranges to UnicodeSet::closeOver.
  // See also https://crbug.com/v8/6727.
  // TODO(jgruber): This only covers the special case of the {0,0x10FFFF} range,
  // which we use frequently internally. But large ranges can also easily be
  // created by the user. We might want to have a more general caching mechanism
  // for such ranges.
  if (ranges->length() == 1 && ranges->at(0).IsEverything(kNonBmpEnd)) return;

  // Use ICU to compute the case fold closure over the ranges.
  icu::UnicodeSet set;
  for (int i = 0; i < ranges->length(); i++) {
    set.add(ranges->at(i).from(), ranges->at(i).to());
  }
  ranges->Clear();
  set.closeOver(USET_CASE_INSENSITIVE);
  // Full case mapping map single characters to multiple characters.
  // Those are represented as strings in the set. Remove them so that
  // we end up with only simple and common case mappings.
  set.removeAllStrings();
  for (int i = 0; i < set.getRangeCount(); i++) {
    ranges->Add(CharacterRange::Range(set.getRangeStart(i), set.getRangeEnd(i)),
                zone);
  }
  // No errors and everything we collected have been ranges.
  CharacterRange::Canonicalize(ranges);
#endif  // V8_INTL_SUPPORT
}

}  // namespace

RegExpNode* RegExpCharacterClass::ToNode(RegExpCompiler* compiler,
                                         RegExpNode* on_success) {
  set_.Canonicalize();
  Zone* zone = compiler->zone();
  ZoneList<CharacterRange>* ranges = this->ranges(zone);
  if (NeedsUnicodeCaseEquivalents(flags_)) {
    AddUnicodeCaseEquivalents(ranges, zone);
  }
  if (IsUnicode(flags_) && !compiler->one_byte() &&
      !contains_split_surrogate()) {
    if (is_negated()) {
      ZoneList<CharacterRange>* negated =
          new (zone) ZoneList<CharacterRange>(2, zone);
      CharacterRange::Negate(ranges, negated, zone);
      ranges = negated;
    }
    if (ranges->length() == 0) {
      JSRegExp::Flags default_flags;
      RegExpCharacterClass* fail =
          new (zone) RegExpCharacterClass(zone, ranges, default_flags);
      return new (zone) TextNode(fail, compiler->read_backward(), on_success);
    }
    if (standard_type() == '*') {
      return UnanchoredAdvance(compiler, on_success);
    } else {
      ChoiceNode* result = new (zone) ChoiceNode(2, zone);
      UnicodeRangeSplitter splitter(ranges);
      AddBmpCharacters(compiler, result, on_success, &splitter);
      AddNonBmpSurrogatePairs(compiler, result, on_success, &splitter);
      AddLoneLeadSurrogates(compiler, result, on_success, &splitter);
      AddLoneTrailSurrogates(compiler, result, on_success, &splitter);
      return result;
    }
  } else {
    return new (zone) TextNode(this, compiler->read_backward(), on_success);
  }
}

int CompareFirstChar(RegExpTree* const* a, RegExpTree* const* b) {
  RegExpAtom* atom1 = (*a)->AsAtom();
  RegExpAtom* atom2 = (*b)->AsAtom();
  uc16 character1 = atom1->data().at(0);
  uc16 character2 = atom2->data().at(0);
  if (character1 < character2) return -1;
  if (character1 > character2) return 1;
  return 0;
}

#ifdef V8_INTL_SUPPORT

// Case Insensitve comparesion
int CompareFirstCharCaseInsensitve(RegExpTree* const* a, RegExpTree* const* b) {
  RegExpAtom* atom1 = (*a)->AsAtom();
  RegExpAtom* atom2 = (*b)->AsAtom();
  icu::UnicodeString character1(atom1->data().at(0));
  return character1.caseCompare(atom2->data().at(0), U_FOLD_CASE_DEFAULT);
}

#else

static unibrow::uchar Canonical(
    unibrow::Mapping<unibrow::Ecma262Canonicalize>* canonicalize,
    unibrow::uchar c) {
  unibrow::uchar chars[unibrow::Ecma262Canonicalize::kMaxWidth];
  int length = canonicalize->get(c, '\0', chars);
  DCHECK_LE(length, 1);
  unibrow::uchar canonical = c;
  if (length == 1) canonical = chars[0];
  return canonical;
}

int CompareFirstCharCaseIndependent(
    unibrow::Mapping<unibrow::Ecma262Canonicalize>* canonicalize,
    RegExpTree* const* a, RegExpTree* const* b) {
  RegExpAtom* atom1 = (*a)->AsAtom();
  RegExpAtom* atom2 = (*b)->AsAtom();
  unibrow::uchar character1 = atom1->data().at(0);
  unibrow::uchar character2 = atom2->data().at(0);
  if (character1 == character2) return 0;
  if (character1 >= 'a' || character2 >= 'a') {
    character1 = Canonical(canonicalize, character1);
    character2 = Canonical(canonicalize, character2);
  }
  return static_cast<int>(character1) - static_cast<int>(character2);
}
#endif  // V8_INTL_SUPPORT

// We can stable sort runs of atoms, since the order does not matter if they
// start with different characters.
// Returns true if any consecutive atoms were found.
bool RegExpDisjunction::SortConsecutiveAtoms(RegExpCompiler* compiler) {
  ZoneList<RegExpTree*>* alternatives = this->alternatives();
  int length = alternatives->length();
  bool found_consecutive_atoms = false;
  for (int i = 0; i < length; i++) {
    while (i < length) {
      RegExpTree* alternative = alternatives->at(i);
      if (alternative->IsAtom()) break;
      i++;
    }
    // i is length or it is the index of an atom.
    if (i == length) break;
    int first_atom = i;
    JSRegExp::Flags flags = alternatives->at(i)->AsAtom()->flags();
    i++;
    while (i < length) {
      RegExpTree* alternative = alternatives->at(i);
      if (!alternative->IsAtom()) break;
      if (alternative->AsAtom()->flags() != flags) break;
      i++;
    }
    // Sort atoms to get ones with common prefixes together.
    // This step is more tricky if we are in a case-independent regexp,
    // because it would change /is|I/ to /I|is/, and order matters when
    // the regexp parts don't match only disjoint starting points. To fix
    // this we have a version of CompareFirstChar that uses case-
    // independent character classes for comparison.
    DCHECK_LT(first_atom, alternatives->length());
    DCHECK_LE(i, alternatives->length());
    DCHECK_LE(first_atom, i);
    if (IgnoreCase(flags)) {
#ifdef V8_INTL_SUPPORT
      alternatives->StableSort(CompareFirstCharCaseInsensitve, first_atom,
                               i - first_atom);
#else
      unibrow::Mapping<unibrow::Ecma262Canonicalize>* canonicalize =
          compiler->isolate()->regexp_macro_assembler_canonicalize();
      auto compare_closure = [canonicalize](RegExpTree* const* a,
                                            RegExpTree* const* b) {
        return CompareFirstCharCaseIndependent(canonicalize, a, b);
      };
      alternatives->StableSort(compare_closure, first_atom, i - first_atom);
#endif  // V8_INTL_SUPPORT
    } else {
      alternatives->StableSort(CompareFirstChar, first_atom, i - first_atom);
    }
    if (i - first_atom > 1) found_consecutive_atoms = true;
  }
  return found_consecutive_atoms;
}

// Optimizes ab|ac|az to a(?:b|c|d).
void RegExpDisjunction::RationalizeConsecutiveAtoms(RegExpCompiler* compiler) {
  Zone* zone = compiler->zone();
  ZoneList<RegExpTree*>* alternatives = this->alternatives();
  int length = alternatives->length();

  int write_posn = 0;
  int i = 0;
  while (i < length) {
    RegExpTree* alternative = alternatives->at(i);
    if (!alternative->IsAtom()) {
      alternatives->at(write_posn++) = alternatives->at(i);
      i++;
      continue;
    }
    RegExpAtom* const atom = alternative->AsAtom();
    JSRegExp::Flags flags = atom->flags();
#ifdef V8_INTL_SUPPORT
    icu::UnicodeString common_prefix(atom->data().at(0));
#else
    unibrow::uchar common_prefix = atom->data().at(0);
#endif  // V8_INTL_SUPPORT
    int first_with_prefix = i;
    int prefix_length = atom->length();
    i++;
    while (i < length) {
      alternative = alternatives->at(i);
      if (!alternative->IsAtom()) break;
      RegExpAtom* const atom = alternative->AsAtom();
      if (atom->flags() != flags) break;
#ifdef V8_INTL_SUPPORT
      icu::UnicodeString new_prefix(atom->data().at(0));
      if (new_prefix != common_prefix) {
        if (!IgnoreCase(flags)) break;
        if (common_prefix.caseCompare(new_prefix, U_FOLD_CASE_DEFAULT) != 0)
          break;
      }
#else
      unibrow::uchar new_prefix = atom->data().at(0);
      if (new_prefix != common_prefix) {
        if (!IgnoreCase(flags)) break;
        unibrow::Mapping<unibrow::Ecma262Canonicalize>* canonicalize =
            compiler->isolate()->regexp_macro_assembler_canonicalize();
        new_prefix = Canonical(canonicalize, new_prefix);
        common_prefix = Canonical(canonicalize, common_prefix);
        if (new_prefix != common_prefix) break;
      }
#endif  // V8_INTL_SUPPORT
      prefix_length = Min(prefix_length, atom->length());
      i++;
    }
    if (i > first_with_prefix + 2) {
      // Found worthwhile run of alternatives with common prefix of at least one
      // character.  The sorting function above did not sort on more than one
      // character for reasons of correctness, but there may still be a longer
      // common prefix if the terms were similar or presorted in the input.
      // Find out how long the common prefix is.
      int run_length = i - first_with_prefix;
      RegExpAtom* const atom = alternatives->at(first_with_prefix)->AsAtom();
      for (int j = 1; j < run_length && prefix_length > 1; j++) {
        RegExpAtom* old_atom =
            alternatives->at(j + first_with_prefix)->AsAtom();
        for (int k = 1; k < prefix_length; k++) {
          if (atom->data().at(k) != old_atom->data().at(k)) {
            prefix_length = k;
            break;
          }
        }
      }
      RegExpAtom* prefix = new (zone)
          RegExpAtom(atom->data().SubVector(0, prefix_length), flags);
      ZoneList<RegExpTree*>* pair = new (zone) ZoneList<RegExpTree*>(2, zone);
      pair->Add(prefix, zone);
      ZoneList<RegExpTree*>* suffixes =
          new (zone) ZoneList<RegExpTree*>(run_length, zone);
      for (int j = 0; j < run_length; j++) {
        RegExpAtom* old_atom =
            alternatives->at(j + first_with_prefix)->AsAtom();
        int len = old_atom->length();
        if (len == prefix_length) {
          suffixes->Add(new (zone) RegExpEmpty(), zone);
        } else {
          RegExpTree* suffix = new (zone) RegExpAtom(
              old_atom->data().SubVector(prefix_length, old_atom->length()),
              flags);
          suffixes->Add(suffix, zone);
        }
      }
      pair->Add(new (zone) RegExpDisjunction(suffixes), zone);
      alternatives->at(write_posn++) = new (zone) RegExpAlternative(pair);
    } else {
      // Just copy any non-worthwhile alternatives.
      for (int j = first_with_prefix; j < i; j++) {
        alternatives->at(write_posn++) = alternatives->at(j);
      }
    }
  }
  alternatives->Rewind(write_posn);  // Trim end of array.
}

// Optimizes b|c|z to [bcz].
void RegExpDisjunction::FixSingleCharacterDisjunctions(
    RegExpCompiler* compiler) {
  Zone* zone = compiler->zone();
  ZoneList<RegExpTree*>* alternatives = this->alternatives();
  int length = alternatives->length();

  int write_posn = 0;
  int i = 0;
  while (i < length) {
    RegExpTree* alternative = alternatives->at(i);
    if (!alternative->IsAtom()) {
      alternatives->at(write_posn++) = alternatives->at(i);
      i++;
      continue;
    }
    RegExpAtom* const atom = alternative->AsAtom();
    if (atom->length() != 1) {
      alternatives->at(write_posn++) = alternatives->at(i);
      i++;
      continue;
    }
    JSRegExp::Flags flags = atom->flags();
    DCHECK_IMPLIES(IsUnicode(flags),
                   !unibrow::Utf16::IsLeadSurrogate(atom->data().at(0)));
    bool contains_trail_surrogate =
        unibrow::Utf16::IsTrailSurrogate(atom->data().at(0));
    int first_in_run = i;
    i++;
    // Find a run of single-character atom alternatives that have identical
    // flags (case independence and unicode-ness).
    while (i < length) {
      alternative = alternatives->at(i);
      if (!alternative->IsAtom()) break;
      RegExpAtom* const atom = alternative->AsAtom();
      if (atom->length() != 1) break;
      if (atom->flags() != flags) break;
      DCHECK_IMPLIES(IsUnicode(flags),
                     !unibrow::Utf16::IsLeadSurrogate(atom->data().at(0)));
      contains_trail_surrogate |=
          unibrow::Utf16::IsTrailSurrogate(atom->data().at(0));
      i++;
    }
    if (i > first_in_run + 1) {
      // Found non-trivial run of single-character alternatives.
      int run_length = i - first_in_run;
      ZoneList<CharacterRange>* ranges =
          new (zone) ZoneList<CharacterRange>(2, zone);
      for (int j = 0; j < run_length; j++) {
        RegExpAtom* old_atom = alternatives->at(j + first_in_run)->AsAtom();
        DCHECK_EQ(old_atom->length(), 1);
        ranges->Add(CharacterRange::Singleton(old_atom->data().at(0)), zone);
      }
      RegExpCharacterClass::CharacterClassFlags character_class_flags;
      if (IsUnicode(flags) && contains_trail_surrogate) {
        character_class_flags = RegExpCharacterClass::CONTAINS_SPLIT_SURROGATE;
      }
      alternatives->at(write_posn++) = new (zone)
          RegExpCharacterClass(zone, ranges, flags, character_class_flags);
    } else {
      // Just copy any trivial alternatives.
      for (int j = first_in_run; j < i; j++) {
        alternatives->at(write_posn++) = alternatives->at(j);
      }
    }
  }
  alternatives->Rewind(write_posn);  // Trim end of array.
}

RegExpNode* RegExpDisjunction::ToNode(RegExpCompiler* compiler,
                                      RegExpNode* on_success) {
  ZoneList<RegExpTree*>* alternatives = this->alternatives();

  if (alternatives->length() > 2) {
    bool found_consecutive_atoms = SortConsecutiveAtoms(compiler);
    if (found_consecutive_atoms) RationalizeConsecutiveAtoms(compiler);
    FixSingleCharacterDisjunctions(compiler);
    if (alternatives->length() == 1) {
      return alternatives->at(0)->ToNode(compiler, on_success);
    }
  }

  int length = alternatives->length();

  ChoiceNode* result =
      new (compiler->zone()) ChoiceNode(length, compiler->zone());
  for (int i = 0; i < length; i++) {
    GuardedAlternative alternative(
        alternatives->at(i)->ToNode(compiler, on_success));
    result->AddAlternative(alternative);
  }
  return result;
}

RegExpNode* RegExpQuantifier::ToNode(RegExpCompiler* compiler,
                                     RegExpNode* on_success) {
  return ToNode(min(), max(), is_greedy(), body(), compiler, on_success);
}

namespace {
// Desugar \b to (?<=\w)(?=\W)|(?<=\W)(?=\w) and
//         \B to (?<=\w)(?=\w)|(?<=\W)(?=\W)
RegExpNode* BoundaryAssertionAsLookaround(RegExpCompiler* compiler,
                                          RegExpNode* on_success,
                                          RegExpAssertion::AssertionType type,
                                          JSRegExp::Flags flags) {
  DCHECK(NeedsUnicodeCaseEquivalents(flags));
  Zone* zone = compiler->zone();
  ZoneList<CharacterRange>* word_range =
      new (zone) ZoneList<CharacterRange>(2, zone);
  CharacterRange::AddClassEscape('w', word_range, true, zone);
  int stack_register = compiler->UnicodeLookaroundStackRegister();
  int position_register = compiler->UnicodeLookaroundPositionRegister();
  ChoiceNode* result = new (zone) ChoiceNode(2, zone);
  // Add two choices. The (non-)boundary could start with a word or
  // a non-word-character.
  for (int i = 0; i < 2; i++) {
    bool lookbehind_for_word = i == 0;
    bool lookahead_for_word =
        (type == RegExpAssertion::BOUNDARY) ^ lookbehind_for_word;
    // Look to the left.
    RegExpLookaround::Builder lookbehind(lookbehind_for_word, on_success,
                                         stack_register, position_register);
    RegExpNode* backward = TextNode::CreateForCharacterRanges(
        zone, word_range, true, lookbehind.on_match_success(), flags);
    // Look to the right.
    RegExpLookaround::Builder lookahead(lookahead_for_word,
                                        lookbehind.ForMatch(backward),
                                        stack_register, position_register);
    RegExpNode* forward = TextNode::CreateForCharacterRanges(
        zone, word_range, false, lookahead.on_match_success(), flags);
    result->AddAlternative(GuardedAlternative(lookahead.ForMatch(forward)));
  }
  return result;
}
}  // anonymous namespace

RegExpNode* RegExpAssertion::ToNode(RegExpCompiler* compiler,
                                    RegExpNode* on_success) {
  NodeInfo info;
  Zone* zone = compiler->zone();

  switch (assertion_type()) {
    case START_OF_LINE:
      return AssertionNode::AfterNewline(on_success);
    case START_OF_INPUT:
      return AssertionNode::AtStart(on_success);
    case BOUNDARY:
      return NeedsUnicodeCaseEquivalents(flags_)
                 ? BoundaryAssertionAsLookaround(compiler, on_success, BOUNDARY,
                                                 flags_)
                 : AssertionNode::AtBoundary(on_success);
    case NON_BOUNDARY:
      return NeedsUnicodeCaseEquivalents(flags_)
                 ? BoundaryAssertionAsLookaround(compiler, on_success,
                                                 NON_BOUNDARY, flags_)
                 : AssertionNode::AtNonBoundary(on_success);
    case END_OF_INPUT:
      return AssertionNode::AtEnd(on_success);
    case END_OF_LINE: {
      // Compile $ in multiline regexps as an alternation with a positive
      // lookahead in one side and an end-of-input on the other side.
      // We need two registers for the lookahead.
      int stack_pointer_register = compiler->AllocateRegister();
      int position_register = compiler->AllocateRegister();
      // The ChoiceNode to distinguish between a newline and end-of-input.
      ChoiceNode* result = new (zone) ChoiceNode(2, zone);
      // Create a newline atom.
      ZoneList<CharacterRange>* newline_ranges =
          new (zone) ZoneList<CharacterRange>(3, zone);
      CharacterRange::AddClassEscape('n', newline_ranges, false, zone);
      JSRegExp::Flags default_flags = JSRegExp::Flags();
      RegExpCharacterClass* newline_atom =
          new (zone) RegExpCharacterClass('n', default_flags);
      TextNode* newline_matcher =
          new (zone) TextNode(newline_atom, false,
                              ActionNode::PositiveSubmatchSuccess(
                                  stack_pointer_register, position_register,
                                  0,   // No captures inside.
                                  -1,  // Ignored if no captures.
                                  on_success));
      // Create an end-of-input matcher.
      RegExpNode* end_of_line = ActionNode::BeginSubmatch(
          stack_pointer_register, position_register, newline_matcher);
      // Add the two alternatives to the ChoiceNode.
      GuardedAlternative eol_alternative(end_of_line);
      result->AddAlternative(eol_alternative);
      GuardedAlternative end_alternative(AssertionNode::AtEnd(on_success));
      result->AddAlternative(end_alternative);
      return result;
    }
    default:
      UNREACHABLE();
  }
  return on_success;
}

RegExpNode* RegExpBackReference::ToNode(RegExpCompiler* compiler,
                                        RegExpNode* on_success) {
  return new (compiler->zone())
      BackReferenceNode(RegExpCapture::StartRegister(index()),
                        RegExpCapture::EndRegister(index()), flags_,
                        compiler->read_backward(), on_success);
}

RegExpNode* RegExpEmpty::ToNode(RegExpCompiler* compiler,
                                RegExpNode* on_success) {
  return on_success;
}

RegExpLookaround::Builder::Builder(bool is_positive, RegExpNode* on_success,
                                   int stack_pointer_register,
                                   int position_register,
                                   int capture_register_count,
                                   int capture_register_start)
    : is_positive_(is_positive),
      on_success_(on_success),
      stack_pointer_register_(stack_pointer_register),
      position_register_(position_register) {
  if (is_positive_) {
    on_match_success_ = ActionNode::PositiveSubmatchSuccess(
        stack_pointer_register, position_register, capture_register_count,
        capture_register_start, on_success_);
  } else {
    Zone* zone = on_success_->zone();
    on_match_success_ = new (zone) NegativeSubmatchSuccess(
        stack_pointer_register, position_register, capture_register_count,
        capture_register_start, zone);
  }
}

RegExpNode* RegExpLookaround::Builder::ForMatch(RegExpNode* match) {
  if (is_positive_) {
    return ActionNode::BeginSubmatch(stack_pointer_register_,
                                     position_register_, match);
  } else {
    Zone* zone = on_success_->zone();
    // We use a ChoiceNode to represent the negative lookaround. The first
    // alternative is the negative match. On success, the end node backtracks.
    // On failure, the second alternative is tried and leads to success.
    // NegativeLookaheadChoiceNode is a special ChoiceNode that ignores the
    // first exit when calculating quick checks.
    ChoiceNode* choice_node = new (zone) NegativeLookaroundChoiceNode(
        GuardedAlternative(match), GuardedAlternative(on_success_), zone);
    return ActionNode::BeginSubmatch(stack_pointer_register_,
                                     position_register_, choice_node);
  }
}

RegExpNode* RegExpLookaround::ToNode(RegExpCompiler* compiler,
                                     RegExpNode* on_success) {
  int stack_pointer_register = compiler->AllocateRegister();
  int position_register = compiler->AllocateRegister();

  const int registers_per_capture = 2;
  const int register_of_first_capture = 2;
  int register_count = capture_count_ * registers_per_capture;
  int register_start =
      register_of_first_capture + capture_from_ * registers_per_capture;

  RegExpNode* result;
  bool was_reading_backward = compiler->read_backward();
  compiler->set_read_backward(type() == LOOKBEHIND);
  Builder builder(is_positive(), on_success, stack_pointer_register,
                  position_register, register_count, register_start);
  RegExpNode* match = body_->ToNode(compiler, builder.on_match_success());
  result = builder.ForMatch(match);
  compiler->set_read_backward(was_reading_backward);
  return result;
}

RegExpNode* RegExpCapture::ToNode(RegExpCompiler* compiler,
                                  RegExpNode* on_success) {
  return ToNode(body(), index(), compiler, on_success);
}

RegExpNode* RegExpCapture::ToNode(RegExpTree* body, int index,
                                  RegExpCompiler* compiler,
                                  RegExpNode* on_success) {
  DCHECK_NOT_NULL(body);
  int start_reg = RegExpCapture::StartRegister(index);
  int end_reg = RegExpCapture::EndRegister(index);
  if (compiler->read_backward()) std::swap(start_reg, end_reg);
  RegExpNode* store_end = ActionNode::StorePosition(end_reg, true, on_success);
  RegExpNode* body_node = body->ToNode(compiler, store_end);
  return ActionNode::StorePosition(start_reg, true, body_node);
}

namespace {

class AssertionSequenceRewriter final {
 public:
  // TODO(jgruber): Consider moving this to a separate AST tree rewriter pass
  // instead of sprinkling rewrites into the AST->Node conversion process.
  static void MaybeRewrite(ZoneList<RegExpTree*>* terms, Zone* zone) {
    AssertionSequenceRewriter rewriter(terms, zone);

    static constexpr int kNoIndex = -1;
    int from = kNoIndex;

    for (int i = 0; i < terms->length(); i++) {
      RegExpTree* t = terms->at(i);
      if (from == kNoIndex && t->IsAssertion()) {
        from = i;  // Start a sequence.
      } else if (from != kNoIndex && !t->IsAssertion()) {
        // Terminate and process the sequence.
        if (i - from > 1) rewriter.Rewrite(from, i);
        from = kNoIndex;
      }
    }

    if (from != kNoIndex && terms->length() - from > 1) {
      rewriter.Rewrite(from, terms->length());
    }
  }

  // All assertions are zero width. A consecutive sequence of assertions is
  // order-independent. There's two ways we can optimize here:
  // 1. fold all identical assertions.
  // 2. if any assertion combinations are known to fail (e.g. \b\B), the entire
  //    sequence fails.
  void Rewrite(int from, int to) {
    DCHECK_GT(to, from + 1);

    // Bitfield of all seen assertions.
    uint32_t seen_assertions = 0;
    STATIC_ASSERT(RegExpAssertion::LAST_TYPE < kUInt32Size * kBitsPerByte);

    // Flags must match for folding.
    JSRegExp::Flags flags = terms_->at(from)->AsAssertion()->flags();
    bool saw_mismatched_flags = false;

    for (int i = from; i < to; i++) {
      RegExpAssertion* t = terms_->at(i)->AsAssertion();
      if (t->flags() != flags) saw_mismatched_flags = true;
      const uint32_t bit = 1 << t->assertion_type();

      if ((seen_assertions & bit) && !saw_mismatched_flags) {
        // Fold duplicates.
        terms_->Set(i, new (zone_) RegExpEmpty());
      }

      seen_assertions |= bit;
    }

    // Collapse failures.
    const uint32_t always_fails_mask =
        1 << RegExpAssertion::BOUNDARY | 1 << RegExpAssertion::NON_BOUNDARY;
    if ((seen_assertions & always_fails_mask) == always_fails_mask) {
      ReplaceSequenceWithFailure(from, to);
    }
  }

  void ReplaceSequenceWithFailure(int from, int to) {
    // Replace the entire sequence with a single node that always fails.
    // TODO(jgruber): Consider adding an explicit Fail kind. Until then, the
    // negated '*' (everything) range serves the purpose.
    ZoneList<CharacterRange>* ranges =
        new (zone_) ZoneList<CharacterRange>(0, zone_);
    RegExpCharacterClass* cc =
        new (zone_) RegExpCharacterClass(zone_, ranges, JSRegExp::Flags());
    terms_->Set(from, cc);

    // Zero out the rest.
    RegExpEmpty* empty = new (zone_) RegExpEmpty();
    for (int i = from + 1; i < to; i++) terms_->Set(i, empty);
  }

 private:
  AssertionSequenceRewriter(ZoneList<RegExpTree*>* terms, Zone* zone)
      : zone_(zone), terms_(terms) {}

  Zone* zone_;
  ZoneList<RegExpTree*>* terms_;
};

}  // namespace

RegExpNode* RegExpAlternative::ToNode(RegExpCompiler* compiler,
                                      RegExpNode* on_success) {
  ZoneList<RegExpTree*>* children = nodes();

  AssertionSequenceRewriter::MaybeRewrite(children, compiler->zone());

  RegExpNode* current = on_success;
  if (compiler->read_backward()) {
    for (int i = 0; i < children->length(); i++) {
      current = children->at(i)->ToNode(compiler, current);
    }
  } else {
    for (int i = children->length() - 1; i >= 0; i--) {
      current = children->at(i)->ToNode(compiler, current);
    }
  }
  return current;
}

static void AddClass(const int* elmv, int elmc,
                     ZoneList<CharacterRange>* ranges, Zone* zone) {
  elmc--;
  DCHECK_EQ(kRangeEndMarker, elmv[elmc]);
  for (int i = 0; i < elmc; i += 2) {
    DCHECK(elmv[i] < elmv[i + 1]);
    ranges->Add(CharacterRange::Range(elmv[i], elmv[i + 1] - 1), zone);
  }
}

static void AddClassNegated(const int* elmv, int elmc,
                            ZoneList<CharacterRange>* ranges, Zone* zone) {
  elmc--;
  DCHECK_EQ(kRangeEndMarker, elmv[elmc]);
  DCHECK_NE(0x0000, elmv[0]);
  DCHECK_NE(String::kMaxCodePoint, elmv[elmc - 1]);
  uc16 last = 0x0000;
  for (int i = 0; i < elmc; i += 2) {
    DCHECK(last <= elmv[i] - 1);
    DCHECK(elmv[i] < elmv[i + 1]);
    ranges->Add(CharacterRange::Range(last, elmv[i] - 1), zone);
    last = elmv[i + 1];
  }
  ranges->Add(CharacterRange::Range(last, String::kMaxCodePoint), zone);
}

void CharacterRange::AddClassEscape(char type, ZoneList<CharacterRange>* ranges,
                                    bool add_unicode_case_equivalents,
                                    Zone* zone) {
  if (add_unicode_case_equivalents && (type == 'w' || type == 'W')) {
    // See #sec-runtime-semantics-wordcharacters-abstract-operation
    // In case of unicode and ignore_case, we need to create the closure over
    // case equivalent characters before negating.
    ZoneList<CharacterRange>* new_ranges =
        new (zone) ZoneList<CharacterRange>(2, zone);
    AddClass(kWordRanges, kWordRangeCount, new_ranges, zone);
    AddUnicodeCaseEquivalents(new_ranges, zone);
    if (type == 'W') {
      ZoneList<CharacterRange>* negated =
          new (zone) ZoneList<CharacterRange>(2, zone);
      CharacterRange::Negate(new_ranges, negated, zone);
      new_ranges = negated;
    }
    ranges->AddAll(*new_ranges, zone);
    return;
  }
  AddClassEscape(type, ranges, zone);
}

void CharacterRange::AddClassEscape(char type, ZoneList<CharacterRange>* ranges,
                                    Zone* zone) {
  switch (type) {
    case 's':
      AddClass(kSpaceRanges, kSpaceRangeCount, ranges, zone);
      break;
    case 'S':
      AddClassNegated(kSpaceRanges, kSpaceRangeCount, ranges, zone);
      break;
    case 'w':
      AddClass(kWordRanges, kWordRangeCount, ranges, zone);
      break;
    case 'W':
      AddClassNegated(kWordRanges, kWordRangeCount, ranges, zone);
      break;
    case 'd':
      AddClass(kDigitRanges, kDigitRangeCount, ranges, zone);
      break;
    case 'D':
      AddClassNegated(kDigitRanges, kDigitRangeCount, ranges, zone);
      break;
    case '.':
      AddClassNegated(kLineTerminatorRanges, kLineTerminatorRangeCount, ranges,
                      zone);
      break;
    // This is not a character range as defined by the spec but a
    // convenient shorthand for a character class that matches any
    // character.
    case '*':
      ranges->Add(CharacterRange::Everything(), zone);
      break;
    // This is the set of characters matched by the $ and ^ symbols
    // in multiline mode.
    case 'n':
      AddClass(kLineTerminatorRanges, kLineTerminatorRangeCount, ranges, zone);
      break;
    default:
      UNREACHABLE();
  }
}

Vector<const int> CharacterRange::GetWordBounds() {
  return Vector<const int>(kWordRanges, kWordRangeCount - 1);
}

#ifdef V8_INTL_SUPPORT
struct IgnoreSet {
  IgnoreSet() : set(BuildIgnoreSet()) {}
  const icu::UnicodeSet set;
};

struct SpecialAddSet {
  SpecialAddSet() : set(BuildSpecialAddSet()) {}
  const icu::UnicodeSet set;
};

icu::UnicodeSet BuildAsciiAToZSet() {
  icu::UnicodeSet set('a', 'z');
  set.add('A', 'Z');
  set.freeze();
  return set;
}

struct AsciiAToZSet {
  AsciiAToZSet() : set(BuildAsciiAToZSet()) {}
  const icu::UnicodeSet set;
};

static base::LazyInstance<IgnoreSet>::type ignore_set =
    LAZY_INSTANCE_INITIALIZER;

static base::LazyInstance<SpecialAddSet>::type special_add_set =
    LAZY_INSTANCE_INITIALIZER;

static base::LazyInstance<AsciiAToZSet>::type ascii_a_to_z_set =
    LAZY_INSTANCE_INITIALIZER;
#endif  // V8_INTL_SUPPORT

// static
void CharacterRange::AddCaseEquivalents(Isolate* isolate, Zone* zone,
                                        ZoneList<CharacterRange>* ranges,
                                        bool is_one_byte) {
  CharacterRange::Canonicalize(ranges);
  int range_count = ranges->length();
#ifdef V8_INTL_SUPPORT
  icu::UnicodeSet others;
  for (int i = 0; i < range_count; i++) {
    CharacterRange range = ranges->at(i);
    uc32 from = range.from();
    if (from > String::kMaxUtf16CodeUnit) continue;
    uc32 to = Min(range.to(), String::kMaxUtf16CodeUnit);
    // Nothing to be done for surrogates.
    if (from >= kLeadSurrogateStart && to <= kTrailSurrogateEnd) continue;
    if (is_one_byte && !RangeContainsLatin1Equivalents(range)) {
      if (from > String::kMaxOneByteCharCode) continue;
      if (to > String::kMaxOneByteCharCode) to = String::kMaxOneByteCharCode;
    }
    others.add(from, to);
  }

  // Set of characters already added to ranges that do not need to be added
  // again.
  icu::UnicodeSet already_added(others);

  // Set of characters in ranges that are in the 52 ASCII characters [a-zA-Z].
  icu::UnicodeSet in_ascii_a_to_z(others);
  in_ascii_a_to_z.retainAll(ascii_a_to_z_set.Pointer()->set);

  // Remove all chars in [a-zA-Z] from others.
  others.removeAll(in_ascii_a_to_z);

  // Set of characters in ranges that are overlapping with special add set.
  icu::UnicodeSet in_special_add(others);
  in_special_add.retainAll(special_add_set.Pointer()->set);

  others.removeAll(in_special_add);

  // Ignore all chars in ignore set.
  others.removeAll(ignore_set.Pointer()->set);

  // For most of the chars in ranges that is still in others, find the case
  // equivlant set by calling closeOver(USET_CASE_INSENSITIVE).
  others.closeOver(USET_CASE_INSENSITIVE);

  // Because closeOver(USET_CASE_INSENSITIVE) may add ASCII [a-zA-Z] to others,
  // but ECMA262 "i" mode won't consider that, remove them from others.
  // Ex: U+017F add 'S' and 's' to others.
  others.removeAll(ascii_a_to_z_set.Pointer()->set);

  // Special handling for in_ascii_a_to_z.
  for (int32_t i = 0; i < in_ascii_a_to_z.getRangeCount(); i++) {
    UChar32 start = in_ascii_a_to_z.getRangeStart(i);
    UChar32 end = in_ascii_a_to_z.getRangeEnd(i);
    // Check if it is uppercase A-Z by checking bit 6.
    if (start & 0x0020) {
      // Add the lowercases
      others.add(start & 0x005F, end & 0x005F);
    } else {
      // Add the uppercases
      others.add(start | 0x0020, end | 0x0020);
    }
  }

  // Special handling for chars in "Special Add" set.
  for (int32_t i = 0; i < in_special_add.getRangeCount(); i++) {
    UChar32 end = in_special_add.getRangeEnd(i);
    for (UChar32 ch = in_special_add.getRangeStart(i); ch <= end; ch++) {
      // Add the uppercase of this character if itself is not an uppercase
      // character.
      // Note: The if condiction cannot be u_islower(ch) because ch could be
      // neither uppercase nor lowercase but Mn.
      if (!u_isupper(ch)) {
        others.add(u_toupper(ch));
      }
      icu::UnicodeSet candidates(ch, ch);
      candidates.closeOver(USET_CASE_INSENSITIVE);
      for (int32_t j = 0; j < candidates.getRangeCount(); j++) {
        UChar32 end2 = candidates.getRangeEnd(j);
        for (UChar32 ch2 = candidates.getRangeStart(j); ch2 <= end2; ch2++) {
          // Add character that is not uppercase to others.
          if (!u_isupper(ch2)) {
            others.add(ch2);
          }
        }
      }
    }
  }

  // Remove all characters which already in the ranges.
  others.removeAll(already_added);

  // Add others to the ranges
  for (int32_t i = 0; i < others.getRangeCount(); i++) {
    UChar32 from = others.getRangeStart(i);
    UChar32 to = others.getRangeEnd(i);
    if (from == to) {
      ranges->Add(CharacterRange::Singleton(from), zone);
    } else {
      ranges->Add(CharacterRange::Range(from, to), zone);
    }
  }
#else
  for (int i = 0; i < range_count; i++) {
    CharacterRange range = ranges->at(i);
    uc32 bottom = range.from();
    if (bottom > String::kMaxUtf16CodeUnit) continue;
    uc32 top = Min(range.to(), String::kMaxUtf16CodeUnit);
    // Nothing to be done for surrogates.
    if (bottom >= kLeadSurrogateStart && top <= kTrailSurrogateEnd) continue;
    if (is_one_byte && !RangeContainsLatin1Equivalents(range)) {
      if (bottom > String::kMaxOneByteCharCode) continue;
      if (top > String::kMaxOneByteCharCode) top = String::kMaxOneByteCharCode;
    }
    unibrow::uchar chars[unibrow::Ecma262UnCanonicalize::kMaxWidth];
    if (top == bottom) {
      // If this is a singleton we just expand the one character.
      int length = isolate->jsregexp_uncanonicalize()->get(bottom, '\0', chars);
      for (int i = 0; i < length; i++) {
        uc32 chr = chars[i];
        if (chr != bottom) {
          ranges->Add(CharacterRange::Singleton(chars[i]), zone);
        }
      }
    } else {
      // If this is a range we expand the characters block by block, expanding
      // contiguous subranges (blocks) one at a time.  The approach is as
      // follows.  For a given start character we look up the remainder of the
      // block that contains it (represented by the end point), for instance we
      // find 'z' if the character is 'c'.  A block is characterized by the
      // property that all characters uncanonicalize in the same way, except
      // that each entry in the result is incremented by the distance from the
      // first element.  So a-z is a block because 'a' uncanonicalizes to ['a',
      // 'A'] and the k'th letter uncanonicalizes to ['a' + k, 'A' + k].  Once
      // we've found the end point we look up its uncanonicalization and
      // produce a range for each element.  For instance for [c-f] we look up
      // ['z', 'Z'] and produce [c-f] and [C-F].  We then only add a range if
      // it is not already contained in the input, so [c-f] will be skipped but
      // [C-F] will be added.  If this range is not completely contained in a
      // block we do this for all the blocks covered by the range (handling
      // characters that is not in a block as a "singleton block").
      unibrow::uchar equivalents[unibrow::Ecma262UnCanonicalize::kMaxWidth];
      int pos = bottom;
      while (pos <= top) {
        int length =
            isolate->jsregexp_canonrange()->get(pos, '\0', equivalents);
        uc32 block_end;
        if (length == 0) {
          block_end = pos;
        } else {
          DCHECK_EQ(1, length);
          block_end = equivalents[0];
        }
        int end = (block_end > top) ? top : block_end;
        length = isolate->jsregexp_uncanonicalize()->get(block_end, '\0',
                                                         equivalents);
        for (int i = 0; i < length; i++) {
          uc32 c = equivalents[i];
          uc32 range_from = c - (block_end - pos);
          uc32 range_to = c - (block_end - end);
          if (!(bottom <= range_from && range_to <= top)) {
            ranges->Add(CharacterRange::Range(range_from, range_to), zone);
          }
        }
        pos = end + 1;
      }
    }
  }
#endif  // V8_INTL_SUPPORT
}

bool CharacterRange::IsCanonical(ZoneList<CharacterRange>* ranges) {
  DCHECK_NOT_NULL(ranges);
  int n = ranges->length();
  if (n <= 1) return true;
  int max = ranges->at(0).to();
  for (int i = 1; i < n; i++) {
    CharacterRange next_range = ranges->at(i);
    if (next_range.from() <= max + 1) return false;
    max = next_range.to();
  }
  return true;
}

ZoneList<CharacterRange>* CharacterSet::ranges(Zone* zone) {
  if (ranges_ == nullptr) {
    ranges_ = new (zone) ZoneList<CharacterRange>(2, zone);
    CharacterRange::AddClassEscape(standard_set_type_, ranges_, false, zone);
  }
  return ranges_;
}

// Move a number of elements in a zonelist to another position
// in the same list. Handles overlapping source and target areas.
static void MoveRanges(ZoneList<CharacterRange>* list, int from, int to,
                       int count) {
  // Ranges are potentially overlapping.
  if (from < to) {
    for (int i = count - 1; i >= 0; i--) {
      list->at(to + i) = list->at(from + i);
    }
  } else {
    for (int i = 0; i < count; i++) {
      list->at(to + i) = list->at(from + i);
    }
  }
}

static int InsertRangeInCanonicalList(ZoneList<CharacterRange>* list, int count,
                                      CharacterRange insert) {
  // Inserts a range into list[0..count[, which must be sorted
  // by from value and non-overlapping and non-adjacent, using at most
  // list[0..count] for the result. Returns the number of resulting
  // canonicalized ranges. Inserting a range may collapse existing ranges into
  // fewer ranges, so the return value can be anything in the range 1..count+1.
  uc32 from = insert.from();
  uc32 to = insert.to();
  int start_pos = 0;
  int end_pos = count;
  for (int i = count - 1; i >= 0; i--) {
    CharacterRange current = list->at(i);
    if (current.from() > to + 1) {
      end_pos = i;
    } else if (current.to() + 1 < from) {
      start_pos = i + 1;
      break;
    }
  }

  // Inserted range overlaps, or is adjacent to, ranges at positions
  // [start_pos..end_pos[. Ranges before start_pos or at or after end_pos are
  // not affected by the insertion.
  // If start_pos == end_pos, the range must be inserted before start_pos.
  // if start_pos < end_pos, the entire range from start_pos to end_pos
  // must be merged with the insert range.

  if (start_pos == end_pos) {
    // Insert between existing ranges at position start_pos.
    if (start_pos < count) {
      MoveRanges(list, start_pos, start_pos + 1, count - start_pos);
    }
    list->at(start_pos) = insert;
    return count + 1;
  }
  if (start_pos + 1 == end_pos) {
    // Replace single existing range at position start_pos.
    CharacterRange to_replace = list->at(start_pos);
    int new_from = Min(to_replace.from(), from);
    int new_to = Max(to_replace.to(), to);
    list->at(start_pos) = CharacterRange::Range(new_from, new_to);
    return count;
  }
  // Replace a number of existing ranges from start_pos to end_pos - 1.
  // Move the remaining ranges down.

  int new_from = Min(list->at(start_pos).from(), from);
  int new_to = Max(list->at(end_pos - 1).to(), to);
  if (end_pos < count) {
    MoveRanges(list, end_pos, start_pos + 1, count - end_pos);
  }
  list->at(start_pos) = CharacterRange::Range(new_from, new_to);
  return count - (end_pos - start_pos) + 1;
}

void CharacterSet::Canonicalize() {
  // Special/default classes are always considered canonical. The result
  // of calling ranges() will be sorted.
  if (ranges_ == nullptr) return;
  CharacterRange::Canonicalize(ranges_);
}

void CharacterRange::Canonicalize(ZoneList<CharacterRange>* character_ranges) {
  if (character_ranges->length() <= 1) return;
  // Check whether ranges are already canonical (increasing, non-overlapping,
  // non-adjacent).
  int n = character_ranges->length();
  int max = character_ranges->at(0).to();
  int i = 1;
  while (i < n) {
    CharacterRange current = character_ranges->at(i);
    if (current.from() <= max + 1) {
      break;
    }
    max = current.to();
    i++;
  }
  // Canonical until the i'th range. If that's all of them, we are done.
  if (i == n) return;

  // The ranges at index i and forward are not canonicalized. Make them so by
  // doing the equivalent of insertion sort (inserting each into the previous
  // list, in order).
  // Notice that inserting a range can reduce the number of ranges in the
  // result due to combining of adjacent and overlapping ranges.
  int read = i;           // Range to insert.
  int num_canonical = i;  // Length of canonicalized part of list.
  do {
    num_canonical = InsertRangeInCanonicalList(character_ranges, num_canonical,
                                               character_ranges->at(read));
    read++;
  } while (read < n);
  character_ranges->Rewind(num_canonical);

  DCHECK(CharacterRange::IsCanonical(character_ranges));
}

void CharacterRange::Negate(ZoneList<CharacterRange>* ranges,
                            ZoneList<CharacterRange>* negated_ranges,
                            Zone* zone) {
  DCHECK(CharacterRange::IsCanonical(ranges));
  DCHECK_EQ(0, negated_ranges->length());
  int range_count = ranges->length();
  uc32 from = 0;
  int i = 0;
  if (range_count > 0 && ranges->at(0).from() == 0) {
    from = ranges->at(0).to() + 1;
    i = 1;
  }
  while (i < range_count) {
    CharacterRange range = ranges->at(i);
    negated_ranges->Add(CharacterRange::Range(from, range.from() - 1), zone);
    from = range.to() + 1;
    i++;
  }
  if (from < String::kMaxCodePoint) {
    negated_ranges->Add(CharacterRange::Range(from, String::kMaxCodePoint),
                        zone);
  }
}

// Scoped object to keep track of how much we unroll quantifier loops in the
// regexp graph generator.
class RegExpExpansionLimiter {
 public:
  static const int kMaxExpansionFactor = 6;
  RegExpExpansionLimiter(RegExpCompiler* compiler, int factor)
      : compiler_(compiler),
        saved_expansion_factor_(compiler->current_expansion_factor()),
        ok_to_expand_(saved_expansion_factor_ <= kMaxExpansionFactor) {
    DCHECK_LT(0, factor);
    if (ok_to_expand_) {
      if (factor > kMaxExpansionFactor) {
        // Avoid integer overflow of the current expansion factor.
        ok_to_expand_ = false;
        compiler->set_current_expansion_factor(kMaxExpansionFactor + 1);
      } else {
        int new_factor = saved_expansion_factor_ * factor;
        ok_to_expand_ = (new_factor <= kMaxExpansionFactor);
        compiler->set_current_expansion_factor(new_factor);
      }
    }
  }

  ~RegExpExpansionLimiter() {
    compiler_->set_current_expansion_factor(saved_expansion_factor_);
  }

  bool ok_to_expand() { return ok_to_expand_; }

 private:
  RegExpCompiler* compiler_;
  int saved_expansion_factor_;
  bool ok_to_expand_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RegExpExpansionLimiter);
};

RegExpNode* RegExpQuantifier::ToNode(int min, int max, bool is_greedy,
                                     RegExpTree* body, RegExpCompiler* compiler,
                                     RegExpNode* on_success,
                                     bool not_at_start) {
  // x{f, t} becomes this:
  //
  //             (r++)<-.
  //               |     `
  //               |     (x)
  //               v     ^
  //      (r=0)-->(?)---/ [if r < t]
  //               |
  //   [if r >= f] \----> ...
  //

  // 15.10.2.5 RepeatMatcher algorithm.
  // The parser has already eliminated the case where max is 0.  In the case
  // where max_match is zero the parser has removed the quantifier if min was
  // > 0 and removed the atom if min was 0.  See AddQuantifierToAtom.

  // If we know that we cannot match zero length then things are a little
  // simpler since we don't need to make the special zero length match check
  // from step 2.1.  If the min and max are small we can unroll a little in
  // this case.
  static const int kMaxUnrolledMinMatches = 3;  // Unroll (foo)+ and (foo){3,}
  static const int kMaxUnrolledMaxMatches = 3;  // Unroll (foo)? and (foo){x,3}
  if (max == 0) return on_success;  // This can happen due to recursion.
  bool body_can_be_empty = (body->min_match() == 0);
  int body_start_reg = RegExpCompiler::kNoRegister;
  Interval capture_registers = body->CaptureRegisters();
  bool needs_capture_clearing = !capture_registers.is_empty();
  Zone* zone = compiler->zone();

  if (body_can_be_empty) {
    body_start_reg = compiler->AllocateRegister();
  } else if (compiler->optimize() && !needs_capture_clearing) {
    // Only unroll if there are no captures and the body can't be
    // empty.
    {
      RegExpExpansionLimiter limiter(compiler, min + ((max != min) ? 1 : 0));
      if (min > 0 && min <= kMaxUnrolledMinMatches && limiter.ok_to_expand()) {
        int new_max = (max == kInfinity) ? max : max - min;
        // Recurse once to get the loop or optional matches after the fixed
        // ones.
        RegExpNode* answer =
            ToNode(0, new_max, is_greedy, body, compiler, on_success, true);
        // Unroll the forced matches from 0 to min.  This can cause chains of
        // TextNodes (which the parser does not generate).  These should be
        // combined if it turns out they hinder good code generation.
        for (int i = 0; i < min; i++) {
          answer = body->ToNode(compiler, answer);
        }
        return answer;
      }
    }
    if (max <= kMaxUnrolledMaxMatches && min == 0) {
      DCHECK_LT(0, max);  // Due to the 'if' above.
      RegExpExpansionLimiter limiter(compiler, max);
      if (limiter.ok_to_expand()) {
        // Unroll the optional matches up to max.
        RegExpNode* answer = on_success;
        for (int i = 0; i < max; i++) {
          ChoiceNode* alternation = new (zone) ChoiceNode(2, zone);
          if (is_greedy) {
            alternation->AddAlternative(
                GuardedAlternative(body->ToNode(compiler, answer)));
            alternation->AddAlternative(GuardedAlternative(on_success));
          } else {
            alternation->AddAlternative(GuardedAlternative(on_success));
            alternation->AddAlternative(
                GuardedAlternative(body->ToNode(compiler, answer)));
          }
          answer = alternation;
          if (not_at_start && !compiler->read_backward()) {
            alternation->set_not_at_start();
          }
        }
        return answer;
      }
    }
  }
  bool has_min = min > 0;
  bool has_max = max < RegExpTree::kInfinity;
  bool needs_counter = has_min || has_max;
  int reg_ctr = needs_counter ? compiler->AllocateRegister()
                              : RegExpCompiler::kNoRegister;
  LoopChoiceNode* center = new (zone) LoopChoiceNode(
      body->min_match() == 0, compiler->read_backward(), min, zone);
  if (not_at_start && !compiler->read_backward()) center->set_not_at_start();
  RegExpNode* loop_return =
      needs_counter ? static_cast<RegExpNode*>(
                          ActionNode::IncrementRegister(reg_ctr, center))
                    : static_cast<RegExpNode*>(center);
  if (body_can_be_empty) {
    // If the body can be empty we need to check if it was and then
    // backtrack.
    loop_return =
        ActionNode::EmptyMatchCheck(body_start_reg, reg_ctr, min, loop_return);
  }
  RegExpNode* body_node = body->ToNode(compiler, loop_return);
  if (body_can_be_empty) {
    // If the body can be empty we need to store the start position
    // so we can bail out if it was empty.
    body_node = ActionNode::StorePosition(body_start_reg, false, body_node);
  }
  if (needs_capture_clearing) {
    // Before entering the body of this loop we need to clear captures.
    body_node = ActionNode::ClearCaptures(capture_registers, body_node);
  }
  GuardedAlternative body_alt(body_node);
  if (has_max) {
    Guard* body_guard = new (zone) Guard(reg_ctr, Guard::LT, max);
    body_alt.AddGuard(body_guard, zone);
  }
  GuardedAlternative rest_alt(on_success);
  if (has_min) {
    Guard* rest_guard = new (compiler->zone()) Guard(reg_ctr, Guard::GEQ, min);
    rest_alt.AddGuard(rest_guard, zone);
  }
  if (is_greedy) {
    center->AddLoopAlternative(body_alt);
    center->AddContinueAlternative(rest_alt);
  } else {
    center->AddContinueAlternative(rest_alt);
    center->AddLoopAlternative(body_alt);
  }
  if (needs_counter) {
    return ActionNode::SetRegisterForLoop(reg_ctr, 0, center);
  } else {
    return center;
  }
}

}  // namespace internal
}  // namespace v8

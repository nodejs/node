#include "glob/glob_unicode.h"

#include <algorithm>
#include <tuple>

#ifdef NODE_HAVE_I18N_SUPPORT
#include <unicode/uchar.h>
#include <unicode/uniset.h>
#include <unicode/ustring.h>
#endif  // NODE_HAVE_I18N_SUPPORT

namespace node::glob {

namespace {

constexpr uint32_t kMaxCodePoint = 0x10FFFF;
constexpr uint32_t kMaxCodeUnit = 0xFFFF;

#ifdef NODE_HAVE_I18N_SUPPORT
uint32_t PropertyMask(UnicodeProperty property) {
  switch (property) {
    case UnicodeProperty::kLetter:
      return U_GC_L_MASK;
    case UnicodeProperty::kLetterNumber:
      return U_GC_NL_MASK;
    case UnicodeProperty::kDecimalNumber:
      return U_GC_ND_MASK;
    case UnicodeProperty::kSpaceSeparator:
      return U_GC_ZS_MASK;
    case UnicodeProperty::kControl:
      return U_GC_CC_MASK;
    case UnicodeProperty::kSeparator:
      return U_GC_Z_MASK;
    case UnicodeProperty::kOther:
      return U_GC_C_MASK;
    case UnicodeProperty::kLowercase:
      return U_GC_LL_MASK;
    case UnicodeProperty::kUppercase:
      return U_GC_LU_MASK;
    case UnicodeProperty::kPunctuation:
      return U_GC_P_MASK;
    case UnicodeProperty::kConnector:
      return U_GC_PC_MASK;
  }
  return 0;
}
#endif  // NODE_HAVE_I18N_SUPPORT

}  // namespace

void CharSet::AddRange(uint32_t lo, uint32_t hi) {
  if (lo > hi) return;
  ranges_.push_back({lo, std::min(hi, kMaxCodePoint)});
}

bool CharSet::AddProperty(UnicodeProperty property) {
#ifdef NODE_HAVE_I18N_SUPPORT
  const uint32_t mask = PropertyMask(property);
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeSet set;
  set.applyIntPropertyValue(
      UCHAR_GENERAL_CATEGORY_MASK, static_cast<int32_t>(mask), status);
  if (U_FAILURE(status)) return false;
  const int32_t count = set.getRangeCount();
  for (int32_t i = 0; i < count; i++) {
    AddRange(static_cast<uint32_t>(set.getRangeStart(i)),
             static_cast<uint32_t>(set.getRangeEnd(i)));
  }
  return true;
#else
  return false;
#endif  // NODE_HAVE_I18N_SUPPORT
}

void CharSet::Seal() {
  if (ranges_.empty()) return;
  std::ranges::sort(
      ranges_, {}, [](const Range& r) { return std::tie(r.lo, r.hi); });
  std::vector<Range> merged;
  merged.push_back(ranges_.front());
  for (size_t i = 1; i < ranges_.size(); i++) {
    Range& last = merged.back();
    const Range& next = ranges_[i];
    // Adjacent ranges merge too, so the result is canonical.
    if (next.lo <= last.hi || next.lo == last.hi + 1) {
      last.hi = std::max(last.hi, next.hi);
    } else {
      merged.push_back(next);
    }
  }
  ranges_ = std::move(merged);
}

bool CharSet::CloseOverCaseFold() {
  Seal();
#ifdef NODE_HAVE_I18N_SUPPORT
  icu::UnicodeSet set;
  for (const Range& range : ranges_) {
    set.add(static_cast<UChar32>(range.lo), static_cast<UChar32>(range.hi));
  }
  set.closeOver(USET_CASE_INSENSITIVE);
  ranges_.clear();
  const int32_t count = set.getRangeCount();
  for (int32_t i = 0; i < count; i++) {
    AddRange(static_cast<uint32_t>(set.getRangeStart(i)),
             static_cast<uint32_t>(set.getRangeEnd(i)));
  }
  Seal();
  return true;
#else
  const std::vector<Range> original = ranges_;
  for (const Range& range : original) {
    for (uint32_t cp = range.lo; cp <= std::min(range.hi, uint32_t{'z'});
         cp++) {
      AddChar(AsciiLower(cp));
      AddChar(AsciiUpper(cp));
    }
  }
  Seal();
  return false;
#endif  // NODE_HAVE_I18N_SUPPORT
}

void CharSet::CanonicalizeMembers() {
  Seal();
  const std::vector<Range> original = ranges_;
  ranges_.clear();
  for (const Range& range : original) {
    if (range.lo > kMaxCodeUnit) continue;
    const uint32_t hi = std::min(range.hi, kMaxCodeUnit);
    for (uint32_t cp = range.lo; cp <= hi; cp++) {
      AddChar(Canonicalize(cp, /*fold=*/false));
      if (cp == kMaxCodeUnit) break;
    }
  }
  Seal();
}

bool CharSet::Contains(uint32_t cp) const {
  // Seal() left the ranges sorted, so this is fairly simple.
  const auto after = std::ranges::upper_bound(
      ranges_, cp, {}, [](const Range& r) { return r.lo; });
  return after != ranges_.begin() && cp <= std::prev(after)->hi;
}

uint32_t Canonicalize(uint32_t cp, bool fold) {
  if (cp <= kMaxAsciiCodePoint) return AsciiUpper(cp);
#ifdef NODE_HAVE_I18N_SUPPORT
  if (fold) {
    return static_cast<uint32_t>(
        u_foldCase(static_cast<UChar32>(cp), U_FOLD_CASE_DEFAULT));
  }
  const uint32_t upper =
      static_cast<uint32_t>(u_toupper(static_cast<UChar32>(cp)));

  // A non-ASCII character whose uppercase mapping is ASCII must
  // not fold into it
  return upper <= kMaxAsciiCodePoint ? cp : upper;
#else
  return cp;
#endif  // NODE_HAVE_I18N_SUPPORT
}

void AppendLowered(const std::u16string& text, std::u16string* out) {
#ifdef NODE_HAVE_I18N_SUPPORT
  const int32_t length = static_cast<int32_t>(text.size());
  const size_t offset = out->size();
  // Lowercasing can lengthen a strings (oddly, see U+0130) so
  // reserve a little slack.
  //
  // Should a pattern for whatever reason contain a number of these
  // odd expansions, the U_BUFFER_OVERFLOW_ERROR case below will handle
  // them using a slower path.
  out->resize(offset + text.size() + 8);
  UErrorCode status = U_ZERO_ERROR;
  int32_t written = u_strToLower(reinterpret_cast<UChar*>(out->data() + offset),
                                 static_cast<int32_t>(out->size() - offset),
                                 reinterpret_cast<const UChar*>(text.data()),
                                 length,
                                 "",
                                 &status);
  if (status == U_BUFFER_OVERFLOW_ERROR) {
    out->resize(offset + written);
    status = U_ZERO_ERROR;
    written = u_strToLower(reinterpret_cast<UChar*>(out->data() + offset),
                           static_cast<int32_t>(out->size() - offset),
                           reinterpret_cast<const UChar*>(text.data()),
                           length,
                           "",
                           &status);
  }
  if (U_FAILURE(status)) {
    out->resize(offset);
    out->append(text);
    return;
  }
  out->resize(offset + written);
#else
  for (char16_t c : text) {
    out->push_back(static_cast<char16_t>(AsciiLower(c)));
  }
#endif  // NODE_HAVE_I18N_SUPPORT
}

}  // namespace node::glob

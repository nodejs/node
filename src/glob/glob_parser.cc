#include "glob/glob_parser.h"

#include <algorithm>
#include <bitset>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_set>

#include "simdutf.h"
#include "util.h"

namespace node::glob {

// U+FFFD, substituted for each byte of an ill-formed UTF-8 sequence.
constexpr char16_t kReplacementCharacter = 0xFFFD;

void Utf8ToUtf16(std::string_view utf8, PatternString* out) {
  PatternString& o = *out;
  o.resize(simdutf::utf16_length_from_utf8(utf8.data(), utf8.size()));
  simdutf::result status = simdutf::convert_utf8_to_utf16_with_errors(
      utf8.data(), utf8.size(), o.data());
  if (status.error == simdutf::SUCCESS) {
    o.resize(status.count);
    return;
  }

  o.resize(utf8.size());
  size_t read = 0;
  size_t written = 0;
  while (read < utf8.size()) {
    const std::string_view rest = utf8.substr(read);
    status = simdutf::convert_utf8_to_utf16_with_errors(
        rest.data(), rest.size(), o.data() + written);
    if (status.error == simdutf::SUCCESS) {
      written += status.count;
      break;
    }
    // On failure `count` is where in the input the error is.
    written += simdutf::convert_valid_utf8_to_utf16(
        rest.data(), status.count, o.data() + written);
    o[written++] = kReplacementCharacter;
    read += status.count + 1;
  }
  o.resize(written);
}

std::string Utf16ToUtf8(PatternView utf16) {
  PatternString well_formed;
  PatternView source = utf16;
  if (simdutf::validate_utf16_with_errors(utf16.data(), utf16.size()).error !=
      simdutf::SUCCESS) {
    well_formed.resize(utf16.size());
    simdutf::to_well_formed_utf16(
        utf16.data(), utf16.size(), well_formed.data());
    source = well_formed;
  }
  std::string out;
  out.resize(simdutf::utf8_length_from_utf16(source.data(), source.size()));
  out.resize(
      simdutf::convert_utf16_to_utf8(source.data(), source.size(), out.data()));
  return out;
}

namespace {

constexpr ptrdiff_t kNone = -1;

ptrdiff_t IndexOf(PatternView str, char16_t c, ptrdiff_t from) {
  if (from < 0) from = 0;
  const size_t pos = str.find(c, static_cast<size_t>(from));
  return pos == PatternView::npos ? kNone : static_cast<ptrdiff_t>(pos);
}

struct Balanced {
  bool found = false;
  size_t start = 0;  // index of '{'
  size_t end = 0;    // index of '}'
};

// Port of balanced-match's range('{', '}', str).
Balanced BalancedBraces(PatternView str) {
  Balanced out;
  ptrdiff_t ai = IndexOf(str, u'{', 0);
  ptrdiff_t bi = IndexOf(str, u'}', ai + 1);
  ptrdiff_t i = ai;
  bool has_result = false;
  ptrdiff_t r0 = 0, r1 = 0;
  if (ai >= 0 && bi > 0) {
    std::vector<ptrdiff_t> begs;
    ptrdiff_t left = static_cast<ptrdiff_t>(str.size());
    ptrdiff_t right = kNone;
    bool has_right = false;
    while (i >= 0 && !has_result) {
      if (i == ai) {
        begs.push_back(i);
        ai = IndexOf(str, u'{', i + 1);
      } else if (begs.size() == 1) {
        r0 = begs.back();
        begs.pop_back();
        r1 = bi;
        has_result = true;
      } else {
        const ptrdiff_t beg = begs.back();
        begs.pop_back();
        if (beg < left) {
          left = beg;
          right = bi;
          has_right = true;
        }
        bi = IndexOf(str, u'}', i + 1);
      }
      i = (ai < bi && ai >= 0) ? ai : bi;
    }
    if (!begs.empty() && has_right) {
      r0 = left;
      r1 = right;
      has_result = true;
    }
  }
  if (has_result) {
    out.found = true;
    out.start = static_cast<size_t>(r0);
    out.end = static_cast<size_t>(r1);
  }
  return out;
}

// Sentinels are drawn from this unicode block because its code points
// are permanently unassigned, so no meaningful pattern contains one.
//
// One may want to salt its sentinels randomly for the same reason,
// but we just hardcode these values for simplicity. The value themselves
// does not matter as much as the unicode character it represents does
// not exist.
constexpr char16_t kNoncharacterFirst = 0xFDD0;
constexpr char16_t kNoncharacterLast = 0xFDEF;
constexpr uint32_t kNoncharacterCount =
    kNoncharacterLast - kNoncharacterFirst + 1;
static_assert(kNoncharacterCount <= std::numeric_limits<uint32_t>::digits,
              "the noncharacter block must fit in the `used` bitmask");

constexpr size_t kCodeUnitCount =
    size_t{std::numeric_limits<char16_t>::max()} + 1;

// Stand-ins for a literal '{' and '}' while braces are expanded.
struct BraceSentinels {
  char16_t open = kNoncharacterFirst;
  char16_t close = kNoncharacterFirst + 1;
};

// Returns nullopt when the pattern already contains every unit a sentinel
// could be drawn from, which leaves no way to mark a literal brace.
//
// Otherwise, returns a sentinel that does not exist in the source regex.
// Prefers unassigned unicode points for speed.
//
// A normal (or any, for that matter) user should never encounter that case.
std::optional<BraceSentinels> ChooseSentinels(PatternView pattern) {
  uint32_t used = 0;
  for (const char16_t c : pattern) {
    if (c >= kNoncharacterFirst && c <= kNoncharacterLast) {
      used |= uint32_t{1} << (c - kNoncharacterFirst);
    }
  }
  BraceSentinels esc;
  char16_t* slots[] = {&esc.open, &esc.close};
  const size_t slot_count = arraysize(slots);
  size_t next = 0;
  for (uint32_t bit = 0; bit < kNoncharacterCount && next < slot_count; bit++) {
    if ((used & (uint32_t{1} << bit)) == 0) {
      *slots[next++] = static_cast<char16_t>(kNoncharacterFirst + bit);
    }
  }
  if (next == slot_count) return esc;
  // The pattern uses nearly the whole noncharacter block; take any two absent
  // units instead. Only units >= 0x80 qualify: sequence expansion synthesises
  // ASCII digits, '-' and letters that the pattern itself need not contain,
  // and one of those would be un-escaped back into a brace at the end.
  //
  // That leaves a pool of exactly kCodeUnitCount - 0x80 (65408) units, which
  // is *smaller* than kMaxPatternLength (65536), so a pattern that spells out
  // every one of them does fit under the length cap and does exhaust the pool.
  // Sooner than silently rewriting one of its literals into a brace, give up.
  auto present = std::make_unique<std::bitset<kCodeUnitCount>>();
  for (const char16_t c : pattern) present->set(c);
  next = 0;
  for (size_t unit = 0x80; unit < kCodeUnitCount && next < slot_count; unit++) {
    if (!present->test(unit)) {
      *slots[next++] = static_cast<char16_t>(unit);
    }
  }
  if (next < slot_count) return std::nullopt;
  return esc;
}

bool AllDigits(PatternView s, size_t from) {
  return from < s.size() &&
         s.find_first_not_of(u"0123456789", from) == PatternView::npos;
}

bool IsSignedInteger(PatternView s) {
  return AllDigits(s, s.starts_with(u'-') ? 1 : 0);
}

// Split on the literal '..' delimiter (JS body.split(/\.\./)).
std::vector<PatternString> SplitDotDot(PatternView body) {
  std::vector<PatternString> out;
  size_t start = 0;
  while (true) {
    const size_t pos = body.find(u"..", start);
    if (pos == PatternView::npos) {
      out.emplace_back(body.substr(start));
      return out;
    }
    out.emplace_back(body.substr(start, pos - start));
    start = pos + 2;
  }
}

// /^-?\d+\.\.-?\d+(?:\.\.-?\d+)?$/
bool IsNumericSequence(PatternView body) {
  const std::vector<PatternString> n = SplitDotDot(body);
  if (n.size() != 2 && n.size() != 3) return false;
  for (const PatternString& part : n) {
    if (!IsSignedInteger(part)) return false;
  }
  return true;
}

// /^[a-zA-Z]\.\.[a-zA-Z](?:\.\.-?\d+)?$/
bool IsAlphaSequence(PatternView body) {
  const std::vector<PatternString> n = SplitDotDot(body);
  if (n.size() != 2 && n.size() != 3) return false;
  if (n[0].size() != 1 || !IsAsciiLetter(n[0][0])) return false;
  if (n[1].size() != 1 || !IsAsciiLetter(n[1][0])) return false;
  if (n.size() == 3 && !IsSignedInteger(n[2])) return false;
  return true;
}

int64_t Numeric(PatternView s) {
  if (IsSignedInteger(s)) {
    int64_t v = 0;
    const bool neg = s.starts_with(u'-');
    for (size_t i = neg ? 1 : 0; i < s.size(); i++) {
      v = v * 10 + (s[i] - u'0');
      // Sequences this large are capped by kBraceExpansionMax anyway.
      if (v > std::numeric_limits<int32_t>::max()) break;
    }
    return neg ? -v : v;
  }
  return s.empty() ? 0 : s[0];
}

// /^-?0\d/
bool IsPadded(PatternView el) {
  size_t i = el.starts_with(u'-') ? 1 : 0;
  return el.size() >= i + 2 && el[i] == u'0' && el[i + 1] >= u'0' &&
         el[i + 1] <= u'9';
}

std::vector<PatternString> Combine(const std::vector<PatternString>& acc,
                                   PatternView pre,
                                   const std::vector<PatternString>& values,
                                   bool drop_empties) {
  std::vector<PatternString> out;
  size_t length = 0;
  for (const PatternString& a : acc) {
    for (const PatternString& v : values) {
      if (out.size() >= kBraceExpansionMax) return out;
      PatternString expansion;
      expansion.reserve(a.size() + pre.size() + v.size());
      expansion.append(a).append(pre).append(v);
      if (drop_empties && expansion.empty()) continue;
      if (length + expansion.size() > kBraceExpansionMaxLength) return out;
      length += expansion.size();
      out.push_back(std::move(expansion));
    }
  }
  return out;
}

std::vector<PatternString> ExpandSequence(PatternView body,
                                          bool is_alpha_sequence) {
  const std::vector<PatternString> n = SplitDotDot(body);
  std::vector<PatternString> out;
  const int64_t x = Numeric(n[0]);
  const int64_t y = Numeric(n[1]);
  const size_t width = std::max(n[0].size(), n[1].size());
  int64_t incr = 1;
  if (n.size() == 3) {
    const int64_t raw = Numeric(n[2]);
    incr = std::max<int64_t>(raw < 0 ? -raw : raw, 1);
  }
  const bool reverse = y < x;
  if (reverse) incr = -incr;
  const bool pad =
      IsPadded(n[0]) || IsPadded(n[1]) || (n.size() == 3 && IsPadded(n[2]));
  size_t length = 0;
  for (int64_t i = x;
       (reverse ? i >= y : i <= y) && out.size() < kBraceExpansionMax;
       i += incr) {
    PatternString c;
    if (is_alpha_sequence) {
      if (i != u'\\') c.push_back(static_cast<char16_t>(i));
      // (String.fromCharCode(i) === '\\' is replaced with '')
    } else {
      const bool neg = i < 0;
      uint64_t v = neg ? static_cast<uint64_t>(-i) : static_cast<uint64_t>(i);
      PatternString digits;
      do {
        digits.insert(digits.begin(), static_cast<char16_t>(u'0' + v % 10));
        v /= 10;
      } while (v != 0);
      PatternString s;
      if (neg) s.push_back(u'-');
      s += digits;
      if (pad) {
        const size_t need = width > s.size() ? width - s.size() : 0;
        if (need > 0) {
          if (neg) {
            c.push_back(u'-');
            c.append(need, u'0');
            c.append(s, 1, PatternString::npos);
          } else {
            c.append(need, u'0');
            c += s;
          }
        } else {
          c = std::move(s);
        }
      } else {
        c = std::move(s);
      }
    }
    if (length + c.size() > kBraceExpansionMaxLength) break;
    length += c.size();
    out.push_back(std::move(c));
  }
  return out;
}

// str.split(',')
std::vector<PatternString> SplitOnCommas(PatternView str) {
  std::vector<PatternString> out;
  size_t start = 0;
  while (true) {
    const size_t pos = str.find(u',', start);
    if (pos == PatternView::npos) {
      out.emplace_back(str.substr(start));
      return out;
    }
    out.emplace_back(str.substr(start, pos - start));
    start = pos + 1;
  }
}

// Appends `next` to `out`, joining across the seam: the JS original grows one
// list by writing the head of the tail onto its own last element.
void MergeParts(std::vector<PatternString>* out,
                std::vector<PatternString>&& next) {
  if (out->empty()) {
    *out = std::move(next);
    return;
  }
  out->back() += next.front();
  out->insert(out->end(),
              std::make_move_iterator(next.begin() + 1),
              std::make_move_iterator(next.end()));
}

// Splits a brace body on its top-level commas, keeping nested `{...}` groups
// whole.
std::vector<PatternString> ParseCommaParts(PatternView str) {
  if (str.empty()) return {PatternString()};
  std::vector<PatternString> parts;
  while (true) {
    const Balanced m = BalancedBraces(str);
    if (!m.found) {
      MergeParts(&parts, SplitOnCommas(str));
      return parts;
    }
    const PatternView pre = str.substr(0, m.start);
    const PatternView body = str.substr(m.start + 1, m.end - m.start - 1);
    const PatternView post = str.substr(m.end + 1);
    // p = pre.split(','); p[last] += '{' + body + '}'
    std::vector<PatternString> p = SplitOnCommas(pre);
    p.back() += u'{';
    p.back() += body;
    p.back() += u'}';
    MergeParts(&parts, std::move(p));
    // ParseCommaParts('') is [''], which the JS original discards.
    if (post.empty()) return parts;
    str = post;
  }
}

// /,(?!,).*\}/: a comma not followed by a comma, later followed by a '}'
// with no line terminator in between, since '.' does not match newlines.
bool PostNeedsRescan(PatternView post) {
  for (size_t i = 0; i < post.size(); i++) {
    if (post[i] != u',') continue;
    if (i + 1 < post.size() && post[i + 1] == u',') continue;
    for (size_t j = i + 1; j < post.size(); j++) {
      const char16_t c = post[j];
      if (c == u'}') return true;
      if (IsLineTerminator(c)) break;
    }
  }
  return false;
}

PatternString Embrace(PatternView s) {
  PatternString out;
  out.reserve(s.size() + 2);
  out.push_back(u'{');
  out.append(s);
  out.push_back(u'}');
  return out;
}

// `depth` counts the enclosing brace groups; `*too_deep` is latched once one
// nests past kMaxNestingDepth, and the caller turns that into a compile error.
std::vector<PatternString> ExpandInner(PatternString str,
                                       bool is_top,
                                       const BraceSentinels& esc,
                                       int depth,
                                       bool* too_deep) {
  if (depth > kMaxNestingDepth) {
    *too_deep = true;
    return {std::move(str)};
  }
  std::vector<PatternString> acc{PatternString()};
  bool drop_empties = false;
  bool first_group = true;
  while (true) {
    if (*too_deep) return acc;
    const Balanced m = BalancedBraces(str);
    if (!m.found) {
      return Combine(acc, str, {PatternString()}, drop_empties);
    }
    const PatternString pre(str.substr(0, m.start));
    const PatternString body(str.substr(m.start + 1, m.end - m.start - 1));
    const PatternString post(str.substr(m.end + 1));
    if (pre.ends_with(u'$')) {
      PatternString wrapped = pre;
      wrapped += u'{';
      wrapped += body;
      wrapped += u'}';
      acc = Combine(
          acc, wrapped, {PatternString()}, drop_empties && post.empty());
      first_group = false;
      if (post.empty()) break;
      str = post;
      continue;
    }
    const bool is_numeric_sequence = IsNumericSequence(body);
    const bool is_alpha_sequence = IsAlphaSequence(body);
    const bool is_sequence = is_numeric_sequence || is_alpha_sequence;
    const bool is_options = body.find(u',') != PatternString::npos;
    if (!is_sequence && !is_options) {
      if (PostNeedsRescan(post)) {
        PatternString next = pre;
        next += u'{';
        next += body;
        next.push_back(esc.close);
        next += post;
        str = std::move(next);
        is_top = true;
        continue;
      }
      PatternString whole = pre;
      whole += u'{';
      whole += body;
      whole += u'}';
      whole += post;
      return Combine(acc, whole, {PatternString()}, drop_empties);
    }
    if (first_group) {
      drop_empties = is_top && !is_sequence;
      first_group = false;
    }
    std::vector<PatternString> values;
    if (is_sequence) {
      values = ExpandSequence(body, is_alpha_sequence);
    } else {
      std::vector<PatternString> n = ParseCommaParts(body);
      if (n.size() == 1) {
        std::vector<PatternString> inner =
            ExpandInner(n[0], false, esc, depth + 1, too_deep);
        n.clear();
        for (PatternString& e : inner) n.push_back(Embrace(e));
        if (n.size() == 1) {
          acc = Combine(
              acc, pre + n[0], {PatternString()}, drop_empties && post.empty());
          if (post.empty()) break;
          str = post;
          continue;
        }
      }
      bool drops_empties = drop_empties && post.empty() && pre.empty();
      for (size_t d = 0; drops_empties && d < acc.size(); d++) {
        if (!acc[d].empty()) drops_empties = false;
      }
      size_t values_length = 0;
      bool done = false;
      for (size_t j = 0; j < n.size() && !done; j++) {
        std::vector<PatternString> expanded =
            ExpandInner(n[j], false, esc, depth + 1, too_deep);
        for (PatternString& v : expanded) {
          if (drops_empties && v.empty()) continue;
          if (values.size() >= kBraceExpansionMax ||
              values_length + v.size() > kBraceExpansionMaxLength) {
            done = true;
            break;
          }
          values_length += v.size();
          values.push_back(std::move(v));
        }
      }
    }
    acc = Combine(acc, pre, values, drop_empties && post.empty());
    if (post.empty()) break;
    str = post;
  }
  return acc;
}

}  // namespace

std::vector<PatternString> BraceExpand(PatternView pattern,
                                       CompileError* error) {
  if (pattern.empty()) return {};
  const std::optional<BraceSentinels> chosen = ChooseSentinels(pattern);
  if (!chosen.has_value()) {
    // Only a pattern long enough to name every unit >= 0x80 gets here.
    // As mentioned earlier in this file, this should never occur unless
    // a user has created a very odd glob specifically meant to trigger
    // this codepath.
    *error = CompileError::kPatternTooLong;
    return {};
  }
  const BraceSentinels esc = *chosen;
  PatternString str(pattern);
  if (str.starts_with(u"{}")) {
    str[0] = esc.open;
    str[1] = esc.close;
  }
  bool too_deep = false;
  std::vector<PatternString> out =
      ExpandInner(std::move(str), true, esc, 0, &too_deep);
  if (too_deep) {
    *error = CompileError::kPatternTooDeep;
    return {};
  }
  for (PatternString& s : out) {
    for (char16_t& c : s) {
      if (c == esc.open)
        c = u'{';
      else if (c == esc.close)
        c = u'}';
    }
  }
  return out;
}

namespace {

// `text` is ASCII, so it compares against either subject width.
template <typename Char>
bool PartIs(const std::basic_string<Char>& p, std::string_view text) {
  return UnitsEqual(std::basic_string_view<Char>(p), text);
}

// parts.indexOf(needle, from)
template <typename Char>
ptrdiff_t IndexOfPart(const std::vector<std::basic_string<Char>>& parts,
                      std::string_view needle,
                      ptrdiff_t from) {
  const size_t start = from < 0 ? 0 : static_cast<size_t>(from);
  const size_t begin = std::min(start, parts.size());
  const auto found = std::ranges::find_if(
      parts.begin() + begin,
      parts.end(),
      [needle](const std::basic_string<Char>& p) { return PartIs(p, needle); });
  return found == parts.end() ? kNone : found - parts.begin();
}

// <pre>/<e>/<rest> -> <pre>/<rest> and the './.'-tail rule
template <typename Char>
bool SqueezeEmptiesAndDots(std::vector<std::basic_string<Char>>* parts) {
  bool did_something = false;
  for (size_t i = 1; i + 1 < parts->size(); i++) {
    const std::basic_string<Char>& p = (*parts)[i];
    if (i == 1 && p.empty() && (*parts)[0].empty()) continue;  // UNC
    if (PartIs(p, ".") || p.empty()) {
      did_something = true;
      parts->erase(parts->begin() + i);
      i--;
    }
  }
  if (parts->size() == 2 && PartIs((*parts)[0], ".") &&
      (PartIs((*parts)[1], ".") || (*parts)[1].empty())) {
    did_something = true;
    parts->pop_back();
  }
  return did_something;
}

}  // namespace

std::vector<PatternString> SlashSplit(PatternView s,
                                      const CompileFlags& flags) {
  std::vector<PatternString> out;
  SlashSplitInto(
      s, flags.windows, [&out](PatternView part) { out.emplace_back(part); });
  return out;
}

template <typename Char>
void LevelTwoFileOptimize(std::vector<std::basic_string<Char>>* parts,
                          const CompileFlags& flags) {
  bool did_something;
  do {
    did_something = SqueezeEmptiesAndDots(parts);
    // <pre>/<p>/../<rest> -> <pre>/<rest>
    ptrdiff_t dd = 0;
    while (kNone != (dd = IndexOfPart(*parts, "..", dd + 1))) {
      if (dd == 0) continue;  // parts[-1] is undefined (falsy) in the JS
      const std::basic_string<Char>& p = (*parts)[dd - 1];
      if (!p.empty() && !PartIs(p, ".") && !PartIs(p, "..") &&
          !PartIs(p, "**") &&
          !(flags.windows && IsWindowsDrive(std::basic_string_view<Char>(p)))) {
        did_something = true;
        parts->erase(parts->begin() + (dd - 1), parts->begin() + (dd + 1));
        dd -= 2;
      }
    }
  } while (did_something);
  if (parts->empty()) parts->emplace_back();
}

template void LevelTwoFileOptimize<char16_t>(
    std::vector<std::basic_string<char16_t>>*, const CompileFlags&);
template void LevelTwoFileOptimize<char8_t>(
    std::vector<std::basic_string<char8_t>>*, const CompileFlags&);

namespace {

void FirstPhasePreProcess(std::vector<std::vector<PatternString>>* glob_parts) {
  bool did_something;
  do {
    did_something = false;
    for (size_t row = 0; row < glob_parts->size(); row++) {
      // Note: the JS iterates with for-of while pushing; appended rows are
      // visited in the same pass. push_back below can reallocate the outer
      // vector, so the row is accessed through a re-seatable pointer.
      auto* parts = &(*glob_parts)[row];
      ptrdiff_t gs = kNone;
      while (kNone != (gs = IndexOfPart(*parts, "**", gs + 1))) {
        ptrdiff_t gss = gs;
        while (gss + 1 < static_cast<ptrdiff_t>(parts->size()) &&
               (*parts)[gss + 1] == u"**") {
          gss++;
        }
        if (gss > gs) {
          parts->erase(parts->begin() + gs + 1, parts->begin() + gss + 1);
        }
        if (static_cast<size_t>(gs + 1) >= parts->size() ||
            (*parts)[gs + 1] != u"..") {
          continue;
        }
        // (`!p` in the JS: a missing part and an empty part are both falsy)
        const bool p_ok = static_cast<size_t>(gs + 2) < parts->size() &&
                          !(*parts)[gs + 2].empty() &&
                          (*parts)[gs + 2] != u"." && (*parts)[gs + 2] != u"..";
        const bool p2_ok = static_cast<size_t>(gs + 3) < parts->size() &&
                           !(*parts)[gs + 3].empty() &&
                           (*parts)[gs + 3] != u"." &&
                           (*parts)[gs + 3] != u"..";
        if (!p_ok || !p2_ok) continue;
        did_something = true;
        // <pre>/**/../<p>/<p>/<rest>
        //   -> {<pre>/../<p>/<p>/<rest>, <pre>/**/<p>/<p>/<rest>}
        parts->erase(parts->begin() + gs);
        std::vector<PatternString> other = *parts;
        other[gs] = u"**";
        glob_parts->push_back(std::move(other));
        parts = &(*glob_parts)[row];
        gs--;
      }
      if (SqueezeEmptiesAndDots(parts)) did_something = true;
      // <pre>/<p>/../<rest> -> <pre>/<rest>
      ptrdiff_t dd = 0;
      while (kNone != (dd = IndexOfPart(*parts, "..", dd + 1))) {
        if (dd == 0) continue;  // parts[-1] is undefined (falsy) in the JS
        const PatternString& p = (*parts)[dd - 1];
        if (!p.empty() && p != u"." && p != u".." && p != u"**") {
          did_something = true;
          const bool need_dot = dd == 1 &&
                                static_cast<size_t>(dd + 1) < parts->size() &&
                                (*parts)[dd + 1] == u"**";
          parts->erase(parts->begin() + (dd - 1), parts->begin() + (dd + 1));
          if (need_dot) {
            parts->insert(parts->begin() + (dd - 1), PatternString(u"."));
          }
          if (parts->empty()) parts->emplace_back();
          dd -= 2;
        }
      }
    }
  } while (did_something);
}

// partsMatch(a, b, emptyGSMatch=true)
bool PartsMatch(const std::vector<PatternString>& a,
                const std::vector<PatternString>& b,
                bool dot,
                std::vector<PatternString>* result) {
  size_t ai = 0;
  size_t bi = 0;
  result->clear();
  char which = 0;
  while (ai < a.size() && bi < b.size()) {
    if (a[ai] == b[bi]) {
      result->push_back(which == 'b' ? b[bi] : a[ai]);
      ai++;
      bi++;
    } else if (a[ai] == u"**" && bi < b.size() && ai + 1 < a.size() &&
               b[bi] == a[ai + 1]) {
      result->push_back(a[ai]);
      ai++;
    } else if (b[bi] == u"**" && ai < a.size() && bi + 1 < b.size() &&
               a[ai] == b[bi + 1]) {
      result->push_back(b[bi]);
      bi++;
    } else if (a[ai] == u"*" && !b[bi].empty() && (dot || b[bi][0] != u'.') &&
               b[bi] != u"**") {
      if (which == 'b') return false;
      which = 'a';
      result->push_back(a[ai]);
      ai++;
      bi++;
    } else if (b[bi] == u"*" && !a[ai].empty() && (dot || a[ai][0] != u'.') &&
               a[ai] != u"**") {
      if (which == 'a') return false;
      which = 'b';
      result->push_back(b[bi]);
      ai++;
      bi++;
    } else {
      return false;
    }
  }
  return a.size() == b.size();
}

void MergeIntoLaterRow(std::vector<std::vector<PatternString>>* glob_parts,
                       size_t i,
                       bool dot,
                       std::vector<PatternString>* matched) {
  for (size_t j = i + 1; j < glob_parts->size(); j++) {
    if (PartsMatch((*glob_parts)[i], (*glob_parts)[j], dot, matched)) {
      (*glob_parts)[i].clear();
      (*glob_parts)[j] = *matched;
      return;
    }
  }
}

void SecondPhasePreProcess(std::vector<std::vector<PatternString>>* glob_parts,
                           const CompileFlags& flags) {
  std::vector<PatternString> matched;
  for (size_t i = 0; i + 1 < glob_parts->size(); i++) {
    MergeIntoLaterRow(glob_parts, i, flags.dot, &matched);
  }
  std::erase_if(*glob_parts, [](const std::vector<PatternString>& gs) {
    return gs.empty();
  });
}

}  // namespace

void PreprocessLevel2(std::vector<std::vector<PatternString>>* glob_parts,
                      const CompileFlags& flags) {
  FirstPhasePreProcess(glob_parts);
  SecondPhasePreProcess(glob_parts, flags);
}

namespace {

bool IsExtglobType(char16_t c) {
  return c == u'!' || c == u'?' || c == u'+' || c == u'*' || c == u'@';
}

// adoptionMap / adoptionWithSpaceMap / adoptionAnyMap
bool CanAdoptType(char parent, char child, int which) {
  auto in = [child](std::string_view s) {
    return s.find(child) != std::string_view::npos;
  };
  switch (which) {
    case 0:
      switch (parent) {
        case '!':
          return in("@");
        case '?':
          return in("?@");
        case '@':
          return in("@");
        case '*':
          return in("*+?@");
        case '+':
          return in("+@");
      }
      return false;
    case 1:
      switch (parent) {
        case '!':
          return in("?");
        case '@':
          return in("?");
        case '+':
          return in("?*");
      }
      return false;
    default:
      switch (parent) {
        case '!':
          return in("?@");
        case '?':
          return in("?@");
        case '@':
          return in("?@");
        case '*':
          return in("*+?@");
        case '+':
          return in("+@?*");
      }
      return false;
  }
}

// usurpMap
char UsurpResult(char parent, char child) {
  switch (parent) {
    case '!':
      return child == '!' ? '@' : 0;
    case '?':
      return (child == '*' || child == '+') ? '*' : 0;
    case '@':
      switch (child) {
        case '!':
          return '!';
        case '?':
          return '?';
        case '@':
          return '@';
        case '*':
          return '*';
        case '+':
          return '+';
      }
      return 0;
    case '+':
      return (child == '?' || child == '*') ? '*' : 0;
  }
  return 0;
}

struct ParseCtx {
  std::vector<std::unique_ptr<SegmentNode>> arena;
  std::vector<SegmentNode*> negs;
  bool filled_negs = false;
  bool too_deep = false;
};

SegmentNode* NewNode(char type, SegmentNode* parent, ParseCtx* ctx) {
  auto node = std::make_unique<SegmentNode>();
  node->type = type;
  node->parent = parent;
  node->parent_index = parent ? parent->parts->size() : 0;
  if (type == '!' && !ctx->filled_negs) {
    ctx->negs.push_back(node.get());
  }
  SegmentNode* raw = node.get();
  ctx->arena.push_back(std::move(node));
  return raw;
}

void PushText(SegmentNode* node, PatternString text) {
  if (text.empty()) return;  // AST.push skips ''
  node->parts->emplace_back(std::move(text));
}

void PushChild(SegmentNode* node, SegmentNode* child) {
  node->parts->emplace_back(child);
}

// AST.clone(parent) / copyIn.
SegmentNode* CloneNode(const SegmentNode* src,
                       SegmentNode* new_parent,
                       ParseCtx* ctx) {
  SegmentNode* c = NewNode(src->type, new_parent, ctx);
  c->empty_ext = src->empty_ext;
  for (const SegmentNode::Part& p : *src->parts) {
    if (p.is_child()) {
      PushChild(c, CloneNode(p.child, c, ctx));
    } else {
      PushText(c, p.text);  // copyIn goes through push(): '' skipped
    }
  }
  return c;
}

// #parseAST(str, ast, pos, opt, extDepth)
// The added argument, depth, is used to make sure we aren't too deep
size_t ParseAstInner(PatternView str,
                     SegmentNode* ast,
                     size_t pos,
                     ParseCtx* ctx,
                     int ext_depth,
                     int depth) {
  bool escaping = false;
  bool in_brace = false;
  size_t brace_start = 0;
  bool brace_neg = false;
  if (ast->type == 0) {
    size_t i = pos;
    PatternString acc;
    while (i < str.size()) {
      const char16_t c = str[i++];
      if (escaping || c == u'\\') {
        escaping = !escaping;
        acc.push_back(c);
        continue;
      }
      if (in_brace) {
        if (i == brace_start + 1) {
          if (c == u'^' || c == u'!') brace_neg = true;
        } else if (c == u']' && !(i == brace_start + 2 && brace_neg)) {
          in_brace = false;
        }
        acc.push_back(c);
        continue;
      } else if (c == u'[') {
        in_brace = true;
        brace_start = i;
        brace_neg = false;
        acc.push_back(c);
        continue;
      }
      const bool do_recurse = IsExtglobType(c) && i < str.size() &&
                              str[i] == u'(' &&
                              ext_depth <= kMaxExtglobRecursion;
      if (do_recurse) {
        if (depth >= kMaxNestingDepth) {
          ctx->too_deep = true;
          return i;
        }
        PushText(ast, std::move(acc));
        acc.clear();
        SegmentNode* ext = NewNode(static_cast<char>(c), ast, ctx);
        i = ParseAstInner(str, ext, i, ctx, ext_depth + 1, depth + 1);
        PushChild(ast, ext);
        if (ctx->too_deep) return i;
        continue;
      }
      acc.push_back(c);
    }
    PushText(ast, std::move(acc));
    return i;
  }
  // Some kind of extglob; pos is at the '('.
  size_t i = pos + 1;
  SegmentNode* part = NewNode(0, ast, ctx);
  std::vector<SegmentNode*> parts;
  PatternString acc;
  while (i < str.size()) {
    const char16_t c = str[i++];
    if (escaping || c == u'\\') {
      escaping = !escaping;
      acc.push_back(c);
      continue;
    }
    if (in_brace) {
      if (i == brace_start + 1) {
        if (c == u'^' || c == u'!') brace_neg = true;
      } else if (c == u']' && !(i == brace_start + 2 && brace_neg)) {
        in_brace = false;
      }
      acc.push_back(c);
      continue;
    } else if (c == u'[') {
      in_brace = true;
      brace_start = i;
      brace_neg = false;
      acc.push_back(c);
      continue;
    }
    const bool adoptable = CanAdoptType(ast->type, static_cast<char>(c), 2);
    const bool do_recurse = IsExtglobType(c) && i < str.size() &&
                            str[i] == u'(' &&
                            (ext_depth <= kMaxExtglobRecursion || adoptable);
    if (do_recurse) {
      if (depth >= kMaxNestingDepth) {
        ctx->too_deep = true;
        return i;
      }
      const int depth_add = adoptable ? 0 : 1;
      PushText(part, std::move(acc));
      acc.clear();
      SegmentNode* ext = NewNode(static_cast<char>(c), part, ctx);
      PushChild(part, ext);
      i = ParseAstInner(str, ext, i, ctx, ext_depth + depth_add, depth + 1);
      if (ctx->too_deep) return i;
      continue;
    }
    if (c == u'|') {
      PushText(part, std::move(acc));
      acc.clear();
      parts.push_back(part);
      part = NewNode(0, ast, ctx);
      continue;
    }
    if (c == u')') {
      if (acc.empty() && ast->parts->empty()) {
        ast->empty_ext = true;
      }
      PushText(part, std::move(acc));
      for (SegmentNode* p : parts) PushChild(ast, p);
      PushChild(ast, part);
      return i;
    }
    acc.push_back(c);
  }
  // Unfinished extglob: revert this node to plain text from the type char on.
  ast->type = 0;
  ast->parts->clear();
  ast->parts->emplace_back(PatternString(str.substr(pos - 1)));
  return i;
}

// #canAdopt / #canAdoptWithSpace
SegmentNode* AdoptableGrandchild(SegmentNode* self,
                                 const SegmentNode::Part& part,
                                 int which) {
  if (!part.is_child()) return nullptr;
  SegmentNode* child = part.child;
  if (child->type != 0 || child->parts->size() != 1 || self->type == 0) {
    return nullptr;
  }
  const SegmentNode::Part& gp = (*child->parts)[0];
  if (!gp.is_child() || gp.child->type == 0) return nullptr;
  if (!CanAdoptType(self->type, gp.child->type, which)) return nullptr;
  return gp.child;
}

void Adopt(SegmentNode* self, size_t index) {
  // this.#parts.splice(index, 1, ...gc.#parts)
  SegmentNode* gc = (*(*self->parts)[index].child->parts)[0].child;
  const std::vector<SegmentNode::Part> moved = *gc->parts;  // copy of refs
  self->parts->erase(self->parts->begin() + index);
  for (size_t k = 0; k < moved.size(); k++) {
    if (moved[k].is_child()) moved[k].child->parent = self;
    self->parts->insert(self->parts->begin() + index + k, moved[k]);
  }
}

void AdoptWithSpace(SegmentNode* self, size_t index, ParseCtx* ctx) {
  SegmentNode* gc = (*(*self->parts)[index].child->parts)[0].child;
  SegmentNode* blank = NewNode(0, gc, ctx);
  blank->parts->emplace_back(PatternString());  // direct '' part, no skip
  PushChild(gc, blank);
  Adopt(self, index);
}

bool CanUsurp(SegmentNode* self) {
  if (self->type == 0 || self->parts->size() != 1) return false;
  const SegmentNode::Part& part = (*self->parts)[0];
  if (!part.is_child()) return false;
  SegmentNode* child = part.child;
  if (child->type != 0 || child->parts->size() != 1) return false;
  const SegmentNode::Part& gp = (*child->parts)[0];
  if (!gp.is_child() || gp.child->type == 0) return false;
  return UsurpResult(self->type, gp.child->type) != 0;
}

void Usurp(SegmentNode* self) {
  SegmentNode* gc = (*(*self->parts)[0].child->parts)[0].child;
  const char nt = UsurpResult(self->type, gc->type);
  // this.#parts = gc.#parts — an ALIAS, not a move: the detached '!' husk
  // stays registered for FillNegs(), which must write through it into the
  // usurper's alternatives ('!(!(x))y' depends on this).
  self->parts = gc->parts;
  for (const auto& p : *self->parts) {
    if (p.is_child()) p.child->parent = self;
  }
  self->type = nt;
  self->empty_ext = false;
}

// Flattening repeats because one adopt or usurp can expose the next, and
// every step removes a node, so it converges quickly.
//
// Should we infinitely recurse, which should never happen, a failsafe
// of ten passes is set below
constexpr int kMaxFlattenPasses = 10;

void Flatten(SegmentNode* node, ParseCtx* ctx) {
  if (!node->is_extglob()) {
    for (const auto& p : *node->parts) {
      if (p.is_child()) Flatten(p.child, ctx);
    }
    return;
  }
  int iterations = 0;
  bool done = false;
  do {
    done = true;
    for (size_t i = 0; i < node->parts->size(); i++) {
      const SegmentNode::Part part = (*node->parts)[i];
      if (!part.is_child()) continue;
      Flatten(part.child, ctx);
      if (AdoptableGrandchild(node, part, 0) != nullptr) {
        done = false;
        Adopt(node, i);
      } else if (AdoptableGrandchild(node, part, 1) != nullptr) {
        done = false;
        AdoptWithSpace(node, i, ctx);
      } else if (node->parts->size() == 1 && CanUsurp(node)) {
        done = false;
        Usurp(node);
      }
    }
  } while (!done && ++iterations < kMaxFlattenPasses);
}

void FillNegs(ParseCtx* ctx) {
  ctx->filled_negs = true;
  while (!ctx->negs.empty()) {
    SegmentNode* n = ctx->negs.back();
    ctx->negs.pop_back();
    if (n->type != '!') continue;
    SegmentNode* p = n;
    SegmentNode* pp = p->parent;
    while (pp != nullptr) {
      for (size_t i = p->parent_index + 1;
           pp->type == 0 && i < pp->parts->size();
           i++) {
        // every part of a '!' node is an alternative sub-AST
        for (const auto& alt : *n->parts) {
          if (!alt.is_child()) continue;
          const SegmentNode::Part src = (*pp->parts)[i];
          if (src.is_child()) {
            PushChild(alt.child, CloneNode(src.child, alt.child, ctx));
          } else {
            PushText(alt.child, PatternString(src.text));
          }
        }
      }
      p = pp;
      pp = p->parent;
    }
  }
}

}  // namespace

bool SegmentNode::IsStart() const {
  if (parent == nullptr) return true;
  if (!parent->IsStart()) return false;
  if (parent_index == 0) return true;
  for (size_t i = 0; i < parent_index && i < parent->parts->size(); i++) {
    const Part& pp = (*parent->parts)[i];
    if (!(pp.is_child() && pp.child->type == '!')) return false;
  }
  return true;
}

bool SegmentNode::IsEnd() const {
  if (parent == nullptr) return true;
  if (parent->type == '!') return true;
  if (!parent->IsEnd()) return false;
  if (type == 0) return true;  // parent->IsEnd() was just checked
  return parent_index == parent->parts->size() - 1;
}

PatternString SegmentNode::ToString() const {
  PatternString out;
  if (type != 0) {
    out.push_back(static_cast<char16_t>(type));
    out.push_back(u'(');
  }
  bool first = true;
  for (const Part& p : *parts) {
    if (type != 0 && !first) out.push_back(u'|');
    first = false;
    if (p.is_child()) {
      out += p.child->ToString();
    } else {
      out += p.text;
    }
  }
  if (type != 0) out.push_back(u')');
  return out;
}

SegmentTree ParseSegmentAst(PatternView part, CompileError* error) {
  ParseCtx ctx;
  SegmentNode* root = NewNode(0, nullptr, &ctx);
  ParseAstInner(part, root, 0, &ctx, 0, 0);
  if (ctx.too_deep) {
    *error = CompileError::kPatternTooDeep;
    return SegmentTree();
  }
  Flatten(root, &ctx);
  FillNegs(&ctx);
  SegmentTree tree;
  tree.root = root;
  tree.arena = std::move(ctx.arena);
  return tree;
}

bool HasGlobMagic(PatternView s) {
  for (size_t i = 0; i < s.size(); i++) {
    const char16_t c = s[i];
    if (c == u'?' || c == u'*' || c == u'[' || c == u']') return true;
    if ((c == u'+' || c == u'@' || c == u'!') && i + 1 < s.size() &&
        s[i + 1] == u'(') {
      for (size_t j = i + 2; j < s.size(); j++) {
        const char16_t d = s[j];
        if (d == u')') return true;
        if (IsLineTerminator(d)) break;
      }
    }
  }
  return false;
}

ParseResult ParsePattern(PatternView raw, const CompileFlags& flags) {
  ParseResult result;
  if (raw.size() > kMaxPatternLength) {
    result.error = CompileError::kPatternTooLong;
    return result;
  }
  // windowsPathsNoEscape: every backslash becomes a slash, on all platforms,
  // so a pattern produced by path.join() on Windows still works.
  PatternString pattern(raw);
  for (char16_t& c : pattern) {
    if (c == u'\\') c = u'/';
  }
  if (pattern.empty()) {
    result.pattern.empty = true;
    return result;
  }
  // globSet = [...new Set(braceExpand(pattern))]
  std::vector<PatternString> glob_set = BraceExpand(pattern, &result.error);
  if (result.error != CompileError::kNone) return result;
  {
    std::vector<PatternString> deduped;
    // The views point into `deduped`, which never reallocates.
    std::unordered_set<std::u16string_view> seen;
    deduped.reserve(glob_set.size());
    for (PatternString& s : glob_set) {
      if (seen.contains(s)) continue;
      deduped.push_back(std::move(s));
      seen.insert(deduped.back());
    }
    glob_set = std::move(deduped);
  }
  std::vector<std::vector<PatternString>> glob_parts;
  glob_parts.reserve(glob_set.size());
  for (const PatternString& s : glob_set) {
    glob_parts.push_back(SlashSplit(s, flags));
  }
  PreprocessLevel2(&glob_parts, flags);

  // windowsNoMagicRoot = isWindows && nocase
  const bool windows_no_magic_root = flags.windows && flags.nocase;

  // '//server/share/' splits into ['', '', 'server', 'share'].
  constexpr size_t kUncRootParts = 4;

  auto magic_at = [](const std::vector<PatternString>& s, size_t i) {
    return i < s.size() ? HasGlobMagic(s[i]) : false;
  };

  for (const std::vector<PatternString>& s : glob_parts) {
    ParsedPattern::Row row;
    size_t literal_prefix = 0;
    if (windows_no_magic_root) {
      const bool is_unc = s.size() >= 2 && s[0].empty() && s[1].empty() &&
                          ((s.size() > 2 && s[2] == u"?") || !magic_at(s, 2)) &&
                          !magic_at(s, 3);
      const bool is_drive =
          !s.empty() && StartsWithWindowsDrive(PatternView(s[0]));
      if (is_unc) {
        literal_prefix = std::min(kUncRootParts, s.size());
      } else if (is_drive) {
        literal_prefix = 1;
      }
    }
    for (size_t i = 0; i < s.size(); i++) {
      PathPart part;
      part.source = s[i];
      if (i >= literal_prefix) {
        // Per-part pattern length assertion from parse().
        if (s[i].size() > kMaxPatternLength) {
          result.error = CompileError::kPatternTooLong;
          return result;
        }
        // Only a segment that is exactly `**` is a globstar. Elsewhere the
        // extra stars are redundant with `*`, so `a/**b` compiles as `a/*b`.
        if (s[i] == u"**") {
          part.kind = PathPart::Kind::kGlobstar;
        } else {
          part.kind = PathPart::Kind::kSegment;
          part.segment = ParseSegmentAst(s[i], &result.error);
          if (result.error != CompileError::kNone) return result;
        }
      } else {
        // Forced-literal Windows root part: matched with '===' semantics
        // (segment.root stays null).
        part.kind = PathPart::Kind::kSegment;
      }
      row.parts.push_back(std::move(part));
    }
    // The UNC '?' restore quirk: //?/c:/... keeps its '?' literal even when
    // the root was not literalized wholesale.
    if (row.parts.size() >= kUncRootParts && row.parts[0].source.empty() &&
        row.parts[1].source.empty() && row.parts[2].source == u"?" &&
        IsWindowsDrive(PatternView(row.parts[3].source))) {
      row.parts[2].segment = SegmentTree();  // literal '?'
      row.parts[2].kind = PathPart::Kind::kSegment;
    }
    result.pattern.rows.push_back(std::move(row));
  }
  return result;
}

}  // namespace node::glob

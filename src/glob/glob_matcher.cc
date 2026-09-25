#include "glob/glob_matcher.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <ranges>
#include <span>

#include "glob/glob_parser.h"
#include "glob/glob_unicode.h"
#include "util-inl.h"

namespace node::glob {

namespace {

// The set of positions in the subject a segment's program can currently be
// at. Carrying every position at once is what makes matching linear in
// (positions x nodes) instead of backtracking, so a pattern like
// `*(a|aa)*(a|aa)b` costs the same as any other.
class PosSet {
 public:
  void Init(size_t positions) {
    nwords_ = (positions + kBitsPerWord - 1) / kBitsPerWord;
    if (nwords_ > kInlineWords)
      heap_.assign(nwords_, 0);
    else
      std::fill_n(inline_, nwords_, 0);
  }
  void Set(size_t i) {
    words()[i >> kWordShift] |= uint64_t{1} << (i & kBitInWordMask);
  }
  bool Test(size_t i) const {
    return (words()[i >> kWordShift] >> (i & kBitInWordMask)) & 1;
  }
  bool Empty() const {
    return std::ranges::all_of(words(), [](uint64_t v) { return v == 0; });
  }
  template <typename F>
  void ForEach(F&& f) const {
    const std::span<const uint64_t> ws = words();
    for (size_t w = 0; w < nwords_; w++) {
      uint64_t word = ws[w];
      while (word != 0) {
        f(w * kBitsPerWord + std::countr_zero(word));
        word &= word - 1;
      }
    }
  }
  void UnionWith(const PosSet& other) {
    const std::span<uint64_t> w = words();
    const std::span<const uint64_t> o = other.words();
    for (size_t i = 0; i < nwords_; i++) w[i] |= o[i];
  }

 private:
  static constexpr size_t kBitsPerWord = std::numeric_limits<uint64_t>::digits;
  static constexpr size_t kWordShift = std::countr_zero(kBitsPerWord);
  static constexpr size_t kBitInWordMask = kBitsPerWord - 1;
  // Four words hold 256 positions, one more than the longest name the
  // common filesystems allow, so only a synthetic subject reaches the heap.
  static constexpr size_t kInlineWords = 4;

  std::span<uint64_t> words() {
    return {nwords_ <= kInlineWords ? inline_ : heap_.data(), nwords_};
  }
  std::span<const uint64_t> words() const {
    return {nwords_ <= kInlineWords ? inline_ : heap_.data(), nwords_};
  }

  size_t nwords_ = 0;
  uint64_t inline_[kInlineWords];
  std::vector<uint64_t> heap_;
};

template <typename Char>
class SegmentEvaluator {
 public:
  using View = std::basic_string_view<Char>;

  SegmentEvaluator(const SegmentProgram& p, View in)
      : p_(p), in_(in), n_(in.size()) {}

  bool Accepts() {
    if (p_.start_no_traversal && IsDotSegment()) return false;
    if (p_.start_no_dot && in_.starts_with('.')) return false;
    PosSet start;
    start.Init(n_ + 1);
    start.Set(0);
    PosSet out = Reach(p_.root, start);
    return out.Test(n_);
  }

 private:
  bool IsDotSegment() const {
    return (n_ == 1 && in_[0] == '.') ||
           (n_ == 2 && in_[0] == '.' && in_[1] == '.');
  }

  // one code point (uflag) or code unit (non-u) at pos; len ∈ {1, 2}.
  uint32_t CharAt(size_t pos, size_t* len) const {
    const uint32_t c = in_[pos];
    if constexpr (sizeof(Char) > 1) {
      if (p_.uflag && IsUnicodeLead(c) && pos + 1 < n_ &&
          IsUnicodeTrail(in_[pos + 1])) {
        *len = 2;
        return CombineSurrogates(c, in_[pos + 1]);
      }
    }
    *len = 1;
    return c;
  }

  bool UnitsEqualFolded(uint32_t a, uint32_t b) const {
    if (a == b) return true;
    if (!p_.nocase) return false;
    return Canonicalize(a, p_.uflag) == Canonicalize(b, p_.uflag);
  }

  // Match a literal run at pos; returns end position or SIZE_MAX.
  size_t TextAt(const PatternString& text, size_t pos) const {
    if (!p_.nocase) {
      if (pos + text.size() > n_) return SIZE_MAX;
      for (size_t i = 0; i < text.size(); i++) {
        if (static_cast<uint32_t>(in_[pos + i]) !=
            static_cast<uint32_t>(text[i])) {
          return SIZE_MAX;
        }
      }
      return pos + text.size();
    }
    // folded comparison, unit-wise (non-u) or cp-wise (u)
    size_t ip = pos;
    size_t tp = 0;
    while (tp < text.size()) {
      if (ip >= n_) return SIZE_MAX;
      if (p_.uflag && sizeof(Char) > 1) {
        size_t il;
        const uint32_t ic = CharAt(ip, &il);
        const char16_t tc0 = text[tp];
        uint32_t tc = tc0;
        size_t tl = 1;
        if (IsUnicodeLead(tc0) && tp + 1 < text.size() &&
            IsUnicodeTrail(text[tp + 1])) {
          tc = CombineSurrogates(tc0, text[tp + 1]);
          tl = 2;
        }
        if (!UnitsEqualFolded(ic, tc)) return SIZE_MAX;
        ip += il;
        tp += tl;
      } else {
        if (!UnitsEqualFolded(in_[ip], text[tp])) return SIZE_MAX;
        ip++;
        tp++;
      }
    }
    return ip;
  }

  // A class matches when some member is case-equivalent to the subject, and
  // only then is the polarity applied.
  bool ClassMatches(const RxNode::CharClass& k, uint32_t c) const {
    if (p_.nocase && !p_.uflag) c = Canonicalize(c, /*fold=*/false);
    if (k.has_positive && (k.positive.Contains(c) != k.negate)) return true;
    if (k.has_negative && (k.negative.Contains(c) == k.negate)) return true;
    return false;
  }

  // [^/]*? expansion from position p into `out`
  void StarChain(size_t p, bool no_empty, PosSet* out) const {
    size_t q = p;
    bool first = true;
    while (true) {
      if (!first || !no_empty) {
        if (out->Test(q)) return;  // chain from q already marked
        out->Set(q);
      }
      if (q >= n_ || in_[q] == '/') return;
      size_t len;
      CharAt(q, &len);
      q += len;
      first = false;
    }
  }

  bool GuardMatchesToEnd(const RxNode& neg, size_t p) {
    PosSet start;
    start.Init(n_ + 1);
    start.Set(p);
    for (uint32_t child : neg.children) {
      PosSet r = Reach(child, start);
      if (r.Test(n_)) return true;
    }
    return false;
  }

  // Kleene closure of `body` applied to `seed`.
  PosSet Closure(uint32_t body, const PosSet& seed) {
    PosSet out = seed;
    PosSet frontier = seed;
    while (!frontier.Empty()) {
      PosSet next = Reach(body, frontier);
      PosSet fresh;
      fresh.Init(n_ + 1);
      bool any = false;
      next.ForEach([&](size_t pos) {
        if (!out.Test(pos)) {
          fresh.Set(pos);
          out.Set(pos);
          any = true;
        }
      });
      if (!any) break;
      frontier = fresh;
    }
    return out;
  }

  PosSet Reach(uint32_t id, const PosSet& in) {
    const RxNode& node = p_.nodes[id];
    PosSet out;
    out.Init(n_ + 1);
    switch (node.kind) {
      case RxNode::Kind::kEmpty:
        return in;
      case RxNode::Kind::kAssert:
        in.ForEach([&](size_t pos) {
          if (node.assert_no_dot && pos < n_ && in_[pos] == '.') return;
          if (node.assert_no_traversal && pos == 0 && IsDotSegment()) {
            return;
          }
          out.Set(pos);
        });
        return out;
      case RxNode::Kind::kText:
        in.ForEach([&](size_t pos) {
          const size_t end = TextAt(node.text, pos);
          if (end != SIZE_MAX) out.Set(end);
        });
        return out;
      case RxNode::Kind::kAnyChar:
        in.ForEach([&](size_t pos) {
          if (pos >= n_ || in_[pos] == '/') return;
          size_t len;
          CharAt(pos, &len);
          out.Set(pos + len);
        });
        return out;
      case RxNode::Kind::kClass:
        in.ForEach([&](size_t pos) {
          if (pos >= n_) return;
          size_t len;
          const uint32_t c = CharAt(pos, &len);
          if (ClassMatches(node.klass, c)) out.Set(pos + len);
        });
        return out;
      case RxNode::Kind::kStarAny:
        in.ForEach([&](size_t pos) {
          if (node.star_no_dot && pos < n_ && in_[pos] == '.') return;
          StarChain(pos, node.star_no_empty, &out);
        });
        return out;
      case RxNode::Kind::kConcat: {
        PosSet cur = in;
        for (uint32_t child : node.children) {
          cur = Reach(child, cur);
          if (cur.Empty()) break;
        }
        return cur;
      }
      case RxNode::Kind::kAlt: {
        for (uint32_t child : node.children) {
          PosSet r = Reach(child, in);
          out.UnionWith(r);
        }
        return out;
      }
      case RxNode::Kind::kOpt: {
        out = in;
        PosSet r = Reach(node.children[0], in);
        out.UnionWith(r);
        return out;
      }
      case RxNode::Kind::kStar:
        return Closure(node.children[0], in);
      case RxNode::Kind::kPlus: {
        PosSet once = Reach(node.children[0], in);
        return Closure(node.children[0], once);
      }
      case RxNode::Kind::kTwoBody: {
        // (?:first)(?:rest)*?
        PosSet first = Reach(node.children[0], in);
        return Closure(node.children[1], first);
      }
      case RxNode::Kind::kNeg: {
        in.ForEach([&](size_t pos) {
          if (node.star_no_dot && pos < n_ && in_[pos] == '.') return;
          if (GuardMatchesToEnd(node, pos)) return;
          StarChain(pos, /*no_empty=*/false, &out);
        });
        return out;
      }
    }
    return out;
  }

  const SegmentProgram& p_;
  View in_;
  const size_t n_;
};

template <typename Char>
bool EndsWith(std::basic_string_view<Char> s, PatternView suffix) {
  return s.size() >= suffix.size() &&
         UnitsEqual(s.substr(s.size() - suffix.size()), suffix);
}

// `f.toLowerCase().endsWith(ext.toLowerCase())`
template <typename Char>
bool EndsWithLowered(std::basic_string_view<Char> f, PatternView lowered_ext) {
  const bool ascii = std::ranges::all_of(
      f, [](Char c) { return static_cast<uint32_t>(c) <= kMaxAsciiCodePoint; });
  if (ascii) {
    return f.size() >= lowered_ext.size() &&
           std::ranges::equal(lowered_ext,
                              f.substr(f.size() - lowered_ext.size()),
                              [](char16_t ext, Char c) {
                                return static_cast<uint32_t>(ext) ==
                                       AsciiLower(static_cast<uint32_t>(c));
                              });
  }
  PatternString widened(f.begin(), f.end());
  PatternString lowered;
  AppendLowered(widened, &lowered);
  return EndsWith<char16_t>(lowered, lowered_ext);
}

template <typename Char>
bool IsDots(std::basic_string_view<Char> f) {
  return (f.size() == 1 && f[0] == '.') ||
         (f.size() == 2 && f[0] == '.' && f[1] == '.');
}

template <typename Char>
bool Equals(std::basic_string_view<Char> f, PatternView text) {
  return UnitsEqual(f, text);
}

// /^text$/i without the 'u' flag: code units compared under Canonicalize
template <typename Char>
bool EqualsFolded(std::basic_string_view<Char> f, PatternView text) {
  if (f.size() != text.size()) return false;
  for (size_t i = 0; i < f.size(); i++) {
    const uint32_t a = static_cast<uint32_t>(f[i]);
    const uint32_t b = static_cast<uint32_t>(text[i]);
    if (a != b && Canonicalize(a, false) != Canonicalize(b, false)) {
      return false;
    }
  }
  return true;
}

template <typename Char>
bool TestSegmentImpl(const SegmentProgram& p, std::basic_string_view<Char> f) {
  switch (p.tier) {
    case SegmentProgram::Tier::kEmpty:
      return f.empty();
    case SegmentProgram::Tier::kLiteral:
      if (p.nocase) return EqualsFolded(f, p.literal);
      return Equals(f, p.literal);
    case SegmentProgram::Tier::kStar:
      // starTest / starTestDot
      if (f.empty()) return false;
      return p.dot ? !IsDots(f) : !f.starts_with('.');
    case SegmentProgram::Tier::kStarDotExt: {
      if (!p.dot && f.starts_with('.')) return false;
      if (p.nocase) return EndsWithLowered(f, p.literal_lower);
      return EndsWith(f, p.literal);
    }
    case SegmentProgram::Tier::kStarDotStar: {
      const bool has_dot =
          f.find(static_cast<Char>('.')) != std::basic_string_view<Char>::npos;
      if (p.dot) return !IsDots(f) && has_dot;
      return !f.starts_with('.') && has_dot;
    }
    case SegmentProgram::Tier::kDotStar:
      return !IsDots(f) && f.starts_with('.');
    case SegmentProgram::Tier::kQmarks: {
      if (f.size() != p.qmarks_len) return false;
      if (p.dot) {
        if (IsDots(f)) return false;
      } else if (f.starts_with('.')) {
        return false;
      }
      if (p.literal.empty()) return true;
      if (p.nocase) return EndsWithLowered(f, p.literal_lower);
      return EndsWith(f, p.literal);
    }
    case SegmentProgram::Tier::kRegex: {
      SegmentEvaluator<Char> ev(p, f);
      return ev.Accepts();
    }
  }
  return false;
}

template <typename Char>
bool TestPartImpl(const PartMatcher& part, std::basic_string_view<Char> f) {
  switch (part.kind) {
    case PartMatcher::Kind::kGlobstar:
      return false;  // handled structurally by the path matcher
    case PartMatcher::Kind::kLiteral:
      return Equals(f, part.literal);
    case PartMatcher::Kind::kProgram:
      return TestSegmentImpl(part.program, f);
  }
  return false;
}

}  // namespace

bool TestPart(const PartMatcher& part, PatternView f) {
  return TestPartImpl<char16_t>(part, f);
}

bool TestPart(const PartMatcher& part, OneByteView f) {
  return TestPartImpl<char8_t>(part, f);
}

namespace {

template <typename Char>
class PathMatcher {
 public:
  using View = std::basic_string_view<Char>;
  // The file's path parts as views
  using Parts = std::span<const View>;

  explicit PathMatcher(const CompiledPattern& pat) : pat_(&pat) {}

  bool Match(View path) {
    if (pat_->empty) return path.empty();
    View f = path;
    if (pat_->flags.windows &&
        path.find(static_cast<Char>('\\')) != View::npos) {
      rewritten_.assign(path.begin(), path.end());
      for (Char& c : rewritten_) {
        if (c == '\\') c = '/';
      }
      f = View(rewritten_.data(), rewritten_.size());
    }
    Parts file = SplitViews(f);

    View basename;
    if (pat_->flags.match_base) {
      for (const View& part : file | std::views::reverse) {
        if (!part.empty()) {
          basename = part;
          break;
        }
      }
    }
    if (std::ranges::any_of(file, IsDots<Char>)) file = NormalizeFile(file);
    for (const CompiledPattern::Row& row : pat_->rows) {
      if (pat_->flags.match_base && row.parts.size() == 1) {
        const Parts only{&basename, 1};
        if (MatchOne(only, row)) return true;
        continue;
      }
      if (MatchOne(file, row)) return true;
    }
    return false;
  }

 private:
  using RowParts = std::vector<PartMatcher>;
  // Path depth held without allocating. Deeper paths still work; they just
  // grow the buffer, and they are rare enough not to matter.
  static constexpr size_t kInlineParts = 32;

  // Minimatch#slashSplit
  Parts SplitViews(View f) {
    size_t count = 0;
    auto push = [&](View v) {
      if (count == parts_.capacity()) {
        parts_.SetLength(count);
        parts_.AllocateSufficientStorage(count * 2);
      }
      parts_.out()[count++] = v;
    };
    SlashSplitInto(f, pat_->flags.windows, push);
    return {parts_.out(), count};
  }

  Parts NormalizeFile(Parts file) {
    owned_.clear();
    owned_.reserve(file.size());
    for (size_t i = 0; i < file.size(); i++) {
      owned_.emplace_back(file[i].begin(), file[i].end());
    }
    LevelTwoFileOptimize(&owned_, pat_->flags);
    owned_views_.clear();
    owned_views_.reserve(owned_.size());
    for (const std::basic_string<Char>& s : owned_) {
      owned_views_.emplace_back(s);
    }
    return {owned_views_.data(), owned_views_.size()};
  }

  static bool PartIsLiteral(const PartMatcher& p, PatternView text) {
    return p.kind == PartMatcher::Kind::kLiteral &&
           PatternView(p.literal) == text;
  }

  bool MatchOne(Parts file, const CompiledPattern::Row& row) {
    size_t file_start = 0;
    size_t pattern_start = 0;
    const RowParts& pattern = row.parts;

    if (pat_->flags.windows) {
      const bool file_drive = !file.empty() && IsWindowsDrive<Char>(file[0]);
      const bool file_unc = !file_drive && file.size() > 3 && file[0].empty() &&
                            file[1].empty() && file[2].size() == 1 &&
                            file[2][0] == '?' && IsWindowsDrive<Char>(file[3]);
      const bool pattern_drive =
          !pattern.empty() && pattern[0].kind == PartMatcher::Kind::kLiteral &&
          IsWindowsDrive<char16_t>(pattern[0].literal);
      const bool pattern_unc = !pattern_drive && pattern.size() > 3 &&
                               PartIsLiteral(pattern[0], u"") &&
                               PartIsLiteral(pattern[1], u"") &&
                               PartIsLiteral(pattern[2], u"?") &&
                               pattern[3].kind == PartMatcher::Kind::kLiteral &&
                               IsWindowsDrive<char16_t>(pattern[3].literal);
      const ptrdiff_t fdi = file_unc ? 3 : file_drive ? 0 : -1;
      const ptrdiff_t pdi = pattern_unc ? 3 : pattern_drive ? 0 : -1;
      if (fdi >= 0 && pdi >= 0) {
        const View fd = file[fdi];
        const PatternString& pd = pattern[pdi].literal;
        if (fd.size() == pd.size() && AsciiLower(fd[0]) == AsciiLower(pd[0]) &&
            static_cast<uint32_t>(fd[1]) == static_cast<uint32_t>(pd[1])) {
          // minimatch rewrites its compiled drive letter to the file's and
          // matches on from there; comparing here and stepping past both is
          // the same thing without mutating the shared compiled pattern.
          pattern_start = pdi + 1;
          file_start = fdi + 1;
        }
      }
    }

    if (row.has_globstar) {
      return MatchGlobstar(file, pattern, file_start, pattern_start) ==
             Tri::kTrue;
    }
    return MatchLinear(
        file, file.size(), pattern, pattern_start, pattern.size(), file_start);
  }

  // #matchOne(file.slice(0, f_end), pattern.slice(p_begin, p_end), partial,
  //           fileIndex, 0)
  bool MatchLinear(Parts file,
                   size_t f_end,
                   const RowParts& pattern,
                   size_t p_begin,
                   size_t p_end,
                   size_t f_index) {
    size_t fi = f_index;
    size_t pi = p_begin;
    const size_t fl = f_end;
    const size_t pl = p_end;
    for (; fi < fl && pi < pl; fi++, pi++) {
      const PartMatcher& p = pattern[pi];
      if (p.kind == PartMatcher::Kind::kGlobstar) return false;
      if (fi >= file.size()) return false;  // sliced view can't exceed store
      if (!TestPartImpl(p, file[fi])) return false;
    }
    if (fi == fl && pi == pl) return true;
    if (fi == fl) return false;
    if (pi == pl) return fi == fl - 1 && fi < file.size() && file[fi].empty();
    return false;
  }

  enum class Tri { kFalse, kNull, kTrue };

  struct Section {
    size_t begin, end;  // pattern part indices
    ptrdiff_t after;    // last possible file index this section can start at
  };

  Tri MatchGlobstar(Parts file,
                    const RowParts& pattern,
                    size_t file_index,
                    size_t pattern_index) {
    // locate first (from pattern_index) and last globstar
    size_t firstgs = pattern_index;
    while (firstgs < pattern.size() &&
           pattern[firstgs].kind != PartMatcher::Kind::kGlobstar) {
      firstgs++;
    }
    size_t lastgs = pattern.size();
    for (size_t i = pattern.size(); i-- > 0;) {
      if (pattern[i].kind == PartMatcher::Kind::kGlobstar) {
        lastgs = i;
        break;
      }
    }
    // head, body, tail
    const size_t head_begin = pattern_index;
    const size_t head_end = firstgs;
    const size_t body_begin = firstgs + 1;
    // slice(firstgs + 1, lastgs) is empty when they are the same globstar
    const size_t body_end = std::max(lastgs, body_begin);
    const size_t tail_begin = lastgs + 1;
    const size_t tail_end = pattern.size();
    const size_t head_len = head_end - head_begin;
    size_t fi = file_index;
    if (head_len > 0) {
      // #matchOne(file.slice(fileIndex, fileIndex + head.length), head, ...)
      const size_t slice_end = std::min(file.size(), fi + head_len);
      if (!MatchLinear(file, slice_end, pattern, head_begin, head_end, fi)) {
        return Tri::kFalse;
      }
      fi += head_len;
    }
    const size_t tail_len = tail_end - tail_begin;
    size_t file_tail_match = 0;
    if (tail_len > 0) {
      if (tail_len + fi > file.size()) return Tri::kFalse;
      size_t tail_start = file.size() - tail_len;
      if (MatchLinear(
              file, file.size(), pattern, tail_begin, tail_end, tail_start)) {
        file_tail_match = tail_len;
      } else {
        // affordance: a/**/* matching a/b/
        if (file.empty() || !file.back().empty() ||
            fi + tail_len == file.size()) {
          return Tri::kFalse;
        }
        tail_start--;
        if (!MatchLinear(
                file, file.size(), pattern, tail_begin, tail_end, tail_start)) {
          return Tri::kFalse;
        }
        file_tail_match = tail_len + 1;
      }
    }
    const size_t body_len = body_end - body_begin;
    if (body_len == 0) {
      bool saw_some = file_tail_match != 0;
      const size_t scan_end =
          file.size() >= file_tail_match ? file.size() - file_tail_match : 0;
      for (size_t i = fi; i < scan_end; i++) {
        const View f = file[i];
        saw_some = true;
        if (IsDots(f) || (!pat_->flags.dot && f.starts_with('.'))) {
          return Tri::kFalse;
        }
      }
      return saw_some ? Tri::kTrue : Tri::kFalse;
    }
    // split body into globstar-delimited sections with their last possible
    // start positions
    std::vector<Section> sections;
    sections.push_back({body_begin, body_begin, 0});
    std::vector<size_t> non_gs_sums{0};
    size_t non_gs = 0;
    for (size_t i = body_begin; i < body_end; i++) {
      if (pattern[i].kind == PartMatcher::Kind::kGlobstar) {
        non_gs_sums.push_back(non_gs);
        sections.push_back({i + 1, i + 1, 0});
      } else {
        sections.back().end = i + 1;
        non_gs++;
      }
    }
    const ptrdiff_t file_length = static_cast<ptrdiff_t>(file.size()) -
                                  static_cast<ptrdiff_t>(file_tail_match);
    {
      size_t i = sections.size() - 1;
      for (Section& s : sections) {
        s.after = file_length - (static_cast<ptrdiff_t>(non_gs_sums[i--]) +
                                 static_cast<ptrdiff_t>(s.end - s.begin));
      }
    }
    return MatchBodySections(
        file, pattern, sections, fi, 0, 0, file_tail_match != 0);
  }

  // #matchGlobStarBodySections
  Tri MatchBodySections(Parts file,
                        const RowParts& pattern,
                        const std::vector<Section>& sections,
                        size_t file_index,
                        size_t body_index,
                        int globstar_depth,
                        bool saw_tail) {
    if (body_index >= sections.size()) {
      // just make sure that there's no bad dots
      for (size_t i = file_index; i < file.size(); i++) {
        saw_tail = true;
        const View f = file[i];
        if (IsDots(f) || (!pat_->flags.dot && f.starts_with('.'))) {
          return Tri::kFalse;
        }
      }
      return saw_tail ? Tri::kTrue : Tri::kFalse;
    }
    const Section& bs = sections[body_index];
    const size_t body_len = bs.end - bs.begin;
    size_t fi = file_index;
    while (static_cast<ptrdiff_t>(fi) <= bs.after) {
      const size_t slice_end = std::min(file.size(), fi + body_len);
      const bool m =
          MatchLinear(file, slice_end, pattern, bs.begin, bs.end, fi);
      // if the cap is exceeded: intentional false negative, for security
      if (m && globstar_depth < kMaxGlobstarRecursion) {
        const Tri sub = MatchBodySections(file,
                                          pattern,
                                          sections,
                                          fi + body_len,
                                          body_index + 1,
                                          globstar_depth + 1,
                                          saw_tail);
        if (sub != Tri::kFalse) return sub;
      }

      if (fi >= file.size()) return Tri::kFalse;
      const View f = file[fi];
      if (IsDots(f) || (!pat_->flags.dot && f.starts_with('.'))) {
        return Tri::kFalse;
      }
      fi++;
    }
    return Tri::kNull;
  }

  const CompiledPattern* pat_;
  std::basic_string<Char> rewritten_;
  MaybeStackBuffer<View, kInlineParts> parts_;
  std::vector<std::basic_string<Char>> owned_;
  std::vector<View> owned_views_;
};

}  // namespace

bool MatchPattern(const CompiledPattern& pattern, PatternView path) {
  PathMatcher<char16_t> m(pattern);
  return m.Match(path);
}

bool MatchPattern(const CompiledPattern& pattern, OneByteView path) {
  PathMatcher<char8_t> m(pattern);
  return m.Match(path);
}

}  // namespace node::glob

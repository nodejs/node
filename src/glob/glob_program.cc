#include "glob/glob_program.h"

#include <algorithm>
#include <span>

#include "glob/glob_unicode.h"

namespace node::glob {

namespace {

// The [^+@!?*[(] character class these shapes end in.
constexpr PatternView kExtChars = u"+@!?*[(";

// Serialize() writes node kinds into a UTF-16 string alongside the '(',
// ')' and '|' it uses for structure. Offsetting the kinds into the Private
// Use Area keeps the two apart, and class bounds are split into halves
// because a code point does not fit in one unit.
//
// (The "Private Use Area" is explained in glob_parser.cc's BraceSentinels)
constexpr char16_t kSerializedKindBase = 0xE000;
constexpr int kCodeUnitBits = 16;
constexpr uint32_t kCodeUnitMask = 0xFFFF;

// Index of the first unit that is not `c`, or the end.
size_t SkipRun(PatternView s, char16_t c, size_t from = 0) {
  const size_t end = s.find_first_not_of(c, from);
  return end == PatternView::npos ? s.size() : end;
}

// /^\*+$/
bool ProbeStar(PatternView s) {
  return !s.empty() && SkipRun(s, u'*') == s.size();
}

// /^\*+([^+@!?*[(]*)$/
bool ProbeStarDotExt(PatternView s, PatternString* ext) {
  const size_t i = SkipRun(s, u'*');
  if (i == 0 || s.find_first_of(kExtChars, i) != PatternView::npos) {
    return false;
  }
  ext->assign(s.substr(i));
  return true;
}

// /^\?+([^+@!?*[(]*)?$/
bool ProbeQmarks(PatternView s, PatternString* ext) {
  const size_t i = SkipRun(s, u'?');
  if (i == 0 || s.find_first_of(kExtChars, i) != PatternView::npos) {
    return false;
  }
  ext->assign(s.substr(i));
  return true;
}

// /^\*+\.\*+$/
bool ProbeStarDotStar(PatternView s) {
  const size_t dot = SkipRun(s, u'*');
  if (dot == 0 || dot >= s.size() || s[dot] != u'.') return false;
  return dot + 1 < s.size() && SkipRun(s, u'*', dot + 1) == s.size();
}

// /^\.\*+$/
bool ProbeDotStar(PatternView s) {
  return s.size() >= 2 && s[0] == u'.' && SkipRun(s, u'*', 1) == s.size();
}

// --- regexpEscape simulation (only the *first characters* of the regex
// source matter, for guard selection) -----------------------------------------

// /[-[\]{}()*+?.,\\^$|#\s]/, where \s is the whole JavaScript whitespace
// set.
bool NeedsRegExpEscape(char16_t c) {
  switch (c) {
    case u'-':
    case u'[':
    case u']':
    case u'{':
    case u'}':
    case u'(':
    case u')':
    case u'*':
    case u'+':
    case u'?':
    case u'.':
    case u',':
    case u'\\':
    case u'^':
    case u'$':
    case u'|':
    case u'#':
    case u' ':
    case u'\t':
    case u'\n':
    case u'\v':
    case u'\f':
    case u'\r':
      return true;
    default:
      return c == 0x00A0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) ||
             c == 0x202F || c == 0x205F || c == 0x3000 || c == 0xFEFF ||
             IsLineTerminator(c);
  }
}

// Characters regexpEscape would escape that are legal identity escapes
// under the 'u' flag (SyntaxCharacter or '/'); everything else escaped in a
// u-mode regular expression makes `new RegExp` throw.
bool IsULegalIdentityEscape(char16_t c) {
  switch (c) {
    case u'^':
    case u'$':
    case u'\\':
    case u'.':
    case u'*':
    case u'+':
    case u'?':
    case u'(':
    case u')':
    case u'[':
    case u']':
    case u'{':
    case u'}':
    case u'|':
    case u'/':
      return true;
    default:
      return false;
  }
}

struct ClassParse {
  bool consumed_any = false;  // consumed > 0
  size_t consumed = 0;
  bool magic = false;
  bool uflag = false;
  bool demoted = false;  // single-char class became a literal
  char16_t demoted_char = 0;
  bool poison = false;  // '$.', an atom that can never match
  bool unsupported_property = false;
  RxNode::CharClass klass;
  // First character of the regex source this class produced ('[', '(', or
  // the demoted literal's escaped/plain first char).
  char16_t src_first = u'[';
  bool src_first_escaped = false;
};

struct PosixClassDef {
  PatternView name;
  // The Unicode properties the class translates to.
  std::span<const UnicodeProperty> properties;
  // Literal members some classes carry alongside their properties.
  std::span<const CharSet::Range> extra;
  // The class needs the `u` flag (property escapes require it).
  bool uflag;
  // '[:graph:]' is emitted negated: [^\p{Z}\p{C}].
  bool negated;
};

using P = UnicodeProperty;

constexpr P kAlnum[] = {P::kLetter, P::kLetterNumber, P::kDecimalNumber};
constexpr P kAlpha[] = {P::kLetter, P::kLetterNumber};
constexpr P kBlank[] = {P::kSpaceSeparator};
constexpr P kCntrl[] = {P::kControl};
constexpr P kDigit[] = {P::kDecimalNumber};
constexpr P kGraph[] = {P::kSeparator, P::kOther};
constexpr P kLower[] = {P::kLowercase};
constexpr P kPrint[] = {P::kOther};
constexpr P kPunct[] = {P::kPunctuation};
constexpr P kSpace[] = {P::kSeparator};
constexpr P kUpper[] = {P::kUppercase};
constexpr P kWord[] = {
    P::kLetter, P::kLetterNumber, P::kDecimalNumber, P::kConnector};

constexpr CharSet::Range kAsciiRange[] = {{0x00, 0x7F}};
constexpr CharSet::Range kXdigitRanges[] = {
    {u'A', u'F'}, {u'a', u'f'}, {u'0', u'9'}};
constexpr CharSet::Range kTab[] = {{u'\t', u'\t'}};
constexpr CharSet::Range kSpaceExtras[] = {{u'\t', u'\t'},
                                           {u'\r', u'\r'},
                                           {u'\n', u'\n'},
                                           {u'\v', u'\v'},
                                           {u'\f', u'\f'}};

const PosixClassDef kPosixClasses[] = {
    {u"[:alnum:]", kAlnum, {}, true, false},
    {u"[:alpha:]", kAlpha, {}, true, false},
    {u"[:ascii:]", {}, kAsciiRange, false, false},
    {u"[:blank:]", kBlank, kTab, true, false},
    {u"[:cntrl:]", kCntrl, {}, true, false},
    {u"[:digit:]", kDigit, {}, true, false},
    {u"[:graph:]", kGraph, {}, true, true},
    {u"[:lower:]", kLower, {}, true, false},
    {u"[:print:]", kPrint, {}, true, false},
    {u"[:punct:]", kPunct, {}, true, false},
    {u"[:space:]", kSpace, kSpaceExtras, true, false},
    {u"[:upper:]", kUpper, {}, true, false},
    {u"[:word:]", kWord, {}, true, false},
    {u"[:xdigit:]", {}, kXdigitRanges, false, false},
};

ClassParse ParseClass(PatternView glob, size_t position) {
  ClassParse out;
  const size_t pos = position;
  size_t i = pos + 1;
  bool saw_start = false;
  bool escaping = false;
  bool negate = false;
  size_t end_pos = pos;
  bool have_range_start = false;
  char16_t range_start = 0;

  struct Added {  // one entry per push into `ranges`
    bool single_char;
    char16_t ch;
  };
  std::vector<Added> added;
  CharSet positive;
  CharSet negative;
  bool has_positive = false;
  bool has_negative = false;
  bool uflag = false;
  bool unsupported_property = false;

  while (i < glob.size()) {
    const char16_t c = glob[i];
    if ((c == u'!' || c == u'^') && i == pos + 1) {
      negate = true;
      i++;
      continue;
    }
    if (c == u']' && saw_start && !escaping) {
      end_pos = i + 1;
      break;
    }
    saw_start = true;
    if (c == u'\\') {
      if (!escaping) {
        escaping = true;
        i++;
        continue;
      }
    }
    if (c == u'[' && !escaping) {
      bool matched_posix = false;
      for (const PosixClassDef& def : kPosixClasses) {
        if (glob.substr(i).starts_with(def.name)) {
          if (have_range_start) {
            // [a-[:alpha:]] poisons the whole remaining glob text.
            out.consumed_any = true;
            out.consumed = glob.size() - pos;
            out.magic = true;
            out.poison = true;
            return out;
          }
          i += def.name.size();
          CharSet& side = def.negated ? negative : positive;
          (def.negated ? has_negative : has_positive) = true;
          for (UnicodeProperty property : def.properties) {
            if (!side.AddProperty(property)) unsupported_property = true;
          }
          for (const CharSet::Range& range : def.extra) {
            side.AddRange(range.lo, range.hi);
          }
          added.push_back({false, 0});
          uflag = uflag || def.uflag;
          matched_posix = true;
          break;
        }
      }
      if (matched_posix) continue;
    }
    escaping = false;
    if (have_range_start) {
      if (c > range_start) {
        positive.AddRange(range_start, c);
        has_positive = true;
        added.push_back({false, 0});
      } else if (c == range_start) {
        positive.AddChar(c);
        has_positive = true;
        added.push_back({true, c});
      }
      // (c < range_start: range dropped entirely)
      have_range_start = false;
      i++;
      continue;
    }
    // possible range start: c-] / c-x / c
    if (i + 2 < glob.size() && glob[i + 1] == u'-' && glob[i + 2] == u']') {
      // 'c-' as two literal members
      positive.AddChar(c);
      positive.AddChar(u'-');
      has_positive = true;
      added.push_back({false, 0});  // "c-" is a 2-char source piece
      i += 2;
      continue;
    }
    if (i + 1 < glob.size() && glob[i + 1] == u'-') {
      range_start = c;
      have_range_start = true;
      i += 2;
      continue;
    }
    positive.AddChar(c);
    has_positive = true;
    added.push_back({true, c});
    i++;
  }

  if (end_pos < i || end_pos == pos) {
    // didn't find the closing ']': not a class, '[' stays literal
    return out;
  }
  out.consumed_any = true;
  out.consumed = end_pos - pos;

  if (!has_positive && !has_negative) {
    // "[]" (or all-dropped): poison
    out.consumed = glob.size() - pos;
    out.magic = true;
    out.poison = true;
    return out;
  }
  // single positive plain char, not negated
  if (!has_negative && !negate && added.size() == 1 && added[0].single_char) {
    out.demoted = true;
    out.demoted_char = added[0].ch;
    out.magic = false;
    const char16_t ch = added[0].ch;
    out.src_first_escaped = NeedsRegExpEscape(ch);
    out.src_first = out.src_first_escaped ? u'\\' : ch;
    return out;
  }

  out.magic = true;
  out.uflag = uflag;
  out.unsupported_property = unsupported_property;

  positive.Seal();
  negative.Seal();
  out.klass.positive = std::move(positive);
  out.klass.negative = std::move(negative);
  out.klass.has_positive = has_positive;
  out.klass.has_negative = has_negative;
  out.klass.negate = negate;
  // comb source starts with '(' when both sides exist, else '['
  out.src_first = (has_positive && has_negative) ? u'(' : u'[';
  return out;
}

struct Lowered {
  uint32_t node = 0;
  bool source_empty = true;  // produced no regex source at all
  // First simulated source characters (up to 5), for guard decisions.
  PatternString src_prefix;

  bool bad_uescape = false;
  bool bad_syntax = false;
};

class SegmentLowering {
 public:
  SegmentLowering(const CompileFlags& flags, SegmentProgram* out)
      : flags_(flags), out_(out) {}

  uint32_t Add(RxNode&& n) {
    out_->nodes.push_back(std::move(n));
    return static_cast<uint32_t>(out_->nodes.size() - 1);
  }

  void AppendPrefix(PatternString* prefix, char16_t c) {
    if (prefix->size() < 5) prefix->push_back(c);
  }
  void AppendPrefix(PatternString* prefix, PatternView s) {
    for (char16_t c : s) {
      if (prefix->size() >= 5) break;
      prefix->push_back(c);
    }
  }

  // #parseGlob: one literal text run
  Lowered LowerText(PatternView text, bool no_empty) {
    Lowered result;
    std::vector<uint32_t> seq;
    bool in_star = false;
    PatternString pending_text;
    auto flush_text = [&]() {
      if (pending_text.empty()) return;
      RxNode n;
      n.kind = RxNode::Kind::kText;
      n.text = std::move(pending_text);
      pending_text.clear();
      seq.push_back(Add(std::move(n)));
    };
    bool escaping = false;
    const bool all_stars = ProbeStar(text);
    auto note_escaped_literal = [&result](char16_t c) {
      if (NeedsRegExpEscape(c) && !IsULegalIdentityEscape(c)) {
        result.bad_uescape = true;
      }
    };
    for (size_t i = 0; i < text.size(); i++) {
      const char16_t c = text[i];
      if (escaping) {
        escaping = false;
        AppendPrefix(
            &result.src_prefix,
            NeedsRegExpEscape(c) ? PatternString{u'\\', c} : PatternString{c});
        note_escaped_literal(c);
        pending_text.push_back(c);
        literal_value_ += c;
        continue;
      }
      if (c == u'*') {
        if (in_star) continue;
        in_star = true;
        flush_text();
        RxNode n;
        n.kind = RxNode::Kind::kStarAny;
        n.star_no_empty = no_empty && all_stars;
        seq.push_back(Add(std::move(n)));
        AppendPrefix(&result.src_prefix, u'[');  // '[^/]*?' / '[^/]+?'
        magic_ = true;
        continue;
      }
      in_star = false;
      if (c == u'\\') {
        if (i == text.size() - 1) {
          pending_text.push_back(u'\\');
          literal_value_ += u'\\';
          AppendPrefix(&result.src_prefix, PatternView(u"\\\\"));
        } else {
          escaping = true;
        }
        continue;
      }
      if (c == u'[') {
        ClassParse cp = ParseClass(text, i);
        if (cp.consumed_any) {
          i += cp.consumed - 1;
          if (cp.demoted) {
            pending_text.push_back(cp.demoted_char);
            literal_value_ += cp.demoted_char;
            note_escaped_literal(cp.demoted_char);
            if (cp.src_first_escaped) {
              AppendPrefix(&result.src_prefix, u'\\');
              AppendPrefix(&result.src_prefix, cp.demoted_char);
            } else {
              AppendPrefix(&result.src_prefix, cp.demoted_char);
            }
          } else if (cp.unsupported_property) {
            bad_property_ = true;
            flush_text();
            RxNode n;
            n.kind = RxNode::Kind::kClass;
            seq.push_back(Add(std::move(n)));
            AppendPrefix(&result.src_prefix, u'[');
            magic_ = true;
          } else if (cp.poison) {
            flush_text();
            RxNode n;
            n.kind = RxNode::Kind::kClass;  // empty class: never matches
            seq.push_back(Add(std::move(n)));
            AppendPrefix(&result.src_prefix, u'$');
            magic_ = true;
          } else {
            flush_text();
            uflag_ = uflag_ || cp.uflag;
            RxNode n;
            n.kind = RxNode::Kind::kClass;
            n.klass = std::move(cp.klass);
            seq.push_back(Add(std::move(n)));
            AppendPrefix(&result.src_prefix, cp.src_first);
            magic_ = true;
          }
          continue;
        }
        // not consumed: fall through as a literal '['
      }
      if (c == u'?') {
        flush_text();
        RxNode n;
        n.kind = RxNode::Kind::kAnyChar;
        seq.push_back(Add(std::move(n)));
        AppendPrefix(&result.src_prefix, u'[');  // '[^/]'
        magic_ = true;
        continue;
      }
      AppendPrefix(
          &result.src_prefix,
          NeedsRegExpEscape(c) ? PatternString{u'\\', c} : PatternString{c});
      note_escaped_literal(c);
      pending_text.push_back(c);
      literal_value_ += c;
    }
    flush_text();
    if (seq.empty()) {
      RxNode n;
      n.kind = RxNode::Kind::kEmpty;
      result.node = Add(std::move(n));
    } else if (seq.size() == 1) {
      result.node = seq[0];
    } else {
      RxNode n;
      n.kind = RxNode::Kind::kConcat;
      n.children = std::move(seq);
      result.node = Add(std::move(n));
    }
    // A nonempty text run always produces regex source.
    result.source_empty = text.empty();
    return result;
  }

  // toRegExpSource(allowDot)
  Lowered LowerNode(SegmentNode* node, bool allow_dot) {
    const bool dot = allow_dot || flags_.dot;
    if (!node->is_extglob()) {
      return LowerPlain(node, allow_dot, dot);
    }
    return LowerExtglob(node, allow_dot, dot);
  }

  bool magic() const { return magic_; }
  bool uflag() const { return uflag_; }
  bool bad_property() const { return bad_property_; }
  const PatternString& literal_value() const { return literal_value_; }

 private:
  Lowered LowerPlain(SegmentNode* node, bool allow_dot, bool dot) {
    const bool no_empty =
        node->IsStart() && node->IsEnd() &&
        std::ranges::all_of(*node->parts, [](const SegmentNode::Part& p) {
          return !p.is_child();
        });
    Lowered result;
    std::vector<uint32_t> seq;
    bool first = true;
    for (const SegmentNode::Part& p : *node->parts) {
      Lowered lp = p.is_child() ? LowerNode(p.child, allow_dot)
                                : LowerText(p.text, no_empty);
      if (first || result.src_prefix.size() < 5) {
        AppendPrefix(&result.src_prefix, lp.src_prefix);
      }
      first = false;
      if (!lp.source_empty) result.source_empty = false;
      result.bad_uescape = result.bad_uescape || lp.bad_uescape;
      result.bad_syntax = result.bad_syntax || lp.bad_syntax;
      seq.push_back(lp.node);
    }
    // A segment beginning with '.' is matched only by a
    // pattern segment beginning with a literal '.', and '.' and '..' are
    // never matched by a wildcard.
    bool start_no_dot = false;
    bool start_no_traversal = false;
    if (node->IsStart() && !node->parts->empty() &&
        !(*node->parts)[0].is_child()) {
      const bool dot_trav_allowed =
          node->parts->size() == 1 && !(*node->parts)[0].is_child() &&
          ((*node->parts)[0].text == u"." || (*node->parts)[0].text == u"..");
      if (!dot_trav_allowed) {
        const PatternString& src = result.src_prefix;
        auto at = [&src](size_t i) -> char16_t {
          return i < src.size() ? src[i] : 0;
        };
        const bool starts_escaped_dot = at(0) == u'\\' && at(1) == u'.';
        const bool starts_two_escaped_dots =
            starts_escaped_dot && at(2) == u'\\' && at(3) == u'.';
        auto is_aps = [](char16_t c) { return c == u'[' || c == u'.'; };
        const bool need_no_trav = (dot && is_aps(at(0))) ||
                                  (starts_escaped_dot && is_aps(at(2))) ||
                                  (starts_two_escaped_dots && is_aps(at(4)));
        const bool need_no_dot = !dot && !allow_dot && is_aps(at(0));
        start_no_dot = !need_no_trav && need_no_dot;
        start_no_traversal = need_no_trav;
      }
    }
    if (node->parent == nullptr) {
      out_->start_no_dot = start_no_dot;
      out_->start_no_traversal = start_no_traversal;
      if (seq.empty()) {
        RxNode n;
        n.kind = RxNode::Kind::kEmpty;
        result.node = Add(std::move(n));
        result.source_empty = true;
        return result;
      }
    } else if (start_no_dot || start_no_traversal) {
      RxNode g;
      g.kind = RxNode::Kind::kAssert;
      g.assert_no_dot = start_no_dot;
      g.assert_no_traversal = start_no_traversal;
      seq.insert(seq.begin(), Add(std::move(g)));
    }
    if (seq.empty()) {
      RxNode n;
      n.kind = RxNode::Kind::kEmpty;
      result.node = Add(std::move(n));
      result.source_empty = true;
    } else if (seq.size() == 1) {
      result.node = seq[0];
    } else {
      RxNode n;
      n.kind = RxNode::Kind::kConcat;
      n.children = std::move(seq);
      result.node = Add(std::move(n));
    }
    return result;
  }

  // #partsToRegExp
  std::vector<Lowered> LowerAlternatives(SegmentNode* node, bool allow_dot) {
    std::vector<Lowered> alts;
    const bool drop_empty = node->IsStart() && node->IsEnd();
    for (const SegmentNode::Part& p : *node->parts) {
      if (!p.is_child()) continue;  // extglob parts are always sub-ASTs
      Lowered l = LowerNode(p.child, allow_dot);
      if (drop_empty && l.source_empty) continue;
      alts.push_back(std::move(l));
    }
    return alts;
  }

  // Serializes a subtree for minimatch's `bodyDotAllowed === body`
  // comparison
  void Serialize(uint32_t id, PatternString* out) const {
    const RxNode& n = out_->nodes[id];
    out->push_back(
        static_cast<char16_t>(kSerializedKindBase + static_cast<int>(n.kind)));
    out->push_back(static_cast<char16_t>(
        u'0' + (n.star_no_empty ? 1 : 0) + (n.star_no_dot ? 2 : 0) +
        (n.assert_no_dot ? 4 : 0) + (n.assert_no_traversal ? 8 : 0)));
    if (n.kind == RxNode::Kind::kText) {
      out->append(n.text);
    } else if (n.kind == RxNode::Kind::kClass) {
      for (const CharSet* set : {&n.klass.positive, &n.klass.negative}) {
        for (const CharSet::Range& r : set->ranges()) {
          out->push_back(static_cast<char16_t>(r.lo & kCodeUnitMask));
          out->push_back(static_cast<char16_t>(r.lo >> kCodeUnitBits));
          out->push_back(static_cast<char16_t>(r.hi & kCodeUnitMask));
          out->push_back(static_cast<char16_t>(r.hi >> kCodeUnitBits));
        }
        out->push_back(u'|');
      }
      out->push_back(static_cast<char16_t>(u'0' + (n.klass.negate ? 1 : 0) +
                                           (n.klass.has_positive ? 2 : 0) +
                                           (n.klass.has_negative ? 4 : 0)));
    }
    out->push_back(u'(');
    for (uint32_t c : n.children) Serialize(c, out);
    out->push_back(u')');
  }

  Lowered LowerExtglob(SegmentNode* node, bool allow_dot, bool dot) {
    Lowered result;
    const bool repeated = node->type == '*' || node->type == '+';
    std::vector<Lowered> alts = LowerAlternatives(node, allow_dot);

    // minimatch's `body === ''` reversion
    if (node->IsStart() && node->IsEnd() && alts.empty() && node->type != '!') {
      // Invalid empty extglob
      const PatternString raw = node->ToString();
      const char type = node->type;
      node->type = 0;
      // A fresh array: clearing through the pointer could clear a parts
      // array another node aliases.
      node->parts = std::make_shared<SegmentNode::Parts>();
      node->parts->emplace_back(PatternString(raw));
      node->empty_ext = false;
      literal_value_ += raw;
      RxNode n;
      n.kind = RxNode::Kind::kText;
      // As raw regex source, `@(||)` is a literal '@' followed by a group
      // matching the empty string
      if (type == '@') {
        n.text = PatternString(1, u'@');
      } else {
        n.text = raw;
        result.bad_syntax = true;
      }
      result.node = Add(std::move(n));
      result.source_empty = raw.empty();
      AppendPrefix(&result.src_prefix, PatternView(raw));
      return result;
    }

    magic_ = true;  // extglobs are inherently magical
    AppendPrefix(&result.src_prefix, u'(');
    result.source_empty = false;

    if (node->type == '!' && node->empty_ext) {
      // The emptyExt flag is set by the parser whenever the last
      // alternative ended empty at the ')' (including `!(a|)` and
      // `!(a|b())` shapes)
      RxNode n;
      n.kind = RxNode::Kind::kStarAny;
      n.star_no_empty = true;
      n.star_no_dot = node->IsStart() && !dot;
      result.node = Add(std::move(n));
      return result;
    }

    auto make_alt = [&](std::vector<Lowered>& list) -> uint32_t {
      if (list.size() == 1) return list[0].node;
      RxNode n;
      if (list.empty()) {
        n.kind = RxNode::Kind::kEmpty;
      } else {
        n.kind = RxNode::Kind::kAlt;
        for (Lowered& l : list) n.children.push_back(l.node);
      }
      return Add(std::move(n));
    };

    auto merge_flags = [&result](const std::vector<Lowered>& list) {
      for (const Lowered& l : list) {
        result.bad_uescape = result.bad_uescape || l.bad_uescape;
        result.bad_syntax = result.bad_syntax || l.bad_syntax;
      }
    };

    if (node->type == '!') {
      // A negated extglob is a guard plus a `[^/]*?` consume, which is what
      // a regular expression can express. That makes it less permissive than
      // bash when something greedy follows: `!(a)*` does not match 'ab',
      // because the guard cannot take 'ab' and leave the `*` empty.
      //
      // That being said, in a future major update, we can potentially
      // change this behavior for less RegExp-compat and more Bash-compat.
      merge_flags(alts);
      RxNode n;
      n.kind = RxNode::Kind::kNeg;
      for (Lowered& l : alts) n.children.push_back(l.node);
      n.star_no_dot = node->IsStart() && !dot && !allow_dot;
      result.node = Add(std::move(n));
      return result;
    }
    merge_flags(alts);

    // bodyDotAllowed: only for repeated extglobs when dots are not already
    // allowed.
    bool has_body_dot = false;
    uint32_t body_dot_alt = 0;
    uint32_t body_alt = make_alt(alts);
    if (repeated && !allow_dot && !flags_.dot) {
      std::vector<Lowered> alts_dot = LowerAlternatives(node, true);
      uint32_t alt_dot = make_alt(alts_dot);
      PatternString a, b;
      Serialize(body_alt, &a);
      Serialize(alt_dot, &b);
      if (a != b) {
        has_body_dot = true;
        body_dot_alt = alt_dot;
        merge_flags(alts_dot);
      }
    }

    RxNode n;
    switch (node->type) {
      case '@':
        result.node = body_alt;
        return result;
      case '?':
        n.kind = RxNode::Kind::kOpt;
        n.children = {body_alt};
        result.node = Add(std::move(n));
        return result;
      case '+':
        if (has_body_dot) {
          n.kind = RxNode::Kind::kTwoBody;
          n.children = {body_alt, body_dot_alt};
        } else {
          n.kind = RxNode::Kind::kPlus;
          n.children = {body_alt};
        }
        result.node = Add(std::move(n));
        return result;
      case '*':
      default:
        if (has_body_dot) {
          RxNode two;
          two.kind = RxNode::Kind::kTwoBody;
          two.children = {body_alt, body_dot_alt};
          const uint32_t two_id = Add(std::move(two));
          n.kind = RxNode::Kind::kOpt;
          n.children = {two_id};
        } else {
          n.kind = RxNode::Kind::kStar;
          n.children = {body_alt};
        }
        result.node = Add(std::move(n));
        return result;
    }
  }

  const CompileFlags& flags_;
  SegmentProgram* out_;
  bool magic_ = false;
  bool uflag_ = false;
  bool bad_property_ = false;
  PatternString literal_value_;
};

}  // namespace

SegmentProgram LowerSegment(SegmentNode* root,
                            const CompileFlags& flags,
                            PatternView source) {
  SegmentProgram program;
  program.dot = flags.dot;

  if (source.empty()) {
    program.tier = SegmentProgram::Tier::kEmpty;
    return program;
  }

  // Shape probes. minimatch installs these in place of the regular
  // expression, so they define the segment's semantics.
  PatternString ext;
  if (ProbeStar(source)) {
    program.tier = SegmentProgram::Tier::kStar;
    return program;
  }
  if (ProbeStarDotExt(source, &ext)) {
    program.tier = SegmentProgram::Tier::kStarDotExt;
    program.nocase = flags.nocase;
    program.literal = std::move(ext);
    if (flags.nocase) AppendLowered(program.literal, &program.literal_lower);
    return program;
  }
  if (ProbeQmarks(source, &ext)) {
    program.tier = SegmentProgram::Tier::kQmarks;
    program.nocase = flags.nocase;
    program.qmarks_len = source.size();
    program.literal = std::move(ext);
    if (flags.nocase) AppendLowered(program.literal, &program.literal_lower);
    return program;
  }
  if (ProbeStarDotStar(source)) {
    program.tier = SegmentProgram::Tier::kStarDotStar;
    return program;
  }
  if (ProbeDotStar(source)) {
    program.tier = SegmentProgram::Tier::kDotStar;
    return program;
  }

  SegmentLowering lowering(flags, &program);
  Lowered l = lowering.LowerNode(root, false);
  program.root = l.node;
  program.uflag = lowering.uflag();
  if (!lowering.magic()) {
    program.tier = SegmentProgram::Tier::kLiteral;
    program.literal = lowering.literal_value();
    // minimatch keeps a non-magic segment a plain string unless nocase applies
    // to literals too, in which case it becomes /^text$/i (never 'u': only
    // classes set the flag, and those are magic).
    program.nocase = flags.nocase && !flags.nocase_magic_only;
    program.nodes.clear();
    program.start_no_dot = false;
    program.start_no_traversal = false;
    return program;
  }
  program.tier = SegmentProgram::Tier::kRegex;
  program.nocase = flags.nocase;
  if (flags.nocase) {
    for (RxNode& node : program.nodes) {
      if (node.kind != RxNode::Kind::kClass) continue;
      for (CharSet* set : {&node.klass.positive, &node.klass.negative}) {
        if (program.uflag) {
          set->CloseOverCaseFold();
        } else {
          set->CanonicalizeMembers();
        }
      }
    }
  }
  // `new RegExp` would throw on this source (e.g. an illegal identity escape
  // under the 'u' flag such as `\-` outside a class, etc)
  program.invalid_regex = (program.uflag && l.bad_uescape) || l.bad_syntax ||
                          lowering.bad_property();
  return program;
}

CompiledPattern CompileProgram(ParsedPattern&& parsed,
                               const CompileFlags& flags) {
  CompiledPattern out;
  out.flags = flags;
  out.empty = parsed.empty;
  for (ParsedPattern::Row& row : parsed.rows) {
    CompiledPattern::Row crow;
    for (PathPart& part : row.parts) {
      PartMatcher m;
      m.source = part.source;
      if (part.kind == PathPart::Kind::kGlobstar) {
        m.kind = PartMatcher::Kind::kGlobstar;
        crow.has_globstar = true;
      } else if (part.segment.root == nullptr) {
        m.kind = PartMatcher::Kind::kLiteral;
        m.literal = part.source;
      } else {
        m.kind = PartMatcher::Kind::kProgram;
        m.program = LowerSegment(part.segment.root, flags, part.source);
        if (m.program.invalid_regex) {
          out.error = CompileError::kInvalidRegExp;
          return out;
        }
        if (m.program.tier == SegmentProgram::Tier::kLiteral &&
            !m.program.nocase) {
          // Fold to a plain literal part ('===' comparison); a case-folded
          // literal stays a program so that TestPart() folds it.
          m.kind = PartMatcher::Kind::kLiteral;
          m.literal = std::move(m.program.literal);
        } else if (m.program.tier == SegmentProgram::Tier::kEmpty) {
          // minimatch's parse('') returns the string '', and the Windows
          // drive and UNC detection in matchOne depends on the part being
          // a string.
          m.kind = PartMatcher::Kind::kLiteral;
          m.literal.clear();
        }
      }
      crow.parts.push_back(std::move(m));
    }
    out.rows.push_back(std::move(crow));
  }
  return out;
}

}  // namespace node::glob

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/d8/mock-dom/css.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "absl/container/flat_hash_set.h"
#include "experimental/d8/mock-dom/isolate-data.h"
#include "include/v8-container.h"

namespace v8 {
namespace mock_dom {

namespace {

// Bumped when a stylesheet changes through the CSSOM.
uint64_t g_style_epoch = 0;

bool IsSpace(char c) { return IsHtmlSpace(c); }

std::string StripComments(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  size_t i = 0;
  while (i < text.size()) {
    if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*') {
      size_t end = text.find("*/", i + 2);
      if (end == std::string_view::npos) break;
      i = end + 2;
      out.push_back(' ');
    } else {
      out.push_back(text[i++]);
    }
  }
  return out;
}

// Returns the index of the first character in |stops| at nesting depth 0 at
// or after |from|, skipping strings and bracketed groups; npos if none.
size_t FindTopLevel(std::string_view text, size_t from, std::string_view stops,
                    bool braces_nest = false) {
  int depth = 0;
  char quote = 0;
  for (size_t i = from; i < text.size(); ++i) {
    char c = text[i];
    if (quote) {
      if (c == '\\') {
        ++i;
      } else if (c == quote) {
        quote = 0;
      }
      continue;
    }
    if (depth == 0 && stops.find(c) != std::string_view::npos) return i;
    if (c == '"' || c == '\'') {
      quote = c;
    } else if (c == '(' || c == '[' || (braces_nest && c == '{')) {
      ++depth;
    } else if ((c == ')' || c == ']' || (braces_nest && c == '}')) &&
               depth > 0) {
      --depth;
    }
  }
  return std::string_view::npos;
}

// Given the index of a '{', returns the index of the matching '}' (or the
// end of the text).
size_t MatchingBrace(std::string_view text, size_t open) {
  int depth = 0;
  char quote = 0;
  for (size_t i = open; i < text.size(); ++i) {
    char c = text[i];
    if (quote) {
      if (c == '\\') {
        ++i;
      } else if (c == quote) {
        quote = 0;
      }
    } else if (c == '"' || c == '\'') {
      quote = c;
    } else if (c == '{') {
      ++depth;
    } else if (c == '}' && --depth == 0) {
      return i;
    }
  }
  return text.size();
}

// Splits a property value on whitespace, keeping functions and strings
// together.
std::vector<std::string> Tokens(std::string_view value) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < value.size()) {
    while (i < value.size() && IsSpace(value[i])) ++i;
    if (i >= value.size()) break;
    size_t end = FindTopLevel(value, i, " \t\n\r\f");
    if (end == std::string_view::npos) end = value.size();
    out.emplace_back(value.substr(i, end - i));
    i = end;
  }
  return out;
}

// --- Lengths -----------------------------------------------------------------

class LengthParser {
 public:
  LengthParser(std::string_view text, double percent_base, double font_size,
               Viewport viewport)
      : text_(text),
        percent_base_(percent_base),
        font_size_(font_size),
        viewport_(viewport) {}

  std::optional<double> Parse() {
    double value = Sum();
    Skip();
    if (!ok_ || pos_ != text_.size() || std::isnan(value)) return std::nullopt;
    return value;
  }

 private:
  void Skip() {
    while (pos_ < text_.size() && IsSpace(text_[pos_])) ++pos_;
  }
  bool Eat(char c) {
    Skip();
    if (pos_ < text_.size() && text_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  double Sum() {
    double value = Product();
    while (ok_) {
      Skip();
      if (pos_ >= text_.size()) break;
      char op = text_[pos_];
      if (op != '+' && op != '-') break;
      ++pos_;
      double rhs = Product();
      value = op == '+' ? value + rhs : value - rhs;
    }
    return value;
  }

  double Product() {
    double value = Atom();
    while (ok_) {
      Skip();
      if (pos_ >= text_.size()) break;
      char op = text_[pos_];
      if (op != '*' && op != '/') break;
      ++pos_;
      double rhs = Atom();
      value = op == '*' ? value * rhs : value / rhs;
    }
    return value;
  }

  double Atom() {
    Skip();
    if (pos_ >= text_.size()) return Fail();
    if (Eat('(')) {
      double value = Sum();
      if (!Eat(')')) return Fail();
      return value;
    }
    char c = text_[pos_];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      size_t start = pos_;
      while (pos_ < text_.size() && text_[pos_] != '(' &&
             !IsSpace(text_[pos_]) && text_[pos_] != ')' &&
             text_[pos_] != ',') {
        ++pos_;
      }
      std::string name = AsciiLower(text_.substr(start, pos_ - start));
      if (!Eat('(')) return Fail();  // a keyword such as auto
      std::vector<double> args;
      do {
        args.push_back(Sum());
      } while (ok_ && Eat(','));
      if (!Eat(')')) return Fail();
      if (name == "calc" && args.size() == 1) return args[0];
      if (name == "min") return *std::min_element(args.begin(), args.end());
      if (name == "max") return *std::max_element(args.begin(), args.end());
      if (name == "clamp" && args.size() == 3) {
        return std::max(args[0], std::min(args[1], args[2]));
      }
      return Fail();
    }
    const char* begin = text_.data() + pos_;
    std::string number_text(text_.substr(pos_));
    char* end = nullptr;
    double number = std::strtod(number_text.c_str(), &end);
    size_t consumed = end - number_text.c_str();
    if (consumed == 0) return Fail();
    // "1e" in "1em" must not be read as an exponent.
    if (consumed > 0 &&
        (begin[consumed - 1] == 'e' || begin[consumed - 1] == 'E')) {
      --consumed;
    }
    pos_ += consumed;
    size_t unit_start = pos_;
    while (pos_ < text_.size() &&
           ((text_[pos_] >= 'a' && text_[pos_] <= 'z') ||
            (text_[pos_] >= 'A' && text_[pos_] <= 'Z') || text_[pos_] == '%')) {
      ++pos_;
    }
    std::string unit = AsciiLower(text_.substr(unit_start, pos_ - unit_start));
    double vw = viewport_.width;
    double vh = viewport_.height;
    if (unit.empty() || unit == "px") return number;
    if (unit == "%") {
      if (std::isnan(percent_base_)) return Fail();
      return number * percent_base_ / 100;
    }
    if (unit == "em") return number * font_size_;
    if (unit == "rem") return number * 16;
    if (unit == "ch" || unit == "ex") return number * font_size_ * 0.5;
    if (unit == "vw" || unit == "dvw" || unit == "svw" || unit == "lvw") {
      return number * vw / 100;
    }
    if (unit == "vh" || unit == "dvh" || unit == "svh" || unit == "lvh") {
      return number * vh / 100;
    }
    if (unit == "vmin") return number * std::min(vw, vh) / 100;
    if (unit == "vmax") return number * std::max(vw, vh) / 100;
    if (unit == "pt") return number * 96 / 72;
    if (unit == "pc") return number * 16;
    if (unit == "in") return number * 96;
    if (unit == "cm") return number * 96 / 2.54;
    if (unit == "mm") return number * 96 / 25.4;
    return Fail();
  }

  double Fail() {
    ok_ = false;
    return 0;
  }

  std::string_view text_;
  double percent_base_;
  double font_size_;
  Viewport viewport_;
  size_t pos_ = 0;
  bool ok_ = true;
};

bool LooksLikeLength(std::string_view token) {
  if (token.empty()) return false;
  char c = token[0];
  return (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' ||
         token.substr(0, 5) == "calc(" || token.substr(0, 4) == "var(";
}

// --- Declarations ------------------------------------------------------------

void Push(std::vector<CSSDeclaration>* out, std::string_view name,
          std::string_view value, bool important) {
  out->push_back({AtomicString(name), std::string(value), important});
}

void ExpandSides(std::vector<CSSDeclaration>* out, const char* const names[4],
                 const std::vector<std::string>& t, bool important) {
  if (t.empty() || t.size() > 4) return;
  const std::string& top = t[0];
  const std::string& right = t.size() > 1 ? t[1] : top;
  const std::string& bottom = t.size() > 2 ? t[2] : top;
  const std::string& left = t.size() > 3 ? t[3] : right;
  Push(out, names[0], top, important);
  Push(out, names[1], right, important);
  Push(out, names[2], bottom, important);
  Push(out, names[3], left, important);
}

std::string BorderWidthOf(const std::vector<std::string>& tokens) {
  bool has_style = false;
  for (const std::string& token : tokens) {
    if (token == "none" || token == "hidden") return "0px";
    if (token == "thin") return "1px";
    if (token == "medium") return "3px";
    if (token == "thick") return "5px";
    if (LooksLikeLength(token)) return token;
    if (token == "solid" || token == "dashed" || token == "dotted" ||
        token == "double" || token == "groove" || token == "ridge" ||
        token == "inset" || token == "outset") {
      has_style = true;
    }
  }
  return has_style ? "3px" : "";
}

void Expand(std::vector<CSSDeclaration>* out, const std::string& name,
            const std::string& value, bool important) {
  static const char* const kMargin[] = {"margin-top", "margin-right",
                                        "margin-bottom", "margin-left"};
  static const char* const kPadding[] = {"padding-top", "padding-right",
                                         "padding-bottom", "padding-left"};
  static const char* const kInset[] = {"top", "right", "bottom", "left"};
  static const char* const kBorderWidth[] = {
      "border-top-width", "border-right-width", "border-bottom-width",
      "border-left-width"};
  Push(out, name, value, important);
  std::vector<std::string> t = Tokens(value);
  if (t.empty()) return;
  if (name == "margin") {
    ExpandSides(out, kMargin, t, important);
  } else if (name == "padding") {
    ExpandSides(out, kPadding, t, important);
  } else if (name == "inset") {
    ExpandSides(out, kInset, t, important);
  } else if (name == "border-width") {
    ExpandSides(out, kBorderWidth, t, important);
  } else if (name == "border" || name == "border-style") {
    std::string width = BorderWidthOf(t);
    if (!width.empty()) {
      for (const char* side : kBorderWidth) Push(out, side, width, important);
    }
  } else if (name == "border-top" || name == "border-right" ||
             name == "border-bottom" || name == "border-left") {
    std::string width = BorderWidthOf(t);
    if (!width.empty()) Push(out, name + "-width", width, important);
  } else if (name == "margin-inline" || name == "padding-inline") {
    std::string base = name.substr(0, name.find('-'));
    Push(out, base + "-left", t[0], important);
    Push(out, base + "-right", t.size() > 1 ? t[1] : t[0], important);
  } else if (name == "margin-block" || name == "padding-block") {
    std::string base = name.substr(0, name.find('-'));
    Push(out, base + "-top", t[0], important);
    Push(out, base + "-bottom", t.size() > 1 ? t[1] : t[0], important);
  } else if (name == "margin-inline-start" || name == "padding-inline-start") {
    Push(out, name.substr(0, name.find('-')) + "-left", value, important);
  } else if (name == "margin-inline-end" || name == "padding-inline-end") {
    Push(out, name.substr(0, name.find('-')) + "-right", value, important);
  } else if (name == "margin-block-start" || name == "padding-block-start") {
    Push(out, name.substr(0, name.find('-')) + "-top", value, important);
  } else if (name == "margin-block-end" || name == "padding-block-end") {
    Push(out, name.substr(0, name.find('-')) + "-bottom", value, important);
  } else if (name == "inline-size") {
    Push(out, "width", value, important);
  } else if (name == "block-size") {
    Push(out, "height", value, important);
  } else if (name == "overflow") {
    Push(out, "overflow-x", t[0], important);
    Push(out, "overflow-y", t.size() > 1 ? t[1] : t[0], important);
  } else if (name == "gap") {
    Push(out, "row-gap", t[0], important);
    Push(out, "column-gap", t.size() > 1 ? t[1] : t[0], important);
  } else if (name == "flex-flow") {
    for (const std::string& token : t) {
      bool is_wrap = token.find("wrap") != std::string::npos;
      Push(out, is_wrap ? "flex-wrap" : "flex-direction", token, important);
    }
  } else if (name == "flex") {
    std::string grow = "1", shrink = "1", basis = "0px";
    if (value == "none") {
      grow = "0";
      shrink = "0";
      basis = "auto";
    } else if (value == "auto") {
      basis = "auto";
    } else if (value == "initial") {
      grow = "0";
      basis = "auto";
    } else {
      int numbers = 0;
      for (const std::string& token : t) {
        char* end = nullptr;
        std::strtod(token.c_str(), &end);
        bool unitless = end != token.c_str() && *end == '\0';
        if (unitless && numbers == 0) {
          grow = token;
          ++numbers;
        } else if (unitless && numbers == 1) {
          shrink = token;
          ++numbers;
        } else {
          basis = token;
        }
      }
    }
    Push(out, "flex-grow", grow, important);
    Push(out, "flex-shrink", shrink, important);
    Push(out, "flex-basis", basis, important);
  } else if (name == "font") {
    for (size_t i = 0; i < t.size(); ++i) {
      if (!LooksLikeLength(t[i])) continue;
      // The first numeric token that is not a weight is "size[/line-height]".
      char* end = nullptr;
      std::strtod(t[i].c_str(), &end);
      if (*end == '\0') continue;  // font-weight such as 400
      size_t slash = t[i].find('/');
      Push(out, "font-size", t[i].substr(0, slash), important);
      if (slash != std::string::npos) {
        Push(out, "line-height", t[i].substr(slash + 1), important);
      }
      std::string family;
      for (size_t k = i + 1; k < t.size(); ++k) {
        if (!family.empty()) family.push_back(' ');
        family += t[k];
      }
      if (!family.empty()) Push(out, "font-family", family, important);
      break;
    }
  }
}

}  // namespace

void ParseDeclarations(std::string_view text,
                       std::vector<CSSDeclaration>* out) {
  size_t i = 0;
  while (i < text.size()) {
    size_t end = FindTopLevel(text, i, ";", true);
    if (end == std::string_view::npos) end = text.size();
    std::string_view declaration = text.substr(i, end - i);
    i = end + 1;
    size_t colon = declaration.find(':');
    if (colon == std::string_view::npos) continue;
    std::string_view name = StripHtmlSpace(declaration.substr(0, colon));
    std::string_view value = StripHtmlSpace(declaration.substr(colon + 1));
    // Nested rules and other things that are not declarations.
    if (name.empty() || value.empty() ||
        name.find_first_of("{}&. \n") != std::string_view::npos) {
      continue;
    }
    bool important = false;
    size_t bang = value.rfind('!');
    if (bang != std::string_view::npos &&
        EqualsIgnoringAsciiCase(StripHtmlSpace(value.substr(bang + 1)),
                                "important")) {
      important = true;
      value = StripHtmlSpace(value.substr(0, bang));
    }
    if (name.substr(0, 2) == "--") {
      out->push_back(
          {AtomicString::FromUtf8(name), std::string(value), important});
    } else {
      Expand(out, AsciiLower(name), std::string(value), important);
    }
  }
}

std::optional<double> ParseCSSLength(const std::string& text,
                                     double percent_base, double font_size,
                                     Viewport viewport) {
  std::string_view trimmed = StripHtmlSpace(text);
  if (trimmed.empty()) return std::nullopt;
  return LengthParser(trimmed, percent_base, font_size, viewport).Parse();
}

// --- Media queries -----------------------------------------------------------

// Evaluates the width/height range and preference features that pages
// commonly query; anything else does not match.
bool EvaluateMediaQuery(std::string query, const Window* window) {
  query = AsciiLower(query);
  double width = window ? window->inner_width : 800;
  double height = window ? window->inner_height : 600;
  auto evaluate_feature = [&](std::string_view feature) -> bool {
    feature = StripHtmlSpace(feature);
    if (feature.empty() || feature == "all" || feature == "screen") return true;
    if (feature == "print" || feature == "speech") return false;
    if (feature.front() == '(' && feature.back() == ')') {
      feature = feature.substr(1, feature.size() - 2);
    }
    size_t colon = feature.find(':');
    std::string_view name = StripHtmlSpace(feature.substr(0, colon));
    std::string value(colon == std::string_view::npos
                          ? std::string_view()
                          : StripHtmlSpace(feature.substr(colon + 1)));
    double number =
        ParseCSSLength(value, 0, 16, Viewport::Of(window)).value_or(0);
    if (name == "min-width") return width >= number;
    if (name == "max-width") return width <= number;
    if (name == "min-height") return height >= number;
    if (name == "max-height") return height <= number;
    if (name == "orientation") {
      return value == (width >= height ? "landscape" : "portrait");
    }
    if (name == "prefers-color-scheme") return value == "light";
    if (name == "prefers-reduced-motion") return value == "no-preference";
    if (name == "hover" || name == "any-hover") return value == "hover";
    if (name == "pointer" || name == "any-pointer") return value == "fine";
    return false;
  };
  // "a and b, c": any comma-separated alternative with all parts true.
  size_t start = 0;
  while (start <= query.size()) {
    size_t comma = query.find(',', start);
    if (comma == std::string::npos) comma = query.size();
    std::string alternative = query.substr(start, comma - start);
    bool all = true;
    size_t i = 0;
    while (i <= alternative.size()) {
      size_t next = alternative.find(" and ", i);
      if (next == std::string::npos) next = alternative.size();
      std::string_view part(alternative.data() + i, next - i);
      part = StripHtmlSpace(part);
      if (part.substr(0, 5) == "only ") part = part.substr(5);
      bool negate = part.substr(0, 4) == "not ";
      if (negate) part = part.substr(4);
      if (evaluate_feature(part) == negate) all = false;
      i = next + 5;
    }
    if (all) return true;
    start = comma + 1;
  }
  return false;
}

// --- Stylesheets -------------------------------------------------------------

namespace {

using Simple = SelectorList::Simple;

uint32_t Specificity(const SelectorList::Complex& complex);

uint32_t MaxSpecificity(const SelectorList& list) {
  uint32_t best = 0;
  for (const SelectorList::Complex& complex : list.selectors()) {
    best = std::max(best, Specificity(complex));
  }
  return best;
}

uint32_t Specificity(const SelectorList::Complex& complex) {
  uint32_t total = 0;
  for (const SelectorList::Compound& compound : complex.compounds) {
    for (const Simple& simple : compound.simples) {
      switch (simple.kind) {
        case Simple::kId:
          total += 1 << 16;
          break;
        case Simple::kClass:
        case Simple::kAttribute:
          total += 1 << 8;
          break;
        case Simple::kTag:
          total += 1;
          break;
        case Simple::kUniversal:
          break;
        case Simple::kPseudo:
          if (simple.list &&
              simple.pseudo != SelectorList::PseudoClass::kHost) {
            total += MaxSpecificity(*simple.list);
          } else {
            total += 1 << 8;
            if (simple.list) total += MaxSpecificity(*simple.list);
          }
          break;
      }
    }
  }
  return total;
}

}  // namespace

std::shared_ptr<StyleSheetContents> StyleSheetContents::Parse(
    std::string_view text, const Window* window) {
  auto contents = std::make_shared<StyleSheetContents>();
  contents->AddRules(StripComments(text), window);
  return contents;
}

void StyleSheetContents::AddRules(std::string_view text, const Window* window) {
  size_t i = 0;
  const size_t n = text.size();
  uint32_t order = static_cast<uint32_t>(rules.size());
  while (i < n) {
    while (i < n && IsSpace(text[i])) ++i;
    if (i >= n) break;
    size_t brace = FindTopLevel(text, i, "{;");
    if (brace == std::string_view::npos) break;
    if (text[brace] == ';') {
      // @charset, @import and friends. Imported sheets are not loaded.
      i = brace + 1;
      continue;
    }
    size_t close = MatchingBrace(text, brace);
    std::string_view prelude = StripHtmlSpace(text.substr(i, brace - i));
    std::string_view body = text.substr(brace + 1, close - brace - 1);
    i = close + 1;
    if (!prelude.empty() && prelude[0] == '@') {
      size_t name_end = prelude.find_first_of(" \t\n\r\f(");
      std::string name = AsciiLower(prelude.substr(1, name_end - 1));
      std::string_view condition = name_end == std::string_view::npos
                                       ? std::string_view()
                                       : prelude.substr(name_end);
      if (name == "media") {
        if (EvaluateMediaQuery(std::string(condition), window)) {
          AddRules(body, window);
        }
      } else if (name == "layer" || name == "supports") {
        AddRules(body, window);
      }
      order = static_cast<uint32_t>(rules.size());
      continue;
    }
    std::string error;
    std::shared_ptr<SelectorList> selectors =
        SelectorList::Parse(prelude, &error);
    // As in CSS, a rule with a selector that does not parse is dropped.
    if (!selectors) continue;
    auto rule = std::make_unique<StyleRule>();
    rule->selectors = selectors;
    ParseDeclarations(body, &rule->declarations);
    if (rule->declarations.empty()) continue;
    const StyleRule* rule_ptr = rule.get();
    rules.push_back(std::move(rule));
    ++order;
    const auto& complexes = selectors->selectors();
    for (uint32_t c = 0; c < complexes.size(); ++c) {
      Entry entry{rule_ptr, c, Specificity(complexes[c]), order};
      const SelectorList::Compound& last = complexes[c].compounds.back();
      const Simple* id = nullptr;
      const Simple* cls = nullptr;
      const Simple* tag = nullptr;
      bool is_host = false;
      for (const Simple& simple : last.simples) {
        if (simple.kind == Simple::kId) id = &simple;
        if (simple.kind == Simple::kClass && !cls) cls = &simple;
        if (simple.kind == Simple::kTag) tag = &simple;
        if (simple.kind == Simple::kPseudo &&
            simple.pseudo == SelectorList::PseudoClass::kHost) {
          is_host = true;
        }
      }
      if (is_host) {
        host.push_back(entry);
      } else if (id) {
        by_id[id->name].push_back(entry);
      } else if (cls) {
        by_class[cls->name].push_back(entry);
      } else if (tag) {
        by_tag[tag->lower_name].push_back(entry);
        if (tag->name != tag->lower_name) by_tag[tag->name].push_back(entry);
      } else {
        universal.push_back(entry);
      }
    }
  }
}

void CSSStyleSheet::DidChange() {
  contents_.reset();
  ++g_style_epoch;
}

void CSSStyleSheet::SetText(std::string_view raw) {
  rule_texts_.clear();
  std::string text = StripComments(raw);
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && IsSpace(text[i])) ++i;
    if (i >= text.size()) break;
    size_t brace = FindTopLevel(text, i, "{;");
    if (brace == std::string::npos) break;
    size_t end = text[brace] == ';' ? brace : MatchingBrace(text, brace);
    end = std::min(end + 1, text.size());
    rule_texts_.emplace_back(text.substr(i, end - i));
    i = end;
  }
  DidChange();
}

bool CSSStyleSheet::InsertRule(const std::string& rule, size_t index) {
  if (index > rule_texts_.size()) return false;
  rule_texts_.insert(rule_texts_.begin() + index, rule);
  DidChange();
  return true;
}

bool CSSStyleSheet::DeleteRule(size_t index) {
  if (index >= rule_texts_.size()) return false;
  rule_texts_.erase(rule_texts_.begin() + index);
  DidChange();
  return true;
}

std::shared_ptr<StyleSheetContents> CSSStyleSheet::Contents(
    const Window* window) {
  if (!contents_) {
    std::string text;
    for (const std::string& rule : rule_texts_) {
      text += rule;
      text.push_back('\n');
    }
    contents_ = StyleSheetContents::Parse(text, window);
  }
  return contents_;
}

CSSStyleSheet* SheetOf(Isolate* isolate, Element* element) {
  static const AtomicString* rel = new AtomicString("rel");
  String source;
  if (element->HasTag("style")) {
    source = element->TextContent();
  } else if (element->HasTag("link")) {
    const AtomicString& rel_value = element->GetAttribute(*rel);
    ElementRareData* rare = element->rare_data();
    if (rel_value.is_null() || !rare || rare->link_sheet_text.is_null() ||
        AsciiLower(rel_value.ToUtf8()).find("stylesheet") ==
            std::string::npos) {
      return nullptr;
    }
    source = rare->link_sheet_text;
  } else {
    return nullptr;
  }
  ElementRareData* rare = element->EnsureRareData(isolate);
  CSSStyleSheet* sheet = static_cast<CSSStyleSheet*>(rare->sheet.Get());
  if (!sheet) {
    sheet = MakeGarbageCollected<CSSStyleSheet>(isolate, element);
    rare->sheet = sheet;
  } else if (rare->sheet_source.impl() == source.impl()) {
    return sheet;
  }
  // The element's text changed (which already invalidated style), so this
  // does not need to count as another change.
  uint64_t epoch = g_style_epoch;
  sheet->SetText(source.ToUtf8());
  g_style_epoch = epoch;
  rare->sheet_source = source;
  return sheet;
}

uint64_t StyleKey(const Document* document) {
  return document->layout_key() + g_style_epoch;
}

// --- The cascade -------------------------------------------------------------

namespace {

const char kUserAgentSheet[] = R"css(
html, body, div, p, h1, h2, h3, h4, h5, h6, ul, ol, menu, dl, dt, dd, pre,
address, article, aside, footer, header, hgroup, main, nav, section, form,
fieldset, figcaption, figure, details, summary, center, search, optgroup,
legend, blockquote, hr, caption, dialog, option, li { display: block }
table { display: table }
thead, tbody, tfoot { display: table-row-group }
tr { display: table-row }
td, th { display: table-cell; padding: 1px }
head, script, style, template, title, meta, link, noscript, datalist, base,
[hidden], input[type=hidden], dialog:not([open]) { display: none }
slot { display: contents }
button, input, select, textarea, img, canvas, iframe, video, meter, progress
  { display: inline-block }
body { margin: 8px }
p, dl { margin-top: 1em; margin-bottom: 1em }
h1 { font-size: 2em; margin-top: 0.67em; margin-bottom: 0.67em }
h2 { font-size: 1.5em; margin-top: 0.83em; margin-bottom: 0.83em }
h3 { font-size: 1.17em; margin-top: 1em; margin-bottom: 1em }
h4 { margin-top: 1.33em; margin-bottom: 1.33em }
h5 { font-size: 0.83em; margin-top: 1.67em; margin-bottom: 1.67em }
h6 { font-size: 0.67em; margin-top: 2.33em; margin-bottom: 2.33em }
ul, ol, menu { margin-top: 1em; margin-bottom: 1em; padding-left: 40px }
blockquote, figure { margin: 1em 40px }
dd { margin-left: 40px }
pre { white-space: pre; margin: 1em 0; font-family: monospace; font-size: 13px }
code, kbd, samp, tt { font-family: monospace; font-size: 13px }
hr { border-width: 1px; margin-top: 0.5em; margin-bottom: 0.5em }
fieldset { margin: 0 2px; padding: 0.35em 0.75em 0.625em; border-width: 2px }
input, textarea, select, button { font-size: 13.3333px }
input { width: 147px; height: 15px; padding: 1px 2px; border-width: 2px }
input[type=checkbox], input[type=radio] { width: 13px; height: 13px;
  margin: 3px 3px 3px 4px; padding: 0; border-width: 0 }
input[type=button], input[type=submit], input[type=reset] { width: auto;
  height: auto; padding: 1px 6px }
button { padding: 1px 6px; border-width: 2px }
textarea { width: 157px; height: 30px; padding: 2px; border-width: 1px;
  font-family: monospace; white-space: pre-wrap }
select { height: 19px; border-width: 1px }
iframe { border-width: 2px }
)css";

const StyleSheetContents& UserAgentSheet() {
  static std::shared_ptr<StyleSheetContents>* sheet =
      new std::shared_ptr<StyleSheetContents>(
          StyleSheetContents::Parse(kUserAgentSheet, nullptr));
  return **sheet;
}

using SheetList = std::vector<std::shared_ptr<StyleSheetContents>>;

struct DocumentStyleData {
  uint64_t epoch = ~uint64_t{0};
  bool all_dirty = false;
  // Roots of subtrees whose styles must be dropped before the next lookup.
  // Holding them strongly keeps removed nodes from being collected (and
  // their addresses reused as map keys) before that happens.
  std::vector<cppgc::Persistent<Node>> dirty_roots;
  absl::flat_hash_set<const void*> dirty_set;
  // Keys are nodes that were connected when the entry was made. Entries of
  // nodes that get removed are dropped through |dirty_roots|.
  absl::flat_hash_map<const void*, std::unique_ptr<ComputedStyle>> styles;
  absl::flat_hash_map<const void*, SheetList> scopes;
};

bool IsInheritedName(std::string_view name) {
  if (name.substr(0, 2) == "--") return true;
  if (name.substr(0, 5) == "font-" || name == "font") return true;
  if (name.substr(0, 5) == "text-") {
    return name != "text-decoration" && name != "text-overflow";
  }
  if (name.substr(0, 10) == "list-style") return true;
  if (name.substr(0, 6) == "stroke" || name.substr(0, 4) == "fill") return true;
  static const char* const kInherited[] = {
      "color",          "line-height",  "letter-spacing",  "word-spacing",
      "white-space",    "visibility",   "cursor",          "direction",
      "pointer-events", "tab-size",     "word-break",      "overflow-wrap",
      "word-wrap",      "writing-mode", "border-collapse", "quotes",
      "hyphens",        "caret-color"};
  for (const char* inherited : kInherited) {
    if (name == inherited) return true;
  }
  return false;
}

bool IsCustomProperty(const AtomicString& name) {
  return name.is_8bit() && name.view8().substr(0, 2) == "--";
}

// Whether a property is inherited, cached per property name.
bool IsInherited(const AtomicString& name) {
  static auto* cache =
      new absl::flat_hash_map<AtomicString, bool, AtomicString::Hash>();
  auto it = cache->find(name);
  if (it != cache->end()) return it->second;
  bool inherited = IsInheritedName(name.ToUtf8());
  cache->emplace(name, inherited);
  return inherited;
}

struct Match {
  uint64_t key;
  const std::vector<CSSDeclaration>* declarations;
};

void CollectFromBucket(const StyleSheetContents::Bucket& bucket,
                       Element* element, const Node* scope, uint64_t tier,
                       uint64_t sheet_index, std::vector<Match>* out) {
  for (const StyleSheetContents::Entry& entry : bucket) {
    if (!entry.rule->selectors->MatchesComplex(entry.complex, element, scope)) {
      continue;
    }
    uint64_t key = (tier << 60) |
                   (static_cast<uint64_t>(entry.specificity & 0xFFFFFF) << 36) |
                   (sheet_index << 24) | (entry.order & 0xFFFFFF);
    out->push_back({key, &entry.rule->declarations});
  }
}

void CollectFromSheet(const StyleSheetContents& sheet, Element* element,
                      const Node* scope, uint64_t tier, uint64_t sheet_index,
                      bool host_rules, std::vector<Match>* out) {
  if (host_rules) {
    CollectFromBucket(sheet.host, element, scope, tier, sheet_index, out);
    return;
  }
  if (!element->id().is_null() && !sheet.by_id.empty()) {
    auto it = sheet.by_id.find(element->id());
    if (it != sheet.by_id.end()) {
      CollectFromBucket(it->second, element, scope, tier, sheet_index, out);
    }
  }
  if (!sheet.by_class.empty()) {
    for (const AtomicString& name : element->class_names()) {
      auto it = sheet.by_class.find(name);
      if (it != sheet.by_class.end()) {
        CollectFromBucket(it->second, element, scope, tier, sheet_index, out);
      }
    }
  }
  auto it = sheet.by_tag.find(element->local_name());
  if (it != sheet.by_tag.end()) {
    CollectFromBucket(it->second, element, scope, tier, sheet_index, out);
  }
  CollectFromBucket(sheet.universal, element, scope, tier, sheet_index, out);
}

// The stylesheets of one tree scope (a document or a shadow root): its
// <style> and <link> elements in tree order, then its adopted sheets.
const SheetList& SheetsFor(DocumentStyleData* data, Node* scope,
                           Window* window) {
  auto it = data->scopes.find(scope);
  if (it != data->scopes.end()) return it->second;
  static const AtomicString* media = new AtomicString("media");
  Isolate* isolate = IsolateData::Get()->isolate;
  SheetList sheets;
  for (Node* n = scope->first_child(); n; n = n->NextInPreOrder(scope)) {
    if (!n->IsElement()) continue;
    Element* element = static_cast<Element*>(n);
    if (!element->HasTag("style") && !element->HasTag("link")) continue;
    CSSStyleSheet* sheet = SheetOf(isolate, element);
    if (!sheet) continue;
    const AtomicString& query = element->GetAttribute(*media);
    if (!query.is_null() && !query.empty() &&
        !EvaluateMediaQuery(query.ToUtf8(), window)) {
      continue;
    }
    sheets.push_back(sheet->Contents(window));
  }
  AdoptedStyleSheets* adopted =
      scope->IsShadowRoot()
          ? static_cast<ShadowRoot*>(scope)->adopted_style_sheets.Get()
          : static_cast<Document*>(scope)->adopted_style_sheets.Get();
  if (adopted) {
    for (const Member<CSSStyleSheet>& sheet : adopted->sheets()) {
      if (sheet) sheets.push_back(sheet->Contents(window));
    }
  }
  return data->scopes.emplace(scope, std::move(sheets)).first->second;
}

// Substitutes var() references.
std::string ResolveVars(const ComputedStyle& style, const std::string& value,
                        int depth) {
  size_t start = value.find("var(");
  if (start == std::string::npos || depth > 8) return value;
  size_t close = FindTopLevel(value, start + 4, ")");
  if (close == std::string::npos) return value;
  std::string_view inside(value.data() + start + 4, close - start - 4);
  size_t comma = FindTopLevel(inside, 0, ",");
  std::string_view name = StripHtmlSpace(inside.substr(0, comma));
  std::string replacement;
  if (const std::string* found = style.Get(name)) {
    replacement = ResolveVars(style, *found, depth + 1);
  } else if (comma != std::string_view::npos) {
    replacement = ResolveVars(
        style, std::string(StripHtmlSpace(inside.substr(comma + 1))),
        depth + 1);
  }
  return ResolveVars(
      style, value.substr(0, start) + replacement + value.substr(close + 1),
      depth + 1);
}

double FontSizeKeyword(const std::string& value, double parent) {
  static const struct {
    const char* name;
    double size;
  } kSizes[] = {{"xx-small", 9},  {"x-small", 10},  {"small", 13},
                {"medium", 16},   {"large", 18},    {"x-large", 24},
                {"xx-large", 32}, {"xxx-large", 48}};
  for (const auto& size : kSizes) {
    if (value == size.name) return size.size;
  }
  if (value == "smaller") return parent / 1.2;
  if (value == "larger") return parent * 1.2;
  return parent;
}

}  // namespace

class StyleEngine {
 public:
  static const ComputedStyle* Compute(Element* element,
                                      DocumentStyleData* data) {
    auto it = data->styles.find(element);
    if (it != data->styles.end()) return it->second.get();

    Document* document = element->document();
    Window* window = document->window();
    Element* parent = element->parent_element();
    if (!parent && element->parent() && element->parent()->IsShadowRoot()) {
      parent = static_cast<ShadowRoot*>(element->parent())->host();
    }
    const ComputedStyle* parent_style =
        parent ? Compute(parent, data) : nullptr;

    auto owned = std::make_unique<ComputedStyle>();
    ComputedStyle* style = owned.get();
    style->parent_ = parent_style;
    style->viewport_ = Viewport::Of(window);

    // Tiers, lowest priority first: user agent, :host rules from the
    // element's own shadow tree, rules of the element's tree scope, inline.
    std::vector<Match> matches;
    CollectFromSheet(UserAgentSheet(), element, nullptr, 0, 0, false, &matches);
    if (ShadowRoot* shadow = element->shadow_root()) {
      const SheetList& sheets = SheetsFor(data, shadow, window);
      for (size_t i = 0; i < sheets.size(); ++i) {
        CollectFromSheet(*sheets[i], element, shadow, 1, i, true, &matches);
      }
    }
    ShadowRoot* containing = element->ContainingShadowRoot();
    Node* scope = containing ? static_cast<Node*>(containing) : document;
    const SheetList& sheets = SheetsFor(data, scope, window);
    for (size_t i = 0; i < sheets.size(); ++i) {
      CollectFromSheet(*sheets[i], element, containing, 2, i, false, &matches);
    }
    static const AtomicString* style_atom = new AtomicString("style");
    const AtomicString& inline_style = element->GetAttribute(*style_atom);
    if (!inline_style.is_null() && !inline_style.empty()) {
      ParseDeclarations(inline_style.ToUtf8(), &style->inline_declarations_);
      matches.push_back({uint64_t{3} << 60, &style->inline_declarations_});
    }
    std::stable_sort(
        matches.begin(), matches.end(),
        [](const Match& a, const Match& b) { return a.key < b.key; });
    for (bool important : {false, true}) {
      for (const Match& match : matches) {
        for (const CSSDeclaration& declaration : *match.declarations) {
          if (declaration.important == important) {
            style->values_[declaration.name] = &declaration.value;
          }
        }
      }
    }

    if (parent_style) {
      style->inherited_ = parent_style->inherited_;
      style->custom_parent_ = parent_style->custom_.empty()
                                  ? parent_style->custom_parent_
                                  : parent_style;
    }

    // Explicit defaulting keywords, then var().
    for (auto entry = style->values_.begin(); entry != style->values_.end();) {
      const std::string& value = *entry->second;
      if (value == "inherit" || value == "initial" || value == "unset" ||
          value == "revert") {
        AtomicString name = entry->first;
        const std::string* inherited = value == "inherit" && parent_style
                                           ? parent_style->Get(name)
                                           : nullptr;
        if (inherited && !IsInherited(name)) {
          entry->second = inherited;
          ++entry;
        } else {
          style->values_.erase(entry++);
        }
        continue;
      }
      ++entry;
    }
    for (auto& entry : style->values_) {
      if (entry.second->find("var(") != std::string::npos) {
        style->owned_values_.push_back(std::make_unique<std::string>(
            ResolveVars(*style, *entry.second, 0)));
        entry.second = style->owned_values_.back().get();
      }
    }

    // Move inherited properties into the map shared down the tree.
    std::shared_ptr<ComputedStyle::ValueMap> own_inherited;
    for (auto entry = style->values_.begin(); entry != style->values_.end();) {
      // font-size stays behind: it inherits as a computed number, not as
      // the declared text (which may be relative to the parent).
      static const AtomicString* font_size = new AtomicString("font-size");
      if (!IsInherited(entry->first) || entry->first == *font_size) {
        ++entry;
        continue;
      }
      if (IsCustomProperty(entry->first)) {
        style->custom_[entry->first] = entry->second;
        style->values_.erase(entry++);
        continue;
      }
      if (!own_inherited) {
        own_inherited =
            style->inherited_
                ? std::make_shared<ComputedStyle::ValueMap>(*style->inherited_)
                : std::make_shared<ComputedStyle::ValueMap>();
      }
      (*own_inherited)[entry->first] = entry->second;
      style->values_.erase(entry++);
    }
    if (own_inherited) style->inherited_ = own_inherited;

    ComputeDerived(style, element);
    return data->styles.emplace(element, std::move(owned)).first->second.get();
  }

 private:
  static const std::string* Own(const ComputedStyle* style, const char* name) {
    auto it = style->values_.find(AtomicString(name));
    return it == style->values_.end() ? nullptr : it->second;
  }

  static void ParseLengths(ComputedStyle* style) {
    static const AtomicString* const kNames[] = {
#define V(Name, css) new AtomicString(css),
        MOCK_DOM_LENGTH_PROPERTIES(V)
#undef V
    };
    for (int i = 0; i < static_cast<int>(LengthId::kCount); ++i) {
      auto it = style->values_.find(*kNames[i]);
      if (it == style->values_.end()) continue;
      const std::string& value = *it->second;
      ComputedStyle::ParsedLength& parsed = style->lengths_[i];
      if (value.find('%') != std::string::npos) {
        char* end = nullptr;
        double number = std::strtod(value.c_str(), &end);
        if (end != value.c_str() && end[0] == '%' && end[1] == '\0') {
          parsed.kind = ComputedStyle::ParsedLength::kPercent;
          parsed.value = number;
        } else {
          parsed.kind = ComputedStyle::ParsedLength::kUnparsed;
        }
      } else if (auto length = ParseCSSLength(value, 0, style->font_size,
                                              style->viewport_)) {
        parsed.kind = ComputedStyle::ParsedLength::kPixels;
        parsed.value = *length;
      }
    }
  }

  static void ComputeDerived(ComputedStyle* style, Element* element) {
    const ComputedStyle* parent = style->parent_;
    double parent_font_size = parent ? parent->font_size : 16;
    style->font_size = parent_font_size;
    if (const std::string* value = Own(style, "font-size")) {
      std::optional<double> size = ParseCSSLength(
          *value, parent_font_size, parent_font_size, style->viewport_);
      style->font_size =
          size ? *size : FontSizeKeyword(*value, parent_font_size);
    }
    style->line_height = style->font_size * 1.15;
    if (const std::string* value = style->Get("line-height")) {
      if (*value != "normal") {
        char* end = nullptr;
        double number = std::strtod(value->c_str(), &end);
        if (end != value->c_str() && *end == '\0') {
          style->line_height = number * style->font_size;
        } else if (std::optional<double> length =
                       ParseCSSLength(*value, style->font_size,
                                      style->font_size, style->viewport_)) {
          style->line_height = *length;
        }
      }
    }
    const std::string* family = style->Get("font-family");
    bool monospace = family && (family->find("mono") != std::string::npos ||
                                family->find("Mono") != std::string::npos ||
                                family->find("Courier") != std::string::npos ||
                                family->find("Menlo") != std::string::npos ||
                                family->find("Consolas") != std::string::npos);
    style->char_width = style->font_size * (monospace ? 0.6 : 0.5);
    ParseLengths(style);

    if (const std::string* value = style->Get("white-space")) {
      style->pre = value->substr(0, 3) == "pre" && *value != "pre-line";
      style->nowrap = *value == "nowrap" || *value == "pre";
    }
    const std::string* position = Own(style, "position");
    if (position && *position != "static") {
      style->positioned = true;
      style->fixed = *position == "fixed";
      style->out_of_flow = style->fixed || *position == "absolute";
    }
    for (const char* axis : {"overflow-x", "overflow-y"}) {
      const std::string* overflow = Own(style, axis);
      if (overflow && *overflow != "visible") style->scroll_container = true;
    }
    const std::string* sizing = Own(style, "box-sizing");
    style->border_box = sizing && *sizing == "border-box";
    const std::string* direction = Own(style, "flex-direction");
    style->flex_column = direction && direction->substr(0, 6) == "column";

    std::string display = "inline";
    if (const std::string* value = Own(style, "display")) display = *value;
    if (element->ns() == Namespace::kSVG &&
        !element->local_name().EqualsAscii("svg") && display != "none") {
      display = "inline";
    }
    bool inline_level = display.substr(0, 6) == "inline";
    using Display = ComputedStyle::Display;
    if (display == "none") {
      style->display = Display::kNone;
    } else if (display == "contents") {
      style->display = Display::kContents;
    } else if (display == "inline") {
      style->display = Display::kInline;
    } else if (display == "flex" || display == "inline-flex") {
      style->display = Display::kFlex;
    } else if (display == "table-row") {
      style->display = Display::kTableRow;
    } else {
      style->display = Display::kBlock;
    }
    const std::string* floating = Own(style, "float");
    bool floated = floating && *floating != "none";
    if ((floated || style->out_of_flow) && style->display != Display::kNone) {
      // Floats and absolutely positioned boxes are blockified. A float then
      // sits in the line like an inline-block, which is as close as this
      // layout gets to floating.
      if (style->display == Display::kInline ||
          style->display == Display::kContents) {
        style->display = Display::kBlock;
        display = "block";
      } else if (inline_level) {
        display = display.substr(7);
      }
      inline_level = floated && !style->out_of_flow;
    }
    style->is_inline_level = inline_level;
    style->display_keyword = display;
  }
};

const std::string* ComputedStyle::Get(const AtomicString& property) const {
  auto it = values_.find(property);
  if (it != values_.end()) return it->second;
  if (IsCustomProperty(property)) {
    for (const ComputedStyle* style = this; style;
         style = style->custom_parent_) {
      auto custom = style->custom_.find(property);
      if (custom != style->custom_.end()) return custom->second;
    }
    return nullptr;
  }
  if (inherited_) {
    auto inherited = inherited_->find(property);
    if (inherited != inherited_->end()) return inherited->second;
  }
  return nullptr;
}

const std::string* ComputedStyle::Get(std::string_view property) const {
  // Property names are ASCII, which atomizes without building a string
  // first. Custom property names may not be.
  for (char c : property) {
    if (static_cast<unsigned char>(c) >= 0x80) {
      return Get(AtomicString::FromUtf8(property));
    }
  }
  return Get(AtomicString(property));
}

std::optional<double> ComputedStyle::Length(std::string_view property,
                                            double percent_base) const {
  const std::string* value = Get(property);
  if (!value) return std::nullopt;
  return ParseCSSLength(*value, percent_base, font_size, viewport_);
}

std::optional<double> ComputedStyle::Length(LengthId id,
                                            double percent_base) const {
  const ParsedLength& parsed = lengths_[static_cast<int>(id)];
  switch (parsed.kind) {
    case ParsedLength::kNone:
      return std::nullopt;
    case ParsedLength::kPixels:
      return parsed.value;
    case ParsedLength::kPercent:
      if (std::isnan(percent_base)) return std::nullopt;
      return parsed.value * percent_base / 100;
    case ParsedLength::kUnparsed: {
      static const AtomicString* const kNames[] = {
#define V(Name, css) new AtomicString(css),
          MOCK_DOM_LENGTH_PROPERTIES(V)
#undef V
      };
      const std::string* value = Get(*kNames[static_cast<int>(id)]);
      return ParseCSSLength(*value, percent_base, font_size, viewport_);
    }
  }
  return std::nullopt;
}

namespace {

void DropStyles(DocumentStyleData* data, Node* root) {
  if (root->IsShadowRoot()) data->scopes.erase(root);
  for (Node* node = root; node; node = node->NextInPreOrder(root)) {
    if (!node->IsElement()) continue;
    data->styles.erase(node);
    if (ShadowRoot* shadow = static_cast<Element*>(node)->shadow_root()) {
      data->scopes.erase(shadow);
      DropStyles(data, shadow);
    }
  }
}

DocumentStyleData* StyleDataOf(Node* node) {
  Document* document =
      node->IsDocument() ? static_cast<Document*>(node) : node->document();
  return static_cast<DocumentStyleData*>(document->style_data.get());
}

}  // namespace

void InvalidateStyle(Node* root) {
  DocumentStyleData* data = StyleDataOf(root);
  if (!data || data->all_dirty) return;
  if (root->IsDocument() || data->dirty_roots.size() >= 1024) {
    InvalidateAllStyles(root->IsDocument() ? static_cast<Document*>(root)
                                           : root->document());
    return;
  }
  if (data->dirty_set.insert(root).second) data->dirty_roots.emplace_back(root);
}

void InvalidateStyleIfCached(Node* root) {
  DocumentStyleData* data = StyleDataOf(root);
  if (!data || data->all_dirty) return;
  if (data->styles.contains(root) || data->scopes.contains(root)) {
    InvalidateStyle(root);
  }
}

void DropCachedStyle(Node* node) {
  DocumentStyleData* data = StyleDataOf(node);
  if (!data) return;
  data->styles.erase(node);
  data->scopes.erase(node);
}

void InvalidateAllStyles(Document* document) {
  DocumentStyleData* data = StyleDataOf(document);
  if (!data) return;
  data->all_dirty = true;
  data->dirty_roots.clear();
  data->dirty_set.clear();
}

const ComputedStyle* ComputedStyleFor(Element* element) {
  if (!element->is_connected()) return nullptr;
  Document* document = element->document();
  if (!document->default_view()) return nullptr;
  if (!document->style_data) {
    document->style_data = std::make_shared<DocumentStyleData>();
  }
  DocumentStyleData* data =
      static_cast<DocumentStyleData*>(document->style_data.get());
  if (data->all_dirty || data->epoch != g_style_epoch) {
    data->styles.clear();
    data->scopes.clear();
    data->all_dirty = false;
    data->epoch = g_style_epoch;
  } else {
    for (const auto& root : data->dirty_roots) {
      DropStyles(data, root.Get());
    }
  }
  data->dirty_roots.clear();
  data->dirty_set.clear();
  return StyleEngine::Compute(element, data);
}

}  // namespace mock_dom
}  // namespace v8

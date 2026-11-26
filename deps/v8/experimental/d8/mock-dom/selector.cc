// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/d8/mock-dom/selector.h"

#include <cstdlib>

#include "experimental/d8/mock-dom/isolate-data.h"

namespace v8 {
namespace mock_dom {

using PseudoClass = SelectorList::PseudoClass;
using Simple = SelectorList::Simple;

// --- Parsing -----------------------------------------------------------------

class SelectorParser {
 public:
  SelectorParser(std::string_view text, std::string* error)
      : text_(text), error_(error) {}

  std::shared_ptr<SelectorList> ParseTopLevel() {
    auto list = ParseList();
    if (!list) return nullptr;
    SkipSpace();
    if (pos_ != text_.size()) return Fail("unexpected character");
    return list;
  }

 private:
  std::nullptr_t Fail(const std::string& reason) {
    if (error_->empty()) {
      *error_ = "'" + std::string(text_) + "' is not a valid selector (" +
                reason + ")";
    }
    return nullptr;
  }

  bool AtEnd() const { return pos_ >= text_.size(); }
  char Peek() const { return AtEnd() ? '\0' : text_[pos_]; }
  bool SkipSpace() {
    size_t start = pos_;
    while (!AtEnd() && IsHtmlSpace(text_[pos_])) ++pos_;
    return pos_ != start;
  }
  static bool IsIdentChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' ||
           static_cast<unsigned char>(c) >= 0x80;
  }
  static int HexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }

  // Consumes a CSS escape after the backslash.
  void ConsumeEscape(std::string* out) {
    if (AtEnd()) return;
    if (HexValue(text_[pos_]) < 0) {
      out->push_back(text_[pos_++]);
      return;
    }
    uint32_t code_point = 0;
    for (int i = 0; i < 6 && !AtEnd() && HexValue(text_[pos_]) >= 0; ++i) {
      code_point = code_point * 16 + HexValue(text_[pos_++]);
    }
    if (!AtEnd() && IsHtmlSpace(text_[pos_])) ++pos_;
    AppendUtf8(out, code_point);
  }

  bool ParseIdent(std::string* out) {
    out->clear();
    while (!AtEnd()) {
      char c = text_[pos_];
      if (IsIdentChar(c)) {
        out->push_back(c);
        ++pos_;
      } else if (c == '\\') {
        ++pos_;
        ConsumeEscape(out);
      } else {
        break;
      }
    }
    return !out->empty();
  }

  bool ParseString(std::string* out) {
    char quote = text_[pos_++];
    out->clear();
    while (!AtEnd() && text_[pos_] != quote) {
      if (text_[pos_] == '\\') {
        ++pos_;
        ConsumeEscape(out);
      } else {
        out->push_back(text_[pos_++]);
      }
    }
    if (AtEnd()) return false;
    ++pos_;
    return true;
  }

  std::shared_ptr<SelectorList> ParseList() {
    auto list = std::make_shared<SelectorList>();
    while (true) {
      SelectorList::Complex complex;
      if (!ParseComplex(&complex)) return nullptr;
      list->selectors_.push_back(std::move(complex));
      SkipSpace();
      if (Peek() != ',') break;
      ++pos_;
    }
    return list;
  }

  bool ParseComplex(SelectorList::Complex* complex) {
    SkipSpace();
    char combinator = 0;
    // A leading combinator ("> li") is only meaningful relative to :scope,
    // which callers spell out; reject it.
    while (true) {
      SelectorList::Compound compound;
      compound.combinator = combinator;
      if (!ParseCompound(&compound)) return false;
      complex->compounds.push_back(std::move(compound));
      bool had_space = SkipSpace();
      char c = Peek();
      if (AtEnd() || c == ',' || c == ')') return true;
      if (c == '>' || c == '+' || c == '~') {
        combinator = c;
        ++pos_;
        SkipSpace();
      } else if (had_space) {
        combinator = ' ';
      } else {
        Fail("unexpected character");
        return false;
      }
    }
  }

  bool ParseCompound(SelectorList::Compound* compound) {
    std::string ident;
    if (Peek() == '*') {
      ++pos_;
      Simple simple;
      simple.kind = Simple::kUniversal;
      compound->simples.push_back(simple);
    } else if (IsIdentChar(Peek()) || Peek() == '\\') {
      ParseIdent(&ident);
      Simple simple;
      simple.kind = Simple::kTag;
      simple.name = AtomicString::FromUtf8(ident);
      simple.lower_name = AtomicString::FromUtf8(AsciiLower(ident));
      compound->simples.push_back(simple);
    }
    if (Peek() == '|') {
      Fail("namespace prefixes are not supported by mock-dom");
      return false;
    }
    while (!AtEnd()) {
      char c = Peek();
      if (c == '#') {
        ++pos_;
        if (!ParseIdent(&ident)) return Fail("expected id"), false;
        Simple simple;
        simple.kind = Simple::kId;
        simple.name = AtomicString::FromUtf8(ident);
        compound->simples.push_back(simple);
      } else if (c == '.') {
        ++pos_;
        if (!ParseIdent(&ident)) return Fail("expected class"), false;
        Simple simple;
        simple.kind = Simple::kClass;
        simple.name = AtomicString::FromUtf8(ident);
        compound->simples.push_back(simple);
      } else if (c == '[') {
        ++pos_;
        Simple simple;
        if (!ParseAttribute(&simple)) return false;
        compound->simples.push_back(std::move(simple));
      } else if (c == ':') {
        ++pos_;
        Simple simple;
        if (!ParsePseudo(&simple)) return false;
        compound->simples.push_back(std::move(simple));
      } else {
        break;
      }
    }
    if (compound->simples.empty()) {
      Fail("expected a selector");
      return false;
    }
    return true;
  }

  bool ParseAttribute(Simple* simple) {
    simple->kind = Simple::kAttribute;
    SkipSpace();
    std::string name;
    if (!ParseIdent(&name)) return Fail("expected attribute name"), false;
    simple->name = AtomicString::FromUtf8(name);
    simple->lower_name = AtomicString::FromUtf8(AsciiLower(name));
    SkipSpace();
    if (Peek() == ']') {
      ++pos_;
      simple->op = Simple::kExists;
      return true;
    }
    char c = Peek();
    if (c == '=') {
      simple->op = Simple::kEquals;
      ++pos_;
    } else if (pos_ + 1 < text_.size() && text_[pos_ + 1] == '=') {
      switch (c) {
        case '~':
          simple->op = Simple::kIncludes;
          break;
        case '|':
          simple->op = Simple::kDashMatch;
          break;
        case '^':
          simple->op = Simple::kPrefix;
          break;
        case '$':
          simple->op = Simple::kSuffix;
          break;
        case '*':
          simple->op = Simple::kSubstring;
          break;
        default:
          return Fail("bad attribute operator"), false;
      }
      pos_ += 2;
    } else {
      return Fail("bad attribute selector"), false;
    }
    SkipSpace();
    if (Peek() == '"' || Peek() == '\'') {
      if (!ParseString(&simple->value)) {
        return Fail("unterminated string"), false;
      }
    } else if (!ParseIdent(&simple->value)) {
      return Fail("expected attribute value"), false;
    }
    SkipSpace();
    if (Peek() == 'i' || Peek() == 'I') {
      simple->case_insensitive = true;
      ++pos_;
      SkipSpace();
    } else if (Peek() == 's' || Peek() == 'S') {
      ++pos_;
      SkipSpace();
    }
    if (Peek() != ']') return Fail("expected ]"), false;
    ++pos_;
    return true;
  }

  bool ParseNth(Simple* simple) {
    size_t close = text_.find(')', pos_);
    if (close == std::string_view::npos) return Fail("expected )"), false;
    std::string argument;
    for (char c : text_.substr(pos_, close - pos_)) {
      if (!IsHtmlSpace(c)) argument.push_back(ToAsciiLower(c));
    }
    pos_ = close + 1;
    if (argument.find("of") != std::string::npos && argument != "odd") {
      return Fail("':nth-child(... of S)' is not supported by mock-dom"), false;
    }
    if (argument == "odd") {
      simple->a = 2;
      simple->b = 1;
    } else if (argument == "even") {
      simple->a = 2;
      simple->b = 0;
    } else {
      size_t n = argument.find('n');
      if (n == std::string::npos) {
        simple->a = 0;
        simple->b = std::atoi(argument.c_str());
      } else {
        std::string a = argument.substr(0, n);
        if (a.empty() || a == "+") {
          simple->a = 1;
        } else if (a == "-") {
          simple->a = -1;
        } else {
          simple->a = std::atoi(a.c_str());
        }
        std::string b = argument.substr(n + 1);
        simple->b = b.empty() ? 0 : std::atoi(b.c_str());
      }
    }
    return true;
  }

  bool ParsePseudo(Simple* simple) {
    simple->kind = Simple::kPseudo;
    bool is_element = false;
    if (Peek() == ':') {
      ++pos_;
      is_element = true;
    }
    std::string name;
    if (!ParseIdent(&name)) return Fail("expected pseudo-class"), false;
    name = AsciiLower(name);
    bool functional = Peek() == '(';
    if (functional) ++pos_;

    if (is_element || name == "before" || name == "after" ||
        name == "first-line" || name == "first-letter") {
      // Pseudo-elements are never returned by the selector APIs.
      if (functional) {
        size_t close = text_.find(')', pos_);
        if (close == std::string_view::npos) return Fail("expected )"), false;
        pos_ = close + 1;
      }
      simple->pseudo = PseudoClass::kNever;
      return true;
    }

    struct Entry {
      const char* name;
      PseudoClass pseudo;
      bool functional;
    };
    static const Entry kEntries[] = {
        {"not", PseudoClass::kNot, true},
        {"is", PseudoClass::kIs, true},
        {"where", PseudoClass::kIs, true},
        {"matches", PseudoClass::kIs, true},
        {"-webkit-any", PseudoClass::kIs, true},
        {"nth-child", PseudoClass::kNthChild, true},
        {"nth-last-child", PseudoClass::kNthLastChild, true},
        {"nth-of-type", PseudoClass::kNthOfType, true},
        {"nth-last-of-type", PseudoClass::kNthLastOfType, true},
        {"first-child", PseudoClass::kFirstChild, false},
        {"last-child", PseudoClass::kLastChild, false},
        {"only-child", PseudoClass::kOnlyChild, false},
        {"first-of-type", PseudoClass::kFirstOfType, false},
        {"last-of-type", PseudoClass::kLastOfType, false},
        {"only-of-type", PseudoClass::kOnlyOfType, false},
        {"empty", PseudoClass::kEmpty, false},
        {"root", PseudoClass::kRoot, false},
        {"scope", PseudoClass::kScope, false},
        {"checked", PseudoClass::kChecked, false},
        {"disabled", PseudoClass::kDisabled, false},
        {"enabled", PseudoClass::kEnabled, false},
        {"focus", PseudoClass::kFocus, false},
        {"focus-visible", PseudoClass::kFocus, false},
        {"focus-within", PseudoClass::kFocusWithin, false},
        {"link", PseudoClass::kAnyLink, false},
        {"any-link", PseudoClass::kAnyLink, false},
        {"defined", PseudoClass::kDefined, false},
        {"required", PseudoClass::kRequired, false},
        {"optional", PseudoClass::kOptional, false},
        {"placeholder-shown", PseudoClass::kPlaceholderShown, false},
        {"indeterminate", PseudoClass::kIndeterminate, false},
        // No pointer, no navigation history, no :target support.
        {"hover", PseudoClass::kNever, false},
        {"active", PseudoClass::kNever, false},
        {"visited", PseudoClass::kNever, false},
        {"target", PseudoClass::kNever, false},
    };
    if (name == "host") {
      simple->pseudo = PseudoClass::kHost;
      if (!functional) return true;
      simple->list = ParseList();
      if (!simple->list) return false;
      SkipSpace();
      if (Peek() != ')') return Fail("expected )"), false;
      ++pos_;
      return true;
    }
    for (const Entry& entry : kEntries) {
      if (name != entry.name) continue;
      if (entry.functional != functional) {
        return Fail("malformed pseudo-class :" + name), false;
      }
      simple->pseudo = entry.pseudo;
      if (!functional) return true;
      if (entry.pseudo == PseudoClass::kNot ||
          entry.pseudo == PseudoClass::kIs) {
        simple->list = ParseList();
        if (!simple->list) return false;
        SkipSpace();
        if (Peek() != ')') return Fail("expected )"), false;
        ++pos_;
        return true;
      }
      return ParseNth(simple);
    }
    return Fail("pseudo-class :" + name + " is not supported by mock-dom"),
           false;
  }

  std::string_view text_;
  std::string* error_;
  size_t pos_ = 0;
};

std::shared_ptr<SelectorList> SelectorList::Parse(std::string_view text,
                                                  std::string* error) {
  SelectorParser parser(text, error);
  auto list = parser.ParseTopLevel();
  if (!list && error->empty()) {
    *error = "'" + std::string(text) + "' is not a valid selector";
  }
  return list;
}

std::shared_ptr<SelectorList> GetSelector(const String& text,
                                          ExceptionState* exception_state) {
  IsolateData* data = IsolateData::Get();
  // Keyed by atom: a selector string that JS passes repeatedly is turned
  // into the same atom each time, so the lookup is a pointer hash.
  AtomicString key(text);
  auto it = data->selectors.find(key);
  if (it != data->selectors.end()) return it->second;
  std::string error;
  std::shared_ptr<SelectorList> list =
      SelectorList::Parse(text.ToUtf8(), &error);
  if (!list) {
    exception_state->Throw("SyntaxError", error);
    return nullptr;
  }
  constexpr size_t kMaxCachedSelectors = 1024;
  if (data->selectors.size() >= kMaxCachedSelectors) data->selectors.clear();
  data->selectors.emplace(key, list);
  return list;
}

// --- Matching ----------------------------------------------------------------

namespace {

bool SameType(const Element* a, const Element* b) {
  return a->ns() == b->ns() && a->local_name() == b->local_name();
}

int IndexAmongSiblings(Element* element, bool of_type, bool from_end) {
  int index = 1;
  if (from_end) {
    for (Element* s = element->next_element_sibling(); s;
         s = s->next_element_sibling()) {
      if (!of_type || SameType(s, element)) ++index;
    }
  } else {
    for (Element* s = element->previous_element_sibling(); s;
         s = s->previous_element_sibling()) {
      if (!of_type || SameType(s, element)) ++index;
    }
  }
  return index;
}

bool MatchesNth(int index, int a, int b) {
  if (a == 0) return index == b;
  int diff = index - b;
  return diff % a == 0 && diff / a >= 0;
}

bool MatchesAttribute(Element* element, const Simple& simple) {
  const AtomicString& attribute_value = element->GetAttribute(
      element->IsHTML() ? simple.lower_name : simple.name);
  if (attribute_value.is_null()) return false;
  if (simple.op == Simple::kExists) return true;
  std::string value = attribute_value.ToUtf8();
  std::string expected = simple.value;
  if (simple.case_insensitive) {
    value = AsciiLower(value);
    expected = AsciiLower(expected);
  }
  switch (simple.op) {
    case Simple::kExists:
      return true;
    case Simple::kEquals:
      return value == expected;
    case Simple::kIncludes: {
      if (expected.empty()) return false;
      size_t i = 0;
      while (i < value.size()) {
        while (i < value.size() && IsHtmlSpace(value[i])) ++i;
        size_t start = i;
        while (i < value.size() && !IsHtmlSpace(value[i])) ++i;
        if (value.compare(start, i - start, expected) == 0 &&
            i - start == expected.size()) {
          return true;
        }
      }
      return false;
    }
    case Simple::kDashMatch:
      return value == expected ||
             (value.size() > expected.size() &&
              value.compare(0, expected.size(), expected) == 0 &&
              value[expected.size()] == '-');
    case Simple::kPrefix:
      return !expected.empty() && value.size() >= expected.size() &&
             value.compare(0, expected.size(), expected) == 0;
    case Simple::kSuffix:
      return !expected.empty() && value.size() >= expected.size() &&
             value.compare(value.size() - expected.size(), expected.size(),
                           expected) == 0;
    case Simple::kSubstring:
      return !expected.empty() && value.find(expected) != std::string::npos;
  }
  return false;
}

bool MatchesPseudo(Element* element, const Simple& simple, const Node* scope) {
  static const AtomicString* href = new AtomicString("href");
  static const AtomicString* required = new AtomicString("required");
  static const AtomicString* placeholder = new AtomicString("placeholder");
  switch (simple.pseudo) {
    case PseudoClass::kNot:
      return !simple.list->Matches(element, scope);
    case PseudoClass::kIs:
      return simple.list->Matches(element, scope);
    case PseudoClass::kFirstChild:
      return !element->previous_element_sibling();
    case PseudoClass::kLastChild:
      return !element->next_element_sibling();
    case PseudoClass::kOnlyChild:
      return !element->previous_element_sibling() &&
             !element->next_element_sibling();
    case PseudoClass::kNthChild:
      return MatchesNth(IndexAmongSiblings(element, false, false), simple.a,
                        simple.b);
    case PseudoClass::kNthLastChild:
      return MatchesNth(IndexAmongSiblings(element, false, true), simple.a,
                        simple.b);
    case PseudoClass::kFirstOfType:
      return IndexAmongSiblings(element, true, false) == 1;
    case PseudoClass::kLastOfType:
      return IndexAmongSiblings(element, true, true) == 1;
    case PseudoClass::kOnlyOfType:
      return IndexAmongSiblings(element, true, false) == 1 &&
             IndexAmongSiblings(element, true, true) == 1;
    case PseudoClass::kNthOfType:
      return MatchesNth(IndexAmongSiblings(element, true, false), simple.a,
                        simple.b);
    case PseudoClass::kNthLastOfType:
      return MatchesNth(IndexAmongSiblings(element, true, true), simple.a,
                        simple.b);
    case PseudoClass::kEmpty:
      for (Node* n = element->first(); n; n = n->next_sibling()) {
        if (n->IsElement()) return false;
        if (n->IsText() && !static_cast<CharacterData*>(n)->data().empty()) {
          return false;
        }
      }
      return true;
    case PseudoClass::kRoot:
      return element->parent() && element->parent()->IsDocument();
    case PseudoClass::kScope:
      if (scope && scope->IsElement()) return element == scope;
      return element->parent() && element->parent()->IsDocument();
    case PseudoClass::kChecked:
      if (element->IsCheckable()) return element->Checked();
      return element->HasTag("option") && element->Selected();
    case PseudoClass::kDisabled:
      return element->IsDisabled();
    case PseudoClass::kEnabled:
      return element->IsFormControl() && !element->IsDisabled();
    case PseudoClass::kFocus:
      return element->document()->focused_element() == element;
    case PseudoClass::kFocusWithin: {
      Element* focused = element->document()->focused_element();
      return focused && element->Contains(focused);
    }
    case PseudoClass::kAnyLink:
      return (element->HasTag("a") || element->HasTag("area")) &&
             element->HasAttribute(*href);
    case PseudoClass::kDefined:
      return !element->has_flag(Node::kIsUndefinedCustom);
    case PseudoClass::kRequired:
      return element->IsFormControl() && element->HasAttribute(*required);
    case PseudoClass::kOptional:
      return element->IsFormControl() && !element->HasAttribute(*required);
    case PseudoClass::kPlaceholderShown:
      return (element->HasTag("input") || element->HasTag("textarea")) &&
             element->HasAttribute(*placeholder) && element->Value().empty();
    case PseudoClass::kIndeterminate:
      return element->rare_data() && element->rare_data()->indeterminate;
    case PseudoClass::kHost:
      return scope && scope->IsShadowRoot() &&
             static_cast<const ShadowRoot*>(scope)->host() == element &&
             (!simple.list || simple.list->Matches(element, nullptr));
    case PseudoClass::kNever:
      return false;
  }
  return false;
}

bool MatchesSimple(Element* element, const Simple& simple, const Node* scope) {
  switch (simple.kind) {
    case Simple::kUniversal:
      return true;
    case Simple::kTag:
      return element->local_name() ==
             (element->IsHTML() ? simple.lower_name : simple.name);
    case Simple::kId:
      return element->id() == simple.name;
    case Simple::kClass:
      return element->HasClass(simple.name);
    case Simple::kAttribute:
      return MatchesAttribute(element, simple);
    case Simple::kPseudo:
      return MatchesPseudo(element, simple, scope);
  }
  return false;
}

bool MatchesFrom(Element* element, const SelectorList::Complex& complex,
                 size_t index, const Node* scope) {
  const SelectorList::Compound& compound = complex.compounds[index];
  for (const Simple& simple : compound.simples) {
    if (!MatchesSimple(element, simple, scope)) return false;
  }
  if (index == 0) return true;
  // A selector in a shadow tree's stylesheet can reach out of the tree only
  // through :host, e.g. ":host(.done) label".
  auto matches_host = [&](Element* top) {
    ContainerNode* root = top->parent();
    if (!root || root != scope || !root->IsShadowRoot()) return false;
    bool has_host = false;
    for (const Simple& simple : complex.compounds[index - 1].simples) {
      if (simple.kind == Simple::kPseudo &&
          simple.pseudo == PseudoClass::kHost) {
        has_host = true;
      }
    }
    return has_host && MatchesFrom(static_cast<ShadowRoot*>(root)->host(),
                                   complex, index - 1, scope);
  };
  switch (compound.combinator) {
    case ' ': {
      Element* top = element;
      for (Element* ancestor = element->parent_element(); ancestor;
           ancestor = ancestor->parent_element()) {
        if (MatchesFrom(ancestor, complex, index - 1, scope)) return true;
        top = ancestor;
      }
      return matches_host(top);
    }
    case '>': {
      Element* parent = element->parent_element();
      if (!parent) return matches_host(element);
      return MatchesFrom(parent, complex, index - 1, scope);
    }
    case '+': {
      Element* previous = element->previous_element_sibling();
      return previous && MatchesFrom(previous, complex, index - 1, scope);
    }
    case '~':
      for (Element* previous = element->previous_element_sibling(); previous;
           previous = previous->previous_element_sibling()) {
        if (MatchesFrom(previous, complex, index - 1, scope)) return true;
      }
      return false;
  }
  return false;
}

// If |list| is a lone "#id" selector, returns the id.
const AtomicString* SingleIdSelector(const SelectorList& list) {
  if (list.selectors().size() != 1) return nullptr;
  const SelectorList::Complex& complex = list.selectors()[0];
  if (complex.compounds.size() != 1) return nullptr;
  const SelectorList::Compound& compound = complex.compounds[0];
  if (compound.simples.size() != 1 || compound.simples[0].kind != Simple::kId) {
    return nullptr;
  }
  return &compound.simples[0].name;
}

}  // namespace

bool SelectorList::Matches(Element* element, const Node* scope) const {
  for (const Complex& complex : selectors_) {
    if (MatchesFrom(element, complex, complex.compounds.size() - 1, scope)) {
      return true;
    }
  }
  return false;
}

bool SelectorList::MatchesComplex(size_t index, Element* element,
                                  const Node* scope) const {
  const Complex& complex = selectors_[index];
  return MatchesFrom(element, complex, complex.compounds.size() - 1, scope);
}

// --- Selector APIs -----------------------------------------------------------

Element* ContainerNode::QuerySelector(Isolate* isolate, const String& selectors,
                                      ExceptionState* exception_state) {
  std::shared_ptr<SelectorList> list = GetSelector(selectors, exception_state);
  if (!list) return nullptr;
  if (const AtomicString* id = SingleIdSelector(*list)) {
    // "#id" on a tree covered by the document's id map is a hash lookup, as
    // in a browser.
    if (is_connected() && !ContainingShadowRoot()) {
      Document* doc = IsDocument() ? static_cast<Document*>(this) : document();
      Element* element = doc->GetElementById(*id);
      if (element && element != this && Contains(element)) return element;
      return nullptr;
    }
  }
  for (Node* n = first(); n; n = n->NextInPreOrder(this)) {
    if (n->IsElement() && list->Matches(static_cast<Element*>(n), this)) {
      return static_cast<Element*>(n);
    }
  }
  return nullptr;
}

void ContainerNode::QuerySelectorAll(Isolate* isolate, const String& selectors,
                                     std::vector<Element*>* out,
                                     ExceptionState* exception_state) {
  std::shared_ptr<SelectorList> list = GetSelector(selectors, exception_state);
  if (!list) return;
  for (Node* n = first(); n; n = n->NextInPreOrder(this)) {
    if (n->IsElement() && list->Matches(static_cast<Element*>(n), this)) {
      out->push_back(static_cast<Element*>(n));
    }
  }
}

bool Element::Matches(Isolate* isolate, const String& selectors,
                      ExceptionState* exception_state) {
  std::shared_ptr<SelectorList> list = GetSelector(selectors, exception_state);
  return list && list->Matches(this, this);
}

Element* Element::Closest(Isolate* isolate, const String& selectors,
                          ExceptionState* exception_state) {
  std::shared_ptr<SelectorList> list = GetSelector(selectors, exception_state);
  if (!list) return nullptr;
  for (Element* e = this; e; e = e->parent_element()) {
    if (list->Matches(e, this)) return e;
  }
  return nullptr;
}

}  // namespace mock_dom
}  // namespace v8

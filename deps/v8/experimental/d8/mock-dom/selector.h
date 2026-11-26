// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_D8_MOCK_DOM_SELECTOR_H_
#define V8_EXPERIMENTAL_D8_MOCK_DOM_SELECTOR_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/d8/mock-dom/dom.h"

namespace v8 {
namespace mock_dom {

// A parsed selector list ("a > b.c, d[e=f]:not(.g)").
class SelectorList {
 public:
  enum class PseudoClass : uint8_t {
    kNot,
    kIs,  // also :where and :matches
    kFirstChild,
    kLastChild,
    kOnlyChild,
    kNthChild,
    kNthLastChild,
    kFirstOfType,
    kLastOfType,
    kOnlyOfType,
    kNthOfType,
    kNthLastOfType,
    kEmpty,
    kRoot,
    kScope,
    kChecked,
    kDisabled,
    kEnabled,
    kFocus,
    kFocusWithin,
    kAnyLink,
    kDefined,
    kRequired,
    kOptional,
    kPlaceholderShown,
    kIndeterminate,
    kHost,   // :host and :host(<selector>)
    kNever,  // valid but never matches here: :hover, :active, :visited, ...
  };

  struct Simple {
    enum Kind : uint8_t { kTag, kUniversal, kId, kClass, kAttribute, kPseudo };
    enum AttributeOp : uint8_t {
      kExists,
      kEquals,
      kIncludes,
      kDashMatch,
      kPrefix,
      kSuffix,
      kSubstring,
    };
    Kind kind = kUniversal;
    // Tag (as written), id, class or attribute name.
    AtomicString name;
    // Tag or attribute name lower-cased, for matching HTML elements.
    AtomicString lower_name;
    AttributeOp op = kExists;
    std::string value;
    bool case_insensitive = false;
    PseudoClass pseudo = PseudoClass::kNever;
    int a = 0;
    int b = 0;
    std::shared_ptr<SelectorList> list;
  };

  struct Compound {
    std::vector<Simple> simples;
    // How this compound relates to the one on its left: ' ', '>', '+', '~'.
    // 0 for the leftmost compound.
    char combinator = 0;
  };

  struct Complex {
    std::vector<Compound> compounds;  // left to right
  };

  // Returns null and fills |error| if |text| is not a selector this engine
  // supports. Unsupported-but-valid CSS is an error too, never a silent
  // non-match.
  static std::shared_ptr<SelectorList> Parse(std::string_view text,
                                             std::string* error);

  // |scope| is the :scope element for the selector APIs. For stylesheet
  // matching it is the shadow root whose sheet the selector comes from (or
  // null for document sheets), which is what :host matches against.
  bool Matches(Element* element, const Node* scope) const;
  bool MatchesComplex(size_t index, Element* element, const Node* scope) const;
  const std::vector<Complex>& selectors() const { return selectors_; }

 private:
  friend class SelectorParser;
  std::vector<Complex> selectors_;
};

// Parses |text|, using the per-isolate selector cache. Throws SyntaxError
// through |exception_state| on failure.
std::shared_ptr<SelectorList> GetSelector(const String& text,
                                          ExceptionState* exception_state);

}  // namespace mock_dom
}  // namespace v8

#endif  // V8_EXPERIMENTAL_D8_MOCK_DOM_SELECTOR_H_

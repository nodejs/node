// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A small CSS implementation: enough of parsing and the cascade that the
// geometry layout.cc reports is driven by the page's own stylesheets. Pages
// size editors, charts and canvases through CSS and then read the result back
// from JS, so without this they would do a different amount of work than in
// a browser.
//
// What it covers: style rules, @media, @layer, @supports, the selectors that
// selector.h supports plus :host, specificity and source order, !important,
// inline styles, inheritance of the common inherited properties, custom
// properties with var(), calc()/min()/max()/clamp() for lengths, and the
// box-model shorthands. Not covered: pseudo-elements, ::slotted, @import,
// @container, CSS nesting, :where() having zero specificity.

#ifndef V8_EXPERIMENTAL_D8_MOCK_DOM_CSS_H_
#define V8_EXPERIMENTAL_D8_MOCK_DOM_CSS_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "experimental/d8/mock-dom/dom.h"
#include "experimental/d8/mock-dom/selector.h"
#include "include/v8-container.h"
#include "include/v8-proxy.h"

namespace v8 {
namespace mock_dom {

// The viewport size that vw/vh units and media queries resolve against.
struct Viewport {
  double width = 800;
  double height = 600;
  static Viewport Of(const Window* window) {
    Viewport viewport;
    if (window) {
      viewport.width = window->inner_width;
      viewport.height = window->inner_height;
    }
    return viewport;
  }
};

struct CSSDeclaration {
  AtomicString name;  // lower-case, or a custom property name
  std::string value;
  bool important = false;
};

// Parses "a: b; c: d !important" into declarations, expanding the box-model
// shorthands (margin, padding, inset, border, overflow, flex, gap, font).
void ParseDeclarations(std::string_view text, std::vector<CSSDeclaration>* out);

struct StyleRule {
  std::shared_ptr<SelectorList> selectors;
  std::vector<CSSDeclaration> declarations;
};

// A parsed stylesheet. Rules are bucketed by the id, class or tag their
// rightmost compound selector requires, so that matching an element only
// looks at rules that could apply to it.
struct StyleSheetContents {
  struct Entry {
    const StyleRule* rule;
    uint32_t complex;  // index into rule->selectors
    uint32_t specificity;
    uint32_t order;
  };
  using Bucket = std::vector<Entry>;
  using BucketMap =
      absl::flat_hash_map<AtomicString, Bucket, AtomicString::Hash>;

  std::vector<std::unique_ptr<StyleRule>> rules;
  BucketMap by_id;
  BucketMap by_class;
  BucketMap by_tag;
  Bucket universal;
  Bucket host;  // rightmost compound contains :host

  // Parses |text|. @media blocks are evaluated against |window| right away.
  static std::shared_ptr<StyleSheetContents> Parse(std::string_view text,
                                                   const Window* window);

 private:
  void AddRules(std::string_view text, const Window* window);
};

// Evaluates a media query list against the window's viewport.
bool EvaluateMediaQuery(std::string query, const Window* window);

// A CSSOM stylesheet: the sheet of a <style> or <link> element, or a
// constructed sheet used through adoptedStyleSheets.
class CSSStyleSheet final : public ScriptWrappable {
 public:
  explicit CSSStyleSheet(Element* owner)
      : ScriptWrappable(Interface::kCSSStyleSheet), owner_(owner) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(owner_);
  }
  Window* WrapperWindow() const override {
    return owner_ ? owner_->WrapperWindow() : nullptr;
  }

  Element* owner() const { return owner_.Get(); }
  const std::vector<std::string>& rule_texts() const { return rule_texts_; }

  // Replaces all rules with those in |text|.
  void SetText(std::string_view text);
  bool InsertRule(const std::string& rule, size_t index);
  bool DeleteRule(size_t index);

  std::shared_ptr<StyleSheetContents> Contents(const Window* window);

 private:
  void DidChange();

  Member<Element> owner_;
  std::vector<std::string> rule_texts_;
  std::shared_ptr<StyleSheetContents> contents_;
};

// Backing store for Document.adoptedStyleSheets and
// ShadowRoot.adoptedStyleSheets. Keeping the sheets in a C++ vector lets the
// cascade read them without calling back into JS, while a lazily created JS
// Proxy over a private array keeps in-place mutations such as .push() in sync.
class AdoptedStyleSheets final
    : public cppgc::GarbageCollected<AdoptedStyleSheets> {
 public:
  explicit AdoptedStyleSheets(Node* scope) : scope_(scope) {}
  void Trace(Visitor* visitor) const {
    if (DeferTraceIfConcurrent(visitor, this)) return;
    visitor->Trace(scope_);
    visitor->Trace(proxy_);
    visitor->Trace(target_);
    for (const auto& sheet : sheets_) visitor->Trace(sheet);
  }

  Node* scope() const { return scope_.Get(); }
  const std::vector<Member<CSSStyleSheet>>& sheets() const { return sheets_; }
  std::vector<Member<CSSStyleSheet>>& mutable_sheets() { return sheets_; }
  TracedReference<Proxy>& proxy() { return proxy_; }
  TracedReference<Array>& target() { return target_; }

 private:
  Member<Node> scope_;
  MOCK_DOM_TRACED_CONTAINER
  std::vector<Member<CSSStyleSheet>> sheets_;
  TracedReference<Proxy> proxy_;
  TracedReference<Array> target_;
};

// The sheet owned by a <style> or <link rel=stylesheet> element, kept in
// sync with the element's text (or the text the loader supplied for a link).
// Null for a link whose stylesheet has not been supplied.
CSSStyleSheet* SheetOf(Isolate* isolate, Element* style_or_link);

// Changes whenever anything that style or layout depends on may have changed
// for |document|: the tree, attributes, text, inline styles or stylesheets.
uint64_t StyleKey(const Document* document);

// Length-valued properties that layout reads for every box. They are parsed
// once when the style is computed.
#define MOCK_DOM_LENGTH_PROPERTIES(V)         \
  V(MarginLeft, "margin-left")                \
  V(MarginRight, "margin-right")              \
  V(MarginTop, "margin-top")                  \
  V(MarginBottom, "margin-bottom")            \
  V(BorderLeftWidth, "border-left-width")     \
  V(BorderRightWidth, "border-right-width")   \
  V(BorderTopWidth, "border-top-width")       \
  V(BorderBottomWidth, "border-bottom-width") \
  V(PaddingLeft, "padding-left")              \
  V(PaddingRight, "padding-right")            \
  V(PaddingTop, "padding-top")                \
  V(PaddingBottom, "padding-bottom")          \
  V(Width, "width")                           \
  V(Height, "height")                         \
  V(MinWidth, "min-width")                    \
  V(MaxWidth, "max-width")                    \
  V(MinHeight, "min-height")                  \
  V(MaxHeight, "max-height")                  \
  V(Left, "left")                             \
  V(Right, "right")                           \
  V(Top, "top")                               \
  V(Bottom, "bottom")                         \
  V(FlexBasis, "flex-basis")                  \
  V(RowGap, "row-gap")                        \
  V(ColumnGap, "column-gap")

enum class LengthId : uint8_t {
#define V(Name, css) k##Name,
  MOCK_DOM_LENGTH_PROPERTIES(V)
#undef V
      kCount
};

// The result of the cascade for one element.
class ComputedStyle {
 public:
  enum class Display : uint8_t {
    kNone,
    kContents,
    kInline,
    kBlock,
    kFlex,
    kTableRow,
  };

  // The cascaded (and for inherited properties, inherited) value with var()
  // resolved. Null if the property is not set anywhere.
  const std::string* Get(std::string_view property) const;
  const std::string* Get(const AtomicString& property) const;
  const std::string* Get(const char* property) const {
    return Get(std::string_view(property));
  }
  bool Is(std::string_view property, std::string_view value) const {
    const std::string* v = Get(property);
    return v && *v == value;
  }

  // Grid, table, list-item and flow-root boxes are laid out as blocks.
  Display display = Display::kInline;
  // The keyword as getComputedStyle reports it.
  std::string display_keyword = "inline";
  // Takes part in a line box instead of stacking vertically.
  bool is_inline_level = true;
  bool out_of_flow = false;  // position: absolute / fixed
  bool fixed = false;
  bool positioned = false;  // position other than static
  bool flex_column = false;
  bool border_box = false;
  bool scroll_container = false;  // overflow other than visible
  bool nowrap = false;            // white-space: nowrap / pre
  bool pre = false;               // white-space: pre / pre-wrap
  double font_size = 16;
  double line_height = 18.4;
  double char_width = 8;

  // Resolves a length-valued property. |percent_base| may be NaN, in which
  // case percentages do not resolve.
  std::optional<double> Length(std::string_view property,
                               double percent_base) const;
  std::optional<double> Length(LengthId id, double percent_base) const;
  double LengthOr(LengthId id, double percent_base, double fallback) const {
    return Length(id, percent_base).value_or(fallback);
  }

 private:
  friend class StyleEngine;
  struct ParsedLength {
    enum Kind : uint8_t { kNone, kPixels, kPercent, kUnparsed };
    Kind kind = kNone;
    double value = 0;
  };
  // Values point into the stylesheets' rules, which the document's style
  // data keeps alive, or into the two members below.
  using ValueMap =
      absl::flat_hash_map<AtomicString, const std::string*, AtomicString::Hash>;
  // Non-inherited properties declared for this element.
  ValueMap values_;
  // Inherited properties in effect for this element. Shared with the
  // parent's style unless this element declares one itself, so a lookup
  // never has to walk up the tree.
  std::shared_ptr<const ValueMap> inherited_;
  // Custom properties declared for this element, and the nearest ancestor
  // style that declares any. They are too many to copy per element, and
  // few elements declare them, so lookups walk this short chain.
  ValueMap custom_;
  const ComputedStyle* custom_parent_ = nullptr;
  std::vector<CSSDeclaration> inline_declarations_;
  std::vector<std::unique_ptr<std::string>> owned_values_;
  ParsedLength lengths_[static_cast<int>(LengthId::kCount)];
  const ComputedStyle* parent_ = nullptr;
  Viewport viewport_;
};

// Returns the computed style of a connected |element|, or null if it has no
// style because it is not in a document that is being displayed. Valid until
// StyleKey() of the document changes.
const ComputedStyle* ComputedStyleFor(Element* element);

// Parses a CSS length (px, em, rem, %, vw, vh, calc(), ...).
std::optional<double> ParseCSSLength(const std::string& text,
                                     double percent_base, double font_size,
                                     Viewport viewport);

}  // namespace mock_dom
}  // namespace v8

#endif  // V8_EXPERIMENTAL_D8_MOCK_DOM_CSS_H_

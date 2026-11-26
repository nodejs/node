// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/d8/mock-dom/layout.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>

#include "experimental/d8/mock-dom/css.h"
#include "experimental/d8/mock-dom/objects.h"

namespace v8 {
namespace mock_dom {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
// "Unbounded" available width, used when measuring max-content sizes.
constexpr double kUnbounded = 1e9;

bool Definite(double value) { return !std::isnan(value); }

struct LayoutData {
  uint64_t key = ~uint64_t{0};
  bool done = false;
  // Keys are connected elements; cleared whenever the tree changes.
  absl::flat_hash_map<const void*, LayoutBox> boxes;
};

double CodePoints(std::string_view text) {
  double count = 0;
  for (char c : text) {
    if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++count;
  }
  return count;
}

std::string AttributeValue(const Element* element, const char* name) {
  const AtomicString& value = element->GetAttribute(AtomicString(name));
  return value.is_null() ? std::string() : value.ToUtf8();
}

double NumberOr(const ComputedStyle* style, const char* property,
                double fallback) {
  const std::string* value = style->Get(property);
  if (!value) return fallback;
  char* end = nullptr;
  double number = std::strtod(value->c_str(), &end);
  return end == value->c_str() ? fallback : number;
}

// Elements whose content is not laid out as children.
bool IsReplaced(const Element* element) {
  if (!element->IsHTML()) return false;
  return element->HasTag("canvas") || element->HasTag("img") ||
         element->HasTag("iframe") || element->HasTag("video") ||
         element->HasTag("input") || element->HasTag("textarea") ||
         element->HasTag("select");
}

// Appends the children of |parent| in the flat tree: a shadow host's
// children are those of its shadow root, and a slot's children are the
// host's children assigned to it.
void FlatChildren(Node* parent, std::vector<Node*>* out) {
  static const AtomicString* name_atom = new AtomicString("name");
  static const AtomicString* slot_atom = new AtomicString("slot");
  if (parent->IsElement()) {
    Element* element = static_cast<Element*>(parent);
    if (ShadowRoot* shadow = element->shadow_root()) {
      for (Node* n = shadow->first(); n; n = n->next_sibling()) {
        out->push_back(n);
      }
      return;
    }
    if (element->HasTag("slot")) {
      if (ShadowRoot* root = element->ContainingShadowRoot()) {
        const AtomicString& name = element->GetAttribute(*name_atom);
        for (Node* n = root->host()->first(); n; n = n->next_sibling()) {
          const AtomicString* assigned = nullptr;
          if (n->IsElement()) {
            assigned = &static_cast<Element*>(n)->GetAttribute(*slot_atom);
          }
          bool wants_default = !assigned || assigned->empty();
          if (name.empty() ? wants_default : (assigned && *assigned == name)) {
            out->push_back(n);
          }
        }
        if (!out->empty()) return;
      }
    }
  }
  for (Node* n = parent->first_child(); n; n = n->next_sibling()) {
    out->push_back(n);
  }
}

struct Edges {
  double margin_left = 0, margin_right = 0, margin_top = 0, margin_bottom = 0;
  double border_left = 0, border_right = 0, border_top = 0, border_bottom = 0;
  double padding_left = 0, padding_right = 0, padding_top = 0,
         padding_bottom = 0;
  double horizontal_margins() const { return margin_left + margin_right; }
  double vertical_margins() const { return margin_top + margin_bottom; }
  double horizontal_inner() const {
    return border_left + border_right + padding_left + padding_right;
  }
  double vertical_inner() const {
    return border_top + border_bottom + padding_top + padding_bottom;
  }
};

Edges EdgesOf(const ComputedStyle* style, double percent_base) {
  Edges e;
  e.margin_left = style->LengthOr(LengthId::kMarginLeft, percent_base, 0);
  e.margin_right = style->LengthOr(LengthId::kMarginRight, percent_base, 0);
  e.margin_top = style->LengthOr(LengthId::kMarginTop, percent_base, 0);
  e.margin_bottom = style->LengthOr(LengthId::kMarginBottom, percent_base, 0);
  e.border_left = style->LengthOr(LengthId::kBorderLeftWidth, kNaN, 0);
  e.border_right = style->LengthOr(LengthId::kBorderRightWidth, kNaN, 0);
  e.border_top = style->LengthOr(LengthId::kBorderTopWidth, kNaN, 0);
  e.border_bottom = style->LengthOr(LengthId::kBorderBottomWidth, kNaN, 0);
  e.padding_left = style->LengthOr(LengthId::kPaddingLeft, percent_base, 0);
  e.padding_right = style->LengthOr(LengthId::kPaddingRight, percent_base, 0);
  e.padding_top = style->LengthOr(LengthId::kPaddingTop, percent_base, 0);
  e.padding_bottom = style->LengthOr(LengthId::kPaddingBottom, percent_base, 0);
  return e;
}

// Margin box size of a laid out box.
struct BoxResult {
  double width = 0;
  double height = 0;
  double margin_bottom = 0;
};

// Size of laid out content.
struct Extent {
  double width = 0;
  double height = 0;
};

class LayoutPass {
  CPPGC_STACK_ALLOCATED();

 public:
  LayoutPass(Document* document, LayoutData* data)
      : window_(document->window()), data_(data) {}

  void Run(Element* root) {
    pending_.emplace_back();
    LayoutElement(root, 0, 0, window_->inner_width, window_->inner_height, kNaN,
                  kNaN, false);
    // Boxes whose containing block is the viewport. Laying one out can add
    // more of them, so iterate by index.
    for (size_t i = 0; i < pending_[0].size(); ++i) {
      Pending pending = pending_[0][i];
      LayoutPositioned(pending, 0, 0, window_->inner_width,
                       window_->inner_height);
    }
  }

 private:
  // An absolutely positioned box waiting for its containing block.
  struct Pending {
    CPPGC_STACK_ALLOCATED();

   public:
    Element* element;
    double static_x;
    double static_y;
  };

  bool IntrinsicSize(const Element* element, double* width, double* height) {
    bool sized_by_default = element->HasTag("canvas") ||
                            element->HasTag("iframe") ||
                            element->HasTag("video");
    if (!sized_by_default && !element->HasTag("img")) return false;
    *width = ParseCSSLength(AttributeValue(element, "width"), kNaN, 16,
                            Viewport::Of(window_))
                 .value_or(sized_by_default ? 300 : 0);
    *height = ParseCSSLength(AttributeValue(element, "height"), kNaN, 16,
                             Viewport::Of(window_))
                  .value_or(sized_by_default ? 150 : 0);
    return true;
  }

  // The margin-box width the element would take with unlimited room.
  double MaxContentWidth(Element* element) {
    auto it = max_content_.find(element);
    if (it != max_content_.end()) return it->second;
    BoxResult result =
        LayoutElement(element, 0, 0, kUnbounded, kNaN, kNaN, kNaN, true);
    max_content_.emplace(element, result.width);
    return result.width;
  }

  // Lays out |element| with its margin box at (x, y). |forced_width| and
  // |forced_height| are margin-box sizes imposed by the parent (flex items,
  // positioned boxes) or NaN. In |measuring| mode boxes with automatic width
  // shrink to their content and nothing wraps.
  BoxResult LayoutElement(Element* element, double x, double y,
                          double available_width, double available_height,
                          double forced_width, double forced_height,
                          bool measuring) {
    const ComputedStyle* style = ComputedStyleFor(element);
    if (!style || style->display == ComputedStyle::Display::kNone) return {};
    if (element->ns() == Namespace::kSVG) {
      return LayoutSVGRoot(element, style, x, y, available_width,
                           available_height);
    }
    if (style->display == ComputedStyle::Display::kContents) {
      // The element has no box of its own; its children take its place.
      Extent content = LayoutFlow(element, style, x, y, available_width,
                                  available_height, measuring);
      LayoutBox& box = data_->boxes[element];
      box = LayoutBox();
      box.x = x;
      box.y = y;
      return {content.width, content.height, 0};
    }

    double percent_base =
        available_width >= kUnbounded ? kNaN : available_width;
    Edges edges = EdgesOf(style, percent_base);
    std::optional<double> css_width =
        style->Length(LengthId::kWidth, percent_base);
    std::optional<double> css_height =
        style->Length(LengthId::kHeight, available_height);
    double intrinsic_width = 0, intrinsic_height = 0;
    bool has_intrinsic =
        IntrinsicSize(element, &intrinsic_width, &intrinsic_height);
    if (has_intrinsic && intrinsic_width > 0 && intrinsic_height > 0) {
      // A replaced element sized in one dimension keeps its aspect ratio.
      if (css_width && !css_height) {
        intrinsic_height = *css_width * intrinsic_height / intrinsic_width;
      } else if (css_height && !css_width) {
        intrinsic_width = *css_height * intrinsic_width / intrinsic_height;
      }
    }

    // Border-box width, or NaN if it follows from the content.
    double width = kNaN;
    if (Definite(forced_width)) {
      width = std::max(0.0, forced_width - edges.horizontal_margins());
    } else if (css_width) {
      width = *css_width + (style->border_box ? 0 : edges.horizontal_inner());
    } else if (has_intrinsic) {
      width = intrinsic_width + edges.horizontal_inner();
    } else if (measuring) {
      // Determined below.
    } else if (style->is_inline_level) {
      width = std::min(MaxContentWidth(element), available_width) -
              edges.horizontal_margins();
    } else {
      width = available_width - edges.horizontal_margins();
    }
    auto clamp_width = [&](double value) {
      if (auto max = style->Length(LengthId::kMaxWidth, percent_base)) {
        value = std::min(
            value, *max + (style->border_box ? 0 : edges.horizontal_inner()));
      }
      if (auto min = style->Length(LengthId::kMinWidth, percent_base)) {
        value = std::max(
            value, *min + (style->border_box ? 0 : edges.horizontal_inner()));
      }
      return std::max(value, 0.0);
    };
    if (Definite(width) && !Definite(forced_width)) width = clamp_width(width);

    // "margin: 0 auto" centres a block of known width.
    if (css_width && !style->is_inline_level && !Definite(forced_width) &&
        !measuring && style->Is("margin-left", "auto") &&
        style->Is("margin-right", "auto")) {
      double room = available_width - width;
      if (room > 0) edges.margin_left = edges.margin_right = room / 2;
    }

    double height = kNaN;
    if (Definite(forced_height)) {
      height = std::max(0.0, forced_height - edges.vertical_margins());
    } else if (css_height) {
      height = *css_height + (style->border_box ? 0 : edges.vertical_inner());
    } else if (has_intrinsic) {
      height = intrinsic_height + edges.vertical_inner();
    }

    double content_x =
        x + edges.margin_left + edges.border_left + edges.padding_left;
    double content_y =
        y + edges.margin_top + edges.border_top + edges.padding_top;
    double content_width =
        Definite(width)
            ? std::max(0.0, width - edges.horizontal_inner())
            : std::max(0.0, available_width - edges.horizontal_margins() -
                                edges.horizontal_inner());
    double content_height = Definite(height)
                                ? std::max(0.0, height - edges.vertical_inner())
                                : kNaN;

    if (style->positioned) pending_.emplace_back();
    Extent content;
    if (!IsReplaced(element)) {
      bool measure_children = measuring || !Definite(width);
      if (style->display == ComputedStyle::Display::kFlex ||
          style->display == ComputedStyle::Display::kTableRow) {
        content = LayoutFlex(element, style, content_x, content_y,
                             content_width, content_height, measure_children);
      } else {
        content = LayoutFlow(element, style, content_x, content_y,
                             content_width, content_height, measure_children);
      }
    }
    if (!Definite(width)) {
      width = clamp_width(content.width + edges.horizontal_inner());
    }
    if (!Definite(height)) height = content.height + edges.vertical_inner();
    if (!Definite(forced_height)) {
      double inner = style->border_box ? 0 : edges.vertical_inner();
      if (auto max = style->Length(LengthId::kMaxHeight, available_height)) {
        height = std::min(height, *max + inner);
      }
      if (auto min = style->Length(LengthId::kMinHeight, available_height)) {
        height = std::max(height, *min + inner);
      }
    }

    LayoutBox& box = data_->boxes[element];
    box.x = x + edges.margin_left;
    box.y = y + edges.margin_top;
    box.width = width;
    box.height = height;
    box.border_left = edges.border_left;
    box.border_top = edges.border_top;
    box.client_width =
        std::max(0.0, width - edges.border_left - edges.border_right);
    box.client_height =
        std::max(0.0, height - edges.border_top - edges.border_bottom);
    box.scroll_width =
        std::max(box.client_width,
                 content.width + edges.padding_left + edges.padding_right);
    box.scroll_height =
        std::max(box.client_height,
                 content.height + edges.padding_top + edges.padding_bottom);

    if (style->positioned) {
      std::vector<Pending> pending = std::move(pending_.back());
      pending_.pop_back();
      double cb_x = box.x + edges.border_left;
      double cb_y = box.y + edges.border_top;
      double cb_width = box.client_width;
      double cb_height = box.client_height;
      for (const Pending& p : pending) {
        LayoutPositioned(p, cb_x, cb_y, cb_width, cb_height);
      }
    }
    return {width + edges.horizontal_margins(),
            height + edges.vertical_margins(), edges.margin_bottom};
  }

  // Width of a run of text as laid out inside an element with |style|.
  static double TextRunWidth(const ComputedStyle* style,
                             std::string_view text) {
    if (style->pre) return CodePoints(text) * style->char_width;
    // Collapse runs of white space.
    double count = 0;
    bool in_space = true;
    bool any = false;
    for (char c : text) {
      if (IsHtmlSpace(c)) {
        if (!in_space) ++count;
        in_space = true;
      } else {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++count;
        in_space = false;
        any = true;
      }
    }
    return any ? count * style->char_width : 0;
  }

  // State of the line box being filled by LayoutFlow.
  struct Line {
    double x = 0;
    double height = 0;
  };

  // Block flow: block-level children stack vertically; inline-level children
  // and text fill line boxes.
  Extent LayoutFlow(Element* parent, const ComputedStyle* style, double x,
                    double y, double width, double height, bool measuring) {
    MOCK_DOM_LOCAL_NODES
    std::vector<Node*> children;
    FlatChildren(parent, &children);
    double cursor_y = y;
    double max_width = 0;
    double previous_margin = 0;
    Line line;
    const bool wraps = !measuring && !style->nowrap && width > 0;

    auto end_line = [&]() {
      if (line.x == 0 && line.height == 0) return;
      cursor_y += line.height;
      max_width = std::max(max_width, line.x);
      line = Line();
      previous_margin = 0;
    };
    auto add_text = [&](double text_width) {
      if (wraps && line.x + text_width > width) {
        double total = line.x + text_width;
        double rows = std::ceil(total / width);
        cursor_y += (rows - 1) * style->line_height;
        max_width = std::max(max_width, width);
        line.x = total - (rows - 1) * width;
      } else {
        line.x += text_width;
      }
      line.height = std::max(line.height, style->line_height);
    };

    for (Node* child : children) {
      if (child->IsText()) {
        std::string text = static_cast<CharacterData*>(child)->data().ToUtf8();
        if (!style->pre) {
          double text_width = TextRunWidth(style, text);
          if (text_width > 0) add_text(text_width);
          continue;
        }
        size_t start = 0;
        while (start <= text.size()) {
          size_t stop = text.find('\n', start);
          bool last = stop == std::string::npos;
          if (last) stop = text.size();
          std::string_view row(text.data() + start, stop - start);
          if (!row.empty() || !last) add_text(TextRunWidth(style, row));
          if (last) break;
          end_line();
          start = stop + 1;
        }
        continue;
      }
      if (!child->IsElement()) continue;
      Element* element = static_cast<Element*>(child);
      const ComputedStyle* child_style = ComputedStyleFor(element);
      if (!child_style ||
          child_style->display == ComputedStyle::Display::kNone) {
        continue;
      }
      if (child_style->out_of_flow) {
        if (!measuring) {
          Pending pending{element, x + line.x, cursor_y};
          (child_style->fixed ? pending_.front() : pending_.back())
              .push_back(pending);
        }
        continue;
      }
      if (element->HasTag("br")) {
        LayoutBox& box = data_->boxes[element];
        box = LayoutBox();
        box.x = x + line.x;
        box.y = cursor_y;
        box.height = style->line_height;
        line.height = std::max(line.height, style->line_height);
        end_line();
        continue;
      }
      if (child_style->is_inline_level) {
        BoxResult result = LayoutElement(element, x + line.x, cursor_y, width,
                                         height, kNaN, kNaN, measuring);
        if (wraps && line.x > 0 && line.x + result.width > width) {
          end_line();
          result = LayoutElement(element, x, cursor_y, width, height, kNaN,
                                 kNaN, measuring);
        }
        line.x += result.width;
        line.height = std::max(line.height, result.height);
        continue;
      }
      end_line();
      // Adjacent vertical margins of siblings collapse.
      double margin_top = child_style->LengthOr(LengthId::kMarginTop, width, 0);
      double overlap = std::max(0.0, std::min(previous_margin, margin_top));
      BoxResult result = LayoutElement(element, x, cursor_y - overlap, width,
                                       height, kNaN, kNaN, measuring);
      cursor_y += result.height - overlap;
      previous_margin = result.margin_bottom;
      max_width = std::max(max_width, result.width);
    }
    end_line();
    return {max_width, cursor_y - y};
  }

  struct FlexItem {
    CPPGC_STACK_ALLOCATED();

   public:
    Element* element = nullptr;  // null for a run of text
    const ComputedStyle* style = nullptr;
    double base = 0;  // margin-box size on the main axis
    double size = 0;
    double grow = 0;
    double shrink = 1;
  };

  void QueuePositioned(Element* element, const ComputedStyle* style, double x,
                       double y) {
    Pending pending{element, x, y};
    (style->fixed ? pending_.front() : pending_.back()).push_back(pending);
  }

  Extent LayoutFlex(Element* parent, const ComputedStyle* style, double x,
                    double y, double width, double height, bool measuring) {
    MOCK_DOM_LOCAL_NODES
    std::vector<Node*> children;
    FlatChildren(parent, &children);
    const bool is_table_row =
        style->display == ComputedStyle::Display::kTableRow;
    const bool column = style->flex_column && !is_table_row;
    std::vector<FlexItem> items;
    for (Node* child : children) {
      FlexItem item;
      if (child->IsText()) {
        std::string text = static_cast<CharacterData*>(child)->data().ToUtf8();
        item.base = TextRunWidth(style, text);
        if (item.base == 0) continue;
        if (column) item.base = style->line_height;
        item.shrink = 0;
        items.push_back(item);
        continue;
      }
      if (!child->IsElement()) continue;
      item.element = static_cast<Element*>(child);
      item.style = ComputedStyleFor(item.element);
      if (!item.style || item.style->display == ComputedStyle::Display::kNone) {
        continue;
      }
      if (item.style->out_of_flow) {
        if (!measuring) QueuePositioned(item.element, item.style, x, y);
        continue;
      }
      item.grow = is_table_row ? 1 : NumberOr(item.style, "flex-grow", 0);
      item.shrink = NumberOr(item.style, "flex-shrink", 1);
      items.push_back(item);
    }
    return column ? FlexColumn(style, &items, x, y, width, height, measuring)
                  : FlexRow(style, &items, x, y, width, height, measuring);
  }

  // Distributes |room| over [begin, end) by flex-grow, or takes the overflow
  // away by flex-shrink.
  static void ResolveFlexSizes(std::vector<FlexItem>* items, size_t begin,
                               size_t end, double room) {
    double total = 0, grow = 0, shrink = 0;
    for (size_t i = begin; i < end; ++i) {
      FlexItem& item = (*items)[i];
      item.size = item.base;
      total += item.base;
      grow += item.grow;
      shrink += item.shrink * item.base;
    }
    double free_space = room - total;
    for (size_t i = begin; i < end; ++i) {
      FlexItem& item = (*items)[i];
      if (free_space > 0 && grow > 0) {
        item.size += free_space * item.grow / grow;
      } else if (free_space < 0 && shrink > 0) {
        item.size = std::max(
            0.0, item.size + free_space * item.shrink * item.base / shrink);
      }
    }
  }

  Extent FlexRow(const ComputedStyle* style, std::vector<FlexItem>* items,
                 double x, double y, double width, double height,
                 bool measuring) {
    const double gap = style->LengthOr(LengthId::kColumnGap, width, 0);
    const double row_gap = style->LengthOr(LengthId::kRowGap, height, 0);
    const std::string* wrap_value = style->Get("flex-wrap");
    const bool wraps =
        !measuring && wrap_value && wrap_value->substr(0, 4) == "wrap";
    const std::string* align = style->Get("align-items");
    const bool stretch = !align || *align == "normal" || *align == "stretch";

    for (FlexItem& item : *items) {
      if (!item.element) continue;
      Edges edges = EdgesOf(item.style, width);
      double extra = edges.horizontal_margins() +
                     (item.style->border_box ? 0 : edges.horizontal_inner());
      std::optional<double> basis =
          item.style->Length(LengthId::kFlexBasis, width);
      if (!basis) basis = item.style->Length(LengthId::kWidth, width);
      item.base = basis ? *basis + extra : MaxContentWidth(item.element);
    }

    Extent extent;
    double cursor_y = y;
    size_t begin = 0;
    while (begin < items->size()) {
      // Find the end of this flex line.
      size_t end = begin;
      double used = 0;
      while (end < items->size()) {
        double next = used + (*items)[end].base + (end > begin ? gap : 0);
        if (wraps && end > begin && next > width) break;
        used = next;
        ++end;
      }
      double gaps = gap * static_cast<double>(end - begin - 1);
      if (measuring) {
        for (size_t i = begin; i < end; ++i)
          (*items)[i].size = (*items)[i].base;
      } else {
        ResolveFlexSizes(items, begin, end, width - gaps);
      }
      bool single_line = begin == 0 && end == items->size();
      double cursor_x = x;
      double line_height = 0;
      for (size_t i = begin; i < end; ++i) {
        FlexItem& item = (*items)[i];
        if (!item.element) {
          line_height = std::max(line_height, style->line_height);
        } else {
          bool stretches = stretch && single_line && Definite(height) &&
                           !item.style->Length(LengthId::kHeight, height);
          BoxResult result =
              LayoutElement(item.element, cursor_x, cursor_y, width, height,
                            item.size, stretches ? height : kNaN, measuring);
          line_height = std::max(line_height, result.height);
        }
        cursor_x += item.size + gap;
      }
      extent.width = std::max(extent.width, cursor_x - x - gap);
      cursor_y += line_height + (end < items->size() ? row_gap : 0);
      begin = end;
    }
    extent.height = cursor_y - y;
    return extent;
  }

  Extent FlexColumn(const ComputedStyle* style, std::vector<FlexItem>* items,
                    double x, double y, double width, double height,
                    bool measuring) {
    const double gap = style->LengthOr(LengthId::kRowGap, height, 0);
    const std::string* align = style->Get("align-items");
    const bool stretch = !align || *align == "normal" || *align == "stretch";
    // Items stretch across the container unless they have a width.
    auto item_width = [&](const FlexItem& item) {
      return stretch && !measuring &&
                     !item.style->Length(LengthId::kWidth, width)
                 ? width
                 : kNaN;
    };

    // First pass: natural heights.
    Extent extent;
    double cursor_y = y;
    for (FlexItem& item : *items) {
      if (item.element) {
        BoxResult result = LayoutElement(item.element, x, cursor_y, width, kNaN,
                                         item_width(item), kNaN, measuring);
        item.base = result.height;
        extent.width = std::max(extent.width, result.width);
      } else {
        extent.width = std::max(extent.width, item.base);
        item.base = style->line_height;
      }
      item.size = item.base;
      cursor_y += item.base + gap;
    }
    if (!items->empty()) cursor_y -= gap;
    extent.height = cursor_y - y;
    if (measuring || !Definite(height) || items->empty()) return extent;

    // With a definite height, items grow into the free space, and items that
    // may be smaller than their content (scroll containers, or an explicit
    // min-height) shrink to fit.
    bool changed = false;
    for (FlexItem& item : *items) {
      if (!item.element) continue;
      Edges edges = EdgesOf(item.style, width);
      if (auto basis = item.style->Length(LengthId::kFlexBasis, height)) {
        double basis_size =
            *basis + edges.vertical_margins() +
            (item.style->border_box ? 0 : edges.vertical_inner());
        bool may_be_smaller = item.style->scroll_container ||
                              item.style->Get("min-height") != nullptr;
        if (basis_size > item.base || may_be_smaller) item.base = basis_size;
      }
      if (!item.style->scroll_container && !item.style->Get("min-height")) {
        item.shrink = 0;
      }
    }
    std::vector<double> natural;
    for (const FlexItem& item : *items) natural.push_back(item.size);
    ResolveFlexSizes(items, 0, items->size(),
                     height - gap * static_cast<double>(items->size() - 1));
    for (size_t i = 0; i < items->size(); ++i) {
      if (std::abs((*items)[i].size - natural[i]) > 0.01) changed = true;
    }
    if (!changed) return extent;

    // Second pass with the resolved heights.
    cursor_y = y;
    for (FlexItem& item : *items) {
      if (item.element) {
        LayoutElement(item.element, x, cursor_y, width, kNaN, item_width(item),
                      item.size, measuring);
      }
      cursor_y += item.size + gap;
    }
    extent.height = cursor_y - gap - y;
    return extent;
  }

  // Lays out an absolutely positioned box against its containing block.
  void LayoutPositioned(const Pending& pending, double cb_x, double cb_y,
                        double cb_width, double cb_height) {
    Element* element = pending.element;
    const ComputedStyle* style = ComputedStyleFor(element);
    if (!style) return;
    std::optional<double> left = style->Length(LengthId::kLeft, cb_width);
    std::optional<double> right = style->Length(LengthId::kRight, cb_width);
    std::optional<double> top = style->Length(LengthId::kTop, cb_height);
    std::optional<double> bottom = style->Length(LengthId::kBottom, cb_height);

    double forced_width = kNaN;
    if (!style->Length(LengthId::kWidth, cb_width)) {
      double room = cb_width - left.value_or(0) - right.value_or(0);
      forced_width =
          left && right ? room : std::min(MaxContentWidth(element), room);
    }
    double forced_height = kNaN;
    if (!style->Length(LengthId::kHeight, cb_height) && top && bottom) {
      forced_height = cb_height - *top - *bottom;
    }

    double x = left ? cb_x + *left : pending.static_x;
    double y = top ? cb_y + *top : pending.static_y;
    BoxResult result = LayoutElement(element, x, y, cb_width, cb_height,
                                     forced_width, forced_height, false);
    // Boxes anchored to the right or bottom edge can only be placed once
    // their size is known.
    bool moved = false;
    if (!left && right) {
      x = cb_x + cb_width - *right - result.width;
      moved = true;
    }
    if (!top && bottom) {
      y = cb_y + cb_height - *bottom - result.height;
      moved = true;
    }
    if (moved) {
      LayoutElement(element, x, y, cb_width, cb_height, forced_width,
                    forced_height, false);
    }
  }

  double SVGNumber(const Element* element, const char* attribute,
                   double percent_base, double fallback) {
    return ParseCSSLength(AttributeValue(element, attribute), percent_base, 16,
                          Viewport::Of(window_))
        .value_or(fallback);
  }

  // An <svg> element is a replaced inline box; what is inside it is placed
  // by attributes, not by CSS layout.
  BoxResult LayoutSVGRoot(Element* element, const ComputedStyle* style,
                          double x, double y, double available_width,
                          double available_height) {
    double percent_base =
        available_width >= kUnbounded ? kNaN : available_width;
    double width =
        style->Length(LengthId::kWidth, percent_base)
            .value_or(SVGNumber(element, "width", percent_base, 300));
    double height =
        style->Length(LengthId::kHeight, available_height)
            .value_or(SVGNumber(element, "height", available_height, 150));
    LayoutSVG(element, x, y, width, height, true);
    return {width, height, 0};
  }

  void LayoutSVG(Element* element, double x, double y, double viewport_width,
                 double viewport_height, bool is_root) {
    const AtomicString& tag = element->local_name();
    LayoutBox box;
    box.x = x;
    box.y = y;
    auto number = [&](const char* attribute, double base) {
      return SVGNumber(element, attribute, base, 0);
    };
    if (is_root) {
      box.width = viewport_width;
      box.height = viewport_height;
    } else if (tag.EqualsAscii("svg") || tag.EqualsAscii("rect") ||
               tag.EqualsAscii("image") || tag.EqualsAscii("foreignObject") ||
               tag.EqualsAscii("use")) {
      box.x = x + number("x", viewport_width);
      box.y = y + number("y", viewport_height);
      box.width = number("width", viewport_width);
      box.height = number("height", viewport_height);
    } else if (tag.EqualsAscii("circle")) {
      double r = number("r", viewport_width);
      box.x = x + number("cx", viewport_width) - r;
      box.y = y + number("cy", viewport_height) - r;
      box.width = box.height = 2 * r;
    } else if (tag.EqualsAscii("ellipse")) {
      double rx = number("rx", viewport_width);
      double ry = number("ry", viewport_height);
      box.x = x + number("cx", viewport_width) - rx;
      box.y = y + number("cy", viewport_height) - ry;
      box.width = 2 * rx;
      box.height = 2 * ry;
    } else if (tag.EqualsAscii("line")) {
      double x1 = number("x1", viewport_width);
      double x2 = number("x2", viewport_width);
      double y1 = number("y1", viewport_height);
      double y2 = number("y2", viewport_height);
      box.x = x + std::min(x1, x2);
      box.y = y + std::min(y1, y2);
      box.width = std::abs(x2 - x1);
      box.height = std::abs(y2 - y1);
    } else if (tag.EqualsAscii("text") || tag.EqualsAscii("tspan")) {
      box.x = x + number("x", viewport_width);
      box.y = y + number("y", viewport_height) - kLineHeight;
      box.width = TextWidth(element->TextContent());
      box.height = kLineHeight;
    }
    box.client_width = box.scroll_width = box.width;
    box.client_height = box.scroll_height = box.height;
    data_->boxes[element] = box;

    // Children are placed relative to this element's origin: the box of a
    // nested viewport, plus any translate() in the transform attribute.
    double origin_x = is_root || tag.EqualsAscii("svg") ? box.x : x;
    double origin_y = is_root || tag.EqualsAscii("svg") ? box.y : y;
    std::string transform = AttributeValue(element, "transform");
    size_t translate = transform.find("translate(");
    if (translate != std::string::npos) {
      const char* p = transform.c_str() + translate + 10;
      char* end = nullptr;
      origin_x += std::strtod(p, &end);
      while (*end == ',' || *end == ' ') ++end;
      if (*end != ')') origin_y += std::strtod(end, nullptr);
    }
    for (Node* child = element->first(); child; child = child->next_sibling()) {
      if (!child->IsElement()) continue;
      Element* child_element = static_cast<Element*>(child);
      if (child_element->ns() != Namespace::kSVG) {
        // HTML inside <foreignObject>.
        LayoutElement(child_element, box.x, box.y, box.width, box.height, kNaN,
                      kNaN, false);
      } else {
        LayoutSVG(child_element, origin_x, origin_y, viewport_width,
                  viewport_height, false);
      }
    }
  }

  Window* window_;
  LayoutData* data_;
  absl::flat_hash_map<const void*, double> max_content_;
  // One list per positioned ancestor being laid out, innermost last. The
  // first list is for the viewport.
  std::vector<std::vector<Pending>> pending_;
};

LayoutData* DataFor(Document* document) {
  if (!document->layout_data) {
    document->layout_data = std::make_shared<LayoutData>();
  }
  LayoutData* data = static_cast<LayoutData*>(document->layout_data.get());
  uint64_t key = StyleKey(document);
  if (data->key != key) {
    data->boxes.clear();
    data->key = key;
    data->done = false;
  }
  return data;
}

}  // namespace

const LayoutBox& LayoutBoxFor(Element* element) {
  static const LayoutBox* empty = new LayoutBox();
  Document* document = element->document();
  if (!element->is_connected() || !document->default_view()) return *empty;
  LayoutData* data = DataFor(document);
  if (!data->done) {
    data->done = true;
    if (Element* root = document->document_element()) {
      LayoutPass(document, data).Run(root);
    }
  }
  auto it = data->boxes.find(element);
  return it == data->boxes.end() ? *empty : it->second;
}

LayoutRect BoundingClientRect(Element* element) {
  const LayoutBox& box = LayoutBoxFor(element);
  return {box.x, box.y, box.width, box.height};
}

double TextWidth(const String& text) {
  return CodePoints(text.ToUtf8()) * kCharWidth;
}

void FontMetrics(Element* element, double* char_width, double* line_height) {
  const ComputedStyle* style = element ? ComputedStyleFor(element) : nullptr;
  *char_width = style ? style->char_width : kCharWidth;
  *line_height = style ? style->line_height : kLineHeight;
}

namespace {

String Pixels(double value) {
  char buffer[32];
  if (value == std::floor(value)) {
    snprintf(buffer, sizeof(buffer), "%.0fpx", value);
  } else {
    snprintf(buffer, sizeof(buffer), "%gpx", value);
  }
  return String::FromLatin1(buffer);
}

bool IsLengthProperty(std::string_view name) {
  static const char* const kPrefixes[] = {"margin-", "padding-", "min-",
                                          "max-"};
  for (const char* prefix : kPrefixes) {
    if (name.substr(0, strlen(prefix)) == prefix) return true;
  }
  if (name.size() > 6 && name.substr(0, 7) == "border-" &&
      name.substr(name.size() - 6) == "-width") {
    return true;
  }
  return name == "top" || name == "left" || name == "right" ||
         name == "bottom" || name == "row-gap" || name == "column-gap" ||
         name == "flex-basis" || name == "text-indent" ||
         name == "letter-spacing" || name == "word-spacing";
}

}  // namespace

String ComputedStyleValue(Element* element, const AtomicString& property) {
  const ComputedStyle* style = ComputedStyleFor(element);
  if (!style || !property.is_8bit()) return String::Empty();
  std::string_view name = property.view8();
  using Display = ComputedStyle::Display;

  if (name == "display") return String::FromUtf8(style->display_keyword);
  if (name == "font-size") return Pixels(style->font_size);
  if (name == "line-height") {
    const std::string* value = style->Get("line-height");
    return !value || *value == "normal" ? String::FromLatin1("normal")
                                        : Pixels(style->line_height);
  }
  if (name == "width" || name == "height") {
    bool is_width = name == "width";
    bool has_box = style->display != Display::kNone &&
                   !(style->display == Display::kInline &&
                     element->ns() != Namespace::kSVG);
    if (!has_box) {
      const std::string* value = style->Get(name);
      return value ? String::FromUtf8(*value) : String::FromLatin1("auto");
    }
    const LayoutBox& box = LayoutBoxFor(element);
    double size = is_width ? box.width : box.height;
    if (!style->border_box) {
      // The used content-box size.
      static const char* const kHorizontal[] = {"padding-left", "padding-right",
                                                "border-left-width",
                                                "border-right-width"};
      static const char* const kVertical[] = {"padding-top", "padding-bottom",
                                              "border-top-width",
                                              "border-bottom-width"};
      for (const char* edge : is_width ? kHorizontal : kVertical) {
        size -= style->Length(edge, box.width).value_or(0);
      }
    }
    return Pixels(std::max(0.0, size));
  }
  if (name == "margin" || name == "padding") {
    std::string out;
    for (const char* side : {"-top", "-right", "-bottom", "-left"}) {
      std::string longhand = std::string(name) + side;
      if (!out.empty()) out.push_back(' ');
      out +=
          ComputedStyleValue(element, AtomicString(std::string_view(longhand)))
              .ToUtf8();
    }
    return String::FromUtf8(out);
  }
  if (const std::string* value = style->Get(name)) {
    if (IsLengthProperty(name)) {
      // Lengths are reported as used pixel values. Percentages that need
      // the containing block stay as written.
      if (auto length = style->Length(name, std::nan(""))) {
        return Pixels(*length);
      }
    }
    return String::FromUtf8(*value);
  }

  // Initial values for the properties pages commonly read. Anything that is
  // not listed reads as "", which is what a browser returns for a property
  // it does not know.
  static const struct {
    const char* name;
    const char* value;
  } kInitialValues[] = {
      {"position", "static"},
      {"visibility", "visible"},
      {"opacity", "1"},
      {"float", "none"},
      {"clear", "none"},
      {"overflow", "visible"},
      {"overflow-x", "visible"},
      {"overflow-y", "visible"},
      {"box-sizing", "content-box"},
      {"font-size", "16px"},
      {"font-family", "serif"},
      {"font-weight", "400"},
      {"font-style", "normal"},
      {"font-variant", "normal"},
      {"font-stretch", "100%"},
      {"font", "16px serif"},
      {"line-height", "normal"},
      {"letter-spacing", "normal"},
      {"word-spacing", "0px"},
      {"text-align", "start"},
      {"text-transform", "none"},
      {"text-indent", "0px"},
      {"text-decoration", "none"},
      {"white-space", "normal"},
      {"word-break", "normal"},
      {"overflow-wrap", "normal"},
      {"tab-size", "8"},
      {"direction", "ltr"},
      {"unicode-bidi", "normal"},
      {"writing-mode", "horizontal-tb"},
      {"color", "rgb(0, 0, 0)"},
      {"background-color", "rgba(0, 0, 0, 0)"},
      {"background-image", "none"},
      {"border-top-width", "0px"},
      {"border-right-width", "0px"},
      {"border-bottom-width", "0px"},
      {"border-left-width", "0px"},
      {"border-top-style", "none"},
      {"border-right-style", "none"},
      {"border-bottom-style", "none"},
      {"border-left-style", "none"},
      {"border-collapse", "separate"},
      {"border-radius", "0px"},
      {"margin-top", "0px"},
      {"margin-right", "0px"},
      {"margin-bottom", "0px"},
      {"margin-left", "0px"},
      {"padding-top", "0px"},
      {"padding-right", "0px"},
      {"padding-bottom", "0px"},
      {"padding-left", "0px"},
      {"top", "auto"},
      {"right", "auto"},
      {"bottom", "auto"},
      {"left", "auto"},
      {"z-index", "auto"},
      {"min-width", "0px"},
      {"min-height", "0px"},
      {"max-width", "none"},
      {"max-height", "none"},
      {"transform", "none"},
      {"transform-origin", "0px 0px"},
      {"transition", "all 0s ease 0s"},
      {"transition-duration", "0s"},
      {"transition-delay", "0s"},
      {"transition-property", "all"},
      {"animation", "none 0s ease 0s 1 normal none running"},
      {"animation-name", "none"},
      {"animation-duration", "0s"},
      {"animation-delay", "0s"},
      {"cursor", "auto"},
      {"pointer-events", "auto"},
      {"user-select", "auto"},
      {"content", "normal"},
      {"zoom", "1"},
      {"vertical-align", "baseline"},
      {"list-style-type", "disc"},
      {"table-layout", "auto"},
      {"flex-direction", "row"},
      {"flex-wrap", "nowrap"},
      {"align-items", "normal"},
      {"justify-content", "normal"},
      {"gap", "normal"},
      {"scroll-behavior", "auto"},
      {"resize", "none"},
      {"outline-width", "0px"},
      {"outline-style", "none"},
      {"fill", "rgb(0, 0, 0)"},
      {"stroke", "none"},
      {"stroke-width", "1px"},
  };
  for (const auto& initial : kInitialValues) {
    if (name == initial.name) return String::FromLatin1(initial.value);
  }
  return String::Empty();
}

}  // namespace mock_dom
}  // namespace v8

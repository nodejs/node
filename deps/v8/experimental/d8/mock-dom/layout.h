// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_D8_MOCK_DOM_LAYOUT_H_
#define V8_EXPERIMENTAL_D8_MOCK_DOM_LAYOUT_H_

#include "experimental/d8/mock-dom/dom.h"

namespace v8 {
namespace mock_dom {

// A simplified layout engine on top of the cascade in css.h. Pages read
// geometry back and size their work by it (how many lines an editor renders,
// how many ticks a chart draws), so the numbers have to follow from the
// page's own CSS, even if they are not pixel-exact.
//
// Implemented: box model, block flow with line boxes, shrink-to-fit,
// flexbox (direction, grow, shrink, basis, wrap, gap, stretch), absolute and
// fixed positioning, table rows as flex rows, shadow trees and slots,
// replaced elements, attribute-driven SVG shapes with translate().
//
// Not implemented: real fonts (each character is 0.5em wide, 0.6em for
// monospace), grid (laid out as blocks), margin collapsing other than
// between siblings, relative offsets, transforms on HTML elements,
// text-align and flex alignment other than stretch.

struct LayoutRect {
  double x = 0;
  double y = 0;
  double width = 0;
  double height = 0;
};

struct LayoutBox {
  // Border box, in viewport coordinates.
  double x = 0;
  double y = 0;
  double width = 0;
  double height = 0;
  // Padding box size.
  double client_width = 0;
  double client_height = 0;
  // Size of the content including overflow.
  double scroll_width = 0;
  double scroll_height = 0;
  double border_left = 0;
  double border_top = 0;
};

// Metrics of the default 16px font.
constexpr double kLineHeight = 18.4;
constexpr double kCharWidth = 8;

// Elements that are not rendered (disconnected, display: none, or in a
// document without a window) have an all-zero box.
const LayoutBox& LayoutBoxFor(Element* element);
LayoutRect BoundingClientRect(Element* element);
// Width of |text| on one line in the default font.
double TextWidth(const String& text);
// Character advance and line height of text inside |element|.
void FontMetrics(Element* element, double* char_width, double* line_height);

// The value getComputedStyle() reports for |property| (lower-case, dashed).
String ComputedStyleValue(Element* element, const AtomicString& property);

}  // namespace mock_dom
}  // namespace v8

#endif  // V8_EXPERIMENTAL_D8_MOCK_DOM_LAYOUT_H_

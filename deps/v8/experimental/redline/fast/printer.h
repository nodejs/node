// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_FAST_PRINTER_H_
#define V8_EXPERIMENTAL_REDLINE_FAST_PRINTER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "experimental/redline/fast/ast.h"

namespace redline {
namespace fast {

struct PrintOptions {
  // Newlines and two-space indentation (otherwise everything on one line).
  bool pretty = true;
  // Append `// nN` comments with node ids after each statement.
  bool node_ids = false;
};

// Output byte range of a printed node.
struct NodeSpan {
  uint32_t node_id = 0;
  int32_t start = 0;
  int32_t end = 0;
};

// Prints a FAST subtree as JavaScript source. The output re-parses to a tree
// with identical semantics (parentheses are derived from precedence, ASI
// hazards are avoided by always emitting semicolons).
std::string Print(const Node* node,
                  const PrintOptions& options = PrintOptions());
std::string Print(const Node* node, const PrintOptions& options,
                  std::vector<NodeSpan>* spans);

// Quotes a cooked (WTF-8) string as a double-quoted JS string literal.
std::string QuoteJsString(std::string_view value);
// Shortest round-trip representation of a double as a JS numeric literal
// (negative values get a leading '-').
std::string NumberToJs(double value);

}  // namespace fast
}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_FAST_PRINTER_H_

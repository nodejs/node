// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_FAST_REQUIREMENTS_H_
#define V8_EXPERIMENTAL_REDLINE_FAST_REQUIREMENTS_H_

#include <string_view>
#include <vector>

#include "experimental/redline/fast/ast.h"

namespace redline {
namespace fast {

// Describes what surrounding context a FAST subtree needs to stay valid.
struct Requirements {
  bool needs_function = false;
  bool needs_async = false;
  bool needs_generator = false;
  bool needs_break = false;
  bool needs_continue = false;
  bool needs_sloppy = false;
  bool needs_this = false;
  bool needs_arguments = false;
  bool needs_super = false;
  bool needs_new_target = false;
  bool has_external_label = false;
  bool has_lexical_declaration = false;
  // Lists labels declared by `node` outside nested functions.
  std::vector<std::string_view> declared_labels;
};

// Scans `node` and returns the syntactic requirements of moving it.
Requirements AnalyzeRequirements(Node* node);

}  // namespace fast
}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_FAST_REQUIREMENTS_H_

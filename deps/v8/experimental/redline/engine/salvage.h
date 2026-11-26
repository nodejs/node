// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_SALVAGE_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_SALVAGE_H_

#include <cstddef>
#include <string_view>
#include <vector>

#include "experimental/redline/exec/execution.h"
#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/fast/tree.h"

namespace redline {

// A statement inside its owning statement list (`Program`, `BlockStatement`,
// `SwitchCase` or `StaticBlock`).
struct StatementLocation {
  std::vector<fast::Statement*>* list = nullptr;
  size_t index = 0;
  fast::Node* owner = nullptr;

  fast::Statement* statement() const {
    return list != nullptr && index < list->size() ? (*list)[index] : nullptr;
  }
  bool valid() const { return statement() != nullptr; }
};

// Maps a 1-based line number in `source` (printed with `spans`) to a covering
// statement in a statement list. With `top_level`, returns the covering
// statement in `Program::body`. Otherwise returns the smallest covering
// statement in any statement list.
StatementLocation FindCoveringStatement(
    fast::Tree* tree, std::string_view source,
    const std::vector<fast::NodeSpan>& spans, int target_line, bool top_level);

// Wraps the statement at `loc` in `try { ... } catch {}`. Splits simple `let`
// or `const` declarations with initializers into `let x` and a guarded
// assignment so downstream references stay valid. Returns false without
// changing `tree` if wrapping is unsafe or unnecessary.
bool WrapStatementForSalvage(fast::Tree* tree, const StatementLocation& loc);

// Re-prints `tree` with spans and wraps the statement indicated by `lines`.
// Tries the outermost top-level statement first when `prefer_top_level` is
// true, then falls back to the other frame if wrapping fails. Returns true
// when `tree` changed.
bool SalvageException(fast::Tree* tree, const ExceptionLines& lines,
                      bool prefer_top_level);

// Whether the throw site and the outermost program frame of `lines` are
// directly in the last statement of `Program::body` of `tree`, not in a
// nested statement list. Then no wrap of SalvageException() can run code
// that the failed run did not run. If the failed run also found no new
// edges, the worker does not run a salvage retry (Worker::FuzzOne).
bool ThrowsInLastTopLevelStatement(fast::Tree* tree,
                                   const ExceptionLines& lines);

// Configured probabilities from the `salvage.*` tunables.
double SalvageChance();
double SalvageToplevelChance();

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_SALVAGE_H_

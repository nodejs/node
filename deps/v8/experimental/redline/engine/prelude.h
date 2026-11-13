// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_PRELUDE_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_PRELUDE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "experimental/redline/fast/tree.h"

namespace redline {

// JavaScript prepended to every program when it is executed (never part of
// the mutable program itself). The default prelude defines the mjsunit
// assertion API as non-throwing stubs so that imported mjsunit tests and
// their descendants keep running (and covering code) instead of failing on
// the first assertion.
std::string_view DefaultPrelude();

// The script that the target runs for a program, the prelude, a new line and
// the program, parsed into one tree. The crash minimizer reduces this tree.
// Thus it can also remove the prelude statements that a crash does not need
// (DESIGN.md 5.6). The tree prints to a script that is equivalent to the
// script of the target, but not always the same text.
class ScriptTree {
 public:
  // Parses `prelude` + "\n" + `program`, or only `program` if `prelude` is
  // empty. Returns nullptr and sets `error` if the prelude or the program
  // does not parse alone. Also if the top-level statements of the script are
  // not the statements of the prelude, then the statements of the program.
  static std::unique_ptr<ScriptTree> Parse(std::string_view prelude,
                                           std::string_view program,
                                           std::string* error);

  fast::Tree* tree() { return tree_.get(); }
  const fast::Tree* tree() const { return tree_.get(); }
  // The top-level statements of the prelude in the parsed script.
  size_t prelude_statements() const { return prelude_statements_; }
  // The number of top-level statements of the tree that came from the
  // prelude, at the time of the call.
  size_t PreludeStatementsNow() const;
  // The number of nodes of the program part of the tree: the root and the
  // top-level statements that came from the program, with their subtrees.
  size_t ProgramNodes() const;
  // Prints the program part of the tree. A statement that a reducer made
  // counts as a program statement.
  std::string PrintProgram();

 private:
  ScriptTree() = default;
  bool FromPrelude(const fast::Node* statement) const {
    return statement->id < first_program_id_;
  }

  std::unique_ptr<fast::Tree> tree_;
  size_t prelude_statements_ = 0;
  // The parser gives the ids in the order of the source text, and the root
  // has id 0. The nodes of the prelude have lower ids than this id. The
  // nodes of the program have this id or higher ids.
  uint32_t first_program_id_ = 1;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_PRELUDE_H_

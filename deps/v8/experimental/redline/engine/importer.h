// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_IMPORTER_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_IMPORTER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/fast/tree.h"

namespace redline {

struct ImportOptions {
  // Directory that load("test/mjsunit/...") paths are relative to
  // (usually the V8 checkout). Empty: the current directory.
  std::string root;
  // Nesting limit for inlined files that load other files.
  int max_depth = 4;
};

struct ImportInfo {
  std::vector<std::string> inlined;  // files whose statements were spliced in
  std::vector<std::string> dropped;  // load() calls that could not be resolved
};

// Parses a test file into a FAST tree for the corpus. `ImportSource` replaces
// top-level `load("f")`, `d8.file.execute("f")` and Test262 `includes:` files
// with the statements of `f`. It removes unresolved loads and loads of
// `mjsunit.js`, `sta.js` or `assert.js` because the prelude provides their
// API. Returns `nullptr` with `error` on a parse error or an excluded Test262
// file.
std::unique_ptr<fast::Tree> ImportSource(std::string_view source,
                                         const std::string& path,
                                         const ImportOptions& options,
                                         ImportInfo* info, std::string* error);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_IMPORTER_H_

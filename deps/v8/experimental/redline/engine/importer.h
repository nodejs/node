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

// The comment of a saved corpus file that holds the d8 flags of the program
// (CorpusFileHeader in engine/fuzzer.h). It is not a `// Flags:` line: the
// flags of a program that Redline saved do not go into the imported flags.
inline constexpr char kRedlineFlagsPrefix[] = "// redline-flags:";

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
  // The d8 flags of the `// Flags:` lines of the file, after
  // SanitizeTestFlags (engine/d8_flags.h), and the flags that it rejected.
  std::vector<std::string> flags;
  std::vector<std::string> rejected_flags;
  // The d8 flags of the kRedlineFlagsPrefix lines, after SanitizeTestFlags.
  // They become Program::flags, but not imported flags.
  std::vector<std::string> program_flags;
};

// Parses a test file into a FAST tree for the corpus. `ImportSource` replaces
// top-level `load("f")`, `d8.file.execute("f")` and Test262 `includes:` files
// with the statements of `f`. It removes unresolved loads and loads of
// `mjsunit.js`, `sta.js` or `assert.js` because the prelude provides their
// API. It reads the d8 flags of the `// Flags:` lines and of the
// kRedlineFlagsPrefix lines into `info`. Returns
// `nullptr` with `error` on a parse error or an excluded Test262 file.
std::unique_ptr<fast::Tree> ImportSource(std::string_view source,
                                         const std::string& path,
                                         const ImportOptions& options,
                                         ImportInfo* info, std::string* error);

// The flags of all `// Flags:` lines of `source`, in their order, without
// a filter.
std::vector<std::string> ParseFlagsLines(std::string_view source);
// The flags of all kRedlineFlagsPrefix lines of `source`, without a filter.
std::vector<std::string> ParseRedlineFlagsLines(std::string_view source);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_IMPORTER_H_

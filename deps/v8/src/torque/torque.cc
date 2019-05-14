// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/torque-compiler.h"

namespace v8 {
namespace internal {
namespace torque {

int WrappedMain(int argc, const char** argv) {
  std::string output_directory;
  bool verbose = false;
  std::vector<std::string> files;

  for (int i = 1; i < argc; ++i) {
    // Check for options
    if (!strcmp("-o", argv[i])) {
      output_directory = argv[++i];
      continue;
    }
    if (!strcmp("-v", argv[i])) {
      verbose = true;
      continue;
    }

    // Otherwise it's a .tq file. Remember it for compilation.
    files.emplace_back(argv[i]);
  }

  SourceFileMap::Scope source_file_map_scope;

  TorqueCompilerOptions options;
  options.output_directory = output_directory;
  options.verbose = verbose;
  options.collect_language_server_data = false;
  options.abort_on_lint_errors = true;

  CompileTorque(files, options);

  return 0;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8

int main(int argc, const char** argv) {
  return v8::internal::torque::WrappedMain(argc, argv);
}

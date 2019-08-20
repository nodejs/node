// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/source-positions.h"
#include "src/torque/torque-compiler.h"

namespace v8 {
namespace internal {
namespace torque {

std::string ErrorPrefixFor(TorqueMessage::Kind kind) {
  switch (kind) {
    case TorqueMessage::Kind::kError:
      return "Torque Error";
    case TorqueMessage::Kind::kLint:
      return "Lint error";
  }
}

int WrappedMain(int argc, const char** argv) {
  std::string output_directory;
  std::string v8_root;
  std::vector<std::string> files;

  for (int i = 1; i < argc; ++i) {
    // Check for options
    if (std::string(argv[i]) == "-o") {
      output_directory = argv[++i];
    } else if (std::string(argv[i]) == "-v8-root") {
      v8_root = std::string(argv[++i]);
    } else {
      // Otherwise it's a .tq file. Remember it for compilation.
      files.emplace_back(argv[i]);
      if (!StringEndsWith(files.back(), ".tq")) {
        std::cerr << "Unexpected command-line argument \"" << files.back()
                  << "\", expected a .tq file.\n";
        base::OS::Abort();
      }
    }
  }

  TorqueCompilerOptions options;
  options.output_directory = std::move(output_directory);
  options.v8_root = std::move(v8_root);
  options.collect_language_server_data = false;
  options.force_assert_statements = false;

  TorqueCompilerResult result = CompileTorque(files, options);

  // PositionAsString requires the SourceFileMap to be set to
  // resolve the file name. Needed to report errors and lint warnings.
  SourceFileMap::Scope source_file_map_scope(*result.source_file_map);

  for (const TorqueMessage& message : result.messages) {
    if (message.position) {
      std::cerr << *message.position << ": ";
    }

    std::cerr << ErrorPrefixFor(message.kind) << ": " << message.message
              << "\n";
  }

  if (!result.messages.empty()) v8::base::OS::Abort();

  return 0;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8

int main(int argc, const char** argv) {
  return v8::internal::torque::WrappedMain(argc, argv);
}

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
  TorqueCompilerOptions options;
  options.collect_language_server_data = false;
  options.force_assert_statements = false;

  std::vector<std::string> files;

  for (int i = 1; i < argc; ++i) {
    // Check for options
    std::string argument(argv[i]);
    if (argument == "-o") {
      options.output_directory = argv[++i];
    } else if (argument == "-v8-root") {
      options.v8_root = std::string(argv[++i]);
    } else if (argument == "-m32") {
#ifdef V8_COMPRESS_POINTERS
      std::cerr << "Pointer compression is incompatible with -m32.\n";
      base::OS::Abort();
#else
      options.force_32bit_output = true;
#endif
    } else if (argument == "-annotate-ir") {
      options.annotate_ir = true;
    } else if (argument == "-strip-v8-root") {
      options.strip_v8_root = true;
    } else {
      // Strip the v8-root in case it is a prefix of the file path itself.
      // This is used when building in Google3.
      if (options.strip_v8_root &&
          argument.substr(0, options.v8_root.size()) == options.v8_root) {
        argument = argument.substr(options.v8_root.size() + 1);
      }
      // Otherwise it's a .tq file. Remember it for compilation.
      files.emplace_back(std::move(argument));
      if (!StringEndsWith(files.back(), ".tq")) {
        std::cerr << "Unexpected command-line argument \"" << files.back()
                  << "\", expected a .tq file.\n";
        base::OS::Abort();
      }
    }
  }

  TorqueCompilerResult result = CompileTorque(files, options);

  // PositionAsString requires the SourceFileMap to be set to
  // resolve the file name. Needed to report errors and lint warnings.
  SourceFileMap::Scope source_file_map_scope(*result.source_file_map);

  for (const TorqueMessage& message : result.messages) {
    if (message.position) {
      std::cerr << PositionAsString(*message.position) << ": ";
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

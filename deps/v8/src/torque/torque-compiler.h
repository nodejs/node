// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_TORQUE_COMPILER_H_
#define V8_TORQUE_TORQUE_COMPILER_H_

#include "src/torque/ast.h"
#include "src/torque/contextual.h"
#include "src/torque/server-data.h"
#include "src/torque/source-positions.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

struct TorqueCompilerOptions {
  std::string output_directory = "";
  std::string v8_root = "";
  bool collect_language_server_data = false;

  // assert(...) are only generated for debug builds. The provide
  // language server support for statements inside asserts, this flag
  // can force generate them.
  bool force_assert_statements = false;

  // Forge (Google3) can only run 64-bit executables. As Torque runs as part
  // of the build process, we need a "cross-compile" mode when we target 32-bit
  // architectures. Note that this does not needed in Chromium/V8 land, since we
  // always build with the same bit width as the target architecture.
  bool force_32bit_output = false;

  // Adds extra comments in output that show Torque intermediate representation.
  bool annotate_ir = false;
};

struct TorqueCompilerResult {
  // Map translating SourceIds to filenames. This field is
  // set on errors, so the SourcePosition of the error can be
  // resolved.
  base::Optional<SourceFileMap> source_file_map;

  // Eagerly collected data needed for the LanguageServer.
  // Set the corresponding options flag to enable.
  LanguageServerData language_server_data;

  // Errors collected during compilation.
  std::vector<TorqueMessage> messages;
};

V8_EXPORT_PRIVATE TorqueCompilerResult
CompileTorque(const std::string& source, TorqueCompilerOptions options);
TorqueCompilerResult CompileTorque(std::vector<std::string> files,
                                   TorqueCompilerOptions options);

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_TORQUE_COMPILER_H_

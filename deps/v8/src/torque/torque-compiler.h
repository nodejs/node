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
  std::string output_directory;
  bool verbose;
  bool collect_language_server_data;
  bool abort_on_lint_errors;
};

struct TorqueCompilerResult {
  // Map translating SourceIds to filenames. This field is
  // set on errors, so the SourcePosition of the error can be
  // resolved.
  SourceFileMap source_file_map;

  // Eagerly collected data needed for the LanguageServer.
  // Set the corresponding options flag to enable.
  LanguageServerData language_server_data;

  // If any error occurred during either parsing or compilation,
  // this field will be set.
  base::Optional<TorqueError> error;
};

V8_EXPORT_PRIVATE TorqueCompilerResult
CompileTorque(const std::string& source, TorqueCompilerOptions options);
TorqueCompilerResult CompileTorque(std::vector<std::string> files,
                                   TorqueCompilerOptions options);

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_TORQUE_COMPILER_H_

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_TORQUE_COMPILER_H_
#define V8_TORQUE_TORQUE_COMPILER_H_

#include "src/torque/ast.h"
#include "src/torque/contextual.h"
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

  static TorqueCompilerOptions Default() { return {"", false, false, false}; }
};

void CompileTorque(std::vector<std::string> files,
                   TorqueCompilerOptions = TorqueCompilerOptions::Default());

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_TORQUE_COMPILER_H_

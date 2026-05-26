// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_TSA_GENERATOR_H_
#define V8_TORQUE_TSA_GENERATOR_H_

#include "src/torque/ast.h"

namespace v8::internal::torque {

void GenerateTSA(Ast& ast, const std::string& output_directory);

}  // namespace v8::internal::torque

#endif  // V8_TORQUE_TSA_GENERATOR_H_

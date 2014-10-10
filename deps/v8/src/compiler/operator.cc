// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/operator.h"

namespace v8 {
namespace internal {
namespace compiler {

Operator::~Operator() {}


SimpleOperator::SimpleOperator(Opcode opcode, Properties properties,
                               int input_count, int output_count,
                               const char* mnemonic)
    : Operator(opcode, properties, mnemonic),
      input_count_(input_count),
      output_count_(output_count) {}


SimpleOperator::~SimpleOperator() {}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

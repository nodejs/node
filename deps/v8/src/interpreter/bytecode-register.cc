// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-register.h"

namespace v8 {
namespace internal {
namespace interpreter {

std::string Register::ToString() const {
  if (is_current_context()) {
    return std::string("<context>");
  } else if (is_function_closure()) {
    return std::string("<closure>");
  } else if (*this == virtual_accumulator()) {
    return std::string("<accumulator>");
  } else if (is_parameter()) {
    int parameter_index = ToParameterIndex();
    if (parameter_index == 0) {
      return std::string("<this>");
    } else {
      std::ostringstream s;
      s << "a" << parameter_index - 1;
      return s.str();
    }
  } else {
    std::ostringstream s;
    s << "r" << index();
    return s.str();
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

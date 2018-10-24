// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>

#include "src/torque/declarable.h"

namespace v8 {
namespace internal {
namespace torque {

std::ostream& operator<<(std::ostream& os, const Callable& m) {
  os << "callable " << m.name() << "(";
  if (m.signature().implicit_count != 0) {
    os << "implicit ";
    TypeVector implicit_parameter_types(
        m.signature().parameter_types.types.begin(),
        m.signature().parameter_types.types.begin() +
            m.signature().implicit_count);
    os << implicit_parameter_types << ")(";
    TypeVector explicit_parameter_types(
        m.signature().parameter_types.types.begin() +
            m.signature().implicit_count,
        m.signature().parameter_types.types.end());
    os << explicit_parameter_types;
  } else {
    os << m.signature().parameter_types;
  }
  os << "): " << *m.signature().return_type;
  return os;
}

std::ostream& operator<<(std::ostream& os, const Builtin& b) {
  os << "builtin " << *b.signature().return_type << " " << b.name()
     << b.signature().parameter_types;
  return os;
}

std::ostream& operator<<(std::ostream& os, const RuntimeFunction& b) {
  os << "runtime function " << *b.signature().return_type << " " << b.name()
     << b.signature().parameter_types;
  return os;
}

std::ostream& operator<<(std::ostream& os, const Generic& g) {
  os << "generic " << g.name() << "<";
  PrintCommaSeparatedList(os, g.declaration()->generic_parameters);
  os << ">";

  return os;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8

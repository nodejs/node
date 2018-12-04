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
  os << "callable " << m.name() << "(" << m.signature().parameter_types
     << "): " << *m.signature().return_type;
  return os;
}

std::ostream& operator<<(std::ostream& os, const Variable& v) {
  os << "variable " << v.name() << ": " << *v.type();
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

void PrintLabel(std::ostream& os, const Label& l, bool with_names) {
  os << l.name();
  if (l.GetParameterCount() != 0) {
    os << "(";
    if (with_names) {
      PrintCommaSeparatedList(os, l.GetParameters(),
                              [](Variable* v) -> std::string {
                                std::stringstream stream;
                                stream << v->name();
                                stream << ": ";
                                stream << *(v->type());
                                return stream.str();
                              });
    } else {
      PrintCommaSeparatedList(
          os, l.GetParameters(),
          [](Variable* v) -> const Type& { return *(v->type()); });
    }
    os << ")";
  }
}

std::ostream& operator<<(std::ostream& os, const Label& l) {
  PrintLabel(os, l, true);
  return os;
}

std::ostream& operator<<(std::ostream& os, const Generic& g) {
  os << "generic " << g.name() << "<";
  PrintCommaSeparatedList(os, g.declaration()->generic_parameters);
  os << ">";

  return os;
}

size_t Label::next_id_ = 0;

}  // namespace torque
}  // namespace internal
}  // namespace v8

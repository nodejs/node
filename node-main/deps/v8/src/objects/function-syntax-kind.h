// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FUNCTION_SYNTAX_KIND_H_
#define V8_OBJECTS_FUNCTION_SYNTAX_KIND_H_

#include "src/utils/utils.h"

namespace v8 {
namespace internal {

enum class FunctionSyntaxKind : uint8_t {
  kAnonymousExpression,
  kNamedExpression,
  kDeclaration,
  kAccessorOrMethod,
  kWrapped,

  kLastFunctionSyntaxKind = kWrapped,
};

inline const char* FunctionSyntaxKind2String(FunctionSyntaxKind kind) {
  switch (kind) {
    case FunctionSyntaxKind::kAnonymousExpression:
      return "AnonymousExpression";
    case FunctionSyntaxKind::kNamedExpression:
      return "NamedExpression";
    case FunctionSyntaxKind::kDeclaration:
      return "Declaration";
    case FunctionSyntaxKind::kAccessorOrMethod:
      return "AccessorOrMethod";
    case FunctionSyntaxKind::kWrapped:
      return "Wrapped";
  }
  UNREACHABLE();
}

inline std::ostream& operator<<(std::ostream& os, FunctionSyntaxKind kind) {
  return os << FunctionSyntaxKind2String(kind);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_FUNCTION_SYNTAX_KIND_H_

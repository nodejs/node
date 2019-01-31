// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_FILE_VISITOR_H_
#define V8_TORQUE_FILE_VISITOR_H_

#include <deque>
#include <string>

#include "src/torque/ast.h"
#include "src/torque/global-context.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class FileVisitor {
 public:
  TypeVector GetTypeVector(const std::vector<TypeExpression*>& v) {
    TypeVector result;
    for (TypeExpression* t : v) {
      result.push_back(Declarations::GetType(t));
    }
    return result;
  }

 protected:
  Signature MakeSignature(const CallableNodeSignature* signature);
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_FILE_VISITOR_H_

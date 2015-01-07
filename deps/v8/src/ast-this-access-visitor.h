// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_THIS_ACCESS_VISITOR_H_
#define V8_AST_THIS_ACCESS_VISITOR_H_
#include "src/ast.h"

namespace v8 {
namespace internal {

class AstThisAccessVisitor : public AstVisitor {
 public:
  explicit AstThisAccessVisitor(Zone* zone);

  bool UsesThis() { return uses_this_; }

#define DECLARE_VISIT(type) void Visit##type(type* node) OVERRIDE;
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

 private:
  bool uses_this_;

  void VisitIfNotNull(AstNode* node) {
    if (node != NULL) Visit(node);
  }

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
  DISALLOW_COPY_AND_ASSIGN(AstThisAccessVisitor);
};
}
}  // namespace v8::internal
#endif  // V8_AST_THIS_ACCESS_VISITOR_H_

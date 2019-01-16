// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_SOURCE_RANGE_AST_VISITOR_H_
#define V8_AST_SOURCE_RANGE_AST_VISITOR_H_

#include <unordered_set>

#include "src/ast/ast-traversal-visitor.h"

namespace v8 {
namespace internal {

class SourceRangeMap;

// Post-processes generated source ranges while the AST structure still exists.
//
// In particular, SourceRangeAstVisitor
//
// 1. deduplicates continuation source ranges, only keeping the outermost one.
// See also: https://crbug.com/v8/8539.
//
// 2. removes the source range associated with the final statement in a block
// or function body if the parent itself has a source range associated with it.
// See also: https://crbug.com/v8/8381.
class SourceRangeAstVisitor final
    : public AstTraversalVisitor<SourceRangeAstVisitor> {
 public:
  SourceRangeAstVisitor(uintptr_t stack_limit, Expression* root,
                        SourceRangeMap* source_range_map);

 private:
  friend class AstTraversalVisitor<SourceRangeAstVisitor>;

  void VisitBlock(Block* stmt);
  void VisitFunctionLiteral(FunctionLiteral* expr);
  bool VisitNode(AstNode* node);

  void MaybeRemoveLastContinuationRange(ZonePtrList<Statement>* stmts);

  SourceRangeMap* source_range_map_ = nullptr;
  std::unordered_set<int> continuation_positions_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_SOURCE_RANGE_AST_VISITOR_H_

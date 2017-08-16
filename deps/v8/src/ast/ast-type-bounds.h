// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A container to associate type bounds with AST Expression nodes.

#ifndef V8_AST_AST_TYPE_BOUNDS_H_
#define V8_AST_AST_TYPE_BOUNDS_H_

#include "src/ast/ast-types.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class Expression;

class AstTypeBounds {
 public:
  explicit AstTypeBounds(Zone* zone) : bounds_map_(zone) {}
  ~AstTypeBounds() {}

  AstBounds get(Expression* expression) const {
    ZoneMap<Expression*, AstBounds>::const_iterator i =
        bounds_map_.find(expression);
    return (i != bounds_map_.end()) ? i->second : AstBounds::Unbounded();
  }

  void set(Expression* expression, AstBounds bounds) {
    bounds_map_[expression] = bounds;
  }

 private:
  ZoneMap<Expression*, AstBounds> bounds_map_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_TYPE_BOUNDS_H_

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_NUMBERING_H_
#define V8_AST_NUMBERING_H_

namespace v8 {
namespace internal {

namespace AstNumbering {
// Assign type feedback IDs and bailout IDs to an AST node tree.
//
bool Renumber(Isolate* isolate, Zone* zone, FunctionLiteral* function);
}
}
}  // namespace v8::internal

#endif  // V8_AST_NUMBERING_H_

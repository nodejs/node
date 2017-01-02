// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_AST_NUMBERING_H_
#define V8_AST_AST_NUMBERING_H_

namespace v8 {
namespace internal {

// Forward declarations.
class FunctionLiteral;
class Isolate;
class Zone;

namespace AstNumbering {
// Assign type feedback IDs, bailout IDs, and generator yield IDs to an AST node
// tree; perform catch prediction for TryStatements.
bool Renumber(Isolate* isolate, Zone* zone, FunctionLiteral* function);
}

// Some details on yield IDs
// -------------------------
//
// In order to assist Ignition in generating bytecode for a generator function,
// we assign a unique number (the yield ID) to each Yield node in its AST. We
// also annotate loops with the number of yields they contain (loop.yield_count)
// and the smallest ID of those (loop.first_yield_id), and we annotate the
// function itself with the number of yields it contains (function.yield_count).
//
// The way in which we choose the IDs is simply by enumerating the Yield nodes.
// Ignition relies on the following properties:
// - For each loop l and each yield y of l:
//     l.first_yield_id  <=  y.yield_id  <  l.first_yield_id + l.yield_count
// - For the generator function f itself and each yield y of f:
//                    0  <=  y.yield_id  <  f.yield_count

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_NUMBERING_H_

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_AST_NUMBERING_H_
#define V8_AST_AST_NUMBERING_H_

#include <stdint.h>

namespace v8 {
namespace internal {

// Forward declarations.
class FunctionLiteral;
class Isolate;
class Zone;
template <typename T>
class ThreadedList;
template <typename T>
class ThreadedListZoneEntry;
template <typename T>
class ZoneVector;

namespace AstNumbering {
// Assign type feedback IDs, bailout IDs, and generator suspend IDs to an AST
// node tree; perform catch prediction for TryStatements. If |eager_literals| is
// non-null, adds any eager inner literal functions into it.
bool Renumber(
    uintptr_t stack_limit, Zone* zone, FunctionLiteral* function,
    ThreadedList<ThreadedListZoneEntry<FunctionLiteral*>>* eager_literals,
    bool collect_type_profile = false);
}

// Some details on suspend IDs
// -------------------------
//
// In order to assist Ignition in generating bytecode for a generator function,
// we assign a unique number (the suspend ID) to each Suspend node in its AST.
// We also annotate loops with the number of suspends they contain
// (loop.suspend_count) and the smallest ID of those (loop.first_suspend_id),
// and we annotate the function itself with the number of suspends it contains
// (function.suspend_count).
//
// The way in which we choose the IDs is simply by enumerating the Suspend
// nodes.
// Ignition relies on the following properties:
// - For each loop l and each suspend y of l:
//     l.first_suspend_id  <=
//         s.suspend_id  < l.first_suspend_id + l.suspend_count
// - For the generator function f itself and each suspend s of f:
//                    0  <=  s.suspend_id  <  f.suspend_count

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_NUMBERING_H_

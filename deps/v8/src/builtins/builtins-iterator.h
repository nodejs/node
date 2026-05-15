// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_ITERATOR_H_
#define V8_BUILTINS_BUILTINS_ITERATOR_H_

#include "src/handles/handles.h"

namespace v8 {
namespace internal {

class Isolate;
class Object;
class JSReceiver;
class Map;

// Helper function to perform IteratorStep in C++.
// Returns the result object (JSIteratorResult), or undefined if done.
// https://tc39.es/ecma262/#sec-iteratorstep
inline MaybeDirectHandle<JSReceiver> IteratorStep(
    Isolate* isolate, DirectHandle<JSReceiver> iterator,
    DirectHandle<Object> next_method, DirectHandle<Map> iterator_result_map);

// https://tc39.es/ecma262/#sec-iteratorclose
inline void IteratorClose(Isolate* isolate, DirectHandle<JSReceiver> iterator);

// Helper for iterating over an iterable, with fast paths for iterables.
// * The int_visitor will be used for Number values on SMI_ELEMENTS arrays and
//   integral TypedArrays where the values fit into a machine integer.
// * The double_visitor will be used for any other Number.
// * The generic_visitor will be used for any non-Number value.
// * The optional max_count_out returns an upper bound on many iterations were
//   performed. Deleted elements or holes might still be counted.
// * The optional max_count limits the number of iterations. It currently only
//   supports limits above the largest uint32.
template <typename IntVisitor, typename DoubleVisitor, typename GenericVisitor>
MaybeDirectHandle<Object> IterableForEach(
    Isolate* isolate, DirectHandle<Object> items, IntVisitor smi_visitor,
    DoubleVisitor double_visitor, GenericVisitor generic_visitor,
    uint64_t* max_count_out = nullptr,
    std::optional<uint64_t> max_count = std::nullopt);

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_ITERATOR_H_

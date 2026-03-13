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

// Helper for iterating over an iterable, with fast paths for Arrays and Sets.
template <typename IntVisitor, typename DoubleVisitor, typename GenericVisitor>
MaybeDirectHandle<Object> IterableForEach(
    Isolate* isolate, DirectHandle<Object> items, IntVisitor smi_visitor,
    DoubleVisitor double_visitor, GenericVisitor generic_visitor,
    std::optional<uint64_t> max_count = std::nullopt);

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_ITERATOR_H_

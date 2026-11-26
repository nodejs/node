// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_v8.h"

v8::internal::Tagged<DescriptorArray> Map::instance_descriptors() const {
    return instance_descriptors_.load(); // Line 4: resolves to getter load!
}

void Map::set_instance_descriptors(v8::internal::Tagged<DescriptorArray> value) {
    instance_descriptors_.store(value); // Line 8: resolves to setter store!
}

// Out-of-line definition (verifies Return Unification!)
v8::internal::DirectHandle<DescriptorArray> AllocateArray(int size) {
    DescriptorArray* raw = new DescriptorArray();
    raw->length = size;
    return v8::internal::DirectHandle<DescriptorArray>(raw); // Line 15
}

v8::internal::DirectHandle<DescriptorArray> ReallocateArray(DescriptorArray& prev) {
    return v8::internal::DirectHandle<DescriptorArray>(&prev);
}

// Sequential loop mutator (verifies Temporal Flow Pruning & operator* Peeling!)
v8::internal::DirectHandle<DescriptorArray> UpdateLoop(v8::internal::DirectHandle<DescriptorArray> desc) {
    v8::internal::DirectHandle<DescriptorArray> current = desc; // Line 20

    // Sequence of mutable updates on same variable name!
    current = AllocateArray((*current).length + 1); // Line 23: allocation update
    current = ReallocateArray(*current);            // Line 24: read *current at 24, write current at 24

    return current;
}

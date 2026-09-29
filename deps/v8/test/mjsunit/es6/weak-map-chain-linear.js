// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --verify-heap

function Foo(x) {
    this.x = x;
}

function createWeakMap() {
    let weak_map = new WeakMap();
    let key;
    let value = new Foo(42);

    // Build a chain where each value references another ephemeron key,
    // triggering the linear-time algorithm for ephemeron processing.
    for (let i = 0; i < 200; ++i) {
        key = new Map();
        weak_map.set(key, value);
        value = new Foo(key);
    }

    return [weak_map, key];
}

let [weak_map, key] = createWeakMap();
gc();

let value = weak_map.get(key);
while (value.x instanceof Map) {
    value = weak_map.get(value.x);
}
assertEquals(42, value.x);

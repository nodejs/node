// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Create a double elements array.
const iterable = [123.123];
assertTrue(%HasDoubleElements(iterable))

iterable.length = 0;
assertTrue(%HasDoubleElements(iterable))

// Should not throw here.
let map = new Map(iterable);
assertEquals(0, map.size);
new WeakMap(iterable); // WeakMap does not have a size

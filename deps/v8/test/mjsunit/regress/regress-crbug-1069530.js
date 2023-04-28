// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

function store(ar, index) {
  ar[index] = "a";
}

let growable_array = [];

// Train IC on growable array
store(growable_array, 0);
store(growable_array, 1);
store(growable_array, 2);
store(growable_array, 3);

// Now make IC polymorphic
var array = [];
Object.defineProperty(array, "length", { value: 3, writable: false });

store(array, 0);
store(array, 1);

// ... and try to grow it.
store(array, 3);
assertEquals(undefined, array[3]);
assertEquals(3, array.length);

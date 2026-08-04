// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
 // Create a FixedDoubleArray
 var arr = [5.65];
 // Force the elements to be EmptyFixedArray
 arr.splice(0);
 // This should create a FixedDoubleArray initialized with holes.
 arr.splice(-4, 9, 10, 20);
 // If the earlier spice didn't create a holes this would fail.
 assertFalse(2 in arr);
}

f();

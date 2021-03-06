// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check that arrays with non-writable length are handled correctly

arr = new Array();
Object.defineProperty(arr, "length", {value: 3, writable: false});
function foo(i, v) { arr[i] = v; }
foo(3);
foo(3, 3);
assertEquals(arr[3], undefined);

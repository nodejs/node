// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Flags: --allow-natives-syntax

function foo(a, i) {
  a[i] = 1;
  return a[i];
}

class MyArray extends (class C extends Array {
}){};

o = new MyArray;

%EnsureFeedbackVectorForFunction(foo);
// initialize IC
assertEquals(1, foo(o, 0));
assertEquals(1, foo(o, 1));

// Change prototype
o.__proto__.__proto__ = new Int32Array(2);


// Check it still works
assertEquals(undefined, foo(o, 2));
assertEquals(undefined, foo(o, 2));

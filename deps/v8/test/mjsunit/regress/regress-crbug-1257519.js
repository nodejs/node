// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

var a = "hello";
function foo(i) {
  var x = a[i];
  return x;
}

// Set up the KeyedLoadIC for monomorphic string load.
foo(4);
foo(4);
foo(4);
// That also handles out of bounds indexes.
assertEquals(foo(8), undefined);

// Add a negative indexed property (not an element, so the
// NoElement protector will not fire).
Object.prototype[-1] = 2;
assertEquals(2, foo(-1));

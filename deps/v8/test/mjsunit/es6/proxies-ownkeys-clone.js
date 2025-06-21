// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var p = new Proxy({a: 1, b: 2}, {
  ownKeys() { return ['a', 'b']; }
});

// clone and return a
function f(a) {
  var y = {...a}
  return y;
}

// Call with different maps to force it into megamorphic state
f({a: 1, b: 2});
f({a1: 1, b1: 3});
f({a2: 1, b2: 3});
f({a3: 1, b3: 4});
f({a4: 1, b4: 5});

// Test that y was initialized correctly in the slow path
var clone = f(p);
assertEquals(clone.a, 1);
assertEquals(clone.b, 2);

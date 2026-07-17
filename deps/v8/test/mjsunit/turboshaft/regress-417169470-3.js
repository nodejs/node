// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan

function foo(a, b, c) {
  a.x = 42;
  b.y = 99; // Transitioning store
  c.x = 17;
  return a.x;
}

let o1 = { x : 27 };
// If we add additional properties, we need to add at least 2 so that `b.y = 99`
// doesn't end up at offset 12, because that's also the offset of .x (which is
// in-object rather than out-of-object), and because we don't have map
// information for backing stores in Late load elimination, we'll assume that
// the store at b.y can alias with the a.x that was previously loaded.
o1.unused1 = 12;
o1.unused2 = 12;

let o2 = { x : 27 };
o2.unused1 = 12;
o2.unused2 = 12;

%PrepareFunctionForOptimization(foo);
assertEquals(17, foo(o1, o1, o1));

%OptimizeFunctionOnNextCall(foo);
assertEquals(17, foo(o2, o2, o2));

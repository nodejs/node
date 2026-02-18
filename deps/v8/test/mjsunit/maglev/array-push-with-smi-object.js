// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function foo(a, thing) {
  a.push(thing);
}
%PrepareFunctionForOptimization(foo);

// To get the [SMI_ELEMENTS, PACKED_ELEMENTS] feedback, we need to first
// call foo with the PACKED_ELEMENTS kind and then with SMI_ELEMENTS kind.

// PACKED_ELEMENTS
let a2 = [{}, {}, {}];
foo(a2, {});

// SMI_ELEMENTS
let a1 = [1, 2, 3];
foo(a1, 11);

%OptimizeMaglevOnNextCall(foo);

foo(a1, 13);

let objs = [{}, {}, {}];
foo(objs, {a: 1});
assertEquals(1, objs[3].a);

foo(objs, 34);
assertEquals(34, objs[4]);

let smis = [1, 2, 3];
foo(smis, 11);
assertEquals(11, smis[3]);

assertTrue(isMaglevved(foo));

// Trying to store an object into a SMI array deopts.
foo(smis, {a: 2});
assertFalse(isMaglevved(foo));
assertEquals(2, smis[4].a);

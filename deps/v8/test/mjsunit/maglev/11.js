// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(x) {
  return x.a
}

function Foo(a) {
  this.a = a
}
%PrepareFunctionForOptimization(f);

// Smi
var o1_1 = new Foo(1);
var o1_2 = new Foo(1);

// Transition map to double, o1 is deprecated, o1's map is a deprecation target.
var o2 = new Foo(1.2);

// Transition map to tagged, o1 is still deprecated.
var an_object = {};
var o3 = new Foo(an_object);

assertEquals(1, f(o1_1));
assertEquals(1.2, f(o2));
assertEquals(an_object, f(o3));

// o1_1 got migrated, but o1_2 hasn't yet.
assertTrue(%HaveSameMap(o1_1,o3));
assertFalse(%HaveSameMap(o1_2,o3));
%OptimizeMaglevOnNextCall(f);

// Deprecated map works
assertEquals(1, f(o1_2));
// Non-deprecated map works
assertEquals(an_object, f(o3));

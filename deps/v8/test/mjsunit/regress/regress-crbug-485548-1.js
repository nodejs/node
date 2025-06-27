// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

var inner = new Array();
inner.a = {
  x: 1
};
inner[0] = 1.5;
inner.b = {
  x: 2
};
assertTrue(%HasDoubleElements(inner));

function foo(o) {
  return o.field.a.x;
};
%PrepareFunctionForOptimization(foo);
var outer = {};
outer.field = inner;
foo(outer);
foo(outer);
foo(outer);
%OptimizeFunctionOnNextCall(foo);
foo(outer);

// Generalize representation of field "a" of inner object.
var v = {
  get x() {
    return 0x7fffffff;
  }
};
inner.a = v;

gc();

var boom = foo(outer);
print(boom);
assertEquals(0x7fffffff, boom);

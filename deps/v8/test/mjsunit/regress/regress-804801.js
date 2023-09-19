// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() { return 42; }
const bound_function = f.bind();
const callable_proxy = new Proxy(function(){}.__proto__, {});

function testSet(ctor) {
  new ctor([]);
  new ctor([{},{}]);
}

function testMap(ctor) {
  new ctor([]);
  new ctor([[{},{}],[{},{}]]);
}

function testAllVariants(set_or_add_function) {
  Set.prototype.add = set_or_add_function;
  testSet(Set);

  WeakSet.prototype.add = set_or_add_function;
  testSet(WeakSet);

  Map.prototype.set = set_or_add_function;
  testMap(Map);

  WeakMap.prototype.set = set_or_add_function;
  testMap(WeakMap);
}

testAllVariants(bound_function);
testAllVariants(callable_proxy);

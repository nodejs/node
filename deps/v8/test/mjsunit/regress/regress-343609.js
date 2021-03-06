// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --block-concurrent-recompilation
// Flags: --expose-gc

function Ctor() {
  this.a = 1;
}

function get_closure() {
  return function add_field(obj) {
    obj.c = 3;
    obj.a = obj.a + obj.c;
    return obj.a;
  }
}
function get_closure2() {
  return function cc(obj) {
    obj.c = 3;
    obj.a = obj.a + obj.c;
  }
}

function dummy() {
  (function () {
    var o = {c: 10};
    var f1 = get_closure2();
    %PrepareFunctionForOptimization(f1);
    f1(o);
    f1(o);
    %OptimizeFunctionOnNextCall(f1);
    f1(o);
  })();
}

var o = new Ctor();
function opt() {
  (function () {
    var f1 = get_closure();
    %PrepareFunctionForOptimization(f1);
    f1(new Ctor());
    f1(new Ctor());
    %OptimizeFunctionOnNextCall(f1);
    f1(o);
  })();
}

// Optimize add_field and install its code in optimized code cache.
opt();
opt();
opt();

// Optimize dummy function to remove the add_field from head of optimized
// function list in the context.
dummy();
dummy();

// Kill add_field in new space GC.
for(var i = 0; i < 3; i++) gc(true);

// Trigger deopt.
o.c = 2.2;

// Fetch optimized code of add_field from cache and crash.
var f2 = get_closure();
f2(new Ctor());

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --verify-heap --turbolev --turbofan

function makeClosures(x, k) {
  const f1 = (y) => x + y;
  function f2(y) {
    return x * y;
  }
  const f3 = async (y) => x - y;
  function* f4(y) {
    yield x + y;
  }
  // Computed property anonymous arrow uses METHOD_WITH_NAME_MAP_INDEX,
  // exercising the in-object property initialization loop (name slot).
  const holder = {[k]: (y) => x + y};
  return [f1, f2, f3, f4, holder];
}

%PrepareFunctionForOptimization(makeClosures);
for (let i = 0; i < 10; i++) {
  const k = 'fn_' + i;
  const [f1, f2, f3, f4, holder] = makeClosures(i, k);
  const f5 = holder[k];
  assertEquals(i + 3, f1(3));
  assertEquals(i * 3, f2(3));
  assertEquals(i + 4, f4(4).next().value);
  assertEquals(i + 1, f5(1));
  assertEquals(k, f5.name);
}

%OptimizeFunctionOnNextCall(makeClosures);
const liveClosures = [];
for (let i = 0; i < 20; i++) {
  const k = 'fn_' + i;
  const [f1, f2, f3, f4, holder] = makeClosures(i, k);
  const f5 = holder[k];
  assertEquals(i + 5, f1(5));
  assertEquals(i * 5, f2(5));
  assertEquals(undefined, f1.prototype);
  assertEquals('object', typeof f2.prototype);
  assertEquals(undefined, f3.prototype);
  assertEquals('object', typeof f4.prototype);
  assertEquals(undefined, f5.prototype);
  assertEquals('fn_' + i, f5.name);
  assertEquals(i + 2, f5(2));
  const inst = new f2(2);
  assertTrue(inst instanceof f2);
  assertEquals(i + 6, f4(6).next().value);
  liveClosures.push(f1, f2, f3, f4, f5);
}
assertOptimized(makeClosures);
gc();
assertEquals(29, liveClosures[liveClosures.length - 1](10));

// Uncompiled / never-called inner closures instantiated multiple times
// (exercises lazy CompileLazy dispatch_handle loaded from FeedbackCell).
function makeUncompiledClosure(x) {
  return function neverCalledDuringWarmup(y) {
    return x + y + 100;
  };
}
%PrepareFunctionForOptimization(makeUncompiledClosure);
makeUncompiledClosure(1);
makeUncompiledClosure(2);
%OptimizeFunctionOnNextCall(makeUncompiledClosure);
const lazyFn = makeUncompiledClosure(7);
assertOptimized(makeUncompiledClosure);
gc();
assertEquals(115, lazyFn(8));

// Single warmup instantiation keeps one_closure_cell_map at compile time,
// exercising the FastNewClosure builtin fallback path and runtime transition.
function makeSingleWarmupClosure(x) {
  return (y) => x + y;
}
%PrepareFunctionForOptimization(makeSingleWarmupClosure);
assertEquals(11, makeSingleWarmupClosure(1)(10));
%OptimizeFunctionOnNextCall(makeSingleWarmupClosure);
const s1 = makeSingleWarmupClosure(2);
const s2 = makeSingleWarmupClosure(3);
assertOptimized(makeSingleWarmupClosure);
gc();
assertEquals(12, s1(10));
assertEquals(13, s2(10));

// Class constructors should still work via FastNewClosure fallback.
function makeClass(v) {
  return class C {
    get() { return v; }
  };
}
%PrepareFunctionForOptimization(makeClass);
for (let i = 0; i < 5; i++) {
  const C = makeClass(i);
  assertEquals(i, new C().get());
}
%OptimizeFunctionOnNextCall(makeClass);
const C10 = makeClass(10);
assertEquals(10, new C10().get());
assertOptimized(makeClass);
gc();

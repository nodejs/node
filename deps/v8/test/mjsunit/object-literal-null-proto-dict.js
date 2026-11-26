// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-fast-null-proto-literals

// With --no-fast-null-proto-literals every object literal with a null
// prototype is created in dictionary mode, regardless of its properties.

function assertDictModeNullProto(o, keys) {
  assertFalse(%HasFastProperties(o));
  assertEquals(null, Object.getPrototypeOf(o));
  assertEquals(keys, Object.keys(o));
  %HeapObjectVerify(o);
}

(function TestLiterals() {
  assertDictModeNullProto({__proto__: null}, []);
  assertDictModeNullProto({__proto__: null, a: 1, b: 2}, ['a', 'b']);
  assertDictModeNullProto({a: 1, __proto__: null, b: 2}, ['a', 'b']);
  assertDictModeNullProto({__proto__: null, 0: 'x', a: 1}, ['0', 'a']);
  assertDictModeNullProto({__proto__: null, ['a']: 1}, ['a']);
  assertDictModeNullProto({__proto__: null, a: Object}, ['a']);
})();

(function TestNested() {
  const o = {__proto__: null, inner: {__proto__: null, x: 1}};
  assertDictModeNullProto(o, ['inner']);
  assertDictModeNullProto(o.inner, ['x']);
})();

(function TestRepeatedCreation() {
  function create() {
    return {__proto__: null, a: 1, b: 2};
  }
  %PrepareFunctionForOptimization(create);
  const o1 = create();
  const o2 = create();
  assertDictModeNullProto(o1, ['a', 'b']);
  assertDictModeNullProto(o2, ['a', 'b']);
  assertNotSame(o1, o2);
  o1.a = 42;
  assertEquals(1, o2.a);
  o2.a = 43;
  assertEquals(42, o1.a);
  assertEquals(1, create().a);
  %OptimizeFunctionOnNextCall(create);
  const o3 = create();
  assertDictModeNullProto(o3, ['a', 'b']);
  assertEquals(1, o3.a);
})();

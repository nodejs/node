// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation --turbofan --fast-null-proto-literals

// Object literals with a null prototype that declare properties are records,
// not dictionaries: they must be created in fast mode, exactly like
// Object.setPrototypeOf({...}, null) or {__proto__: null, ...source}.

function assertNullProtoRecord(o, keys) {
  assertTrue(%HasFastProperties(o));
  assertEquals(null, Object.getPrototypeOf(o));
  assertEquals(keys, Object.keys(o));
  assertFalse('toString' in o);
  assertFalse('hasOwnProperty' in o);
  %HeapObjectVerify(o);
}

(function TestBasicRecord() {
  function create() {
    return {__proto__: null, a: 1, b: 2, c: 3};
  }
  const o1 = create();
  assertNullProtoRecord(o1, ['a', 'b', 'c']);
  assertEquals(1, o1.a);
  assertEquals(2, o1.b);
  assertEquals(3, o1.c);

  // Subsequent creations clone the boilerplate and keep the same map.
  const o2 = create();
  assertNullProtoRecord(o2, ['a', 'b', 'c']);
  assertNotSame(o1, o2);
  assertTrue(%HaveSameMap(o1, o2));

  // The boilerplate is copied: mutating one instance must not affect others.
  // The first call only initializes the feedback slot and the second one
  // creates the cached boilerplate, so mutate both o1 and o2 to verify that
  // the boilerplate is cloned rather than aliased.
  o1.a = 42;
  o2.a = 43;
  assertEquals(42, o1.a);
  assertEquals(43, o2.a);
  const o3 = create();
  assertEquals(1, o3.a);
  assertTrue(%HaveSameMap(o1, o3));
})();

(function TestProtoPositionDoesNotMatter() {
  const first = {__proto__: null, a: 1, b: 2};
  const middle = {a: 1, __proto__: null, b: 2};
  const last = {a: 1, b: 2, __proto__: null};
  assertNullProtoRecord(first, ['a', 'b']);
  assertNullProtoRecord(middle, ['a', 'b']);
  assertNullProtoRecord(last, ['a', 'b']);
  assertTrue(%HaveSameMap(first, middle));
  assertTrue(%HaveSameMap(first, last));
})();

(function TestSameMapAsSetPrototypeOf() {
  const literal = {__proto__: null, a: 1, b: 2};
  const viaSetPrototypeOf = Object.setPrototypeOf({a: 1, b: 2}, null);
  assertNullProtoRecord(viaSetPrototypeOf, ['a', 'b']);
  assertTrue(%HaveSameMap(literal, viaSetPrototypeOf));
})();

(function TestNonConstantValues() {
  function create(x) {
    return {__proto__: null, fn: Object, value: x, nested: {y: x}};
  }
  const o = create(7);
  assertNullProtoRecord(o, ['fn', 'value', 'nested']);
  assertSame(Object, o.fn);
  assertEquals(7, o.value);
  assertEquals(7, o.nested.y);
  assertEquals(Object.prototype, Object.getPrototypeOf(o.nested));
})();

(function TestNestedNullProtoLiterals() {
  function create() {
    return {__proto__: null, inner: {__proto__: null, x: 1}};
  }
  const o1 = create();
  const o2 = create();
  assertNullProtoRecord(o1, ['inner']);
  assertNullProtoRecord(o1.inner, ['x']);
  assertNotSame(o1.inner, o2.inner);
  assertTrue(%HaveSameMap(o1.inner, o2.inner));
  o1.inner.x = 2;
  assertEquals(1, o2.inner.x);
  o2.inner.x = 3;
  assertEquals(2, o1.inner.x);
  assertEquals(1, create().inner.x);
})();

(function TestElements() {
  function create() {
    return {__proto__: null, 0: 'zero', 1: 'one', name: 'n'};
  }
  const o = create();
  assertTrue(%HasFastProperties(o));
  assertEquals(null, Object.getPrototypeOf(o));
  assertEquals('zero', o[0]);
  assertEquals('one', o[1]);
  assertEquals('n', o.name);
  assertEquals(['0', '1', 'name'], Object.keys(o));
  %HeapObjectVerify(o);
})();

(function TestSparseElements() {
  // Sparse elements put the literal in DICTIONARY_ELEMENTS; the named
  // properties still stay fast.
  function create() {
    return {__proto__: null, 100000: 'sparse', name: 'n'};
  }
  const o1 = create();
  const o2 = create();
  assertTrue(%HasFastProperties(o1));
  assertFalse(%HasFastElements(o1));
  assertEquals(null, Object.getPrototypeOf(o1));
  assertEquals('sparse', o1[100000]);
  assertEquals('n', o1.name);
  assertEquals(['100000', 'name'], Object.keys(o1));
  assertTrue(%HaveSameMap(o1, o2));
  o1[100000] = 'changed';
  assertEquals('sparse', o2[100000]);
  assertEquals('sparse', create()[100000]);
  %HeapObjectVerify(o1);
})();

(function TestDoubleFields() {
  function create() {
    return {__proto__: null, d: 1.5, s: 1};
  }
  const o1 = create();
  const o2 = create();
  assertNullProtoRecord(o1, ['d', 's']);
  assertEquals(1.5, o1.d);
  assertEquals(1, o1.s);
  assertTrue(%HaveSameMap(o1, o2));
  // Mutable double fields must not be shared with the boilerplate.
  o1.d = 2.5;
  o2.d = 3.5;
  assertEquals(2.5, o1.d);
  assertEquals(3.5, o2.d);
  assertEquals(1.5, create().d);
  // Storing a non-number generalizes the field; the next instance must not
  // be affected by the deprecated map.
  o1.d = 'str';
  assertEquals('str', o1.d);
  const o3 = create();
  assertEquals(1.5, o3.d);
  assertEquals(null, Object.getPrototypeOf(o3));
  %HeapObjectVerify(o3);
})();

(function TestLargeLiteral() {
  // Literals up to JSObject::kMaxInObjectProperties (252) fit in the object
  // literal map cache and stay in fast mode; larger literals fall back to the
  // cached null-prototype dictionary map.
  for (const [kSize, expectFast] of [[200, true], [252, true], [253, false]]) {
    let source = '({__proto__: null';
    for (let i = 0; i < kSize; i++) source += `, p${i}: ${i}`;
    source += '})';
    const create = new Function('return ' + source);
    const o1 = create();
    const o2 = create();
    assertEquals(expectFast, %HasFastProperties(o1));
    assertEquals(null, Object.getPrototypeOf(o1));
    assertEquals(kSize, Object.keys(o1).length);
    for (let i = 0; i < kSize; i++) assertEquals(i, o1[`p${i}`]);
    assertTrue(%HaveSameMap(o1, o2));
    o1.p0 = 42;
    assertEquals(0, o2.p0);
    assertEquals(0, create().p0);
    %HeapObjectVerify(o1);
  }
})();

(function TestEmptyLiteralStaysDictionary() {
  // An empty null-prototype literal is assumed to be used as a dictionary,
  // just like Object.create(null).
  const empty = {__proto__: null};
  assertFalse(%HasFastProperties(empty));
  assertEquals(null, Object.getPrototypeOf(empty));
  assertFalse(%HasFastProperties(Object.create(null)));

  // Computed keys are not part of the boilerplate, so this is treated like
  // an empty literal.
  const key = 'a';
  const computed = {__proto__: null, [key]: 1};
  assertFalse(%HasFastProperties(computed));
  assertEquals(null, Object.getPrototypeOf(computed));
  assertEquals(1, computed.a);
})();

(function TestTransitionsToDictionaryWhenUsedAsMap() {
  // Deleting a property normalizes the object.
  const o = {__proto__: null, a: 1, b: 2};
  assertTrue(%HasFastProperties(o));
  delete o.a;
  assertFalse(%HasFastProperties(o));
  assertEquals(['b'], Object.keys(o));

  // Adding many dynamic keys eventually normalizes the object as well.
  const map = {__proto__: null, seed: 0};
  assertTrue(%HasFastProperties(map));
  for (let i = 0; i < 100; i++) {
    map['key' + i] = i;
  }
  assertFalse(%HasFastProperties(map));
  assertEquals(0, map.seed);
  assertEquals(99, map.key99);
  assertEquals(null, Object.getPrototypeOf(map));
})();

(function TestPropertyAccessSemantics() {
  const o = {__proto__: null, a: 1};
  assertEquals(undefined, o.toString);
  assertEquals(undefined, o.constructor);
  assertEquals(undefined, o.__proto__);
  assertThrows(() => `${o}`, TypeError);

  // Own __proto__ defined via a computed key is a regular property.
  const withOwnProto = {__proto__: null, a: 1, ['__proto__']: 'own'};
  assertEquals('own', withOwnProto.__proto__);
  assertEquals(null, Object.getPrototypeOf(withOwnProto));
  assertEquals(['a', '__proto__'], Object.keys(withOwnProto));
})();

(function TestOptimizedCode() {
  function create(v) {
    return {__proto__: null, a: v, b: 2};
  }
  %PrepareFunctionForOptimization(create);
  const interpreted = create(1);
  assertNullProtoRecord(interpreted, ['a', 'b']);
  create(1);

  %OptimizeMaglevOnNextCall(create);
  const maglev = create(1);
  assertNullProtoRecord(maglev, ['a', 'b']);
  assertTrue(%HaveSameMap(interpreted, maglev));
  assertNotSame(interpreted, maglev);

  %OptimizeFunctionOnNextCall(create);
  const turbofan = create(1);
  assertNullProtoRecord(turbofan, ['a', 'b']);
  assertTrue(%HaveSameMap(interpreted, turbofan));
  assertNotSame(interpreted, turbofan);
  assertOptimized(create);

  // Mutating an instance must not leak into the boilerplate.
  turbofan.a = 42;
  assertEquals(1, create(1).a);
})();

(function TestMonomorphicPropertyAccess() {
  function create(i) {
    return {__proto__: null, x: i, y: i + 1};
  }
  function sum(o) {
    return o.x + o.y;
  }
  %PrepareFunctionForOptimization(sum);
  for (let i = 0; i < 10; i++) {
    assertEquals(2 * i + 1, sum(create(i)));
  }
  %OptimizeFunctionOnNextCall(sum);
  assertEquals(21, sum(create(10)));
  assertOptimized(sum);
  assertEquals(23, sum(create(11)));
  assertOptimized(sum);
})();

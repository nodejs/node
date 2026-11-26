// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function makeCreator(count) {
  const props = [];
  for (let i = 0; i < count; i++) {
    if (i % 3 === 0) {
      props.push(`p${i}: x + ${i}`);
    } else if (i % 3 === 1) {
      props.push(`p${i}: x + 0.5`);
    } else {
      props.push(`p${i}: obj`);
    }
  }
  return new Function('x', 'obj', `return {${props.join(', ')}};`);
}

function verifyLiteral(o, count, x, obj, expectFast) {
  assertEquals(expectFast, %HasFastProperties(o));
  assertEquals(count, Object.keys(o).length);
  assertEquals(x, o.p0);
  assertEquals(x + 0.5, o.p1);
  assertSame(obj, o.p2);
  const last = count - 1;
  const expectedLast =
      last % 3 === 0 ? x + last : (last % 3 === 1 ? x + 0.5 : obj);
  assertEquals(expectedLast, o[`p${last}`]);
  %HeapObjectVerify(o);
}

// Literals with up to JSObject::kMaxInObjectProperties (252) properties get a
// fast map from the map cache. Larger literals are in dictionary mode and
// initialize non-constant properties through DefineNamedOwnInLiteralIC.
for (const [count, expectFast] of [
         [127, true], [128, true], [252, true], [253, false], [260, false]]) {
  const create = makeCreator(count);
  const marker = {tag: count};
  %PrepareFunctionForOptimization(create);
  const o1 = create(1, marker);
  const o2 = create(2, marker);
  verifyLiteral(o1, count, 1, marker, expectFast);
  verifyLiteral(o2, count, 2, marker, expectFast);
  assertTrue(%HaveSameMap(o1, o2));

  // Mutating double and smi fields on one instance must not alias another.
  o1.p0 = 999;
  o1.p1 = 42.25;
  assertEquals(2, o2.p0);
  assertEquals(2.5, o2.p1);

  %OptimizeFunctionOnNextCall(create);
  const o3 = create(3, marker);
  verifyLiteral(o3, count, 3, marker, expectFast);
  assertOptimized(create);
  assertTrue(%HaveSameMap(o2, o3));

  // CloneObjectIC shares the map cache limit.
  if (!expectFast) continue;
  const cloneFn = new Function('src', 'return {...src};');
  %PrepareFunctionForOptimization(cloneFn);
  for (let i = 0; i < 5; i++) cloneFn(o3);
  %OptimizeFunctionOnNextCall(cloneFn);
  const cloned = cloneFn(o3);
  verifyLiteral(cloned, count, 3, marker, true);
}

// Stores into a dictionary-mode literal preserve dictionary const tracking.
(function TestDictLiteralDefineOwnAndConstTracking() {
  const dictCreate = makeCreator(255);
  const marker = {};
  for (let i = 0; i < 6; i++) {
    const o = dictCreate(10, marker);
    assertFalse(%HasFastProperties(o));
    if (%IsDictPropertyConstTrackingEnabled()) {
      assertTrue(%HasOwnConstDataProperty(o, 'p0'));
      o.p0 = 11;
      assertFalse(%HasOwnConstDataProperty(o, 'p0'));
    }
  }
})();

// Class fields share the store handler with literals but must keep full
// [[DefineOwnProperty]] semantics for existing dictionary entries.
(function TestClassFieldOnDictionaryReceiver() {
  function makeDictReceiver() {
    const r = {a: 1, b: 2};
    delete r.a;
    assertFalse(%HasFastProperties(r));
    return r;
  }

  class Base {
    constructor(r) {
      return r;
    }
  }
  class Derived extends Base {
    x = 99;
  }

  for (let i = 0; i < 6; i++) {
    const r = makeDictReceiver();
    r.x = i;
    assertEquals(99, new Derived(r).x);
  }

  {
    const r = makeDictReceiver();
    Object.defineProperty(
        r, 'x',
        {value: 1, writable: true, enumerable: true, configurable: false});
    assertThrows(() => new Derived(r), TypeError);
    assertEquals(1, r.x);
  }

  {
    const r = makeDictReceiver();
    let setterCalled = false;
    Object.defineProperty(r, 'x', {
      get() {
        return 1;
      },
      set() {
        setterCalled = true;
      },
      enumerable: false,
      configurable: true,
    });
    new Derived(r);
    assertFalse(setterCalled);
    assertEquals(
        {value: 99, writable: true, enumerable: true, configurable: true},
        Object.getOwnPropertyDescriptor(r, 'x'));
  }
})();

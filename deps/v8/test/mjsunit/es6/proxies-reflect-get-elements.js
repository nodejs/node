// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function get(o, k, r) {
  return Reflect.get(o, k, r);
}
%PrepareFunctionForOptimization(get);

function test(f) {
  f();
  f();
  %OptimizeFunctionOnNextCall(get);
  f();
  assertOptimized(get);
}

const receiver = {};

// Element loads from fast and non-extensible / sealed / frozen elements.
test(() => {
  for (const o of [[1, 2, 3], [1.5, 2.5, 3.5], [{}, 'x', 3],
                   Object.preventExtensions([1, 2, 3]),
                   Object.seal([1, 2, 3]), Object.freeze([1, 2, 3]),
                   {0: 1, 1: 2, 2: 3}]) {
    assertEquals(o[0], get(o, 0, receiver));
    assertEquals(o[2], get(o, '2', receiver));
    assertEquals(undefined, get(o, 3, receiver));
    assertEquals(undefined, get(o, 1000, receiver));
  }
});

// Holes and out-of-bounds indices continue on the prototype chain.
test(() => {
  const proto = [10, 11, 12, 13];
  const o = [0, , 2];
  Object.setPrototypeOf(o, proto);
  assertEquals(11, get(o, 1, receiver));
  assertEquals(13, get(o, 3, receiver));
  const d = [0.5, , 2.5];
  Object.setPrototypeOf(d, proto);
  assertEquals(11, get(d, 1, receiver));
});

// Same for nonextensible, sealed and frozen elements.
test(() => {
  const proto = [10, 11, 12, 13];
  for (const make of [Object.preventExtensions, Object.seal, Object.freeze]) {
    const o = [0, , 2];
    Object.setPrototypeOf(o, proto);
    make(o);
    assertEquals(0, get(o, 0, receiver));
    assertEquals(11, get(o, 1, receiver));
    assertEquals(13, get(o, 3, receiver));
    assertEquals(11, new Proxy(o, {})[1]);
    assertEquals(2, get(Object.create(o), 2, receiver));
  }
});

// Backing store slack beyond the array length is out of bounds.
test(() => {
  Array.prototype[5] = 'proto';
  try {
    const a = [1, 2, 3];
    a.push(4);
    const d = [1.5, 2.5, 3.5];
    d.push(4.5);
    for (const o of [a, d]) {
      assertEquals(undefined, get(o, 4, receiver));
      assertEquals(undefined, get(o, '4', receiver));
      assertEquals('proto', get(o, 5, receiver));
      assertEquals('proto', get(o, '5', receiver));
      assertEquals(undefined, new Proxy(o, {})[4]);
      assertEquals('proto', new Proxy(o, {})[5]);
    }
  } finally {
    delete Array.prototype[5];
  }
});

// Indexed getters on the prototype see the receiver, not the holder.
test(() => {
  const proto = {};
  Object.defineProperty(proto, 1, {get() { return this; }});
  const o = [0, , 2];
  Object.setPrototypeOf(o, proto);
  assertSame(receiver, get(o, 1, receiver));
  const p = new Proxy(o, {});
  assertSame(p, get(o, 1, p));
  assertSame(p, p[1]);
});

// Proxies on the prototype chain receive the original receiver.
test(() => {
  const proto = new Proxy({}, {
    get(target, key, r) { return [key, r]; }
  });
  const o = [0, , 2];
  Object.setPrototypeOf(o, proto);
  const [key, r] = get(o, 1, receiver);
  assertEquals('1', key);
  assertSame(receiver, r);
  assertEquals(0, get(o, 0, receiver));
});

// Typed arrays do not consult the prototype for integer indices.
test(() => {
  const ta = new Uint8Array([1, 2]);
  Object.setPrototypeOf(ta, [7, 7, 7, 7]);
  assertEquals(2, get(ta, 1, receiver));
  assertEquals(undefined, get(ta, 3, receiver));
  const o = Object.create(ta);
  assertEquals(1, get(o, 0, receiver));
  assertEquals(undefined, get(o, 3, receiver));
});

// String wrappers and dictionary elements.
test(() => {
  const s = new String('ab');
  assertEquals('b', get(s, 1, receiver));
  assertEquals(undefined, get(s, 2, receiver));
  const o = [];
  o[100000] = 'far';
  assertEquals('far', get(o, 100000, receiver));
});

// Negative and too-large integer keys are named properties.
test(() => {
  const o = [1, 2, 3];
  o[-1] = 'minus';
  o[-2] = 'minus2';
  o[4294967295] = 'big';
  o[4294967296] = 'bigger';
  assertEquals('minus', get(o, -1, receiver));
  assertEquals('minus2', get(o, -2, receiver));
  assertEquals('big', get(o, 4294967295, receiver));
  assertEquals('bigger', get(o, 4294967296, receiver));
  const p = new Proxy(o, {});
  assertEquals('minus', p[-1]);
  assertEquals('big', p[4294967295]);
});

// Proxy [[Get]] / [[Set]] invariants on index keys.
function load(p, k) {
  return p[k];
}
function store(p, k, v) {
  'use strict';
  p[k] = v;
}
const lie = {get() { return 42; }};
const yes = {set() { return true; }};
// Warm up the ICs so that they use the proxy handlers.
for (let i = 0; i < 3; i++) {
  load(new Proxy([1], lie), 0);
  store(new Proxy([1], yes), 0, 1);
}

function checkGet(target, key, throws) {
  const p = new Proxy(target, lie);
  for (const f of [() => Reflect.get(p, key), () => load(p, key),
                   () => load(p, key), () => [p].map(x => x[key])[0]]) {
    if (throws) {
      assertThrows(f, TypeError);
    } else {
      assertEquals(42, f());
    }
  }
}

function checkSet(target, key, throws) {
  const p = new Proxy(target, yes);
  for (const f of [() => store(p, key, 5), () => store(p, key, 5)]) {
    if (throws) {
      assertThrows(f, TypeError);
    } else {
      f();
    }
  }
}

(function TestInvariants() {
  for (const check of [checkGet, checkSet]) {
    check([1, 2], 0, false);
    check([1.5, 2.5], '1', false);
    check([1, , 2], 1, false);
    check(Object.seal([1, 2]), 0, false);
    check(Object.preventExtensions([1, 2]), 0, false);
    check(new Uint8Array(2), 0, false);
    check(Object.freeze([1, 2]), 0, true);
    check(Object.freeze([1, 2]), '1', true);
    check(new String('ab'), 0, true);
    check(new Proxy(Object.freeze([1, 2]), {}), 0, true);
    const o = [1, 2];
    Object.defineProperty(o, 0, {writable: false, configurable: false});
    check(o, 0, true);
    const big = {};
    Object.defineProperty(big, 1234567, {value: 1});
    check(big, 1234567, true);
    check(big, '1234567', true);
    const huge = [1, 2];
    Object.defineProperty(huge, 4294967295, {value: 1});
    check(huge, 4294967295, true);
    check(huge, '4294967295', true);
  }
  const getless = [1, 2];
  Object.defineProperty(getless, 0, {set(v) {}, configurable: false});
  checkGet(getless, 0, true);
  const setless = [1, 2];
  Object.defineProperty(setless, 1, {get() {}, configurable: false});
  checkSet(setless, 1, true);
  // Matching values are fine.
  checkGet(Object.freeze([42]), 0, false);
})();

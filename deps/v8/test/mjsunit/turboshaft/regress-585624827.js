// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

// Test that Turboshaft does not constant-fold Word32/Word64 equality between
// two distinct HeapObject string constants to false when at least one is not
// internalized, because internalization + GC ThinString shortcutting can make
// both constants refer to the same internalized string object at runtime.

// 1. Direct equality between a compile-time non-internalized string constant
// and an internalized string constant.
(function testNonInternalizedVsInternalized() {
  Object.defineProperty(globalThis, 'P1', {
    value: 'true',
    configurable: true,
  });

  function foo(act) {
    let a = P1;
    if (!act) return;
    return a === 'true';
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(true));

  let non_intern = String.fromCharCode(116, 114, 117, 101);
  delete globalThis.P1;
  Object.defineProperty(globalThis, 'P1', {
    value: non_intern,
    writable: false,
  });
  foo(false);
  %OptimizeFunctionOnNextCall(foo);
  foo(false);

  %InternalizeString(non_intern);
  gc();
  assertTrue(foo(true));
})();

// 2. Equality between two distinct compile-time non-internalized string
// constants with identical contents.
(function testTwoNonInternalized() {
  Object.defineProperty(globalThis, 'P2a', {
    value: 'hello',
    configurable: true,
  });
  Object.defineProperty(globalThis, 'P2b', {
    value: 'hello',
    configurable: true,
  });

  function foo(act) {
    let a = P2a;
    let b = P2b;
    if (!act) return;
    return a === b;
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(true));

  let s1 = String.fromCharCode(104, 101, 108, 108, 111);
  let s2 = String.fromCharCode(104, 101, 108, 108, 111);
  delete globalThis.P2a;
  delete globalThis.P2b;
  Object.defineProperty(globalThis, 'P2a', { value: s1, writable: false });
  Object.defineProperty(globalThis, 'P2b', { value: s2, writable: false });
  foo(false);
  %OptimizeFunctionOnNextCall(foo);
  foo(false);

  %InternalizeString(s1);
  %InternalizeString(s2);
  gc();
  assertTrue(foo(true));
})();

// 3. Simplified ConsString length consistency test: ensures a shared string
// length computation and a duplicated equality check agree after GC ThinString
// shortcutting, preventing a malformed ConsString.
(function testConsStringLengthConsistency() {
  let state = { index: 0, outer_len: 0, inner_str: null };
  Object.defineProperty(globalThis, 'P3', {
    value: 'true',
    configurable: true,
  });

  function foo(h, j, k, input, act) {
    let a = P3;
    if (!act) return;
    let str = input + '';
    let wrapper = new String(str);
    let eq = '' + (a === ('' + (wrapper.length > 3)));
    let prefix = '' + ((eq.length & 1) << 28);
    if (str.length > 2) {
      let outer_concat = prefix + 'AAAAAAAAAAAAA';
      state.outer_len = outer_concat.length;
      if (h && j && k && wrapper.length > 3) {
        state.index = str.indexOf('k');
        let eq = '' + (a === ('' + (wrapper.length > 3)));
        let prefix = '' + ((eq.length & 1) << 28);
        state.inner_str = prefix;
        return prefix + 'BBBBBBBBBBBBB';
      }
      return null;
    }
    if (k) {
      for (let z = 0; z < 8; z++) state.outer_len = z;
      return prefix;
    }
    return null;
  }

  %PrepareFunctionForOptimization(foo);
  for (let i = 0; i < 10; i++) {
    foo(true, true, true, i % 2 ? 'zzzz' : 'zz', true);
  }

  let non_intern = String.fromCharCode(116, 114, 117, 101);
  delete globalThis.P3;
  Object.defineProperty(globalThis, 'P3', {
    value: non_intern,
    writable: false,
  });
  foo(true, true, true, 'zzzz', false);
  %OptimizeFunctionOnNextCall(foo);
  foo(true, true, true, 'zzzz', false);

  %InternalizeString(non_intern);
  gc();

  let res = foo(true, true, true, 'zzzz', true);
  assertEquals('0', state.inner_str);
  assertEquals(1, state.inner_str.length);
  assertEquals(14, state.outer_len);
  assertEquals(14, res.length);
  assertEquals('0BBBBBBBBBBBBB', res);
  assertEquals(-1, res.indexOf('x'));
})();

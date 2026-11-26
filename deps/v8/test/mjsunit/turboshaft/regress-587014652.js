// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc
// Flags: --no-optimize-on-next-call-optimizes-to-maglev

// Verify that Turboshaft does not constant-fold equality between two distinct
// HeapConstant handles to false after GC ThinString shortcutting makes both
// handles point to the same InternalizedString.

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

  if (%IsTurbofanEnabled() && %IsConcurrentRecompilationSupported()) {
    %WaitForBackgroundOptimization();
    const sync_point = 'TurboshaftMachineLowering';
    const timeout_ms = 10000;
    %BlockAt(sync_point, timeout_ms);
    %OptimizeFunctionOnNextCall(foo, 'concurrent');
    foo(false);
    assertTrue(%WaitUntilBlocked(sync_point, timeout_ms));

    %InternalizeString(non_intern);
    gc();

    assertTrue(%Resume(sync_point));
    %FinalizeOptimization();
  }

  assertTrue(foo(true));
})();

(function testTwoNonInternalized() {
  let init = %InternalizeString(String.fromCharCode(104, 101, 108, 108, 111));
  Object.defineProperty(globalThis, 'P2a', {
    value: init,
    configurable: true,
  });
  Object.defineProperty(globalThis, 'P2b', {
    value: init,
    configurable: true,
  });

  function bar(act) {
    let a = P2a;
    let b = P2b;
    if (!act) return;
    return a === b;
  }

  %PrepareFunctionForOptimization(bar);
  assertTrue(bar(true));

  let s1 = String.fromCharCode(104, 101, 108, 108, 111, 95, 53, 56, 55);
  let s2 = String.fromCharCode(104, 101, 108, 108, 111, 95, 53, 56, 55);
  delete globalThis.P2a;
  delete globalThis.P2b;
  Object.defineProperty(globalThis, 'P2a', { value: s1, writable: false });
  Object.defineProperty(globalThis, 'P2b', { value: s2, writable: false });
  bar(false);

  if (%IsTurbofanEnabled() && %IsConcurrentRecompilationSupported()) {
    %WaitForBackgroundOptimization();
    const sync_point = 'TurboshaftMachineLowering';
    const timeout_ms = 10000;
    %BlockAt(sync_point, timeout_ms);
    %OptimizeFunctionOnNextCall(bar, 'concurrent');
    bar(false);
    assertTrue(%WaitUntilBlocked(sync_point, timeout_ms));

    %InternalizeString(s1);
    %InternalizeString(s2);
    gc();

    assertTrue(%Resume(sync_point));
    %FinalizeOptimization();
  }

  assertTrue(bar(true));
})();

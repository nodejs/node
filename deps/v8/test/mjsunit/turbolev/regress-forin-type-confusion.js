// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function firstKeyViaForIn(o) {
  for (const k in o) { return k; }
  return 'empty';
}

%PrepareFunctionForOptimization(firstKeyViaForIn);

const arr = [1];
firstKeyViaForIn(arr);
firstKeyViaForIn(arr);

%OptimizeFunctionOnNextCall(firstKeyViaForIn);
firstKeyViaForIn(arr);

// It should stay optimized because generic ForIn calls the ForIn builtins.
assertEquals("0", firstKeyViaForIn(arr));
assertOptimized(firstKeyViaForIn);

(function() {
  function firstKeyEnumCache(o) {
    for (const k in o) { return k; }
    return 'empty';
  }
  %PrepareFunctionForOptimization(firstKeyEnumCache);

  const obj = {a: 1};
  assertEquals("a", firstKeyEnumCache(obj));
  assertEquals("a", firstKeyEnumCache(obj));

  %OptimizeFunctionOnNextCall(firstKeyEnumCache);
  assertEquals("a", firstKeyEnumCache(obj));
  assertOptimized(firstKeyEnumCache);

  // Passing an array returns a ForInEnumeratorHolder, which must safely deopt
  // when compiled for ForInHint::kEnumCacheKeysAndIndices instead of
  // type-confusing ForInEnumeratorHolder with Map.
  assertEquals("0", firstKeyEnumCache([1]));
})();

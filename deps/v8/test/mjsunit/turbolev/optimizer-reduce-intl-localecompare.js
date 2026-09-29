// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

// `cmp` is lazily inlined so that the localeCompare call is reduced post-inline
// by the optimizer (StringLocaleCompareIntl) rather than at build time. With
// V8_INTL_SUPPORT disabled the call stays generic, but `foo` still optimizes.
function cmp(a, b) {
  for (let i = 0; i < 5; i++);
  return a.localeCompare(b);
}

function foo(a, b) { return cmp(a, b); }

%PrepareFunctionForOptimization(cmp);
%PrepareFunctionForOptimization(foo);

assertEquals(0, foo("a", "a"));
assertTrue(foo("a", "b") < 0);
assertTrue(foo("b", "a") > 0);

%OptimizeFunctionOnNextCall(foo);
assertEquals(0, foo("a", "a"));
assertTrue(foo("a", "b") < 0);
assertOptimized(foo);

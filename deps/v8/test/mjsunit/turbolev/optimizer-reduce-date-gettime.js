// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

const d = new Date(2024, 5, 15, 10, 30, 45);
// Captured at runtime: getTime() of a local-time Date is timezone-dependent.
const expected = d.getTime();

function getTime(date) {
  // Big enough that `getTime` is lazily inlined, so the getTime call is reduced
  // post-inline by the optimizer (LoadFloat64 path) rather than at build time.
  for (let i = 0; i < 5; i++);
  return date.getTime();
}

function foo(date) { return getTime(date); }

%PrepareFunctionForOptimization(getTime);
%PrepareFunctionForOptimization(foo);

assertEquals(expected, foo(d));
assertEquals(expected, foo(d));

%OptimizeFunctionOnNextCall(foo);
assertEquals(expected, foo(d));
assertOptimized(foo);

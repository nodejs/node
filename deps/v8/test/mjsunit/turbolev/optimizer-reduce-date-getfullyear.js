// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

const d = new Date(2024, 5, 15, 10, 30, 45);

function getField(date) {
  // Big enough that `getField` is lazily inlined, so the getFullYear call is
  // reduced post-inline by the optimizer rather than at build time.
  for (let i = 0; i < 5; i++);
  return date.getFullYear();
}

function foo(date) { return getField(date); }

%PrepareFunctionForOptimization(getField);
%PrepareFunctionForOptimization(foo);

assertEquals(2024, foo(d));
assertEquals(2024, foo(d));

%OptimizeFunctionOnNextCall(foo);
assertEquals(2024, foo(d));
assertOptimized(foo);

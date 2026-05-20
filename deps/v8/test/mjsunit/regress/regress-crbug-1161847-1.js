// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(first_run) {
  let o = { x: 0 };
  if (first_run) assertTrue(%HasOwnConstDataProperty(o, 'x'));
  Object.defineProperty(o, 'x', { writable: false });
  delete o.x;
  o.x = 23;
  if (first_run) assertFalse(%HasOwnConstDataProperty(o, 'x'));
}
%PrepareFunctionForOptimization(foo);
foo(true);
foo(false);
%OptimizeFunctionOnNextCall(foo);
foo(false);

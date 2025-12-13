// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function f10(a) {
  // The param is either the string (this will throw) or the ArrayIterator.
  a.next();
}
%PrepareFunctionForOptimization(f10);

function foo() {
  const v5 = [-26843545];
  const v7 = v5.keys();
  let f8 = 0;
  const v9 = `dateStyle${f8}formatRange`; // Cons string
  try { f10(v9); } catch (e) { }
  f10(v7);
}
%PrepareFunctionForOptimization(foo);

foo();

%OptimizeMaglevOnNextCall(foo);

foo();

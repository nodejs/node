// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* generator() {
  yield undefined;
}
function bar(x) {
  const objects = [];
  for (let obj of generator()) {
  }
  return objects[0];
}
function foo() {
  try { undefined[0] = bar(); } catch (e) { }
  Math.min(bar(), bar(), bar());
}
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();

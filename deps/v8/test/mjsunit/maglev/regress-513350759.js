// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function factory() {
  var v = 0;
  function inner(hh, o) {
    v = o;
    hh();
    return v;
  }
  function outer(fn, marker) {
    const h = () => { v = 1.1; };
    return fn(h, marker);
  }
  return { inner, outer };
}

const keep = [];
for (let i = 0; i < 5; i++) {
  const f = factory();
  %PrepareFunctionForOptimization(f.inner);
  for (let j = 0; j < 5; j++) f.inner(() => {}, 0);
  keep.push(f);
}

const { inner, outer } = factory();
%PrepareFunctionForOptimization(inner);
%PrepareFunctionForOptimization(outer);

const marker = { x: 42 };
for (let i = 0; i < 30; i++) assertEquals(1.1, outer(inner, marker));

%OptimizeMaglevOnNextCall(outer);
assertEquals(1.1, outer(inner, marker));

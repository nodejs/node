// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --gc-global

const keep_parent_map = {};
keep_parent_map.a = 1;

function makeObject() {
  const o = {};
  o.a = 1;
  o.b = 2;
  return o;
}
%NeverOptimizeFunction(makeObject);

function getSecondKey(s) {
  s = String(s);
  let first = true;
  for (const key in makeObject()) {
    if (first) {
      first = false;
      %SimulateNewspaceFull();
      continue;
    }
    // +s lowers to pure StringToNumber, which schedules before the enum-cache
    // load and triggers GC when flattening the ConsString.
    const n = +s;
    return ((n === 0) + (key === 'a')) === 0 ? key : n;
  }
}

%PrepareFunctionForOptimization(getSecondKey);
assertEquals('b', getSecondKey('1.25'));
assertEquals('b', getSecondKey('1.25'));
%OptimizeFunctionOnNextCall(getSecondKey);
assertEquals('b', getSecondKey('1.25' + ' '.repeat(20)));

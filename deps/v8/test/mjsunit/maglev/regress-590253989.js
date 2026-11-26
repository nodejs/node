// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

const keepParentMap = {};
keepParentMap.a = 1;

function makeHolder() {
  const o = {};
  o.a = 1;
  o.b = 2;
  const h = {};
  h.x = o;
  return h;
}
%NeverOptimizeFunction(makeHolder);

const other = {y: 0, x: 0};

function getSecondKey() {
  const h = makeHolder();
  const o = h.x;
  let first = true;
  for (const key in o) {
    if (first) {
      first = false;
      h.x = o;
      other.x = 1;
      other.y = h.x.a;
      %MajorGCForCompilerTesting();
      continue;
    }
    return key;
  }
}

%PrepareFunctionForOptimization(getSecondKey);
assertEquals('b', getSecondKey());
assertEquals('b', getSecondKey());
%OptimizeMaglevOnNextCall(getSecondKey);
assertEquals('b', getSecondKey());
if (!isTurboFanned(getSecondKey)) assertMaglevved(getSecondKey);

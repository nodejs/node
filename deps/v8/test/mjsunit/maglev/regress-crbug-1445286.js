// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

const floats = new Float64Array(10);
function f(proto) {
  const o = {
      __proto__: proto,
  };
  o.h = 1601;
  o.name;
  [v0, ...rest] = floats;
  return o;
}

%PrepareFunctionForOptimization(f);

for (let i = 0; i < 20; ++i) {
  f();
  const o = f({});
  o.h = v0;
}

%OptimizeMaglevOnNextCall(f);

f();

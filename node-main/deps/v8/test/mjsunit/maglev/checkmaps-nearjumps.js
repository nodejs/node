// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --allow-natives-syntax --max-valid-polymorphic-map-count=100

// Test based on regress-crbug-1445286.js; this will generate a lot of maps
// which are passed to CheckMaps.

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

for (let i = 0; i < 100; ++i) {
  %OptimizeMaglevOnNextCall(f);
  f();
  const o = f({});
  o.h = v0;
}

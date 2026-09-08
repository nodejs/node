// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --invocation-count-for-maglev=10
// Flags: --no-lazy-feedback-allocation --maglev-non-eager-inlining --single-threaded

function probe(arr) {
  Object.setPrototypeOf(arr, new Proxy(Object.getPrototypeOf(arr), {
    get(t, p) { return Reflect.get(t, p); }
  }));
}

function main() {
  for (let i = 0; i < 50; i++) {
    const arr = [i];
    probe(arr);
    function f() {
      const o = { __proto__: arr };
      o.keys();
      return o;
    }
    const o = f();
    o.keys();
    o.keys();
    %OptimizeOsr();
  }
}

%PrepareFunctionForOptimization(main);
main();

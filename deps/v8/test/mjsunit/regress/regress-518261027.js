// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fast-proxy-ic --allow-natives-syntax

class MyTrap {
  constructor(p, r) {}
}
const handler = {
  get: MyTrap
};
const proxy = new Proxy({a: 1}, handler);
function test(p) {
  return p.a;
}

%PrepareFunctionForOptimization(test);
assertThrows(() => test(proxy), TypeError);
assertThrows(() => test(proxy), TypeError);

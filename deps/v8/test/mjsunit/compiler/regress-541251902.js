// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

class Base {
  constructor(x) { return x; }
}

class P extends Base {
  #t = 42;
  static get_t(o) {
    return o.#t;
  }
}

const warm = new Proxy({}, {});
new P(warm);

%PrepareFunctionForOptimization(P.get_t);
assertEquals(42, P.get_t(warm));
assertEquals(42, P.get_t(warm));

%OptimizeFunctionOnNextCall(P.get_t);
assertEquals(42, P.get_t(warm));

// Non-callable proxies share the context-wide proxy_map. When an identity hash
// is assigned (e.g. via Map/Set), properties_or_hash_ contains a Smi hash instead
// of a NameDictionary. Accessing private fields on such a proxy must safely throw
// a TypeError instead of assuming all objects with proxy_map have a dictionary.
const map = new Map();
const victims = [];
for (let i = 0; i < 25; i++) {
  const victim = new Proxy({}, {});
  map.set(victim, 1);
  victims.push(victim);
}

for (const victim of victims) {
  assertThrows(() => P.get_t(victim), TypeError);
}

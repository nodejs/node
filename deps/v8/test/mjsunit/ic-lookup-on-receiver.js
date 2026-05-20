// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestLookupOnReceiver() {
  let log = [];

  function f(o, v) {
    o.x = v;
    return o.x;
  }

  let p = {};
  Object.defineProperty(
      p, "x",
      {
        get: function() { return 153; },
        set: function(v) { log.push("set"); },
        configurable: true
      });

  let o = Object.create(p);
  // Turn o to dictionary mode.
  for (let i = 0; i < 2048; i++) {
    o["p"+i] = 0;
  }
  assertFalse(%HasFastProperties(o));

  for (let i = 0; i < 5; i++) {
    log.push(f(o, i));
  }

  Object.defineProperty(o, "x", { value: 0, configurable: true, writable: true});

  for (let i = 0; i < 5; i++) {
    log.push(f(o, 42 + i));
  }

  assertEquals(log,
               ["set", 153, "set", 153, "set", 153, "set", 153, "set", 153,
                42, 43, 44, 45, 46]);
})();

// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-staging

var log = [];
class Test {
  [Symbol.dispose]() {
    log.push(42);
  }

  f() {
    log.push(43);
  }
}

{
  using listener = new Test();
  g = () => listener;
}
g().f();

assertEquals(log, [42, 43]);

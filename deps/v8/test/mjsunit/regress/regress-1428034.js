// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --stress-lazy-source-positions

eval(`
var f = (
  a = function in_arrowhead_args() {
    return function inner() {
      b = 42;
    };
  },
  b = 1,
) => {
  assertEquals(1, b);
  a()();
  assertEquals(42, b);
};
f();
`);

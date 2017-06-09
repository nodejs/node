// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --min-semi-space-size=32

// We need to set --min-semi-space-size to enable allocation site pretenuring.

function foo(i) {
  with({}) {};
  x = {};
  x.a = 0.23;
  x.b = 0.3;
  return x;
}

var all = [];
function step() {
  for (var i = 0; i < 100; i++) {
    var z = foo(i);
    // Write unboxed double in object slack.
    z.c = 0.1 + z.b
    all.push(z);
  }
  gc(1);
  gc(1);
}

step();
// Now foo will allocate objects in old space.
step();

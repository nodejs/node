// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --deopt-every-n-times=1 --no-enable-sse4-1

function g(f, x, name) {
  var v2 = f(x);
  for (var i = 0; i < 13000; i++) {
    f(i);
  }
  var v1 = f(x);
  assertEquals(v1, v2);
}

g(Math.sin, 6.283185307179586, "Math.sin");
